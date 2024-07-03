#!/bin/bash

# This script can be used if we find it necessary, for code signing reasons,
# not to alter the install name of `libiconv.2.dylib` during compilation. macOS
# complains when we do it, saying that the code signature has been invalidated,
# but we haven't noticed any ill effects… yet.
#
# But this script would allow us to point `superstring.node` at the correct
# library by figuring out `libicov.2.dylib`’s existing install name, rather
# than setting it to a known value in an earlier step.

product_dir=$1

# Ask for the current install name expected by `superstring.node`. We need to
# know this in order to change it in the next step.
current_install_name=$(otool -L "$product_dir/superstring.node" | awk 'BEGIN{FS=OFS=" "};NR==2{print $1}')

# Now use `install_name_tool` to tell `superstring.node` to instead look for
# `libiconv.2.dylib` at a path relative to itself.
install_name_tool -change \
  "$current_install_name" \
  "@loader_path/../../vendor/libiconv/lib/libiconv.2.dylib" \
  "$product_dir/superstring.node"
