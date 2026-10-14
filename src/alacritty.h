/* C99Term config reader.
 * Loads Alacritty TOML and YAML, plus C99Term's own config directory.
 * This is an st fork; the parser is new code under the same MIT/X license.
 */
#ifndef ALACRITTY_H
#define ALACRITTY_H

#define ALA_MAX_BINDS 256
#define ALA_MAX_ARGS  16

enum ala_action {
	ALA_NONE = 0,
	ALA_RECEIVE,
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

enum ala_startup {
	ALA_START_WINDOWED = 0,
	ALA_START_MAXIMIZED = 1,
	ALA_START_FULLSCREEN = 2
};

typedef struct {
	unsigned int mods;
	unsigned int keysym;
	unsigned int button;
	int action;
	char chars[96];
} AlaBind;

typedef struct {
	char *font;
	float opacity;              /* <0 unset */
	int history;                /* <0 unset */
	int multiplier;             /* <0 unset */
	int pad_x, pad_y;           /* <0 unset */
	int dynamic_padding;        /* <0 unset */
	int decorations_none;       /* <0 unset, 1 = None */
	int save_to_clipboard;      /* <0 unset */
	int cursor_shape;           /* 0 unset, else 2/4/6 */
	int cursor_blink;           /* <0 unset */
	float cursor_thickness;     /* <0 unset */
	int columns, lines;         /* window.dimensions, <0 unset */
	int pos_x, pos_y;           /* window.position, INT_MIN unset */
	int startup_mode;           /* ala_startup, <0 unset */
	char *title;                /* window.title */
	int dynamic_title;          /* <0 unset */
	char *fg, *bg;
	char *cursor_color, *cursor_text;
	char *normal[8];
	char *bright[8];
	char *shell;
	char *shell_args[ALA_MAX_ARGS];
	int shell_argc;
	char *workdir;
	char *term;
	char *worddelims;
	AlaBind binds[ALA_MAX_BINDS];
	int nbinds;
	int loaded;
	char path[4096];
	char migrated_from[4096];   /* set if we copied an alacritty config */
} AlaConfig;

/* Runtime values after a config is applied. Tests assert every window
 * field that was set in the file is present here, so a parameter cannot
 * be parsed and then silently dropped. */
typedef struct {
	float opacity;
	int history;
	int multiplier;
	int pad_x, pad_y;
	int dynamic_padding;
	int decorations_none;
	int save_to_clipboard;
	int columns, lines;
	int pos_x, pos_y;
	int startup_mode;
	int dynamic_title;
	char title[256];
	char font[512];
	char fg[64], bg[64];
} AlaApplied;

void alacritty_load(AlaConfig *cfg, const char *config_file,
                    char **overrides, int noverrides);
void alacritty_materialize(const AlaConfig *cfg, AlaApplied *out);
void alacritty_free(AlaConfig *cfg);

#endif
