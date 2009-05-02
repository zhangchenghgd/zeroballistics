

import os

import Blender
from Blender import Registry, Window



_REGISTRY_KEY = "ZeroBal"



#------------------------------------------------------------------------------
def _setRegistryDefault(name, default):
    try: config[name]
    except KeyError:
        config[name] = default
        save()


#------------------------------------------------------------------------------
def _exportDirCallback(filename):
    global config
    config["export_dir"] = os.path.dirname(filename)
    save() # This is run asynchronously...

#------------------------------------------------------------------------------
#  Get export data dir from user if registry is not set yet, user
#  verbosity from config
def _init():
    global config,_verbosity

    config = Registry.GetKey(_REGISTRY_KEY, True)
    if config is None: config = {}

    try: config["export_dir"]
    except KeyError:
        Window.FileSelector(_exportDirCallback, "Select Export Dir")


    _setRegistryDefault("export_dir",              "/data")
    _setRegistryDefault("blender_executable_file", "blender")
    _setRegistryDefault("nvdxt_executable_file",   "nvdxt.exe")
    _setRegistryDefault("quick_n_dirty",           "False")
    _setRegistryDefault("export_all_scenes",       "True")
    
    save()

#------------------------------------------------------------------------------
def save():
    global config
    Registry.SetKey(_REGISTRY_KEY, config, True )



_init()

