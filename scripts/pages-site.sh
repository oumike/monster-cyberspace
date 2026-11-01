#!/bin/bash
# Builds the browser flasher site (GitHub Pages) from a release's factory
# images:
#
#   scripts/pages-site.sh <tag> <dir with monster-cyberspace-*-<tag>.bin> <out dir>
#
# Writes <out>/index.html and the Fluffy modals' files (from site/),
# <out>/firmware/<image>.bin, <out>/manifests/<board>.json (ESP Web Tools
# manifests: one merged image at 0x0; fluffy.js adds the mascot blob) and
# <out>/boards.json ({"version", "boards": [slug, ...]}) for the page.
# The images are copied because browsers cannot fetch GitHub release assets
# (no CORS headers). Run by .github/workflows/pages.yml.
#
# Bash 3.2 (macOS's stock bash).
set -euo pipefail

if [[ $# -ne 3 ]]; then
    sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
fi
tag="$1" src="$2" out="$3"

cd "$(git rev-parse --show-toplevel)"
mkdir -p "$out/firmware" "$out/manifests"
cp site/index.html site/fluffy.js site/fluffy.css site/mascot.mjs site/mascots.json "$out/"
touch "$out/.nojekyll"

slugs=()
for f in "$src"/monster-cyberspace-*-"$tag".bin; do
    [[ -e "$f" ]] || continue
    name="$(basename "$f")"
    slug="${name#monster-cyberspace-}"
    slug="${slug%-"$tag".bin}"
    cp "$f" "$out/firmware/$name"
    # Every board is an ESP32-S3 but the X4, an ESP32-C3 (scripts/release.sh
    # env_chip). The part path is relative to the manifest.
    chip="ESP32-S3"
    [[ "$slug" == "x4" ]] && chip="ESP32-C3"
    cat > "$out/manifests/$slug.json" <<EOF
{
  "name": "Monster for Cyberspace",
  "version": "$tag",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "$chip",
      "parts": [{ "path": "../firmware/$name", "offset": 0 }]
    }
  ]
}
EOF
    slugs+=("$slug")
done

if [[ ${#slugs[@]} -eq 0 ]]; then
    echo "Error: no monster-cyberspace-*-$tag.bin in $src" >&2
    exit 1
fi

{
    printf '{\n  "version": "%s",\n  "boards": [' "$tag"
    sep=""
    for s in "${slugs[@]}"; do printf '%s"%s"' "$sep" "$s"; sep=", "; done
    printf ']\n}\n'
} > "$out/boards.json"

echo "Flasher site for $tag in $out: ${slugs[*]}"
