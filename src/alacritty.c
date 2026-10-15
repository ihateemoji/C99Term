/* C99Term config loader.
 * TOML subset (Alacritty >= 0.13) and a YAML subset (older Alacritty).
 * Window, scrolling, font, colors, cursor, selection, shell, env and bindings.
 */
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifdef ALA_NO_X11
#define ShiftMask   (1<<0)
#define ControlMask (1<<2)
#define Mod1Mask    (1<<3)
#define Mod4Mask    (1<<6)
#define Button1 1
#define Button2 2
#define Button3 3
#define Button4 4
#define Button5 5
#define XK_A 0x41
#define XK_a 0x61
#define XK_0 0x30
#define XK_C 0x43
#define XK_V 0x56
#define XK_Prior 0xff55
#define XK_Next 0xff56
#define XK_Home 0xff50
#define XK_End 0xff57
#define XK_Insert 0xff63
#define XK_Escape 0xff1b
#define XK_space 0x20
#define XK_Tab 0xff09
#define XK_Return 0xff0d
#define XK_BackSpace 0xff08
#define XK_Delete 0xffff
#define XK_Up 0xff52
#define XK_Down 0xff54
#define XK_Left 0xff51
#define XK_Right 0xff53
#define XK_equal 0x3d
#define XK_plus 0x2b
#define XK_minus 0x2d
#define XK_KP_Add 0xffab
#define XK_KP_Subtract 0xffad
#else
#include <X11/X.h>
#include <X11/keysym.h>
#endif

#include "alacritty.h"

#define MAX_KV 2048
#define KEYLEN 192
#define VALLEN 1024

static int
strcaseeq(const char *a, const char *b)
{
	unsigned char ca, cb;

	if (!a || !b)
		return 0;
	while (*a && *b) {
		ca = (unsigned char)*a++;
		cb = (unsigned char)*b++;
		if (tolower(ca) != tolower(cb))
			return 0;
	}
	return *a == 0 && *b == 0;
}

typedef struct {
	char key[KEYLEN];
	char val[VALLEN];
	int is_str;
} Kv;

static Kv kvs[MAX_KV];
static int nkv;

static int looks_yaml(const char *path, const char *buf);
static void parse_yaml(const char *text);

static void
kvset(const char *key, const char *val, int is_str)
{
	int i;

	for (i = 0; i < nkv; i++) {
		if (!strcmp(kvs[i].key, key)) {
			snprintf(kvs[i].val, VALLEN, "%s", val);
			kvs[i].is_str = is_str;
			return;
		}
	}
	if (nkv >= MAX_KV)
		return;
	snprintf(kvs[nkv].key, KEYLEN, "%s", key);
	snprintf(kvs[nkv].val, VALLEN, "%s", val);
	kvs[nkv].is_str = is_str;
	nkv++;
}

static const char *
kvget(const char *key)
{
	int i;

	for (i = nkv - 1; i >= 0; i--)
		if (!strcmp(kvs[i].key, key))
			return kvs[i].val;
	return NULL;
}

static void
expand_home(const char *in, char *out, size_t outlen)
{
	const char *home;

	if (in[0] == '~' && (in[1] == '/' || in[1] == '\0')) {
		home = getenv("HOME");
		if (!home) {
			struct passwd *pw = getpwuid(getuid());
			home = pw ? pw->pw_dir : "";
		}
		snprintf(out, outlen, "%s%s", home, in + 1);
	} else {
		snprintf(out, outlen, "%s", in);
	}
}

static int
unescape(const char *in, char *out, size_t outlen)
{
	size_t o = 0;

	while (*in && o + 1 < outlen) {
		if (*in == '\\' && in[1]) {
			in++;
			switch (*in) {
			case 'n': out[o++] = '\n'; break;
			case 't': out[o++] = '\t'; break;
			case 'r': out[o++] = '\r'; break;
			case '\\': out[o++] = '\\'; break;
			case '"': out[o++] = '"'; break;
			case 'u': {
				unsigned int cp = 0;
				int k;
				in++;
				for (k = 0; k < 4 && isxdigit((unsigned char)in[k]); k++)
					cp = cp * 16 + (isdigit((unsigned char)in[k])
						? in[k] - '0'
						: tolower((unsigned char)in[k]) - 'a' + 10);
				in += k - 1;
				if (cp < 0x80) {
					out[o++] = (char)cp;
				} else if (cp < 0x800 && o + 2 < outlen) {
					out[o++] = 0xc0 | (cp >> 6);
					out[o++] = 0x80 | (cp & 0x3f);
				} else if (o + 3 < outlen) {
					out[o++] = 0xe0 | (cp >> 12);
					out[o++] = 0x80 | ((cp >> 6) & 0x3f);
					out[o++] = 0x80 | (cp & 0x3f);
				}
				break;
			}
			default:
				out[o++] = *in;
			}
			in++;
		} else {
			out[o++] = *in++;
		}
	}
	out[o] = 0;
	return (int)o;
}

/* Recursive-descent over a buffer. prefix is the current table path. */
static const char *parse_value(const char *p, const char *end,
                               const char *prefix, int bind_idx);

static void
skip(const char **p, const char *end)
{
	const char *s = *p;

	for (;;) {
		while (s < end && isspace((unsigned char)*s))
			s++;
		if (s < end && *s == '#') {
			while (s < end && *s != '\n')
				s++;
			continue;
		}
		break;
	}
	*p = s;
}

