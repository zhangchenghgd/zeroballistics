#!/bin/sh

master_dir="/home/flaminggaming/c++/gamecode/tools/master_server"
process=`ps auxw | grep master\_server | grep -v grep | awk '{print $11}'`

if [ -z "$process" ]; then

  echo "Couldn't find Zero master server running, restarting it."
  cd "$master_dir"
  LD_LIBRARY_PATH=/home/flaminggaming/c++/lib nohup ./master_server >/dev/null&
  echo ""

fi

