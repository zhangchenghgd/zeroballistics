



from Blender import Draw


from zero.registry import config


(V_HIGH, V_INFO, V_DEBUG) = range(3)

_verbosity = V_DEBUG


# set this to a file object to supress pop-up menu messages and write
# them to the file instead.
logfile = None


#------------------------------------------------------------------------------
def message(s, priority = V_INFO):
    if priority <= _verbosity:
        print s    
    if priority == V_HIGH:
        if logfile: logfile.write(s + "\n")
        else : Draw.PupMenu(s)


try: _verbosity = config["verbosity"]
except KeyError: config["verbosity"] = _verbosity