static int
parse_key(const char **p, const char *end, char *out, size_t outlen)
{
	const char *s = *p;
	size_t n = 0;

	skip(&s, end);
	if (s < end && *s == '"') {
		s++;
		while (s < end && *s != '"' && n + 1 < outlen) {
			if (*s == '\\' && s + 1 < end)
				out[n++] = *++s;
			else
				out[n++] = *s;
			s++;
		}
		if (s < end && *s == '"')
			s++;
		out[n] = 0;
		*p = s;
		return n > 0;
	}
	while (s < end && (isalnum((unsigned char)*s) || *s == '_' || *s == '-')
	       && n + 1 < outlen)
		out[n++] = *s++;
	out[n] = 0;
	*p = s;
	return n > 0;
}

static void
join_key(char *dst, size_t dstlen, const char *prefix, const char *key)
{
	if (prefix && prefix[0])
		snprintf(dst, dstlen, "%s.%s", prefix, key);
	else
		snprintf(dst, dstlen, "%s", key);
}

static const char *
parse_string(const char *p, const char *end, char *out, size_t outlen)
{
	char raw[VALLEN * 2];
	size_t n = 0;
	int multi = 0;

	if (p + 2 < end && p[0] == '"' && p[1] == '"' && p[2] == '"') {
		multi = 1;
		p += 3;
		if (p < end && *p == '\n')
			p++;
	} else if (p < end && *p == '"') {
		p++;
	} else if (p < end && *p == '\'') {
		p++;
		while (p < end && *p != '\'' && n + 1 < sizeof raw)
			raw[n++] = *p++;
		if (p < end && *p == '\'')
			p++;
		raw[n] = 0;
		snprintf(out, outlen, "%s", raw);
		return p;
	} else {
		out[0] = 0;
		return p;
	}

	if (multi) {
		while (p + 2 < end && !(p[0] == '"' && p[1] == '"' && p[2] == '"')
		       && n + 1 < sizeof raw)
			raw[n++] = *p++;
		if (p + 2 < end)
			p += 3;
	} else {
		while (p < end && *p != '"' && n + 1 < sizeof raw) {
			if (*p == '\\' && p + 1 < end) {
				raw[n++] = *p++;
				if (n + 1 < sizeof raw)
					raw[n++] = *p++;
				continue;
			}
			raw[n++] = *p++;
		}
		if (p < end && *p == '"')
			p++;
	}
	raw[n] = 0;
	unescape(raw, out, outlen);
	return p;
}

static const char *
parse_array(const char *p, const char *end, const char *fullkey)
{
	int i = 0;
	char elemkey[KEYLEN], val[VALLEN];

	p++; /* [ */
	skip(&p, end);
	while (p < end && *p != ']') {
		skip(&p, end);
		if (p >= end || *p == ']')
			break;
		if (*p == '"' || *p == '\'') {
			p = parse_string(p, end, val, sizeof val);
			snprintf(elemkey, sizeof elemkey, "%s.%d", fullkey, i++);
			kvset(elemkey, val, 1);
		} else {
			/* nested array/table not needed for alacritty imports */
			while (p < end && *p != ',' && *p != ']')
				p++;
		}
		skip(&p, end);
		if (p < end && *p == ',')
			p++;
	}
	if (p < end && *p == ']')
		p++;
	snprintf(elemkey, sizeof elemkey, "%s.len", fullkey);
	snprintf(val, sizeof val, "%d", i);
	kvset(elemkey, val, 0);
	return p;
}

static const char *
parse_inline_table(const char *p, const char *end, const char *prefix)
{
	char key[96], full[KEYLEN];

	p++; /* { */
	skip(&p, end);
	while (p < end && *p != '}') {
		if (!parse_key(&p, end, key, sizeof key))
			break;
		skip(&p, end);
		if (p < end && *p == '=')
			p++;
		join_key(full, sizeof full, prefix, key);
		p = parse_value(p, end, full, -1);
		skip(&p, end);
		if (p < end && *p == ',')
			p++;
		skip(&p, end);
	}
	if (p < end && *p == '}')
		p++;
	return p;
}

static const char *
parse_value(const char *p, const char *end, const char *prefix, int bind_idx)
{
	char val[VALLEN];
	char keyused[KEYLEN];

	(void)bind_idx;
	skip(&p, end);
	if (p >= end)
		return p;
	snprintf(keyused, sizeof keyused, "%s", prefix ? prefix : "");
	if (*p == '"') {
		p = parse_string(p, end, val, sizeof val);
		kvset(keyused, val, 1);
	} else if (*p == '\'') {
		p = parse_string(p, end, val, sizeof val);
		kvset(keyused, val, 1);
	} else if (*p == '{') {
		p = parse_inline_table(p, end, keyused);
	} else if (*p == '[') {
		p = parse_array(p, end, keyused);
	} else {
		size_t n = 0;
		while (p < end && *p != '\n' && *p != ',' && *p != '}' && *p != ']'
		       && *p != '#' && n + 1 < sizeof val) {
			if (isspace((unsigned char)*p))
				break;
			val[n++] = *p++;
		}
		val[n] = 0;
		kvset(keyused, val, 0);
	}
	return p;
}

static void
parse_toml(const char *text, const char *basedir, int depth);

static char loaded_paths[32][4096];
static int nloaded;

