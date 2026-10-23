/* C99Term config tests.
 * Agent-friendly: one line per check, PASS/FAIL, non-zero exit on failure.
 * Build: make test
 * These tests do not need X11. They fail if a window parameter is parsed
 * and then dropped by alacritty_materialize.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "alacritty.h"

static int failed;
static int passed;

static void
check(const char *name, int cond)
{
	if (cond) {
		printf("PASS  %s\n", name);
		passed++;
	} else {
		printf("FAIL  %s\n", name);
		failed++;
	}
}

static void
write_file(const char *path, const char *text)
{
	FILE *f = fopen(path, "w");
	if (!f) {
		perror(path);
		exit(1);
	}
	fputs(text, f);
	fclose(f);
}

static void
test_toml_window(void)
{
	AlaConfig cfg;
	AlaApplied a;
	char *ov[] = { "window.opacity=0.5" };

	memset(&cfg, 0, sizeof cfg);
	alacritty_load(&cfg, "config/example.toml", ov, 1);
	alacritty_materialize(&cfg, &a);

	check("toml loaded", cfg.loaded);
	check("toml opacity parsed", cfg.opacity > 0.49 && cfg.opacity < 0.51);
	check("toml opacity not dropped", a.opacity > 0.49 && a.opacity < 0.51);
	check("toml padding x", cfg.pad_x == 8 && a.pad_x == 8);
	check("toml padding y", cfg.pad_y == 4 && a.pad_y == 4);
	check("toml dynamic_padding", cfg.dynamic_padding == 1 && a.dynamic_padding == 1);
	check("toml decorations none", cfg.decorations_none == 1 && a.decorations_none == 1);
	check("toml columns", cfg.columns == 100 && a.columns == 100);
	check("toml lines", cfg.lines == 30 && a.lines == 30);
	check("toml position x", cfg.pos_x == 40 && a.pos_x == 40);
	check("toml position y", cfg.pos_y == 40 && a.pos_y == 40);
	check("toml startup windowed", cfg.startup_mode == ALA_START_WINDOWED);
	check("toml title", cfg.title && !strcmp(cfg.title, "C99Term"));
	check("toml title materialized", !strcmp(a.title, "C99Term"));
	check("toml history", cfg.history == 20000 && a.history == 20000);
	check("toml multiplier", cfg.multiplier == 5 && a.multiplier == 5);
	check("toml font", cfg.font && strstr(cfg.font, "monospace") && strstr(cfg.font, "13"));
	check("toml bg", cfg.bg && !strcmp(cfg.bg, "#1d2021") && !strcmp(a.bg, "#1d2021"));
	check("toml fg", cfg.fg && !strcmp(cfg.fg, "#d5c4a1"));
	check("toml normal black", cfg.normal[0] && !strcmp(cfg.normal[0], "#282828"));
	check("toml cursor beam", cfg.cursor_shape == 6);
	check("toml cursor blink off", cfg.cursor_blink == 0);
	check("toml save_to_clipboard", cfg.save_to_clipboard == 1);
	check("toml shell", cfg.shell && !strcmp(cfg.shell, "/bin/sh"));
	check("toml shell arg", cfg.shell_argc == 1 && !strcmp(cfg.shell_args[0], "-l"));
	alacritty_free(&cfg);
}

static void
test_yaml_window(void)
{
	AlaConfig cfg;
	AlaApplied a;

	memset(&cfg, 0, sizeof cfg);
	alacritty_load(&cfg, "config/example.yml", NULL, 0);
	alacritty_materialize(&cfg, &a);

	check("yaml loaded", cfg.loaded);
	check("yaml opacity", cfg.opacity > 0.89 && cfg.opacity < 0.91 && a.opacity > 0.89);
	check("yaml padding", cfg.pad_x == 8 && cfg.pad_y == 4 && a.pad_x == 8);
	check("yaml dimensions", cfg.columns == 100 && cfg.lines == 30);
	check("yaml position", cfg.pos_x == 40 && cfg.pos_y == 40);
	check("yaml decorations", cfg.decorations_none == 1);
	check("yaml history", cfg.history == 20000);
	check("yaml font family", cfg.font && strstr(cfg.font, "monospace"));
	check("yaml bg", cfg.bg && !strcmp(cfg.bg, "#1d2021"));
	check("yaml cursor", cfg.cursor_shape == 6);
	check("yaml title kept", a.title[0] && !strcmp(a.title, "C99Term"));
	alacritty_free(&cfg);
}

static void
test_params_not_ignored(void)
{
	AlaConfig cfg;
	AlaApplied a;
	const char *toml =
		"window.opacity=0.42\n"
		"window.padding.x=3\n"
		"window.padding.y=9\n"
		"window.dimensions.columns=120\n"
		"window.dimensions.lines=40\n"
		"window.position.x=7\n"
		"window.position.y=11\n"
		"window.decorations=\"None\"\n"
		"window.dynamic_padding=true\n"
		"scrolling.history=1234\n"
		"scrolling.multiplier=7\n";
    char *ov[1];

	write_file("/tmp/c99term-params.toml", toml);
	memset(&cfg, 0, sizeof cfg);
	ov[0] = "window.opacity=0.42";
	alacritty_load(&cfg, "/tmp/c99term-params.toml", NULL, 0);
	alacritty_materialize(&cfg, &a);

	check("override-free opacity kept", a.opacity > 0.41 && a.opacity < 0.43);
	check("pad x not ignored", a.pad_x == 3);
	check("pad y not ignored", a.pad_y == 9);
	check("columns not ignored", a.columns == 120);
	check("lines not ignored", a.lines == 40);
	check("pos x not ignored", a.pos_x == 7);
	check("pos y not ignored", a.pos_y == 11);
	check("decorations not ignored", a.decorations_none == 1);
	check("dynamic_padding not ignored", a.dynamic_padding == 1);
	check("history not ignored", a.history == 1234);
	check("multiplier not ignored", a.multiplier == 7);
	/* A field that was set must not come back as the unset sentinel. */
	check("columns sentinel cleared", cfg.columns != -1);
	check("pos sentinel cleared", cfg.pos_x != (-2147483647 - 1));
	alacritty_free(&cfg);
}

