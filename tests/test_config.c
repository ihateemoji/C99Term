/* C99Term config tests.
 * Agent-friendly: one line per check, PASS/FAIL, non-zero exit on failure.
 * Build: make test
 * These tests do not need X11. They fail if a window parameter is parsed
 * and then dropped by alacritty_materialize.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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

int
main(void)
{
	test_toml_window();
	test_yaml_window();
	test_params_not_ignored();
	test_bindings_and_import();
	test_missing_and_comments();
	test_maximized();
	printf("\n%d passed, %d failed\n", passed, failed);
	return failed ? 1 : 0;
}