static int
load_file(const char *path, int depth)
{
	FILE *f;
	char *buf;
	long len;
	char dir[4096], *slash;
	int i;

	if (depth > 16 || !path || !path[0])
		return 0;
	for (i = 0; i < nloaded; i++)
		if (!strcmp(loaded_paths[i], path))
			return 1;
	if (nloaded < 32)
		snprintf(loaded_paths[nloaded++], 4096, "%s", path);
	f = fopen(path, "r");
	if (!f)
		return 0;
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return 0;
	}
	len = ftell(f);
	if (len < 0 || len > 8 * 1024 * 1024) {
		fclose(f);
		return 0;
	}
	rewind(f);
	buf = malloc((size_t)len + 1);
	if (!buf) {
		fclose(f);
		return 0;
	}
	if (fread(buf, 1, (size_t)len, f) != (size_t)len) {
		free(buf);
		fclose(f);
		return 0;
	}
	buf[len] = 0;
	fclose(f);
	snprintf(dir, sizeof dir, "%s", path);
	slash = strrchr(dir, '/');
	if (slash)
		*slash = 0;
	else
		snprintf(dir, sizeof dir, ".");
	if (looks_yaml(path, buf))
		parse_yaml(buf);
	else
		parse_toml(buf, dir, depth);
	free(buf);
	return 1;
}

static void
resolve_import(const char *spec, const char *basedir, char *out, size_t outlen)
{
	char exp[4096];

	expand_home(spec, exp, sizeof exp);
	if (exp[0] == '/')
		snprintf(out, outlen, "%s", exp);
	else
		snprintf(out, outlen, "%s/%s", basedir, exp);
}

static void
parse_toml(const char *text, const char *basedir, int depth)
{
	const char *p = text, *end = text + strlen(text);
	char table[KEYLEN] = "";
	char key[96], full[KEYLEN];
	int bind_idx = -1;
	int mouse_idx = -1;
	const char *lenstr;
	char ikey[KEYLEN], path[4096];
	int i, nimp;

	while (p < end) {
		skip(&p, end);
		if (p >= end)
			break;
		if (*p == '[') {
			int arr = 0;
			p++;
			if (p < end && *p == '[') {
				arr = 1;
				p++;
			}
			skip(&p, end);
			table[0] = 0;
			while (p < end && *p != ']') {
				char part[96];
				if (!parse_key(&p, end, part, sizeof part))
					break;
				if (table[0])
					strncat(table, ".", sizeof table - strlen(table) - 1);
				strncat(table, part, sizeof table - strlen(table) - 1);
				skip(&p, end);
				if (p < end && *p == '.')
					p++;
			}
			if (p < end && *p == ']')
				p++;
			if (arr && p < end && *p == ']')
				p++;
			if (arr && !strcmp(table, "keyboard.bindings")) {
				const char *cur = kvget("keyboard.bindings.len");
				bind_idx = cur ? atoi(cur) : 0;
				snprintf(full, sizeof full, "%d", bind_idx + 1);
				kvset("keyboard.bindings.len", full, 0);
				snprintf(table, sizeof table, "keyboard.bindings.%d", bind_idx);
			} else if (arr && !strcmp(table, "mouse.bindings")) {
				const char *cur = kvget("mouse.bindings.len");
				mouse_idx = cur ? atoi(cur) : 0;
				snprintf(full, sizeof full, "%d", mouse_idx + 1);
				kvset("mouse.bindings.len", full, 0);
				snprintf(table, sizeof table, "mouse.bindings.%d", mouse_idx);
			}
			continue;
		}
		if (!parse_key(&p, end, key, sizeof key)) {
			if (p < end)
				p++;
			continue;
		}
		/* dotted keys: window.opacity = 0.8, including --option snippets */
		skip(&p, end);
		while (p < end && *p == '.') {
			char part[96];
			p++;
			if (!parse_key(&p, end, part, sizeof part))
				break;
			if (strlen(key) + strlen(part) + 2 < sizeof key) {
				strcat(key, ".");
				strcat(key, part);
			}
			skip(&p, end);
		}
		if (p < end && *p == '=')
			p++;
		join_key(full, sizeof full, table, key);
		p = parse_value(p, end, full, bind_idx);
	}

	/* imports: top-level import and general.import, processed in order */
	lenstr = kvget("import.len");
	nimp = lenstr ? atoi(lenstr) : 0;
	for (i = 0; i < nimp; i++) {
		snprintf(ikey, sizeof ikey, "import.%d", i);
		if (!kvget(ikey) || !kvget(ikey)[0])
			continue;
		resolve_import(kvget(ikey), basedir, path, sizeof path);
		kvset(ikey, "", 1); /* don't reimport on recursive parse */
		load_file(path, depth + 1);
	}
	lenstr = kvget("general.import.len");
	nimp = lenstr ? atoi(lenstr) : 0;
	for (i = 0; i < nimp; i++) {
		snprintf(ikey, sizeof ikey, "general.import.%d", i);
		if (!kvget(ikey) || !kvget(ikey)[0])
			continue;
		resolve_import(kvget(ikey), basedir, path, sizeof path);
		kvset(ikey, "", 1);
		load_file(path, depth + 1);
	}
}

