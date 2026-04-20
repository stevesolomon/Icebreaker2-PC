#!/usr/bin/env bash
# Case-sensitive audit of asset references vs files on disk.
# IMPORTANT: /mnt/e/* on WSL is case-insensitive, so [ -e ] gives false
# negatives. We do a proper case-sensitive string match against a disk
# listing instead.
set -e
cd "$(dirname "$0")/.."

# 1. Pull every "$boot/..." reference out of source, normalize to assets/...
grep -rhoE '\$boot/[^"]+\.(cel|3do|ttf|anim|aifc?|imag|AIF|AIFC|AIFF|CEL|3DO|TTF|ANIM|IMAG)' src/ \
    | sort -u \
    | sed -e 's|^\$boot/IceFiles/|assets/|' \
          -e 's|^\$boot/icefiles/|assets/|' \
          -e 's|^\$boot/|assets/|' \
    > /tmp/refs.txt

# 2. Capture exact-case disk listing.
find assets -type f | sort > /tmp/disk_exact.txt
# 3. Lowercase index: lower\tactual
awk '{ orig=$0; lc=tolower($0); print lc "\t" orig }' /tmp/disk_exact.txt > /tmp/disk_lc.txt

mismatch=0
missing=0
> /tmp/case_mismatches.txt
while IFS= read -r ref; do
    [ -z "$ref" ] && continue
    # exact case-sensitive match against disk?
    if grep -qxF "$ref" /tmp/disk_exact.txt; then continue; fi
    # case-insensitive lookup?
    lc=$(echo "$ref" | tr '[:upper:]' '[:lower:]')
    actual=$(awk -F'\t' -v k="$lc" '$1==k { print $2; exit }' /tmp/disk_lc.txt)
    if [ -n "$actual" ]; then
        echo "CASE: ref='$ref'  actual='$actual'" | tee -a /tmp/case_mismatches.txt
        mismatch=$((mismatch+1))
    else
        echo "MISS: $ref"
        missing=$((missing+1))
    fi
done < /tmp/refs.txt

echo "---"
echo "case mismatches: $mismatch"
echo "missing files:   $missing"

