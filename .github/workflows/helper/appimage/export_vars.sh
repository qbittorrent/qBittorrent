#!/bin/sh

# this file is called from AppRun so 'root_dir' will point to where AppRun is
root_dir="$(readlink -f "$(dirname "$0")")"

# Insert the default values because after the test we prepend our path
# and it will create problems with DEs (eg KDE) that don't set the variable
# and rely on the default paths
if [ -z "${XDG_DATA_DIRS}" ]; then
    XDG_DATA_DIRS="/usr/local/share/:/usr/share/"
fi

export XDG_DATA_DIRS="${root_dir}/usr/share:${XDG_DATA_DIRS}"