static int
join2(char *dst, size_t dstlen, const char *a, const char *b, char sep)
{
	size_t na, nb;

	if (!a || !b || dstlen == 0)
		return 0;
	na = strlen(a);
	nb = strlen(b);
	if (na + 1 + nb + 1 > dstlen)
		return 0;
	memcpy(dst, a, na);
	dst[na] = sep;
	memcpy(dst + na + 1, b, nb + 1);
	return 1;
}

static int
looks_yaml(const char *path, const char *buf)
{
	const char *ext = strrchr(path, '.');
	const char *p;

	if (ext && (strcaseeq(ext, ".yml") || strcaseeq(ext, ".yaml")))
		return 1;
	p = buf;
	while (*p && isspace((unsigned char)*p))
		p++;
	if (*p == '[')
		return 0;
	return strchr(buf, ':') != NULL && strchr(buf, '=') == NULL;
}

/* Indentation-based YAML subset used by older Alacritty configs. */
static void
parse_yaml(const char *text)
{
	const char *p = text;
	char stack[16][KEYLEN];
	int ind[16];
	int list_idx[16];
	int depth = 0;
	char line[1024];

	memset(ind, 0, sizeof ind);
	memset(list_idx, 0, sizeof list_idx);
	stack[0][0] = 0;
	while (*p) {
		const char *eol = strchr(p, '\n');
		int n, spaces = 0, i;
		char key[96], val[VALLEN], full[KEYLEN];
		const char *s;

		if (!eol)
			eol = p + strlen(p);
		n = (int)(eol - p);
		if (n >= (int)sizeof line)
			n = (int)sizeof line - 1;
		memcpy(line, p, (size_t)n);
		line[n] = 0;
		p = *eol ? eol + 1 : eol;

		s = line;
		while (*s == ' ') {
			spaces++;
			s++;
		}
		if (*s == '\0' || *s == '#' || *s == '\r')
			continue;
		while (depth > 0 && spaces <= ind[depth])
			depth--;
		if (*s == '-') {
			s++;
			while (*s == ' ')
				s++;
			if (depth > 0) {
				char num[16];
				snprintf(num, sizeof num, "%d", list_idx[depth]++);
				if (join2(full, sizeof full, stack[depth], num, '.')
				    && depth + 1 < 16) {
					depth++;
					memcpy(stack[depth], full, strlen(full) + 1);
					ind[depth] = spaces;
				}
			}
			if (*s == '\0')
				continue;
		}
		i = 0;
		while (s[i] && s[i] != ':' && i + 1 < (int)sizeof key)
			key[i] = s[i], i++;
		key[i] = 0;
		while (i > 0 && isspace((unsigned char)key[i - 1]))
			key[--i] = 0;
		if (s[i] != ':')
			continue;
		s += i + 1;
		while (*s == ' ')
			s++;
		if (*s == '"' || *s == '\'') {
			char q = *s++;
			i = 0;
			while (*s && *s != q && i + 1 < (int)sizeof val)
				val[i++] = *s++;
			val[i] = 0;
		} else {
			i = 0;
			while (*s && *s != '#' && *s != '\r' && i + 1 < (int)sizeof val)
				val[i++] = *s++;
			while (i > 0 && isspace((unsigned char)val[i - 1]))
				i--;
			val[i] = 0;
		}
		if (stack[depth][0]) {
			if (!join2(full, sizeof full, stack[depth], key, '.'))
				continue;
		} else {
			if (strlen(key) + 1 > sizeof full)
				continue;
			memcpy(full, key, strlen(key) + 1);
		}
		if (val[0] == 0) {
			if (depth + 1 < 16) {
				depth++;
				snprintf(stack[depth], KEYLEN, "%s", full);
				ind[depth] = spaces;
				list_idx[depth] = 0;
			}
		} else {
			kvset(full, val, 1);
		}
	}
}

static int
color_index(const char *name)
{
	static const char *names[] = {
		"black", "red", "green", "yellow", "blue", "magenta", "cyan", "white"
	};
	int i;

	for (i = 0; i < 8; i++)
		if (!strcmp(name, names[i]))
			return i;
	return -1;
}

static unsigned int
mods_of(const char *s)
{
	unsigned int m = 0;
	char buf[128], *tok, *save = NULL;

	if (!s)
		return 0;
	snprintf(buf, sizeof buf, "%s", s);
	for (tok = strtok_r(buf, "|+", &save); tok; tok = strtok_r(NULL, "|+", &save)) {
		while (*tok == ' ')
			tok++;
		if (strcaseeq(tok, "Control") || strcaseeq(tok, "Ctrl"))
			m |= ControlMask;
		else if (strcaseeq(tok, "Shift"))
			m |= ShiftMask;
		else if (strcaseeq(tok, "Alt") || strcaseeq(tok, "Option"))
			m |= Mod1Mask;
		else if (strcaseeq(tok, "Super") || strcaseeq(tok, "Logo")
		         || strcaseeq(tok, "Command"))
			m |= Mod4Mask;
	}
	return m;
}

