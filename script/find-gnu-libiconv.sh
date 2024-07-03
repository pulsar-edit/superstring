#!/bin/bash

# The purpose of this script is to find a copy of GNU `libiconv` on this macOS
# machine. Since newer versions of macOS include a FreeBSD `libiconv`, we no
# longer assume it's safe to use any ambient `libiconv.dylib` we find.
#
# For this reason, we try to detect a Homebrew installation of `libiconv`; we
# also allow the user to install GNU `libiconv` manually and specify the path
# via an environment variable.
#
# We might eventually replace this approach with an explicit vendorization of
# the specific files needed, but that would require a universal build of
# `libiconv.2.dylib`. For now, letting the user provide their `libiconv` has
# the advantage of very likely matching the system's architecture.

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
  # First, we allow the user to specify a path and override our heuristics.
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

# We expect the `dylib` we need to be at this exact path.
dylib_path="${source}/lib/libiconv.2.dylib"

if [ ! -f "$dylib_path" ]; then
  echoerr "Invalid location for libiconv. Expected to find: ${dylib_path} but it was not present."
  usage
  exit 1
fi

# We need the `include` directory for compilation, plus the `libiconv.2.dylib`
# file. We'll also copy over the README and license files for compliance.
cp -R "${source}/include" "$TARGET/"
cp "${dylib_path}" "$TARGET/lib/"
cp "${source}/COPYING.LIB" "$TARGET/"
cp "${source}/README" "$TARGET/"


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
