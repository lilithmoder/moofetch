#!/usr/bin/env bash
# Regression test: moofetch must behave identically to upstream fastfetch 2.68.1 for
# deterministic outputs (no animation involved).
#
# Usage:
#   tests/config_compat.sh [path/to/moofetch] [path/to/upstream-fastfetch]
#
# Build the upstream baseline with:
#   git clone --depth 1 --branch 2.68.1 https://github.com/fastfetch-cli/fastfetch /tmp/fastfetch-2.68.1
#   cmake -B /tmp/fastfetch-2.68.1/build -S /tmp/fastfetch-2.68.1 -DCMAKE_BUILD_TYPE=Release
#   cmake --build /tmp/fastfetch-2.68.1/build -j

set -u

MOOFETCH="${1:-./build/moofetch}"
UPSTREAM="${2:-/tmp/opencode/upstream/build/fastfetch}"

if [[ ! -x "$MOOFETCH" ]]; then echo "moofetch not found: $MOOFETCH" >&2; exit 2; fi
if [[ ! -x "$UPSTREAM" ]]; then echo "upstream fastfetch not found: $UPSTREAM" >&2; exit 2; fi

STRUCTURE="title:os:kernel:shell:terminal:locale"
failures=0

# 1. Deterministic info structure must be byte-identical
moo=$("$MOOFETCH" --pipe --structure "$STRUCTURE")
up=$("$UPSTREAM" --pipe --structure "$STRUCTURE")
if [[ "$moo" == "$up" ]]; then
    echo "ok:   deterministic info structure identical"
else
    echo "FAIL: deterministic info structure differs"
    diff <(printf '%s\n' "$moo") <(printf '%s\n' "$up") | head -20
    failures=$((failures + 1))
fi

# 2. Every builtin logo must render byte-identically
mismatches=0
while IFS= read -r logo; do
    moo=$("$MOOFETCH" --pipe --logo-type builtin --logo "$logo" --structure title)
    up=$("$UPSTREAM" --pipe --logo-type builtin --logo "$logo" --structure title)
    if [[ "$moo" != "$up" ]]; then
        echo "FAIL: logo '$logo' differs"
        mismatches=$((mismatches + 1))
    fi
done < <("$UPSTREAM" --list-logos autocompletion)
if [[ $mismatches -eq 0 ]]; then
    echo "ok:   all builtin logos render identically"
else
    failures=$((failures + mismatches))
fi

# 3. Every upstream preset must parse and run cleanly
preset_failures=0
for preset in presets/*.jsonc presets/examples/*.jsonc; do
    [[ -e "$preset" ]] || continue
    err=$("$MOOFETCH" -c "$preset" --pipe --show-errors true 2>&1 >/dev/null)
    code=$?
    if [[ $code -ne 0 || "$err" == *"Error"* || "$err" == *"Logo:"* ]]; then
        echo "FAIL: preset $preset (exit $code): $err"
        preset_failures=$((preset_failures + 1))
    fi
done
if [[ $preset_failures -eq 0 ]]; then
    echo "ok:   all presets run cleanly"
else
    failures=$((failures + preset_failures))
fi

# 4. Existing fastfetch configs are still picked up as a fallback
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$tmp/fastfetch"
cat > "$tmp/fastfetch/config.jsonc" <<'EOF'
{ "logo": { "type": "builtin", "source": "arch" }, "modules": ["title", "os"] }
EOF
out=$(XDG_CONFIG_HOME="$tmp" "$MOOFETCH" --pipe)
if [[ "$out" == *"OS:"* ]]; then
    echo "ok:   fastfetch config fallback works"
else
    echo "FAIL: fastfetch config fallback"
    failures=$((failures + 1))
fi

# 5. moofetch configs take precedence over fastfetch configs
mkdir -p "$tmp/moofetch"
cat > "$tmp/moofetch/config.jsonc" <<'EOF'
{ "logo": { "type": "none" }, "modules": ["kernel"] }
EOF
out=$(XDG_CONFIG_HOME="$tmp" "$MOOFETCH" --pipe)
if [[ "$out" == "Kernel:"* ]]; then
    echo "ok:   moofetch config takes precedence"
else
    echo "FAIL: moofetch config precedence (got: ${out:0:60})"
    failures=$((failures + 1))
fi

echo
if [[ $failures -gt 0 ]]; then
    echo "$failures failure(s)"
    exit 1
fi
echo "all compatibility checks passed"
