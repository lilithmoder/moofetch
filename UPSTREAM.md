# Upstream provenance

`moofetch` is a **hard fork** of [fastfetch](https://github.com/fastfetch-cli/fastfetch).

| | |
|---|---|
| Upstream repository | https://github.com/fastfetch-cli/fastfetch |
| Forked from tag | `2.68.1` |
| Upstream commit | `1c1136ebd1e943d6d2ba7c204a11deee3e948dd8` |
| Upstream commit date | 2026-09-01 |
| Fork date | 2026-09-21 |
| Upstream license | MIT (see `LICENSE`, upstream copyright retained) |

## Fork policy

This is a hard fork: upstream releases are **not** tracked continuously. The upstream
base commit above is tagged in this repository's history so specific upstream fixes can
still be cherry-picked manually if ever needed.

## Rebranding scope

- User-facing surfaces were renamed (`moofetch`, `mooflash`, config/data dirs, man page,
  completions, schema references).
- Internal C identifiers and file names (`fastfetch.h`, `FF*`/`ff*` prefixes,
  `libfastfetch`, `FASTFETCH_*` macros) were deliberately **kept** to keep the diff
  against upstream small and future cherry-picks feasible.
- `moofetch` still reads fastfetch's config/data directories as a fallback (see
  `README.md` and `PLAN.md` §8.5) for drop-in compatibility.

## Fork cleanup

The following upstream project artifacts were removed because they are specific to the
upstream project (and would be misleading or broken in this fork):

- `.github/workflows/*` (upstream build matrix) — replaced with `ci.yml` and `release.yml`
- `.github/FUNDING.yml`, issue templates, stale/dependabot config, benchmark dashboard
- `debian/` packaging (upstream package metadata) — add fork-specific packaging if needed
- `screenshots/`, `README-cn.md`

## Fork home

The fork lives at <https://github.com/lilithmoder/moofetch>; `$schema` output and package
metadata point there.

Published artifacts:

| | |
|---|---|
| Repository | https://github.com/lilithmoder/moofetch |
| First release | https://github.com/lilithmoder/moofetch/releases/tag/v0.1.0 |
| Browser converter (GitHub Pages) | https://lilithmoder.github.io/moofetch/ |
| Provenance tag | `upstream-2.68.1` (the upstream base commit) |

The full upstream history is included in this repository (not just the base commit).
The AUR `Maintainer:` line and the Homebrew `sha256` still need real values before
publishing (see `packaging/README.md`).