static unsigned int
keysym_of(const char *s)
{
	if (!s || !s[0])
		return 0;
	if (!strcmp(s, "Copy"))
		return XK_C; /* paired with Control|Shift by alacritty defaults */
	if (!strcmp(s, "Paste"))
		return XK_V;
	if (!strcmp(s, "PageUp"))
		return XK_Prior;
	if (!strcmp(s, "PageDown"))
		return XK_Next;
	if (!strcmp(s, "Home"))
		return XK_Home;
	if (!strcmp(s, "End"))
		return XK_End;
	if (!strcmp(s, "Insert"))
		return XK_Insert;
	if (!strcmp(s, "Escape"))
		return XK_Escape;
	if (!strcmp(s, "Space"))
		return XK_space;
	if (!strcmp(s, "Tab"))
		return XK_Tab;
	if (!strcmp(s, "Return") || !strcmp(s, "Enter"))
		return XK_Return;
	if (!strcmp(s, "Back"))
		return XK_BackSpace;
	if (!strcmp(s, "Delete"))
		return XK_Delete;
	if (!strcmp(s, "Up"))
		return XK_Up;
	if (!strcmp(s, "Down"))
		return XK_Down;
	if (!strcmp(s, "Left"))
		return XK_Left;
	if (!strcmp(s, "Right"))
		return XK_Right;
	if (!strcmp(s, "Equals") || !strcmp(s, "="))
		return XK_equal;
	if (!strcmp(s, "Plus") || !strcmp(s, "+"))
		return XK_plus;
	if (!strcmp(s, "Minus") || !strcmp(s, "-"))
		return XK_minus;
	if (!strcmp(s, "NumpadAdd"))
		return XK_KP_Add;
	if (!strcmp(s, "NumpadSubtract"))
		return XK_KP_Subtract;
	if (!strcmp(s, "Key0") || !strcmp(s, "0"))
		return XK_0;
	if (strlen(s) == 1) {
		if (s[0] >= 'A' && s[0] <= 'Z')
			return XK_A + (s[0] - 'A');
		if (s[0] >= 'a' && s[0] <= 'z')
			return XK_a + (s[0] - 'a');
		if (s[0] >= '0' && s[0] <= '9')
			return XK_0 + (s[0] - '0');
	}
	return 0;
}

static unsigned int
button_of(const char *s)
{
	if (!s)
		return 0;
	if (!strcmp(s, "Left"))
		return Button1;
	if (!strcmp(s, "Middle"))
		return Button2;
	if (!strcmp(s, "Right"))
		return Button3;
	if (!strcmp(s, "WheelUp"))
		return Button4;
	if (!strcmp(s, "WheelDown"))
		return Button5;
	if (isdigit((unsigned char)s[0]))
		return (unsigned int)atoi(s);
	return 0;
}

static int
action_of(const char *s)
{
	if (!s)
		return ALA_NONE;
	if (!strcmp(s, "Copy"))
		return ALA_COPY;
	if (!strcmp(s, "Paste"))
		return ALA_PASTE;
	if (!strcmp(s, "PasteSelection") || !strcmp(s, "CopySelection"))
		return !strcmp(s, "CopySelection") ? ALA_COPY : ALA_PASTE_SEL;
	if (!strcmp(s, "IncreaseFontSize"))
		return ALA_FONT_INC;
	if (!strcmp(s, "DecreaseFontSize"))
		return ALA_FONT_DEC;
	if (!strcmp(s, "ResetFontSize"))
		return ALA_FONT_RESET;
	if (!strcmp(s, "ScrollPageUp"))
		return ALA_SCROLL_PAGE_UP;
	if (!strcmp(s, "ScrollPageDown"))
		return ALA_SCROLL_PAGE_DOWN;
	if (!strcmp(s, "ScrollHalfPageUp"))
		return ALA_SCROLL_HALF_UP;
	if (!strcmp(s, "ScrollHalfPageDown"))
		return ALA_SCROLL_HALF_DOWN;
	if (!strcmp(s, "ScrollLineUp"))
		return ALA_SCROLL_LINE_UP;
	if (!strcmp(s, "ScrollLineDown"))
		return ALA_SCROLL_LINE_DOWN;
	if (!strcmp(s, "ScrollToTop"))
		return ALA_SCROLL_TOP;
	if (!strcmp(s, "ScrollToBottom"))
		return ALA_SCROLL_BOTTOM;
	if (!strcmp(s, "ClearHistory"))
		return ALA_CLEAR_HIST;
	if (!strcmp(s, "CreateNewWindow"))
		return ALA_NEW_WINDOW;
	if (!strcmp(s, "ReceiveChar"))
		return ALA_RECEIVE;
	if (!strcmp(s, "None"))
		return ALA_NONE;
	return ALA_NONE;
}

static char *
dupstr(const char *s)
{
	return s ? strdup(s) : NULL;
}

/* Alacritty accepts 0xrrggbb and 0xrrggbbaa. XftColorAllocName only
 * accepts #rrggbb (or a named colour), so rewrite the 0x form. */
static char *
dupcolor(const char *s)
{
	char buf[16];
	const char *hex;
	size_t n;

	if (!s)
		return NULL;
	if (s[0] != '0' || (s[1] != 'x' && s[1] != 'X'))
		return strdup(s);
	hex = s + 2;
	n = strlen(hex);
	/* Keep rrggbb; drop a trailing alpha byte if present. */
	if (n != 6 && n != 8)
		return strdup(s);
	buf[0] = '#';
	memcpy(buf + 1, hex, 6);
	buf[7] = 0;
	return strdup(buf);
}

