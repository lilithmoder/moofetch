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

## TODOs for the fork owner

- The repository URL `https://github.com/moofetch/moofetch` is a **placeholder** used in
  `$schema` output and package metadata. Update it (and `CPACK_PACKAGE_CONTACT` in
  `CMakeLists.txt`) once the real repository/owner is known.