static void
test_bindings_and_import(void)
{
	AlaConfig cfg;

	write_file("/tmp/c99term-theme.toml",
	           "[colors.bright]\nblack = \"#928374\"\n");
	write_file("/tmp/c99term-main.toml",
	           "import = [\"/tmp/c99term-theme.toml\"]\n"
	           "[[keyboard.bindings]]\n"
	           "key = \"N\"\n"
	           "mods = \"Control|Shift\"\n"
	           "action = \"CreateNewWindow\"\n");
	memset(&cfg, 0, sizeof cfg);
	alacritty_load(&cfg, "/tmp/c99term-main.toml", NULL, 0);
	check("import bright black", cfg.bright[0] && !strcmp(cfg.bright[0], "#928374"));
	check("binding count", cfg.nbinds == 1);
	check("binding mods control|shift", cfg.binds[0].mods == ((1u << 2) | (1u << 0)));
	check("binding action new window", cfg.binds[0].action == ALA_NEW_WINDOW);
	alacritty_free(&cfg);
}

static void
test_missing_and_comments(void)
{
	AlaConfig cfg;
	AlaApplied a;

	memset(&cfg, 0, sizeof cfg);
	alacritty_load(&cfg, "/tmp/does-not-exist-c99term.toml", NULL, 0);
	check("missing file does not crash", 1);
	check("missing file not loaded", !cfg.loaded);

	write_file("/tmp/c99term-comments.toml",
	           "# a comment\n"
	           "[window]\n"
	           "opacity = 0.7  # trailing\n"
	           "padding = { x = 1, y = 2 }\n");
	memset(&cfg, 0, sizeof cfg);
	alacritty_load(&cfg, "/tmp/c99term-comments.toml", NULL, 0);
	alacritty_materialize(&cfg, &a);
	check("commented opacity", a.opacity > 0.69 && a.opacity < 0.71);
	check("inline padding", a.pad_x == 1 && a.pad_y == 2);
	alacritty_free(&cfg);
}

