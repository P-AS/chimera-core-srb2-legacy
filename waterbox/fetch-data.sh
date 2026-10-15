#!/bin/sh
# fetch-data.sh - the game's data for the gate where none is installed (CI).
#
# usage: waterbox/fetch-data.sh [<out folder>]   (default build/srb2-legacy-data)
#
# SRB2's data is Sonic Team Junior's, freely distributed, and never carried by
# this repository: this takes the files the core declares as firmware
# (srb2.srb, zones.dta, player.dta, rings.dta, patch.dta, music.dta and
# legacy.pk3) from srb2-preservation's assets repository on Codeberg
# (codeberg.org/srb2-preservation/assets, branch legacy, through git lfs), at
# the commit named below, and checks each against waterbox.config's SHA-1 -
# so the gate runs on exactly the files the package declares. They are the
# same files as SRB2 Legacy 2.1.30's SRB2-Legacy-v2130-Full.zip. A folder that
# already holds them, checked, is left as it is. Needs git-lfs.
set -eu
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/.." && pwd)"
out="${1:-$root/build/srb2-legacy-data}"
repo="https://codeberg.org/srb2-preservation/assets.git"
commit="1af2863f5755481c672b7a3a35ccd2133cc80b89"

check() {
	python3 - "$here/waterbox.config" "$out" <<'PY'
import hashlib, json, os, sys
cfg, out = json.load(open(sys.argv[1])), sys.argv[2]
bad = []
for fw in cfg["firmware"]:
    path = os.path.join(out, fw["name"])
    if not os.path.exists(path):
        bad.append(f"{fw['name']}: missing")
        continue
    h = hashlib.sha1(open(path, "rb").read()).hexdigest()
    if h != fw["sha1"]:
        bad.append(f"{fw['name']}: sha1 {h}, declared {fw['sha1']}")
if bad:
    print("\n".join(bad), file=sys.stderr)
    sys.exit(1)
PY
}

if check 2>/dev/null; then
	echo "data: $out (already there, checked)"
	exit 0
fi
command -v git-lfs >/dev/null 2>&1 || { echo "fetch-data: git-lfs is needed (apt-get install git-lfs)" >&2; exit 1; }
mkdir -p "$out"
clone="$out/.assets"
rm -rf "$clone"
# the pointers first, then only the declared files' objects, at the pinned commit
GIT_LFS_SKIP_SMUDGE=1 git clone -q --no-checkout --filter=blob:none "$repo" "$clone"
git -C "$clone" -c advice.detachedHead=false checkout -q "$commit"
names="$(python3 -c 'import json,sys; print(",".join(f["name"] for f in json.load(open(sys.argv[1]))["firmware"]))' "$here/waterbox.config")"
git -C "$clone" lfs pull --include "$names"
for f in $(echo "$names" | tr ',' ' '); do
	cp "$clone/$f" "$out/$f"
done
rm -rf "$clone"
check
echo "data: $out (SRB2 Legacy's data, from srb2-preservation/assets at ${commit%"${commit#???????}"}, checked)"
