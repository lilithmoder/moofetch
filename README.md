# moofetch

`moofetch` is a system information tool for Linux, *BSD, macOS and Windows — a **fork of
[fastfetch](https://github.com/fastfetch-cli/fastfetch)** that adds **animated ASCII
logos**.

<p align="center">
  <em>Animated distro logos play next to the regular system information, freeze on a final
  frame, and exit like a normal fetch tool.</em>
</p>

## Features

* **Animated ASCII logos** — multi-frame, colored animations rendered in place next to
  the info output, for the `left`, `top` and `right` logo positions.
* **35 built-in animations**: Arch, CachyOS, Debian, Fedora, Ubuntu, openSUSE (incl.
  Leap/Tumbleweed), Linux Mint, Pop!_OS, Manjaro, EndeavourOS, Garuda, NixOS, Gentoo,
  Alpine, Kali, Void, elementary, Zorin, MX, deepin, Artix, RHEL, Rocky, AlmaLinux,
  CentOS, Slackware, Raspberry Pi OS, Parrot, Devuan — plus slow 3D-spin variants
  (`arch_rotate`, `cachyos_rotate`) and generic `default`/`spinner` animations.
  Auto-detected from your distro.
* **Custom animations** in a simple, documented plain-text `.anim` format
  (see [`examples/README.md`](examples/README.md)) — theme colors (`$1`–`$9`) and raw
  ANSI/truecolor supported. Converters: `tools/gif2anim` (GIF → `.anim` on the command
  line) and a **browser-based converter** at [`web/index.html`](web/index.html)
  (drag & drop a GIF, preview it, download the `.anim`).
* **Drop-in compatible** with fastfetch: any existing fastfetch `config.jsonc` and all
  fastfetch CLI options work unchanged. moofetch reads `~/.config/moofetch/` first and
  falls back to `~/.config/fastfetch/`.
* **One self-contained binary** — bundled animations are compiled in; no data files
  required.
* **Script-safe** — when piped or redirected, the hold frame is printed statically with
  no cursor movement escapes.

## Building

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build   # optional
```

The build produces `moofetch` (and `mooflash`, which prints all information without a
logo). Python 3 is used at build time to generate the man page and embed the example
animations.

## Usage

```sh
moofetch                                          # auto-detects the distro and animates
                                                  # if a built-in animation exists
moofetch --logo-type builtin                      # static logo (classic fastfetch look)
moofetch --logo-type animation --logo cachyos     # a specific built-in animation
moofetch --logo-type animation --logo ~/my.anim   # your own animation
moofetch --list-animations                        # list built-in animations
moofetch --logo-type animation --logo arch \
         --logo-animation-fps 15 --logo-animation-loop 3
```

Configuration (`~/.config/moofetch/config.jsonc`):

```jsonc
{
    "logo": {
        "type": "animation",
        "source": "cachyos",
        "animation": {
            "fps": 12,
            "loop": 2,        // 0 = loop until keypress or timeout
            "timeout": 5000,  // ms, used when loop is 0
            "hold": "last"    // frame to freeze on
        }
    }
}
```

All general fastfetch options and modules apply unchanged — see the
[fastfetch wiki](https://github.com/fastfetch-cli/fastfetch/wiki/Configuration) for the
full option reference.

### Playback behaviour

On distros with a built-in animation, moofetch animates the logo by default — set
`"logo": { "type": "builtin" }` (or use `--logo-type builtin`) to keep the classic
static logo. The animation plays for the configured number of loops, freezes on the hold
frame, and then moofetch exits — safe for shell startup files. With `"loop": 0` it runs
until a key is pressed or the timeout is reached. Animation only plays when stdout is an
interactive terminal; everywhere else the hold frame is printed statically. All logo
positions (`left`, `top`, `right`) are supported.

Existing fastfetch configurations are fully supported. A config that does not mention the
logo (or uses a non-animation logo type) behaves exactly like fastfetch, except that a
bare `moofetch` run may animate as described above.

## Creating animations

The `.anim` format, the bundled examples and the `tools/gif2anim` converter are
documented in [`examples/README.md`](examples/README.md). There is also a self-contained
browser converter at [`web/index.html`](web/index.html) — open it locally (or host the
`web/` directory with GitHub Pages) and drag a GIF onto it.

## Project status

This is a hard fork pinned to fastfetch `2.68.1`; see [`UPSTREAM.md`](UPSTREAM.md) for
provenance. Known limitations:

* Animated logos are POSIX-terminal only (Windows prints the hold frame statically).
* Animation cannot persist after moofetch exits — the shell owns the terminal afterwards.
  (Terminal-native image protocols could, but that is out of scope; see
  [`PLAN.md`](PLAN.md) §17.)

Packaging files (AUR, Nix flake, Homebrew template) live in [`packaging/`](packaging/);
they need the real repository URL before use.

## Credits and license

moofetch is based on [fastfetch](https://github.com/fastfetch-cli/fastfetch) by Carter Li
and contributors, and is distributed under the MIT license (see [`LICENSE`](LICENSE)).
