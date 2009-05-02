#!/bin/sh

# traverse symlinks
SCRIPT="$0"
if [ -L "${SCRIPT}" ]; then
    SCRIPT="`readlink -f "${SCRIPT}"`"
fi

# go to the program directory
cd "`dirname "$SCRIPT"`"

# add libraries to the ld path
export LD_LIBRARY_PATH={$LD_LIBRARY_PATH}:./shared_libs

# run game
./tank.x86

# return exitcode
EXITCODE="$?"
exit ${EXITCODE}