static void
apply_bindings(AlaConfig *cfg)
{
	const char *lenstr;
	int n, i;
	char key[KEYLEN];

	lenstr = kvget("keyboard.bindings.len");
	n = lenstr ? atoi(lenstr) : 0;
	for (i = 0; i < n && cfg->nbinds < ALA_MAX_BINDS; i++) {
		const char *k, *mods, *act, *chars;
		AlaBind *b = &cfg->binds[cfg->nbinds];

		snprintf(key, sizeof key, "keyboard.bindings.%d.key", i);
		k = kvget(key);
		snprintf(key, sizeof key, "keyboard.bindings.%d.mods", i);
		mods = kvget(key);
		snprintf(key, sizeof key, "keyboard.bindings.%d.action", i);
		act = kvget(key);
		snprintf(key, sizeof key, "keyboard.bindings.%d.chars", i);
		chars = kvget(key);
		if (!k)
			continue;
		memset(b, 0, sizeof *b);
		b->mods = mods_of(mods);
		b->keysym = keysym_of(k);
		if (!b->keysym)
			continue;
		if (chars) {
			b->action = ALA_CHARS;
			snprintf(b->chars, sizeof b->chars, "%s", chars);
		} else {
			b->action = action_of(act);
		}
		cfg->nbinds++;
	}
	lenstr = kvget("mouse.bindings.len");
	n = lenstr ? atoi(lenstr) : 0;
	for (i = 0; i < n && cfg->nbinds < ALA_MAX_BINDS; i++) {
		const char *m, *mods, *act;
		AlaBind *b = &cfg->binds[cfg->nbinds];

		snprintf(key, sizeof key, "mouse.bindings.%d.mouse", i);
		m = kvget(key);
		snprintf(key, sizeof key, "mouse.bindings.%d.mods", i);
		mods = kvget(key);
		snprintf(key, sizeof key, "mouse.bindings.%d.action", i);
		act = kvget(key);
		if (!m)
			continue;
		memset(b, 0, sizeof *b);
		b->button = button_of(m);
		b->mods = mods_of(mods);
		b->action = action_of(act);
		if (!b->button)
			continue;
		cfg->nbinds++;
	}
}

static void
fill_config(AlaConfig *cfg)
{
	const char *s;
	char font[512];
	const char *family = "monospace";
	const char *style = "Regular";
	const char *size = NULL;
	int i;

	cfg->opacity = -1;
	cfg->history = -1;
	cfg->multiplier = -1;
	cfg->pad_x = cfg->pad_y = -1;
	cfg->dynamic_padding = -1;
	cfg->decorations_none = -1;
	cfg->save_to_clipboard = -1;
	cfg->cursor_shape = 0;
	cfg->cursor_blink = -1;
	cfg->cursor_thickness = -1;

	cfg->columns = cfg->lines = -1;
	cfg->pos_x = cfg->pos_y = INT_MIN;
	cfg->startup_mode = -1;
	cfg->dynamic_title = -1;

	if ((s = kvget("window.opacity")))
		cfg->opacity = (float)atof(s);
	if ((s = kvget("window.dimensions.columns")) || (s = kvget("window.dimensions.cols")))
		cfg->columns = atoi(s);
	if ((s = kvget("window.dimensions.lines")) || (s = kvget("window.dimensions.rows")))
		cfg->lines = atoi(s);
	if ((s = kvget("window.position.x")))
		cfg->pos_x = atoi(s);
	if ((s = kvget("window.position.y")))
		cfg->pos_y = atoi(s);
	if ((s = kvget("window.startup_mode"))) {
		if (strcaseeq(s, "Maximized"))
			cfg->startup_mode = ALA_START_MAXIMIZED;
		else if (strcaseeq(s, "Fullscreen"))
			cfg->startup_mode = ALA_START_FULLSCREEN;
		else
			cfg->startup_mode = ALA_START_WINDOWED;
	}
	if ((s = kvget("window.title")))
		cfg->title = dupstr(s);
	if ((s = kvget("window.dynamic_title")))
		cfg->dynamic_title = (strcaseeq(s, "true") || !strcmp(s, "1"));
	if ((s = kvget("scrolling.history")))
		cfg->history = atoi(s);
	if ((s = kvget("scrolling.multiplier")))
		cfg->multiplier = atoi(s);
	if ((s = kvget("window.padding.x")))
		cfg->pad_x = atoi(s);
	if ((s = kvget("window.padding.y")))
		cfg->pad_y = atoi(s);
	if ((s = kvget("window.dynamic_padding")))
		cfg->dynamic_padding = (strcaseeq(s, "true") || !strcmp(s, "1"));
	if ((s = kvget("window.decorations")))
		cfg->decorations_none = strcaseeq(s, "None");
	if ((s = kvget("selection.save_to_clipboard")))
		cfg->save_to_clipboard = (strcaseeq(s, "true") || !strcmp(s, "1"));
	if ((s = kvget("selection.semantic_escape_chars")))
		cfg->worddelims = dupstr(s);
	if ((s = kvget("cursor.style.shape")) || (s = kvget("cursor.style"))) {
		if (strcaseeq(s, "Underline"))
			cfg->cursor_shape = 4;
		else if (strcaseeq(s, "Beam"))
			cfg->cursor_shape = 6;
		else if (strcaseeq(s, "Block"))
			cfg->cursor_shape = 2;
	}
	if ((s = kvget("cursor.style.blinking"))) {
		if (strcaseeq(s, "On") || strcaseeq(s, "Always"))
			cfg->cursor_blink = 1;
		else
			cfg->cursor_blink = 0;
	}
	if ((s = kvget("cursor.thickness")))
		cfg->cursor_thickness = (float)atof(s);
	if ((s = kvget("font.normal.family")))
		family = s;
	if ((s = kvget("font.normal.style")))
		style = s;
	if ((s = kvget("font.size")))
		size = s;
	if (kvget("font.normal.family") || size) {
		if (size)
			snprintf(font, sizeof font,
			         "%s:size=%s:style=%s:antialias=true", family, size, style);
		else
			snprintf(font, sizeof font,
			         "%s:style=%s:antialias=true", family, style);
		cfg->font = dupstr(font);
	}
	cfg->fg = dupcolor(kvget("colors.primary.foreground"));
	cfg->bg = dupcolor(kvget("colors.primary.background"));
	cfg->cursor_color = dupcolor(kvget("colors.cursor.cursor"));
	cfg->cursor_text = dupcolor(kvget("colors.cursor.text"));
	for (i = 0; i < 8; i++) {
		char k[64];
		static const char *names[] = {
			"black", "red", "green", "yellow",
			"blue", "magenta", "cyan", "white"
		};
		snprintf(k, sizeof k, "colors.normal.%s", names[i]);
		cfg->normal[i] = dupcolor(kvget(k));
		snprintf(k, sizeof k, "colors.bright.%s", names[i]);
		cfg->bright[i] = dupcolor(kvget(k));
		(void)color_index;
	}
	if ((s = kvget("terminal.shell")))
		cfg->shell = dupstr(s);
	else if ((s = kvget("terminal.shell.program")))
		cfg->shell = dupstr(s);
	for (i = 0; i < ALA_MAX_ARGS; i++) {
		char k[64];
		snprintf(k, sizeof k, "terminal.shell.args.%d", i);
		if (!kvget(k))
			break;
		cfg->shell_args[cfg->shell_argc++] = dupstr(kvget(k));
	}
	if ((s = kvget("terminal.working_directory")) || (s = kvget("working_directory")))
		cfg->workdir = dupstr(s);
	if ((s = kvget("env.TERM")))
		cfg->term = dupstr(s);
	for (i = 0; i < nkv; i++) {
		if (!strncmp(kvs[i].key, "env.", 4) && kvs[i].val[0])
			setenv(kvs[i].key + 4, kvs[i].val, 1);
	}
	apply_bindings(cfg);
	cfg->loaded = 1;
}

