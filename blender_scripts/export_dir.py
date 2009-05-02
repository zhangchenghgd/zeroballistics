#!BPY
"""
Name: 'Export all models'
Blender: 244
Group: 'Wizards'
Tooltip: 'Exports all models from a given directory'
"""

import os
from tempfile import mkstemp

from Blender import Window, Text, Draw

from zero.export_utils import execCmd

from zero.registry import config

#------------------------------------------------------------------------------
#  blender is called to execute this script for each file in
#  turn. this avoids memory corruption and python issues...
_EXPORT_HELPER_SCRIPT="""
import Blender
Blender.Load(filename)

import os, os.path
import Blender, Blender.Draw


import zero.msg
zero.msg.logfile = open(logfilename, "w")

from zero import msg
from zero.msg import message

from zero.export_utils import export

Blender.Window.Redraw(-1)

try:
    export(True, False)
except Exception, m:
    message(str(m), msg.V_HIGH)

Blender.Quit()
"""

#------------------------------------------------------------------------------
def fileCallback(filename):
    blend_dir = os.path.dirname(filename)

    output_valid = False
    output = None
    
    for p in os.listdir(blend_dir):
        if os.path.splitext(p)[1] != ".blend": continue

        (log_fd,log_path) = mkstemp(suffix=".txt", text=True)
        log_path = log_path.replace("\\", "/")
        os.close(log_fd)

        # Write helper script
        (fd,path) = mkstemp(suffix=".py", text=True)
        script_file = os.fdopen(fd, "w+b")
        complete_path = os.path.join(blend_dir, p)
        script_file.write("filename = \"" + complete_path.replace("\\", "/") + "\"\n" +
                          _EXPORT_HELPER_SCRIPT.replace("logfilename",
                                                        "\"" + log_path + "\""))
        script_file.close()


        # Now execute the helper script for the current filename
        execCmd( config["blender_executable_file"], "-P " + path)


        # See if anything was written into the log
        log = open(log_path, "r").read()
        if len(log):
            output_valid = True
            if not output: output = Text.New("result")
            output.write(p + ":\n" + log + "\n")
        
        os.remove(path)
        os.remove(log_path)

    if output_valid:
        Draw.PupMenu("|".join(output.asLines()))
        Draw.PupMenu("Check result in text window")


Window.FileSelector(fileCallback, "Select .blend dir")
