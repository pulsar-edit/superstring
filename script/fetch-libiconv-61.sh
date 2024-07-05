
#!/bin/bash

# The purpose of this script is to find a copy of GNU `libiconv` on this macOS
# machine. Since newer versions of macOS include a FreeBSD `libiconv`, we no
# longer assume it's safe to use any ambient `libiconv.dylib` we find.
#
# For this reason, we download a known good version of `libiconv` from
# https://github.com/apple-oss-distributions/libiconv/tree/libiconv-61.
#
# We might eventually replace this approach with an explicit vendorization of
# the specific files needed, but that would require a universal build of
# `libiconv.2.dylib`. For now, letting the user provide their `libiconv` has
# the advantage of very likely matching the system's architecture.

echoerr() { echo "$@\n" >&2; }


usage() {
  echoerr "superstring requires the GNU libiconv library, which macOS no longer bundles in recent versions. This package attempts to compile it from GitHub. If you're seeing this message, something has gone wrong; check the README for information and consider filing an issue."
}

# Identify the directory of this script.
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

ROOT="$SCRIPT_DIR/.."
SCRATCH="$ROOT/scratch"
EXT="$ROOT/ext"

cleanup() {
  if [ -d "$SCRATCH" ]; then
    rm -rf "$SCRATCH"
  fi
}
trap cleanup SIGINT EXIT

create-if-missing() {
  if [ -z "$1" ]; then
    echoerr "Error: $1 is a file."
    usage
    exit 1
  fi
  if [ ! -d "$1" ]; then
    mkdir "$1"
  fi
}

create-if-missing "$EXT"
create-if-missing "$SCRATCH"

dylib_path="$EXT/lib/libiconv.2.dylib"

# If this path already exists, we'll assume libiconv has already been fetched
# and compiled. Otherwise we'll do it now.
if [ ! -L "$dylib_path" ]; then
  echo "Path $dylib_path is missing; fetching and installing libiconv."
  cd $SCRATCH
  git clone -b libiconv-61 "https://github.com/apple-oss-distributions/libiconv.git"
  cd libiconv/libiconv
  ./configure --prefix="$EXT" --libdir="$EXT/lib"
  make
  make install

  if [ ! -L "$dylib_path" ]; then
    echoerr "Error: expected $dylib_path to be present, but it was not. Cannot proceed."
    usage
    exit 1
  else
    # Remove the directories we don't need.
    rm -rf "$EXT/bin"
    rm -rf "$EXT/share"
  fi
else
  echo "Path $dylib_path is already present; skipping installation of libiconv."
fi

cd $ROOT

# We expect this path to exist and be a symbolic link that points to a file.
if [ ! -L "$dylib_path" ]; then
  echoerr "Error: expected $dylib_path to be present, but it was not. Cannot proceed."
  usage
  exit 1
fi

# Set the install name of this library to something neutral and predictable to
# make a later step easier.
#
# NOTE: macOS complains about this action invalidating the library's code
# signature. This has not been observed to have any negative effects for
# Pulsar, possibly because we sign and notarize the entire app at a later stage
# of the build process. But if it _did_ have negative effects, we could switch
# to a different approach and skip this step. See the `binding.gyp` file for
# further details.

install_name_tool -id "libiconv.2.dylib" "${dylib_path}"

cleanup
