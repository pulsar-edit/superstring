#!/bin/bash

echoerr() { echo "$@\n" >&2; }

usage() {
  echoerr "superstring requires the GNU libiconv library. You can install it with Homebrew (\`brew install libiconv\`) and we'll be able to detect its presence. You may also define a SUPERSTRING_LIBICONV_PATH variable set to the absolute path of your libiconv installation. (This path should have \`lib\` and \`include\` as child directories.)"
}

# Identify the directory of this script.
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

# Find this package's `vendor` directory; make sure it exists.
VENDOR="$SCRIPT_DIR/../vendor"
if [ ! -d "$VENDOR" ]; then
  echoerr "Aborting; expected $VENDOR to be a directory, but it was not."
  exit 1
fi

TARGET="$VENDOR/libiconv"

# Make a `libiconv` directory for us to vendorize into.
if [ ! -d "$TARGET" ]; then
  mkdir "$TARGET"
fi

if [[ ! -z "${SUPERSTRING_LIBICONV_PATH}" ]]; then
  # First is to allow the user to specify a path and override our heuristics.
  # This should propagate even if the user ran `yarn install` from a project
  # that has `superstring` as a dependency.
  source="${SUPERSTRING_LIBICONV_PATH}"
elif command -v brew &> /dev/null; then
  # If that variable isn't set, then we check if this machine has Homebrew
  # installed. If so, we'll opt into Homebrew's version of `libiconv`. This is
  # the safest option because we can reasonably conclude that this `libiconv`
  # is the right flavor and matches the system's architecture.
  source="$(brew --prefix)/opt/libiconv"
else
  # If neither of these things is true, we won't try to add an entry to
  # `library_dirs`.
  usage
  exit 1
fi

if [ ! -d "$source" ]; then
  echoerr "Expected $source to be the path to GNU libiconv, but it is not a directory. "
  usage
  exit 1
fi

dylib_path="${source}/lib/libiconv.2.dylib"

if [ ! -f "$dylib_path" ]; then
  echoerr "Invalid location for libiconv. Expected to find: ${dylib_path} but it was not present."
  usage
  exit 1
fi

# Recursively copy the contents of the source to the destination, following
# symlinks.
cp -R "${source}/include" "$TARGET/"
cp "${dylib_path}" "$TARGET/lib/"

# Set the install name of this library to something neutral.
install_name_tool -id "libiconv.2.dylib" "${dylib_path}"