static void
test_maximized(void)
{
	AlaConfig cfg;

	write_file("/tmp/c99term-max.toml",
	           "[window]\nstartup_mode = \"Maximized\"\ndecorations = \"Full\"\n");
	memset(&cfg, 0, sizeof cfg);
	alacritty_load(&cfg, "/tmp/c99term-max.toml", NULL, 0);
	check("maximized mode", cfg.startup_mode == ALA_START_MAXIMIZED);
	check("full decorations not none", cfg.decorations_none == 0);
	alacritty_free(&cfg);
}

static void
test_ox_colors(void)
{
	AlaConfig cfg;

	write_file("/tmp/c99term-ox.yml",
	           "colors:\n"
	           "  primary:\n"
	           "    background: '0x1d1f21'\n"
	           "    foreground: '0xc5c8c6ff'\n"
	           "  normal:\n"
	           "    red: '0xcc6666'\n");
	memset(&cfg, 0, sizeof cfg);
	alacritty_load(&cfg, "/tmp/c99term-ox.yml", NULL, 0);
	check("0x bg rewritten to hash", cfg.bg && !strcmp(cfg.bg, "#1d1f21"));
	check("0x fg drops alpha", cfg.fg && !strcmp(cfg.fg, "#c5c8c6"));
	check("0x normal red rewritten", cfg.normal[1] && !strcmp(cfg.normal[1], "#cc6666"));
	alacritty_free(&cfg);
}

static void
test_clamping_and_security(void)
{
	AlaConfig cfg;
	AlaApplied a;
	char *ov_huge[] = { "scrolling.history=999999999999",
	                    "window.dimensions.columns=100000",
	                    "window.dimensions.lines=-5",
	                    "scrolling.multiplier=1000",
	                    "window.padding.x=-10" };
	char *ov_env[] = { "env.PATH=/evil", "env.LD_PRELOAD=bad.so",
	                   "env.SAFE_VAR=ok", "env.TERM=xterm-256color" };

	/* Clear any previous env pollution for the test */
	unsetenv("PATH");
	unsetenv("LD_PRELOAD");
	unsetenv("SAFE_VAR");
	unsetenv("TERM");

	memset(&cfg, 0, sizeof cfg);
	alacritty_load(&cfg, NULL, ov_huge, 5);
	alacritty_materialize(&cfg, &a);
	check("huge history clamped", cfg.history == 1000000 && a.history == 1000000);
	check("huge columns clamped", cfg.columns == 10000 && a.columns == 10000);
	check("negative lines ignored", cfg.lines == -1);
	check("huge multiplier clamped", cfg.multiplier == 100);
	check("negative pad ignored or clamped", cfg.pad_x == 0);
	alacritty_free(&cfg);

	memset(&cfg, 0, sizeof cfg);
	alacritty_load(&cfg, NULL, ov_env, 4);
	check("dangerous PATH not set", getenv("PATH") == NULL);
	check("dangerous LD_PRELOAD not set", getenv("LD_PRELOAD") == NULL);
	check("safe env var set", getenv("SAFE_VAR") && !strcmp(getenv("SAFE_VAR"), "ok"));
	check("TERM allowed", getenv("TERM") && !strcmp(getenv("TERM"), "xterm-256color"));

	alacritty_free(&cfg);
}

/* Empty ~/.config/c99term must still receive an Alacritty config.
 * A directory that already has a C99Term config must not be overwritten.
 * Each case uses its own XDG dir so the in-process import cache does not
 * hide a second load of the same path. */
