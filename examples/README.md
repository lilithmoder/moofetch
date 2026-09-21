# Animated ASCII logos (`.anim` files)

This directory contains the animations that ship with moofetch. They are:

* **real-world examples** of the `.anim` format, and
* **embedded into the moofetch binary at build time**, so they work without any data
  files installed.

| File | Effect |
|---|---|
| `arch`, `cachyos`, `debian`, `fedora`, `ubuntu`, `opensuse`, `opensuse_leap`, `opensuse_tumbleweed`, `linuxmint`, `pop`, `manjaro`, `endeavouros`, `garuda`, `nixos`, `gentoo`, `alpine`, `kali`, `void`, `elementary`, `zorin`, `mx`, `deepin`, `artix`, `rhel`, `rocky`, `almalinux`, `centos`, `slackware`, `raspbian`, `parrot`, `devuan` (`.anim`) | Moving highlight band over the distro's built-in ASCII art |
| `arch_rotate.anim`, `cachyos_rotate.anim` | Slow 3D spin around the vertical axis: 36 frames, one 360° revolution in ~4.5 s, then freezes on the original logo |
| `default.anim` | Color-rotating status dots (generic logo) |
| `spinner.anim` | Braille spinner (custom art) |

The file name is the built-in name; list them all with `moofetch --list-animations`.
Distro names are matched against the detected OS id case-insensitively, with `-` and `_`
treated as equal (so `opensuse-tumbleweed` finds `opensuse_tumbleweed.anim`).

## Usage

```sh
# Built-in animation by name
moofetch --logo-type animation --logo arch

# Auto-detect the animation matching your distro (falls back to the static logo)
moofetch --logo-type animation

# Your own file
moofetch --logo-type animation --logo ~/my-animation.anim
```

Or in `~/.config/moofetch/config.jsonc`:

```jsonc
{
    "logo": {
        "type": "animation",
        "source": "cachyos",          // built-in name or path to a .anim file
        "animation": {
            "fps": 12,                // 1-60
            "loop": 2,                // 0 = loop until keypress/timeout
            "timeout": 5000,          // ms, only used when loop is 0
            "hold": "last"            // "first" or "last" frame to freeze on
        }
    }
}
```

All four `animation` values are optional and override the directives inside the `.anim`
file. Existing fastfetch configurations keep working unchanged; only this new key was
added.

The animation only plays when stdout is an interactive terminal and the logo position is
`left`. When piping/redirecting (`moofetch | cat`), in `--format json` mode, or with
`--logo-position top/right`, the hold frame is printed statically instead — scripts never
see cursor movement escapes.

## File format

A `.anim` file is a UTF-8 text file:

```
# Comment lines start with '#' (header only)
!fps 12
!loop 2
!timeout 5000
!hold last
---
first frame line 1
first frame line 2
---
second frame line 1
second frame line 2
---
third frame line 1
...
```

### Header

Everything before the **first** `---` separator is the header. It may contain:

* `# ...` — comments (ignored)
* `!fps <1-60>` — playback speed, default `12`
* `!loop <0-1000>` — loops to play, default `2`; `0` means "loop until a key is pressed
  or `!timeout` is reached"
* `!timeout <1-600000>` — milliseconds, default `5000`, only used with `!loop 0`
* `!hold <first|last>` — frame to freeze on after playback, default `last`

Unknown directives are an error (with a line number), so typos never go unnoticed.

A file without any `---` separator is valid and is treated as a single static frame.

### Frames

Each `---` line starts a new frame. Everything up to the next `---` (or end of file) is
that frame, verbatim. Blank lines and leading spaces are significant.

### Colors

Two mechanisms can be mixed freely:

1. **Theme placeholders** `$1` … `$9`, resolved through moofetch's normal logo color
   pipeline (the same syntax built-in logos use). `$$` prints a literal `$`.
   Use these to follow the user's configured `logo.color` values.
2. **Raw ANSI SGR sequences** (`ESC[…m`), passed through untouched. This is what tools
   like `chafa` emit; it allows truecolor frames.

### Rules and limits

* All frames are padded at load time to the same width and height, so every frame
  occupies exactly the same terminal region. You can use different-sized frames, but
  shorter ones will be padded with spaces/blank lines.
* Visible width ignores ANSI escapes and `$N` placeholders; tabs count as 4 columns.
* Limits: 256 frames, 512 lines per frame, 512 visible columns per line, 2 MiB total.
* CRLF line endings are accepted.
* Keep the animation narrower than your terminal so the redraw never wraps.

## Creating your own animation

### From a GIF or image sequence

There are two converters:

* **Browser converter** — open [`../web/index.html`](../web/index.html) (works offline,
  nothing is uploaded), drag in a GIF or a set of image frames, preview the animation,
  tweak width/fps/charset/colors and download the `.anim`. This is the easiest option and
  needs no installed tools.
* **Command line** — `tools/gif2anim` uses ImageMagick (frame extraction) and `chafa`
  (ANSI rendering):

  ```sh
  python3 tools/gif2anim input.gif -o my.anim --width 30 --fps 12 --loop 2
  python3 tools/gif2anim --check my.anim   # validate an existing file
  ```

  Run `python3 tools/gif2anim --help` for all options. Both `chafa` and ImageMagick
  (`magick` or `convert`) must be installed.

### From a web GIF-to-ASCII converter

Any tool that can produce per-frame ANSI/ASCII text works:

1. Export each frame of your GIF as text (or a `.txt` per frame).
2. Concatenate the frames into one file, putting a line containing exactly `---` between
   consecutive frames.
3. Optionally add a header (`!fps 12`, `!loop 2`, …) at the top.
4. Validate: `python3 tools/gif2anim --check my.anim`.

### By hand

The format is designed to be hand-editable: copy a built-in logo from
`src/logo/ascii/*/` (or use `moofetch --print-logos`), add a few frames with colors
changed, and you have an animation. The bundled files in this directory were generated
by `tools/gen-animations.py`, which is a good starting point for scripted effects.
