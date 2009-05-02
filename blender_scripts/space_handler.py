#SPACEHANDLER.VIEW3D.DRAW

import Blender
from Blender import Registry, Draw


eventCallback = Registry.GetKey("updateButtons")
if eventCallback and len(eventCallback) != 0: eventCallback[0]()

