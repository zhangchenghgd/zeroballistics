#!/bin/sh

SCRIPT="$0"
SERVER_EXE="$1"

process=`ps auxw | grep $SERVER_EXE | grep -v start_server | grep -v grep | awk '{print $11}'`


if [ -z "$process" ]; then

  # go to the program directory
  cd "`dirname "$SCRIPT"`"

  # send informative email.
  tail $SERVER_EXE.log | mail -s "$SERVER_EXE was restarted by cron." quanticode@gmx.net

  # add libraries to the ld path
  export LD_LIBRARY_PATH={$LD_LIBRARY_PATH}:./shared_libs

  # start server
  nohup ./$SERVER_EXE &>/dev/null </dev/null &
fi