static int
first_existing(char *out, size_t outlen)
{
	const char *xdg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	char buf[4096];
	const char *cands[12];
	int i, n = 0;

	if (!home)
		home = "";
	if (xdg && xdg[0]) {
		snprintf(buf, sizeof buf, "%s/c99term/c99term.toml", xdg);
		cands[n++] = strdup(buf);
		snprintf(buf, sizeof buf, "%s/c99term/c99term.yml", xdg);
		cands[n++] = strdup(buf);
		snprintf(buf, sizeof buf, "%s/alacritty/alacritty.toml", xdg);
		cands[n++] = strdup(buf);
		snprintf(buf, sizeof buf, "%s/alacritty/alacritty.yml", xdg);
		cands[n++] = strdup(buf);
		snprintf(buf, sizeof buf, "%s/alacritty.toml", xdg);
		cands[n++] = strdup(buf);
	}
	snprintf(buf, sizeof buf, "%s/.config/c99term/c99term.toml", home);
	cands[n++] = strdup(buf);
	snprintf(buf, sizeof buf, "%s/.config/c99term/c99term.yml", home);
	cands[n++] = strdup(buf);
	snprintf(buf, sizeof buf, "%s/.config/alacritty/alacritty.toml", home);
	cands[n++] = strdup(buf);
	snprintf(buf, sizeof buf, "%s/.config/alacritty/alacritty.yml", home);
	cands[n++] = strdup(buf);
	snprintf(buf, sizeof buf, "%s/.alacritty.toml", home);
	cands[n++] = strdup(buf);
	cands[n++] = strdup("/etc/alacritty/alacritty.toml");
	for (i = 0; i < n; i++) {
		if (cands[i] && access(cands[i], R_OK) == 0) {
			snprintf(out, outlen, "%s", cands[i]);
			return 1;
		}
	}
	return 0;
}

/* If ~/.config/c99term does not exist, create it and copy the first
 * Alacritty config we can find. We copy, we do not delete the original. */
