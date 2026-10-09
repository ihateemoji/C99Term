/* Alacritty TOML config reader for st.
 * Supports the subset used by current alacritty.toml files:
 * general.import, window, scrolling, font, colors, cursor, selection,
 * terminal.shell, env, keyboard.bindings, mouse.bindings.
 */
#ifndef ALACRITTY_H
#define ALACRITTY_H

#define ALA_MAX_BINDS 256
#define ALA_MAX_ARGS  16

enum ala_action {
	ALA_NONE = 0,
	ALA_RECEIVE,          /* disable binding, let the key through */
	ALA_COPY,
	ALA_PASTE,
	ALA_PASTE_SEL,
	ALA_FONT_INC,
	ALA_FONT_DEC,
	ALA_FONT_RESET,
	ALA_SCROLL_PAGE_UP,
	ALA_SCROLL_PAGE_DOWN,
	ALA_SCROLL_HALF_UP,
	ALA_SCROLL_HALF_DOWN,
	ALA_SCROLL_LINE_UP,
	ALA_SCROLL_LINE_DOWN,
	ALA_SCROLL_TOP,
	ALA_SCROLL_BOTTOM,
	ALA_CLEAR_HIST,
	ALA_NEW_WINDOW,
	ALA_CHARS
};

typedef struct {
	unsigned int mods;    /* X modifier mask; 0 means no mods required */
	unsigned int keysym;  /* KeySym, or 0 if this is a mouse binding */
	unsigned int button;  /* X button, or 0 if this is a key binding */
	int action;
	char chars[96];
} AlaBind;

typedef struct {
	char *font;                 /* fontconfig pattern, or NULL */
	float opacity;              /* 0..1, negative if unset */
	int history;                /* -1 if unset */
	int multiplier;             /* -1 if unset */
	int pad_x, pad_y;           /* -1 if unset */
	int dynamic_padding;        /* -1 if unset */
	int decorations_none;       /* -1 if unset */
	int save_to_clipboard;      /* -1 if unset */
	int cursor_shape;           /* 0 unset, else st cursor shape 2/4/6 */
	int cursor_blink;           /* -1 unset, 0 off, 1 on */
	float cursor_thickness;     /* <0 unset */
	char *fg, *bg;
	char *cursor_color, *cursor_text;
	char *normal[8];
	char *bright[8];
	char *shell;
	char *shell_args[ALA_MAX_ARGS];
	int shell_argc;
	char *workdir;
	char *term;
	char *worddelims;           /* semantic escape chars */
	AlaBind binds[ALA_MAX_BINDS];
	int nbinds;
	int loaded;
	char path[4096];
} AlaConfig;

/* Load the first existing Alacritty config, then imports, then overrides.
 * overrides are raw TOML snippets ("window.opacity=0.8").
 * cfg is zeroed by the caller except path if config_file is set. */
void alacritty_load(AlaConfig *cfg, const char *config_file,
                    char **overrides, int noverrides);

void alacritty_free(AlaConfig *cfg);

#endif
