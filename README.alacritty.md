# st-alacritty

Patch for suckless `st` 0.9.3 (git `04ce0d643ed17793803e8516f4c9a5b13b93c400`, 2026-07-02) that reads a current Alacritty TOML config and matches the Linux/BSD keyboard, clipboard, scrollback, and opacity behaviour closely enough to use the same config file.

This is not a GPU terminal and it is not a full Alacritty reimplementation. It is `st` plus the pieces people actually switch for.

## What it takes from alacritty.toml

Searched, first hit wins (same order as Alacritty on Unix):

1. `--config-file PATH`
2. `$XDG_CONFIG_HOME/alacritty/alacritty.toml`
3. `$XDG_CONFIG_HOME/alacritty.toml`
4. `$HOME/.config/alacritty/alacritty.toml`
5. `$HOME/.alacritty.toml`
6. `/etc/alacritty/alacritty.toml`

`import` and `[general].import` are loaded (absolute, `~/`, or relative to the file). Later files and `--option` snippets override earlier keys.

| Alacritty key | Effect |
| --- | --- |
| `window.opacity` | real alpha background (needs a compositor: picom, compton, ...) |
| `window.padding.x` / `.y` | `borderpx` (the larger of the two; st has one padding) |
| `window.dynamic_padding` | stored; leftover pixels stay at the right/bottom |
| `window.decorations = "None"` | Motif hint, no title bar |
| `scrolling.history` | scrollback lines, default 10000, max you set (0 disables) |
| `scrolling.multiplier` | wheel / default line step, default 3 |
| `font.normal.family` / `.style`, `font.size` | fontconfig pattern (`size=` is points) |
| `colors.primary.foreground` / `.background` | default fg/bg |
| `colors.normal.*`, `colors.bright.*` | the 16-colour palette |
| `colors.cursor.cursor` / `.text` | cursor and reverse cursor, unless `CellForeground` / `CellBackground` |
| `cursor.style.shape` | Block / Underline / Beam |
| `cursor.style.blinking` | Off/Never disables blink |
| `cursor.thickness` | underline/bar thickness |
| `selection.save_to_clipboard` | copy to CLIPBOARD when the mouse selection finishes |
| `selection.semantic_escape_chars` | word-select delimiters |
| `terminal.shell` | string or `{ program, args }` (used when you do not pass `-e`) |
| `terminal.working_directory` | `chdir` before the shell |
| `[env]` | exported into the shell |
| `[[keyboard.bindings]]` | see actions below |
| `[[mouse.bindings]]` | Left/Middle/Right/WheelUp/WheelDown |

`--option 'window.opacity=0.8'` is a one-line TOML override, same idea as Alacritty `-o`. st's own `-o` is still the i/o file, so the long option is `--option`.

## Keyboard and mouse (Linux/BSD Alacritty defaults)

These are compiled in, and any matching `keyboard.bindings` entry overrides them. `action = "ReceiveChar"` lets the key through to the application.

- Ctrl+Shift+C copy selection to CLIPBOARD
- Ctrl+Shift+V paste CLIPBOARD (bracketed paste if the app asked for it)
- Shift+Insert paste PRIMARY
- middle click paste PRIMARY
- Shift+PageUp / Shift+PageDown scroll one page
- Shift+Home / Shift+End jump to top / bottom of scrollback
- Ctrl+=, Ctrl++, Ctrl+KP_Add increase font size
- Ctrl+-, Ctrl+KP_Subtract decrease font size
- Ctrl+0 reset font size
- Shift+wheel always scrolls history
- wheel scrolls history unless the app is in mouse-tracking mode (vim, less, ...)
- Ctrl+Shift+N open another window (not an Alacritty default; convenient extra)

Config actions that work: Copy, Paste, PasteSelection, IncreaseFontSize, DecreaseFontSize, ResetFontSize, ScrollPageUp/Down, ScrollHalfPageUp/Down, ScrollLineUp/Down, ScrollToTop/Bottom, ClearHistory, CreateNewWindow, ReceiveChar, None, and `chars = "..."`.

Scrollback is the primary screen only. The alternate screen (vim, less) does not write history, which matches the usual terminal split.

## Build

```sh
# dependencies: libx11-dev libxft-dev libxrender-dev libfontconfig-dev
patch -p1 < st-alacritty-0.9.3.diff   # from a clean st 0.9.3 / current git tree
make
sudo make install
```

Or build this tree directly. `config.h` is generated from `config.def.h` on first build; delete it if you change the defaults.

Transparency does nothing visible until a compositor is running. Opacity `1.0` stays on the normal visual and does not require one.

TERM defaults to `xterm-256color` so terminfo matches Alacritty. Override with `[env] TERM = "st-256color"` if you want st's own entry (`tic -sx st.info`).

## Not a drop-in for these Alacritty features

- GPU rendering, vsync, damage tracking
- Wayland
- Vi mode and search (Ctrl+Shift+F / B are ignored)
- hint mode / URL hints
- tabs, new-tab, live config reload, `alacritty msg`
- YAML configs (Alacritty 0.13+ is TOML; this reader is TOML only)
- ligatures, separate bold/italic font families (style is passed to fontconfig; st still synthesises bold/italic from that face)
- kitty keyboard protocol, OSC 8 hyperlinks
- per-side padding and exact dynamic padding
- `window.decorations` values other than `None` vs full
- TOML 1.1 edge cases (inline arrays of tables, dotted keys inside inline tables). Normal alacritty.toml files and `--option key=value` work, including imports.

## Upstream

Base: https://git.suckless.org/st commit `04ce0d643ed17793803e8516f4c9a5b13b93c400`
Config format: Alacritty 0.17 TOML (`alacritty(5)`, `alacritty-bindings(5)`).
