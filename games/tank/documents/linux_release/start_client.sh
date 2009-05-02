
#!/bin/sh
cd "$(dirname $0)"
export LD_LIBRARY_PATH={$LD_LIBRARY_PATH}:./shared_libs
./tank.x86