static int
ensure_c99term_dir(char *out, size_t outlen, char *migrated, size_t miglen)
{
	const char *xdg = getenv("XDG_CONFIG_HOME");
	const char *home = getenv("HOME");
	char dir[4096], dest[4112], src[4096];
	struct stat st;
	FILE *in, *o;
	char buf[4096];
	size_t n;

	if (!home)
		home = "";
	if (xdg && xdg[0])
		snprintf(dir, sizeof dir, "%s/c99term", xdg);
	else
		snprintf(dir, sizeof dir, "%s/.config/c99term", home);
	if (!join2(dest, sizeof dest, dir, "c99term.toml", '/'))
		return 0;
	if (stat(dir, &st) == 0 && S_ISDIR(st.st_mode)) {
		if (access(dest, R_OK) == 0) {
			snprintf(out, outlen, "%s", dest);
			return 1;
		}
		if (!join2(dest, sizeof dest, dir, "c99term.yml", '/'))
			return 0;
		if (access(dest, R_OK) == 0) {
			snprintf(out, outlen, "%s", dest);
			return 1;
		}
		return 0;
	}
	mkdir(dir, 0755);
	/* parent .config may be missing */
	if (stat(dir, &st) != 0) {
		char parent[4096];
		snprintf(parent, sizeof parent, "%s/.config", home);
		mkdir(parent, 0755);
		mkdir(dir, 0755);
	}
	if (!first_existing(src, sizeof src))
		return 0;
	/* first_existing now prefers c99term, which we just created empty.
	 * Walk alacritty paths explicitly. */
	src[0] = 0;
	{
		const char *try[] = {
			NULL, NULL, NULL, NULL, NULL
		};
		char a[4096], b[4096], c[4096], d[4096];
		int i;
		if (xdg && xdg[0]) {
			snprintf(a, sizeof a, "%s/alacritty/alacritty.toml", xdg);
			snprintf(b, sizeof b, "%s/alacritty/alacritty.yml", xdg);
		} else {
			a[0] = b[0] = 0;
		}
		snprintf(c, sizeof c, "%s/.config/alacritty/alacritty.toml", home);
		snprintf(d, sizeof d, "%s/.config/alacritty/alacritty.yml", home);
		try[0] = a[0] ? a : NULL;
		try[1] = b[0] ? b : NULL;
		try[2] = c;
		try[3] = d;
		try[4] = "/etc/alacritty/alacritty.toml";
		for (i = 0; i < 5; i++) {
			if (try[i] && access(try[i], R_OK) == 0) {
				snprintf(src, sizeof src, "%s", try[i]);
				break;
			}
		}
	}
	if (!src[0])
		return 0;
	in = fopen(src, "r");
	if (!in)
		return 0;
	{
		const char *base = "c99term.toml";
		size_t sl = strlen(src);
		if ((sl >= 5 && !strcmp(src + sl - 5, ".yaml"))
		    || (sl >= 4 && !strcmp(src + sl - 4, ".yml")))
			base = "c99term.yml";
		if (!join2(dest, sizeof dest, dir, base, '/')) {
			fclose(in);
			return 0;
		}
	}
	o = fopen(dest, "w");
	if (!o) {
		fclose(in);
		return 0;
	}
	while ((n = fread(buf, 1, sizeof buf, in)) > 0)
		fwrite(buf, 1, n, o);
	fclose(in);
	fclose(o);
	snprintf(out, outlen, "%s", dest);
	if (migrated)
		snprintf(migrated, miglen, "%s", src);
	return 1;
}

void
alacritty_materialize(const AlaConfig *cfg, AlaApplied *out)
{
	memset(out, 0, sizeof *out);
	out->opacity = cfg->opacity >= 0 ? cfg->opacity : 1.f;
	out->history = cfg->history >= 0 ? cfg->history : 10000;
	out->multiplier = cfg->multiplier > 0 ? cfg->multiplier : 3;
	out->pad_x = cfg->pad_x >= 0 ? cfg->pad_x : 0;
	out->pad_y = cfg->pad_y >= 0 ? cfg->pad_y : 0;
	out->dynamic_padding = cfg->dynamic_padding > 0;
	out->decorations_none = cfg->decorations_none > 0;
	out->save_to_clipboard = cfg->save_to_clipboard > 0;
	out->columns = cfg->columns;
	out->lines = cfg->lines;
	out->pos_x = cfg->pos_x;
	out->pos_y = cfg->pos_y;
	out->startup_mode = cfg->startup_mode;
	out->dynamic_title = cfg->dynamic_title > 0;
	if (cfg->title)
		snprintf(out->title, sizeof out->title, "%s", cfg->title);
	if (cfg->font)
		snprintf(out->font, sizeof out->font, "%s", cfg->font);
	if (cfg->fg)
		snprintf(out->fg, sizeof out->fg, "%s", cfg->fg);
	if (cfg->bg)
		snprintf(out->bg, sizeof out->bg, "%s", cfg->bg);
}

void
alacritty_load(AlaConfig *cfg, const char *config_file,
               char **overrides, int noverrides)
{
	char path[4096];
	int i;

	nkv = 0;
	memset(kvs, 0, sizeof kvs);
	path[0] = 0;
	cfg->migrated_from[0] = 0;
	if (config_file && config_file[0]) {
		expand_home(config_file, path, sizeof path);
	} else if (!ensure_c99term_dir(path, sizeof path,
	                               cfg->migrated_from, sizeof cfg->migrated_from)
	           && !first_existing(path, sizeof path)) {
		path[0] = 0;
	}
	if (path[0] && load_file(path, 0)) {
		snprintf(cfg->path, sizeof cfg->path, "%s", path);
		cfg->loaded = 1;
	}
	for (i = 0; i < noverrides; i++) {
		if (overrides[i])
			parse_toml(overrides[i], ".", 0);
	}
	if (nkv)
		fill_config(cfg);
}

void
alacritty_free(AlaConfig *cfg)
{
	int i;

	free(cfg->font);
	free(cfg->fg);
	free(cfg->bg);
	free(cfg->cursor_color);
	free(cfg->cursor_text);
	free(cfg->shell);
	free(cfg->workdir);
	free(cfg->term);
	free(cfg->worddelims);
	free(cfg->title);
	for (i = 0; i < 8; i++) {
		free(cfg->normal[i]);
		free(cfg->bright[i]);
	}
	for (i = 0; i < cfg->shell_argc; i++)
		free(cfg->shell_args[i]);
}
