# VBinDiff — Side-by-Side & Colorized Fork

Based on madsen/vbindiff (the canonical upstream). Two changes from stock
VBinDiff:

## 1. Side-by-side layout (opt-in via `-s`/`--side-by-side`)

By default VBinDiff behaves exactly as before: two files stacked one above
the other, fits an 80-column terminal.

Pass `-s` to instead show the two files side by side (left/right) with a
colored divider between them:

    vbindiff file1 file2              # default: stacked, like stock vbindiff
    vbindiff -s file1 file2           # side by side, 16 bytes/line, needs ~155 cols
    vbindiff -s -w 8 file1 file2      # side by side, 8 bytes/line, needs ~85 cols

`-w`/`--width` sets bytes per line in either mode (default 16). It matters
most in side-by-side mode, where it's the main lever for fitting your
terminal width — each pane needs roughly `10 + 3*width + width/8 + width`
columns, so two panes plus a divider need about `2*that + 3`.

If your terminal is too narrow for the requested width, VBinDiff exits
with a clear error telling you the required width, rather than corrupting
the display.

## 2. Configurable colors (default matches original vbindiff)

The default palette is unchanged from stock VBinDiff — the classic
white-on-blue theme, red for differences, yellow for edits, reverse-video
filename/mode bars. The one addition is the pane divider in side-by-side
mode, styled to match (white on blue, bold).

Colors are fully configurable via a config file, if you want to move away
from the original look:

    vbindiff -c mytheme.conf file1 file2

Or drop a file at `~/.vbindiffrc` to have it load automatically every run
(silently ignored if it doesn't exist; parse errors in an explicit `-c`
file are reported and abort startup).

See `vbindiff-example.conf` in this repo for the file format and a sample
"classic diff" green/red theme.

## Implementation notes

- `lineWidth` (bytes/line) is now a runtime value instead of a compile-time
  constant, so the file-buffer indexing was changed from a 2D array trick
  to explicit `row*lineWidth + col` arithmetic.
- `sideBySide` (default false) gates between the original stacked layout
  code path and the new side-by-side one throughout `calcScreenLayout()`,
  `initialize()`, and `positionInWin()`.
- Screen layout (`leftMar2`, `paneWidth`, `screenWidth`) is computed from
  `lineWidth`/`sideBySide` at startup via `computeLayoutMetrics()`.
- Colors moved from 4 hardcoded curses color pairs to one configurable
  `{fg, bg, attrs}` triple per UI style, loaded into curses color pairs at
  startup and optionally overridden by a config file before that.
- The Goto/Find popup windows position themselves relative to the left
  pane, right pane, or centered across both in side-by-side mode; in
  stacked mode they use the original top/bottom vertical positioning.
- Gotcha fixed along the way: a `GetOpt` `ArgFunc` callback for a no-argument
  flag must return `false` (not `true`) when it doesn't consume an
  argument, or the library will swallow the next positional argument
  (e.g. your first filename) as if it belonged to the flag.

## Man page

`tools/vbindiff.pod.tt` (the POD-in-Template-Toolkit source `vbindiff.1` is
generated from) has been updated to document `-s`, `-w`, and `-c`, with a
short "Side by side mode" and "Color configuration" section. Building it
requires the `libtemplate-perl` package (`Template.pm`); if that's not
installed, `make` still builds the `vbindiff` binary fine and just skips
the man page.

## Build

Same as upstream:

    git submodule update --init --recursive   # fetches GetOpt dependency
    autoreconf -i
    ./configure
    make
