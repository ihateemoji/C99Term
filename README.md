# C99Term

A C99 fork of [st](https://st.suckless.org/). It reads an Alacritty config (TOML or YAML) and matches the Linux clipboard, scrollback, font, and window behaviour closely enough to reuse that file.

This is still st. It is not a GPU terminal, and it does not implement Vi mode, search, hints or Wayland (f Wayland). 

## Build

- a C99 compiler (`gcc`, `clang`, `tcc`, ...)
- X11, Xft, Xrender and fontconfig headers

```bash
make
make test
sudo make install
```

`make` produces `c99term`. `make install` also links it as `st`.

## Config

On first run, if `~/.config/c99term` does not exist, C99Term creates it and
copies the first Alacritty config it finds (`alacritty.toml` or `alacritty.yml`).
The original is left in place.

Examples: `config/example.toml`, `config/example.yml`.

## Keys

Alacritty's Linux defaults:

- Ctrl+Shift+C / V copy and paste the CLIPBOARD
- Shift+Insert and middle-click paste PRIMARY
- Shift+PageUp / PageDown, Shift+Home / End scroll
- Ctrl+= / - / 0 zoom
- wheel scrolls history; on the alternate screen (vim, less) it moves the
  cursor, unless Shift is held
- typing or pasting jumps back to the bottom immediately
- dragging a selection to the top or bottom edge scrolls and extends the
  highlight, including while the button is held still
- a selection survives scrolling

## Tests

`make test` runs the config suite. It does not need a display. A failure means
a window parameter was dropped between the file and the applied struct.
Output is one `PASS`/`FAIL` line per check.
