# Tests

Two test suites cover moofetch. Both are run in CI (`.github/workflows/ci.yml`).

## `pty_smoke.py` — animation behaviour

Runs the built binary under a pseudo-terminal that emulates the terminal queries moofetch
sends (DSR cursor position, text-area size), then asserts on the captured byte stream:

* animation plays the expected number of frames and freezes on the hold frame
* cursor is hidden and restored (also on SIGINT)
* infinite mode (`loop: 0`) stops on timeout
* no clear-to-end-of-line escapes are ever emitted (info columns stay untouched)
* piped output is fully static (no cursor/sync-output escapes)
* options are honoured from a JSONC config file
* tall logos (logo taller than the info block) animate correctly

```sh
python3 tests/pty_smoke.py build/moofetch
```

## `config_compat.sh` — upstream compatibility

Diffs moofetch against a stock fastfetch `2.68.1` build for deterministic outputs:

* a fixed info structure must be byte-identical
* every builtin logo must render byte-identically
* every preset must run cleanly
* fastfetch configs are still read as a fallback, while `moofetch/` configs take precedence

```sh
# Build the upstream baseline first (see the script header)
tests/config_compat.sh build/moofetch /tmp/fastfetch-2.68.1/build/fastfetch
```
