# Animfetch — Implementation Plan & Handoff Document

> **For the implementing agent:** This document is the single source of truth for the
> project. It was written after direct verification of the upstream fastfetch codebase
> (GitHub `dev` branch + release `2.68.1`) and approved by the project owner. Follow it
> phase by phase (§14). Do **not** start coding beyond the phase you are asked to do.
> Where you are told to "verify during implementation," do so before relying on the
> detail — everything else here has been fact-checked against upstream.

---

## 1. Project overview

**Animfetch** (working title — see §3) is a **hard fork of fastfetch** that adds
**animated ASCII logo support**: multi-frame, colored ASCII animations that play in the
terminal next to the normal system-information display, then freeze on a final frame and
exit like a normal fetch tool.

Goals, in priority order:

1. Animated ASCII logos rendered beside the standard fastfetch info output.
2. User-supplied animation files in a dead-simple plain-text format (infinite
   customisability), with bundled, ready-made animations for popular distros (CachyOS,
   Arch, Debian, Ubuntu, …) shipped in an `examples/` directory that doubles as format
   documentation.
3. A single self-contained binary (bundled animations compiled in; no required data files).
4. Full drop-in compatibility with existing fastfetch configs and CLI usage.

Non-goals (v1): see §16.2.

---

## 2. Background facts (verified)

### 2.1 Why fastfetch, not neofetch

- **Neofetch** was archived by its author in April 2024; it is a single ~10k-line bash
  script. It is a dead upstream.
