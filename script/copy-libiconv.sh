#!/bin/bash
set -euo pipefail

# Copy the vendored libiconv next to the built product. Resolves the symlink
# (`ext/lib/libiconv.2.dylib` points at the versioned dylib) and installs the
# copy atomically, so two targets postbuilding in parallel can't observe a
# half-written file.

src="$1"
dest="$2"

tmp="$(mktemp "${dest}.XXXXXX")"
trap 'rm -f "$tmp"' EXIT

# `-L` because `ext/lib/libiconv.2.dylib` is a symlink to the versioned dylib.
# We want the real file here — a link would still point outside of `build/`,
# which is what we're getting away from.
cp -L "$src" "$tmp"

# We _must_ get the permissions right here; a `.dylib` with 0600 would work for
# the user who built it but fail for anyone else.
chmod 755 "$tmp"

mv -f "$tmp" "$dest"
