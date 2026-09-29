#!/usr/bin/env bash
# Prints Markdown release notes for tag $1: the commit subjects since the
# previous v* tag, plus how to install the AppImage. Used by the AppImage
# workflow; run locally to preview: cpp/packaging/release-notes.sh v0.2.0
set -euo pipefail

tag="${1:?usage: release-notes.sh <tag>}"
repo="${GITHUB_REPOSITORY:-macery/dnd5e}"
previous="$(git describe --tags --abbrev=0 --match 'v*' "$tag^" 2>/dev/null || true)"

echo "## Changes"
echo
if [ -n "$previous" ]; then
    git log --no-merges --reverse --pretty='- %s' "$previous..$tag"
    echo
    echo "**Full changelog:** https://github.com/$repo/compare/$previous...$tag"
else
    git log --no-merges --reverse --pretty='- %s' "$tag" -- cpp
fi

cat <<NOTES

## Install (Linux x86_64)

\`\`\`sh
wget https://github.com/$repo/releases/download/$tag/DnD_Engine-x86_64.AppImage
chmod +x DnD_Engine-x86_64.AppImage && ./DnD_Engine-x86_64.AppImage
\`\`\`

Runs on Ubuntu 24.04 and newer distros; nothing else to install. If it won't start, install FUSE 2
(\`sudo apt install libfuse2t64\`) or run it with \`APPIMAGE_EXTRACT_AND_RUN=1\`. Each machine keeps its
own save file; characters don't transfer between machines yet.
NOTES