- **Fastfetch** (https://github.com/fastfetch-cli/fastfetch) is the actively maintained
  successor: C, CMake, single binary, JSONC config, modular architecture, large config
  ecosystem.

### 2.2 Upstream animation status (important!)

- Upstream fastfetch has **no animated ASCII support** (open feature request:
  fastfetch-cli/fastfetch#1423).
- Upstream **does** recently have an `animationFrame` feature for **image-protocol logos
  only** (kitty/iterm etc.): `FFOptionsLogo` already contains an
  `int32_t animationFrame` field with macros `FF_LOGO_ANIMATION_FRAME_FIRST (1)` and
  `FF_LOGO_ANIMATION_FRAME_ANIMATE (0)`, and a corresponding CLI/JSON option. **This is
  unrelated to our ASCII animation.** Our additions must coexist with it without naming
  collisions (see §8 for the chosen names, which deliberately live in a `logo.animation`
  sub-object and `--logo-animation-*` flags that complement, not clash with,
  `--logo-animation-frame`).
- Third-party workarounds exist (`fastfetch-gif` AUR package — kitty-protocol only;
  `brrtfetch` — external wrapper). None provide terminal-independent ASCII animation.

### 2.3 Upstream baseline to fork

- Repository: `https://github.com/fastfetch-cli/fastfetch`
- **Pin to release tag `2.68.1`** (published 2026-09-01). Record the exact commit hash in
  `UPSTREAM.md` at fork time.
- License: MIT (retain notice + attribution in the fork).
- Build: CMake. Most hardware/OS detection libraries are `dlopen`ed at runtime, so a
  minimal build needs little more than a C compiler, CMake and standard dev headers.
  Follow upstream's README build instructions for the baseline build.

### 2.4 Verified upstream layout & hook points

| Path | Role |
|---|---|
| `src/fastfetch.c` | main entry, top-level flow |
| `src/flashfetch.c` | "flashfetch" variant binary (prints everything; no logo) — keep as-is |
| `src/logo/logo.c`, `src/logo/logo.h` | logo printing. Key functions: `ffLogoPrint()`, `ffLogoPrintChars(const char* data, bool doColorReplacement)`, `ffLogoPrintLine()`, `ffLogoPrintRemaining()`, `ffLogoGetBuiltinForName()` |
| `src/logo/builtin.c` | builtin ASCII logos embedded as C data (`extern const FFlogo* ffLogoBuiltins[]`) — **this is the pattern to mirror for embedding animations** |
| `src/options/logo.h`, `src/options/logo.c` | `FFOptionsLogo` struct and all logo option plumbing: `ffOptionsInitLogo()`, `ffOptionsParseLogoCommandLine()`, `ffOptionsParseLogoJsonConfig(FFOptionsLogo*, yyjson_val*, yyjson_val**)`, `ffOptionsGenerateLogoJsonConfig()`, `ffOptionsDestroyLogo()` |
| `src/options/` (overall) | global option parsing / config dispatch |
| `src/common/` | shared utilities (strbuf, io, terminal helpers, JSON via `yyjson` from `src/3rdparty/`) |
| `src/modules/` | info modules (do not touch) |
| `doc/help.json` | CLI `--help` text data (add new flags here) |
| `doc/fastfetch.1.in` | man page template |
| `doc/json_schema.json` | JSON schema for config files (extend, don't break) |
| `presets/` | upstream example config presets (use as compatibility test fixtures) |

`FFLogoType` enum (verified, from `src/options/logo.h`): `FF_LOGO_TYPE_AUTO`,
`FF_LOGO_TYPE_BUILTIN`, `FF_LOGO_TYPE_SMALL`, `FF_LOGO_TYPE_FILE`,
`FF_LOGO_TYPE_FILE_RAW`, `FF_LOGO_TYPE_DATA`, `FF_LOGO_TYPE_DATA_RAW`,
`FF_LOGO_TYPE_COMMAND_RAW`, `FF_LOGO_TYPE_IMAGE_SIXEL`, `FF_LOGO_TYPE_IMAGE_KITTY`,
`FF_LOGO_TYPE_IMAGE_KITTY_DIRECT`, `FF_LOGO_TYPE_IMAGE_KITTY_ICAT`,
`FF_LOGO_TYPE_IMAGE_ITERM`, `FF_LOGO_TYPE_IMAGE_CHAFA`, `FF_LOGO_TYPE_IMAGE_RAW`,
`FF_LOGO_TYPE_NONE`. **Append** `FF_LOGO_TYPE_ANIMATION` (string name `"animation"`) —
append, don't insert.

`FFLogoPosition`: `FF_LOGO_POSITION_LEFT` (default), `_TOP`, `_RIGHT`.

> Verify during implementation: how config search paths are implemented (grep for
> `config.jsonc` under `src/common/` / `src/options/`), and what terminal-query helpers
> already exist (grep for `\x1b[` / `6n` / `Device Attributes` usage, e.g. in terminal /
> image-protocol detection code) — reuse them for the cursor-position query in §7.4.

---

## 3. Naming

**`animfetch` is a placeholder working title.** The owner has not picked a final name
(candidates floated: animfetch, motionfetch, moofetch, fluxfetch). Confirm with the
owner before the rebrand phase (Phase 0.3); until then, use `animfetch` consistently —
binary name, CMake project name, config dir, man page, etc. — so a final rename is one
mechanical pass.

---

## 4. Decisions

### 4.1 Locked decisions (owner-approved)

1. **Hard fork** of fastfetch `2.68.1`. We will not track upstream continuously. (Tag
   the upstream base commit in git so a specific upstream fix *could* be cherry-picked
   later if ever needed.)
2. **Animations come from plain-text frame files** (`.anim`, spec in §6). Users point the
   config/CLI at their own files.
3. **Bundled distro animations** (CachyOS, Arch, Debian, Ubuntu + generics) ship in an
   `examples/` directory — which serves as real-world format documentation — **and** are
   embedded into the binary at build time, so the binary works standalone.
4. **Playback is configurable, default finite**: play N loops, freeze on the final frame,
   exit. Optional infinite mode stops on keypress or timeout.
5. **Strict config/CLI superset of upstream**: any valid fastfetch `config.jsonc` and any
   fastfetch CLI invocation must behave identically (byte-identical static output when
   animation is not in play).
6. **Config path precedence**: `~/.config/animfetch/` first, then fall back to
   `~/.config/fastfetch/` (drop-in replacement with its own identity).
7. **POSIX terminals only for animation in v1**: on Windows, non-TTY stdout, or any
   unsupported condition, fall back to printing the final frame statically.

### 4.2 Open decisions (confirm with owner when reached)

- Final project name (§3).
- Whether to keep building the `flashfetch` companion binary (recommend: keep, renamed
  if trivial; zero functional changes).

---

## 5. How it works (user-facing summary)

```sh
animfetch                                   # auto-detects distro; animates if a builtin
                                            # animation exists for it, else static logo
animfetch --logo-type animation --logo cachyos
animfetch --logo-type animation --logo ~/art/my.anim \
          --logo-animation-fps 15 --logo-animation-loop 3
animfetch | cat                             # static final frame, no escape spam
```

Config (`~/.config/animfetch/config.jsonc`, else fastfetch's):

```jsonc
{
  "logo": {
    "type": "animation",
    "source": "cachyos",              // builtin animation name, or path to .anim file
    "animation": {
      "fps": 12,                      // 1..60, default 12
      "loop": 2,                      // N loops; 0 = infinite until keypress/timeout
      "timeout": 5000,                // ms, only used when loop = 0
      "hold": "last"                  // "first" | "last" frame to freeze on
    }
  }
}
```

---

## 6. The `.anim` file format (specification)

Design goals: trivially hand-editable; trivially produced from the output of existing
GIF→ASCII/ANSI tools (web or CLI) by adding separator lines; native support for both
fastfetch-style `$1`–`$9` theme color placeholders **and** raw ANSI SGR color sequences
(so `chafa` output can be embedded verbatim).

### 6.1 Grammar

```
<file>    := <header> (<separator> <frame>)*
<header>  := (<directive> | <comment> | <blank>)*        ; ends at first separator
<directive> := "!" <key> SP <value> NEWLINE
<comment> := "#" <anything> NEWLINE                       ; header section only
<separator> := "---" NEWLINE                              ; exactly three dashes, own line
<frame>   := <line>*                                      ; verbatim text lines
```

- The header runs from the start of the file until the first `---` line.
- Recognized directives: `!fps <1-60>`, `!loop <0-1000>`, `!timeout <ms>`,
  `!hold first|last`. Unknown `!` directives → hard error with line number (fail loudly
  so typos are caught). Directives after the first `---` are **frame data**, not
  directives.
- A file with **no** `---` separator is valid: a single static frame.
- Everything after a separator up to the next separator (or EOF) is one frame, verbatim.
- Frames are **not** trimmed internally; leading/trailing spaces in art are meaningful.
- Encoding: UTF-8. Lines are split on `\n`; a `\r\n` file must be handled (strip `\r`).
- Max limits (parser-enforced, clear errors): 256 frames, 512 lines per frame,
  512 **visible** columns per line.

### 6.2 Colors in frames

Two mechanisms may be mixed freely within a frame:

1. **fastfetch placeholders** `$1`…`$9` (and `$$` for a literal `$`), resolved through the
   existing logo color pipeline (`ffLogoPrintChars(data, true)`), so animations track the
   user's configured logo colors exactly like builtin logos do.
2. **Raw ANSI SGR sequences** (`ESC[…m`), passed through untouched (this is what `chafa`
   emits; enables truecolor/frame-accurate color).

### 6.3 Width semantics (critical)

- "Visible width" of a line = character count after (a) resolving `$N` placeholders and
  (b) skipping all ANSI CSI sequences (`ESC[ … letter`).
- The parser computes each frame's max visible width and **pads every line with spaces to
  the frame's width** (padding appended after any reset sequence, so colors don't bleed).
  Frames may differ in width from each other; the renderer pads all frames to the maximum
  visible width across the whole file. **All frame rewriting is exact-width with no
  clear-to-EOL** — this is what guarantees the info columns to the right are never
  touched (§7.3).

### 6.4 Reference example (`examples/ubuntu.anim` sketch)

```
# Ubuntu — subtle color wave. Base art: upstream fastfetch builtin "ubuntu" logo.
!fps 10
!loop 2
---
          _                $1
      ---(_)               $1
  _/  ---  \               $1
 (_) |   |                 $1
   \  --- _/               $1
      ---(_)               $1
---
          _                $2
      ---(_)               $2
  _/  ---  \               $2
 (_) |   |                 $2
   \  --- _/               $2
      ---(_)               $2
```

(Real files: full upstream logo art, wave across several palette steps — see §9.2.)

---

## 7. Renderer design (`src/logo/animation.c`, new)

### 7.1 Placement & hook

- Add `FF_LOGO_TYPE_ANIMATION` handling inside `ffLogoPrint()`'s type dispatch
  (`src/logo/logo.c`): when the logo type is `ANIMATION`, load/parse the `.anim` (from
  builtin table by name, else file path), **print frame 0 through the normal upstream
  code path** (as if it were a static `FILE` logo), let all info modules print exactly as
  usual, and only afterwards run the animation pass that redraws the logo region.
- This keeps 100 % of upstream layout behavior: `--logo-width/height`,
  `--logo-padding-*`, `--logo-print-remaining`, separator, colors — all keep working.

### 7.2 Preconditions (all must hold, else static fallback)

Fall back to "print hold frame statically, behave exactly like upstream" when **any** of:
- `stdout` is not a TTY (`--pipe`, redirect, `--format json` — anything non-interactive);
- platform is Windows (v1);
- logo position is `TOP` or `RIGHT` (v1 animates `LEFT` only);
- computed animation region width ≥ terminal width (avoid auto-wrap corruption);
- cursor-position query (7.4) fails or times out;
- the `.anim` has exactly 1 frame (nothing to animate).

The fallback path must be indistinguishable from upstream output.

### 7.3 Animation region

- Rows: the logo art lines actually printed (respecting `paddingTop`, and
  `printRemaining` truncation/extension). Columns: from column 1 through
  `paddingLeft + artVisibleWidth` (frame lines are pre-padded with `paddingLeft` spaces).
- Every redraw writes exactly that many visible columns per row, **never more**, and
  never uses erase-in-line — the info text to the right is never written over.

### 7.4 Frame loop algorithm

1. **Before** the main print: query cursor position via DSR (`ESC[6n`, read response on
   stdin with a ~50 ms timeout; reuse upstream terminal-query helpers if present). Save
   `row0`. (If upstream flow makes a pre-print query awkward, query immediately after and
   instead compute `row0` from `row1` minus printed rows — implementer's choice; the
   requirement is a correct region.)
2. Main print proceeds normally (frame 0 + info).
3. **After** the main print: query cursor position → `row1`. Region rows =
   `[row0 + paddingTop, row0 + paddingTop + min(artLines, printedRows - paddingTop))`.
   (Tune against real output during implementation; the PTY test in §13 is the judge.)
4. Hide cursor: `ESC[?25l`. Install signal handlers (7.5). For infinite mode, put stdin
   in non-canonical non-blocking mode (save/restore termios).
5. For each frame (respecting `loop`; infinite if `0`):
   - Begin synchronized output: `ESC[?2026h` (ignored by terminals that don't support
     DEC mode 2026 — harmless).
   - For each region row `i`: `ESC[<row>;1H` (absolute CUP) then write the padded frame
     line. Absolute positioning per line is deliberately chosen over relative movement
     for robustness.
   - End synchronized output: `ESC[?2026l`. `fflush(stdout)`.
   - Sleep one frame budget (`clock_nanosleep(CLOCK_MONOTONIC)`; skip the sleep if
     drawing overran the budget).
   - Infinite mode: poll stdin; any keypress, or elapsed time ≥ `timeout` ms, ends the
     loop. `SIGWINCH` sets a `volatile sig_atomic_t` flag that also ends the loop
     gracefully (leave the current frame in place).
6. Finish: draw the `hold` frame once (default `last`), move the cursor below the region
   (`ESC[<row1>;1H` style absolute positioning), show cursor `ESC[?25l`→`ESC[?25h`,
   restore termios/handlers, exit normally.

### 7.5 Signals & cleanup

- `SIGINT`/`SIGTERM` during animation: show cursor, move below the region, restore
  termios, `_exit(130)`/`_exit(143)`. No half-drawn state, no lost echo.
- All escape-state changes (cursor hidden, termios) must be restored on **every** exit
  path. Structure the code with a single cleanup function.

### 7.6 Timing defaults

`fps` default 12 (clamp 1–60), `loop` default: file directive else 2, `timeout` default
5000 ms, `hold` default `last`. Config/CLI override file directives; file directives
override built-in defaults.

---

## 8. Config & CLI integration

### 8.1 New options in `FFOptionsLogo` (`src/options/logo.h`)

```c
// ASCII animation (FF_LOGO_TYPE_ANIMATION). Independent of animationFrame above,
// which upstream uses for image-protocol logos — do NOT merge or rename either.
uint32_t animFps;        // 0 = unset (use file directive / default)
int32_t  animLoop;       // -1 = unset; 0 = infinite; N>0 = N loops
uint32_t animTimeoutMs;  // 0 = unset (default 5000)
bool     animHoldFirst;  // false = hold last frame (default)
```

Plumb through `ffOptionsInitLogo`, `ffOptionsParseLogoCommandLine`,
`ffOptionsParseLogoJsonConfig`, `ffOptionsGenerateLogoJsonConfig`, `ffOptionsDestroyLogo`
following the existing per-field patterns in `src/options/logo.c`.

### 8.2 CLI flags

| Flag | Value |
|---|---|
| `--logo-type animation` | (enum value, follows existing `--logo-type` parsing) |
| `--logo-animation-fps <1-60>` | frames per second |
| `--logo-animation-loop <N>` | 0 = infinite until keypress/timeout |
| `--logo-animation-timeout <ms>` | timeout for infinite mode |
| `--logo-animation-hold <first\|last>` | freeze frame |

Note the sibling upstream flag `--logo-animation-frame` (image logos). Ours are
deliberately parallel in naming but apply only to `animation`-type logos; the help text
must say so. Add entries to `doc/help.json`, the man page `doc/fastfetch.1.in`, and shell
completions if the CLI-completion generator needs the new flags (check how
`--list-logos`-style completions are produced).

### 8.3 JSONC config

Extend the `logo` object (parsed in `ffOptionsParseLogoJsonConfig`, generated in
`ffOptionsGenerateLogoJsonConfig`, schema in `doc/json_schema.json`):

```jsonc
"logo": {
  "type": "animation",
  "source": "cachyos",
  "animation": { "fps": 12, "loop": 2, "timeout": 5000, "hold": "last" }
}
```

- `--gen-config` / `--gen-config-force` must emit the new keys when set.
- Unknown keys anywhere else remain an error exactly as upstream (superset, not laxer).

### 8.4 Source resolution for `type: animation`

1. `source` empty → use the **auto-detected distro name** (same detection upstream uses
   for builtin logos); if a builtin animation with that name exists use it, else fall
   back to upstream's normal static builtin logo (no error).
2. `source` matches a builtin animation name (case-insensitive, same matching style as
   `ffLogoGetBuiltinForName`) → use embedded data.
3. Otherwise treat `source` as a filesystem path to a `.anim` file; a missing/unreadable/
   invalid file is a normal user-facing error, same style as `--logo-type file`.

### 8.5 Config file discovery

`~/.config/animfetch/config.jsonc` (and the other XDG/platform equivalents) takes
precedence; if absent, fall back to upstream's fastfetch config paths unchanged. `-c` /
`--config` behavior is unchanged. Implement where upstream resolves config paths (grep
for `config.jsonc` in `src/`); keep `--list-config-paths` accurate.

---

## 9. Bundled animations, `examples/`, and embedding

### 9.1 Layout

```
examples/
  README.md          # full .anim format spec (§6) + how to make your own (§10)
  cachyos.anim
  arch.anim
  debian.anim
  ubuntu.anim
  fedora.anim
  default.anim       # generic: subtle shimmer on the fastfetch/animfetch logo
  spinner.anim       # generic minimal spinner
```

`examples/` is the **single canonical source**: CMake embeds from this directory, so the
repo files are simultaneously (a) built-in animations, (b) user documentation, and
(c) test fixtures. No copies elsewhere.

### 9.2 Content guidelines

- Base art: copy the exact ASCII lines from upstream `src/logo/builtin.c` for each distro
  (guarantees correct proportions beside the info block and upstream-consistent colors).
- Effects per distro (hand-authored, 6–20 frames each): color wave across `$1`–`$3`
  steps, shimmer/sparkle, blink. Keep motion tasteful at 10–15 fps.
- Each file starts with a `#` comment header explaining what it demonstrates.

### 9.3 Build-time embedding (mirrors upstream builtin logos)

- New script `tools/embed_animations.py` (Python 3, stdlib only): reads
  `examples/*.anim`, emits a generated C file (into the build dir) containing
  ```c
  typedef struct { const char* name; const char* data; } FFBuiltinAnimation;
  const FFBuiltinAnimation ffBuiltinAnimations[] = { ... };
  ```
  with properly escaped string literals (or `xxd`-style byte arrays — implementer's
  choice, but escaped C strings keep the file readable like upstream `builtin.c`).
- Wire via a CMake custom command + generated source added to the binary target. Python 3
  becomes a build-time-only dependency (already required by parts of upstream's tooling;
  if a pure-CMake generator is preferred and simple, that's acceptable too).
- Embedded animations are parsed by **the same parser** as file-based ones (§6) — one
  code path.
- `--list-logos` may optionally also list builtin animations (or add
  `--list-animations`); nice-to-have, not required for v1.

---

## 10. Converter tool (`tools/gif2anim`)

Python 3 script (stdlib only) that turns an animated GIF (or any image sequence) into a
`.anim` file, orchestrating existing tools:

```sh
tools/gif2anim input.gif -o out.anim --width 30 --fps 12 [--loop 0]
```

Pipeline:
1. Extract + coalesce frames: ImageMagick (`convert input.gif -coalesce frame_%04d.png`)
   (detect and use `magick` on newer installs).
2. Per frame, render to ANSI text: `chafa --size <W>x<H> --colors full --format symbols
   --animate off frame.png` (flags approximate — pin them during implementation; the
   requirement is raw SGR-colored symbol output at fixed size).
3. Assemble: header (`!fps`, optional `!loop`), frames verbatim separated by `---`.
4. Sanity-check output parses with the animfetch parser (`--check` mode: just validate an
   existing `.anim`).

`examples/README.md` must document:
- this tool's usage;
- a manual recipe for **web-based** GIF→ASCII converters: export/convert with any such
  tool, paste frames into a text file, insert `---` between frames, optionally add a
  `!fps` header — done. (Do not hard-depend on any specific website.)

---

## 11. Rebrand checklist (Phase 0)

Mechanical, no functional changes beyond renaming:

- [ ] CMake: `project(...)` name, binary target/output name `fastfetch` → `animfetch`.
- [ ] Binary-visible strings: `--version` output, `--help` header, man page
      (`doc/fastfetch.1.in` → `doc/animfetch.1.in`, contents + CMake reference).
- [ ] Config dir `~/.config/fastfetch` → `~/.config/animfetch` (+ fallback, §8.5);
      data/preset install dirs; `$XDG` handling; `--list-config-paths`,
      `--list-data-paths` output.
- [ ] `$schema` URL / any upstream project URLs in docs and `doc/json_schema.json`
      (point at the fork's repo once it exists; until then leave a TODO comment).
- [ ] README: new project README (credit fastfetch prominently); keep `LICENSE` (MIT)
      with upstream copyright retained.
- [ ] `UPSTREAM.md`: upstream repo URL, pinned tag `2.68.1`, exact commit hash, date.
- [ ] Decide `flashfetch` handling (§4.2); default: build it unchanged.
- [ ] Do **not** rename internal C identifiers/prefixes (`ff*`, `FF*`) — pointless churn
      that makes future cherry-picks painful. Rebrand is user-facing surfaces only.

---

## 12. Suggested repository layout (end state)

```
animfetch/
  PLAN.md                  # this file (keep until project matures)
  UPSTREAM.md
  README.md
  LICENSE
  CMakeLists.txt
  cmake/
  doc/ (animfetch.1.in, help.json, json_schema.json)
  examples/ (*.anim, README.md)
  presets/ (upstream, untouched)
  src/ (upstream tree + src/logo/animation.c/.h + generated builtin animations)
  tests/ (pty_smoke.py, config_compat.sh, README.md)
  tools/ (embed_animations.py, gif2anim)
  .github/workflows/ (ci.yml, release.yml)
```

---

## 13. Testing & QA

### 13.1 PTY smoke test (`tests/pty_smoke.py`, Python 3 stdlib: `pty`, `select`, `os`)

Runs the built binary under a pseudo-terminal (`TERM=xterm-256color`) with a small test
`.anim` (3 frames, unique token per frame, `!fps 60` for speed) and asserts on captured
bytes:
1. `ESC[?25l` (cursor hide) occurs, and `ESC[?25h` occurs before process exit;
2. each frame's unique token appears (animation actually drew frames);
3. frame redraws use absolute CUP sequences and never write past the padded region width
   (assert no occurrence of erase-in-line `ESC[K` / `ESC[0K` / `ESC[2K` in animation
   output);
4. the hold frame's token is the last frame token emitted;
5. exit code 0; total wall time within expected bounds (frames × 1/fps, ±margin).

Companion cases:
- **Pipe mode** (`animfetch … | cat`): no `ESC[?25` sequences at all; final frame text
  present; output otherwise upstream-identical.
- **Infinite mode**: `loop: 0`, `timeout: 300` — terminates on its own.
- **SIGINT**: send during animation; assert `ESC[?25h` still emitted and prompt state
  sane (echo restored).

### 13.2 Config compatibility test (`tests/config_compat.sh`)

- Run every preset in upstream `presets/` with `-c`: all must succeed with output
  identical to a stock upstream `fastfetch 2.68.1` build of the same preset (diff the
  piped outputs — build upstream once in CI for this baseline).
- An existing-user `config.jsonc` (fixture with no animation keys) must parse with zero
  errors and unchanged output.

### 13.3 Manual terminal matrix (before release)

kitty, foot, alacritty, GNOME Terminal (VTE), Konsole, WezTerm, tmux, screen, SSH from
each of a couple of hosts; plus inside VS Code's terminal. Verify: no flicker/tearing,
info columns never corrupted, cursor restored, resize mid-animation is safe.

---

## 14. Phased implementation plan

Each phase ends green and committable. Do not skip exit criteria.

### Phase 0 — Fork & rebrand
1. Clone upstream, checkout tag `2.68.1`; record hash in `UPSTREAM.md`; verify the stock
   build passes (`cmake -B build && cmake --build build`; run `./build/fastfetch`).
2. Import into the new repo (fresh history is fine for a hard fork; keep the upstream
   commit hash documented).
3. Rebrand per §11.
**Exit:** `animfetch` builds; `--version` shows the new name; behavior identical to
upstream 2.68.1 (spot-check + preset diff harness from §13.2 passes).

### Phase 1 — `.anim` parser
1. `src/logo/animation.h/.c`: data model + parser per §6 (header directives, separator
   splitting, CR stripping, visible-width calc skipping CSI sequences, `$N` placeholder
   awareness, padding, limits, precise error messages with line numbers).
2. Wire the CMake target; add `tools/embed_animations.py` + codegen producing the
   builtin-animation table from `examples/*.anim` (start with 1–2 stub files).
**Exit:** parser loads valid files, rejects malformed ones with clear errors; embedded
table builds and is iterable.

### Phase 2 — Renderer
1. Static fallback preconditions (§7.2), DSR cursor query, region computation (§7.3),
   frame loop with sync-output (§7.4), signal/termios cleanup (§7.5), timing (§7.6).
2. Hardcode a temporary test path (e.g. env var pointing at a `.anim`) until Phase 3
   lands real config plumbing — remove the hack at the end of Phase 3.
**Exit:** PTY smoke test core case passes in kitty + alacritty + GNOME Terminal + tmux;
pipe mode is byte-clean; SIGINT mid-animation restores the terminal.

### Phase 3 — Config/CLI integration
1. §8 in full: enum value, options struct fields, CLI flags, JSON parse/generate,
   `doc/help.json`, man page, `doc/json_schema.json`, config-dir precedence,
   source resolution incl. distro auto-detection (§8.4).
2. Remove Phase 2's temporary test hook.
**Exit:** every stock fastfetch config/preset runs unchanged (§13.2 green); new options
work from config and CLI; `--gen-config` emits them; `--help` documents them.

### Phase 4 — Content
1. Author the distro animations per §9.2 (cachyos, arch, debian, ubuntu, fedora,
   default, spinner).
2. `examples/README.md` (format spec + recipes), `tools/gif2anim` (§10).
**Exit:** `animfetch` on each named distro auto-animates; `gif2anim` converts a sample
GIF into a working `.anim`; a user following only `examples/README.md` can make their own
animation.

### Phase 5 — QA & release
1. Complete §13 suite in CI (GitHub Actions: build + PTY smoke + compat diff vs upstream
   baseline).
2. Manual terminal matrix (§13.3); fix fallout.
3. Release workflow: tag → build single-binary Linux x86_64 artifact (mirror upstream's
   release build profile) + tarball + checksums; README install instructions.
**Exit:** CI green on the release tag; artifact runs standalone on a clean machine (glibc
baseline per upstream releases).

---

## 15. Risks & mitigations

| Risk | Mitigation |
|---|---|
| Flicker/tearing on some terminals | DEC 2026 synchronized output where supported; single-buffered frame writes; exact-width absolute-CUP redraws elsewhere |
| Corrupting the info columns | Never write past the padded region width; never use erase-in-line; PTY test asserts both |
| Terminal resize mid-animation | SIGWINCH → stop loop, freeze current frame |
| Malformed/huge `.anim` files | Parser caps (frames/lines/columns), line-numbered errors |
| Cursor/termios left in bad state on crash | Single cleanup path; signal handlers; PTY test asserts `ESC[?25h` on every exit incl. SIGINT |
| Naming collision with upstream's image-logo `animationFrame` | Separate `logo.animation` object / `--logo-animation-{fps,loop,timeout,hold}`; documented in help text (§8.2) |
| DSR query hangs on exotic terminals | ~50 ms timeout → static fallback (§7.2) |
| Windows builds break | All animation code behind POSIX guards; Windows takes the static fallback path |
| `chafa`/ImageMagick flag drift in `gif2anim` | `--check` validation mode; document minimum versions |

---

## 16. Compatibility guarantees & non-goals

### 16.1 Guarantees
- Any valid fastfetch `config.jsonc` runs unchanged; every upstream CLI flag behaves as
  upstream.
- Piped/redirected output is byte-identical to upstream (static final frame when an
  animation would have played).
- Single self-contained binary; no required runtime data files.
- MIT license, upstream attribution retained.

### 16.2 Non-goals (v1)
- Animation that persists after the process exits (impossible for ASCII — the shell owns
  the terminal afterwards).
- Windows-native animation (static fallback only).
- Logo positions `top`/`right` animation (static fallback only).
- Runtime GIF decoding in the binary (the `tools/gif2anim` converter covers this).
- Tracking upstream releases continuously (hard fork).

---

## 17. Stretch goals

Status as of the current implementation (see §19):

1. **Kitty/iTerm2 graphics-protocol animated logos** — **not implemented.** This is the
   only route to animation that persists after exit (the terminal emulator loops the
   image itself), but it requires multi-frame GIF decoding plus terminal-specific
   protocol work and cannot be verified in this environment. Note that `--logo-type
   kitty-direct` with an animated image file may already animate on terminals that decode
   the file themselves; that is untested and upstream behaviour.
2. **Single-file HTML web converter** — **done:** `web/index.html` (drag a GIF, preview,
   download `.anim`; GitHub Pages-ready), covered by `tests/web_converter.test.cjs`.
3. **Community animation packs / `--list-animations` / packaging** — **done:** 35 bundled
   animations including slow-spin variants; `--list-animations`; packaging scaffolding in
   `packaging/` and `flake.nix` (needs the real repository URL before publishing).
4. **Animation for `top`/`right` logo positions** — **done** (covered by PTY tests).
   **Windows Terminal animation support — not implemented** (Windows still prints the
   hold frame statically).

---

## 18. Appendix: quick reference for the implementing agent

First commands:

```sh
git clone https://github.com/fastfetch-cli/fastfetch
cd fastfetch && git checkout 2.68.1 && git rev-parse HEAD   # record in UPSTREAM.md
cmake -B build && cmake --build build -j
./build/fastfetch --version && ./build/fastfetch | head
```

Useful greps once inside the tree:

```sh
grep -rn "ffLogoPrint" src/logo/                 # print flow / hook point
grep -rn "animationFrame" src/                   # upstream image-frame feature (do not clash)
grep -rn "config.jsonc" src/                     # config path resolution
grep -rn "x1b\[" src/common/ src/detection/ | grep -i "6n\|DSR"  # terminal query helpers
grep -rn "logo-type" doc/help.json               # where --logo-type values are documented
```

ANSI references used in §7:
- CUP: `ESC[{row};{col}H` · DSR: `ESC[6n` (response `ESC[{row};{col}R`)
- Cursor hide/show: `ESC[?25l` / `ESC[?25h`
- Synchronized output (DEC 2026): `ESC[?2026h` / `ESC[?2026l`

## 19. Implementation status (2026-09-21)

All phases are implemented and verified on the `moofetch` hard fork:

| Phase | Status | Where |
|---|---|---|
| 0 — Fork & rebrand | Done | `UPSTREAM.md`, renamed binaries/dirs/docs |
| 1 — `.anim` parser | Done | `src/logo/animation.c` (+ `animation.h`) |
| 2 — Renderer | Done | `src/logo/animation.c`, hooks in `src/logo/logo.c` |
| 3 — Config/CLI | Done | `src/options/logo.*`, `src/fastfetch.c`, docs/schema/man |
| 4 — Content | Done | `examples/*.anim`, `examples/README.md`, `tools/gif2anim`, `tools/gen-animations.py` |
| 5 — QA & release | Done | `tests/pty_smoke.py`, `tests/config_compat.sh`, `.github/workflows/{ci,release}.yml` |

Verified:

- `tests/pty_smoke.py` — animation playback, hold frames, infinite timeout, SIGINT
  cleanup, pipe-mode static output, config-file options, tall logos.
- `tests/config_compat.sh` — deterministic info output and **all builtin logos render
  byte-identically** to stock fastfetch 2.68.1; all presets run cleanly; fastfetch config
  fallback and `moofetch/` precedence work.
- `cmake --install` installs `moofetch`, `mooflash`, man page, bash/zsh/fish completions
  and presets under `share/moofetch/`.

Deviations from this plan:

- Upstream **release 2.68.1 has no `animationFrame` field** (it exists only on upstream
  `dev`), so there is no naming collision at all. Our options are `logo.animation` and
  `--logo-animation-{fps,loop,timeout,hold}`; `--list-animations` was added.
- Animation data is embedded with **pure CMake** (`fastfetch_load_text` + `file(WRITE)`),
  no Python codegen step is required.
- The DSR cursor query is performed **once after printing**; the region start row is
  derived from the final cursor row and the known line counts (scroll-safe, simpler than
  two queries).
- Frames are normalized to **not** end with a trailing newline, mirroring how upstream
  parses logo files; this keeps piped output byte-identical to the static logo.
- Bare `moofetch` prefers a builtin animation for the detected OS (plan §5). Piped output
  is unaffected because the hold frame has the same characters as the static logo.
- The interactive `--gen-config` UI offers the new `animation` logo type; the
  non-interactive `--gen-config` keeps upstream behaviour (modules only).
- Upstream project artifacts (`debian/`, upstream CI matrix, issue templates,
  `screenshots/`, `README-cn.md`) were removed as part of the hard fork; see `UPSTREAM.md`.

Stretch goals follow-up (see §17):

- Added 28 more bundled distro animations (35 total) and slow 3D-spin variants
  `arch_rotate.anim` / `cachyos_rotate.anim` (36 frames, one revolution, freeze on the
  original).
- Animation now works for `top` and `right` logo positions, with PTY tests.
- Added the browser converter (`web/index.html`, Node-tested) and packaging scaffolding
  (`packaging/`, `flake.nix`).
- Not done: kitty/iTerm2 persistent animation and Windows animation (see §17).

End of document.