static void
test_empty_dir_migrates(void)
{
	AlaConfig cfg;
	AlaApplied a;
	const char *old_home = getenv("HOME");
	const char *old_xdg = getenv("XDG_CONFIG_HOME");
	char home[64], xdg[80];
	char cdir[128], adir[128], src[160], dst[160], ysrc[160], ydst[160];
	FILE *f;

	snprintf(home, sizeof home, "/tmp/c99term-mig-home");
	mkdir(home, 0755);
	setenv("HOME", home, 1);

	/* Case 1: empty C99Term dir, Alacritty TOML present. */
	snprintf(xdg, sizeof xdg, "/tmp/c99term-mig-xdg-empty");
	mkdir(xdg, 0755);
	snprintf(cdir, sizeof cdir, "%s/c99term", xdg);
	snprintf(adir, sizeof adir, "%s/alacritty", xdg);
	mkdir(cdir, 0755);
	mkdir(adir, 0755);
	snprintf(src, sizeof src, "%s/alacritty.toml", adir);
	snprintf(dst, sizeof dst, "%s/c99term.toml", cdir);
	unlink(dst);
	write_file(src, "window.opacity = 0.33\n");
	setenv("XDG_CONFIG_HOME", xdg, 1);

	memset(&cfg, 0, sizeof cfg);
	alacritty_load(&cfg, NULL, NULL, 0);
	alacritty_materialize(&cfg, &a);
	check("empty dir migrates", cfg.loaded);
	check("migrated opacity", a.opacity > 0.32 && a.opacity < 0.34);
	check("migrated_from set", cfg.migrated_from[0]
	      && strstr(cfg.migrated_from, "alacritty.toml"));
	f = fopen(dst, "r");
	check("copied toml exists", f != NULL);
	if (f)
		fclose(f);
	alacritty_free(&cfg);

	/* Case 2: C99Term config already there. Must not be replaced. */
	snprintf(xdg, sizeof xdg, "/tmp/c99term-mig-xdg-have");
	mkdir(xdg, 0755);
	snprintf(cdir, sizeof cdir, "%s/c99term", xdg);
	snprintf(adir, sizeof adir, "%s/alacritty", xdg);
	mkdir(cdir, 0755);
	mkdir(adir, 0755);
	snprintf(src, sizeof src, "%s/alacritty.toml", adir);
	snprintf(dst, sizeof dst, "%s/c99term.toml", cdir);
	write_file(dst, "window.opacity = 0.11\n");
	write_file(src, "window.opacity = 0.99\n");
	setenv("XDG_CONFIG_HOME", xdg, 1);

	memset(&cfg, 0, sizeof cfg);
	alacritty_load(&cfg, NULL, NULL, 0);
	alacritty_materialize(&cfg, &a);
	check("existing c99term not overwritten",
	      a.opacity > 0.10 && a.opacity < 0.12);
	check("no second migration", cfg.migrated_from[0] == 0);
	alacritty_free(&cfg);

	/* Case 3: empty dir, YAML source only. Copied as c99term.yml. */
	snprintf(xdg, sizeof xdg, "/tmp/c99term-mig-xdg-yaml");
	mkdir(xdg, 0755);
	snprintf(cdir, sizeof cdir, "%s/c99term", xdg);
	snprintf(adir, sizeof adir, "%s/alacritty", xdg);
	mkdir(cdir, 0755);
	mkdir(adir, 0755);
	snprintf(dst, sizeof dst, "%s/c99term.toml", cdir);
	snprintf(ydst, sizeof ydst, "%s/c99term.yml", cdir);
	snprintf(ysrc, sizeof ysrc, "%s/alacritty.yml", adir);
	unlink(dst);
	unlink(ydst);
	write_file(ysrc, "window:\n  opacity: 0.44\n");
	setenv("XDG_CONFIG_HOME", xdg, 1);

	memset(&cfg, 0, sizeof cfg);
	alacritty_load(&cfg, NULL, NULL, 0);
	alacritty_materialize(&cfg, &a);
	check("empty dir migrates yaml",
	      cfg.loaded && a.opacity > 0.43 && a.opacity < 0.45);
	f = fopen(ydst, "r");
	check("yaml copied as yml", f != NULL);
	if (f)
		fclose(f);
	check("toml not created for yaml source", access(dst, R_OK) != 0);
	alacritty_free(&cfg);

	if (old_home)
		setenv("HOME", old_home, 1);
	else
		unsetenv("HOME");
	if (old_xdg)
		setenv("XDG_CONFIG_HOME", old_xdg, 1);
	else
		unsetenv("XDG_CONFIG_HOME");
}

int
main(void)
{
	test_toml_window();
	test_yaml_window();
	test_params_not_ignored();
	test_bindings_and_import();
	test_missing_and_comments();
	test_maximized();
	test_ox_colors();
	test_clamping_and_security();
	test_empty_dir_migrates();
	printf("\n%d passed, %d failed\n", passed, failed);
	return failed ? 1 : 0;
}

