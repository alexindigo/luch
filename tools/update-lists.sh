#!/bin/sh
# Refresh the vendored URL-cleaning lists from upstream and print a
# diffstat. Lists are data: refreshing them needs a rebuild/reinstall,
# no code change. Run before releases.
set -eu

cd "$(dirname "$0")/.."

base="https://raw.githubusercontent.com/brave/adblock-lists/master/brave-lists"
for list in debounce query-filter clean-urls; do
    curl -fsSL "$base/$list.json" -o "assets/lists/$list.json"
done
# The PSL project asks that the list be pulled only from this URL.
curl -fsSL "https://publicsuffix.org/list/public_suffix_list.dat" \
    -o assets/lists/public_suffix_list.dat

git diff --stat -- assets/lists/ || true
