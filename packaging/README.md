# Packaging

Packaging files for moofetch. **All of them contain a placeholder repository URL
(`https://github.com/moofetch/moofetch`)** — update it (and any checksums) once the fork
has a real home, then they are ready to use.

| File | Target | Status |
|---|---|---|
| `aur/PKGBUILD` | Arch Linux / AUR (`moofetch-git`) | Ready once the repo URL is real; builds from git, so no checksums needed |
| `../flake.nix` + `../nix/package.nix` | Nix flakes | Builds the flake's own source (`src = self`), no hashes needed |
| `homebrew/moofetch.rb` | Homebrew (macOS/Linux) | Template — needs a tagged release tarball and its `sha256` |

Build the binary locally the same way every packager does:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build --prefix /usr
```

Build-time dependencies: a C compiler, CMake ≥ 3.12 and Python 3 (for the man page and
for embedding `examples/*.anim`). Runtime: none beyond libc; animations are compiled into
the binary.
