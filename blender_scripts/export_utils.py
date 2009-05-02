


import math,sys,re

import os.path

from sets import Set

import Blender
from Blender import Types, Draw, Object, Scene, Mesh, Window

import cmutils
from cmutils import vecCross


from zero import msg
from zero.msg import message




BBM_LAYER          = 11
SHAPE_LAYER        = 12


PROGRESS_UPDATE_INTERVAL = 200



#------------------------------------------------------------------------------
def export(all_scenes, quick_n_dirty):

    from zero.export_ode    import exportOde
    from zero.export_bbm    import exportBbm
    from zero.export_images import exportImages

    orig_current_scene = Scene.GetCurrent()

    if all_scenes:
        exported_scenes = Scene.Get()
    else:
        exported_scenes = [orig_current_scene]

    Window.WaitCursor(True)
    for cur_scene in exported_scenes:
        cur_scene.makeCurrent()

        Window.DrawProgressBar(0.0, "Exporting images")
        exportImages()
        Window.DrawProgressBar(1.0, "Finished exporting images")

        Window.DrawProgressBar(0.0, "Exporting BBM")
        exportOde()
        Window.DrawProgressBar(1.0, "Finished exporting bbm")


        Window.DrawProgressBar(0.0, "Exporting Shapes")
#     profile.runctx("exportBbm(quick_n_dirty_button.getValue())",
#                    globals(), locals()
#                    #, "/home/christian/Desktop/profile"
#                    )
        exportBbm(quick_n_dirty)
        Window.DrawProgressBar(1.0, "Finished exporting shapes")


    Window.WaitCursor(False)
    orig_current_scene.makeCurrent()
        
    


#------------------------------------------------------------------------------
def getPropertySafe(obj, prop):
    try:
        return obj.getProperty(prop).getData()
    except:
        return None


#------------------------------------------------------------------------------
def hasProperty(obj, prop):
    try:
        obj.getProperty(prop).getData()
        return True
    except: return False



#------------------------------------------------------------------------------
#  If a property with the specified name exists, it is
#  returned. Else a new property with the given default name is
#  created.
def getOrAddProperty(prop_name, default, obj):
    if obj is None: return 0
    try:
        prop = obj.getProperty(prop_name)
        return prop.getData()
    except:               
        obj.addProperty(prop_name, default)
        return default



#------------------------------------------------------------------------------
def setOrAddProperty(prop_name, value, obj):
    try:
        prop = obj.getProperty(prop_name)
        prop.setData(value)
    except:
        obj.addProperty(prop_name, value)


#------------------------------------------------------------------------------
#  Returns the "PROPS" object, creates it if neccessary.
def getPropsObject():
    for o in Scene.GetCurrent().objects:
        if getCanonicalName(o.getName()) == "PROPS": return o
    props_object = Scene.GetCurrent().objects.new("Empty")
    props_object.setName("PROPS")
    return props_object


#------------------------------------------------------------------------------
#  Selects the given faces for the specified object
def selectFaces(obj, faces):
    assert(type(obj.getData()) is Types.NMeshType)
    
    # remove any selection
    for o in Object.GetSelected(): o.select(False)
    obj.select(True)
    makeObjectVisible(obj)

    Mesh.Mode(Mesh.SelectModes.FACE)

    obj.getData(False, True).sel = False

    for f in faces: f.sel = True

    Blender.Window.EditMode(1)
    Blender.Window.Redraw(-1)    


    
        

#------------------------------------------------------------------------------
def checkForDegenerates(object_list):
    def isFaceDegenerate(v0,v1,v2):
        cross = vecCross(v1.co - v0.co, v2.co - v0.co)
        f = reduce(lambda a,b:a+b, map(lambda a:a*a, cross), 0)
        return f < 1e-14

    for cur_obj in object_list:
        if not type(cur_obj.getData()) is Types.NMeshType: continue
        
        degenerate_faces = []

        mesh = cur_obj.getData(False, True)

        for face in mesh.faces:    
            if isFaceDegenerate(face.v[0], face.v[1], face.v[2]): degenerate_faces.append(face)
            if len(face.v) == 4:
                if isFaceDegenerate(face.v[2], face.v[3], face.v[0]): degenerate_faces.append(face)



        if len(degenerate_faces):
            selectFaces(cur_obj, degenerate_faces)
            raise Exception("\"" + cur_obj.getName() + "\" has degenerate faces. Aborting export.")
                


#------------------------------------------------------------------------------
def execCmd(cmd, params):

    # to avoid messy issues with space in path, change to executable directory...
    cmd_dir = os.path.dirname(cmd)
    if cmd_dir: os.chdir(cmd_dir)

    res = os.system(os.path.basename(cmd) + " " + params)
    if res != 0:
        message("\"" + cmd + " " + params + "\" exited with status %d. See console." %res, msg.V_HIGH)

    return res





#------------------------------------------------------------------------------
def addObjectToLayer(obj, layer):
    try: obj.layers.index(layer)
    except:
        ls = obj.layers
        ls.append(layer)
        obj.layers = ls


#------------------------------------------------------------------------------
def removeObjectFromLayer(obj, layer):    
    try:
        ls = obj.layers
        ls.remove(layer)
        if ls == []: ls = [1]
        obj.layers = ls
    except: pass

#------------------------------------------------------------------------------
def isObjectInLayer(obj, layer):
    try:
        obj.layers.index(layer)
        return True
    except: return False
    
#------------------------------------------------------------------------------
#  If obj is none, make the currently active object visible
def makeObjectVisible(obj = None):

    cur_scene = Scene.GetCurrent()
    selected_layers = Set(cur_scene.getLayers())
    
    if obj is None:
        # Find selected object
        cur_scene.setLayers(range(1,21)) # Neccessary to get selected object in subsequent call...
        try:
            obj = Object.GetSelected()[0]
        except:
            return
        finally:
            cur_scene.setLayers(list(selected_layers))
        

    obj.restrictDisplay = False
        
    object_layers = Set(obj.layers)
    if len(object_layers.intersection(selected_layers)) == 0:
        # Object invisible, activate all layers object is in
        cur_scene.setLayers(list(object_layers.union(selected_layers)))
        Blender.Redraw()        


    

#------------------------------------------------------------------------------
def getChildren(object):
    children = []

    for o in Object.Get():
        if o.getParent() != object:
            continue
        else:
            children.append(o)
            
    return children    

#------------------------------------------------------------------------------
def splitPath(path):
    ret = []

    head,tail = os.path.split(path)
    while len(tail) != 0:
        ret.insert(0,tail)
        head,tail = os.path.split(head)

    return ret



#------------------------------------------------------------------------------
#  Strips the number ".xxx" from the name.
def getCanonicalName(name):

    if re.compile("[^a-zA-Z0-9_.:]").search(name):
        raise Exception("\"" + name + "\"" + " doesn't adhere to naming conventions. Aborting export.")
    
    parts = name.rsplit('.', 1)

    try:
        # See whether last .XXX is number. If so, remove it
        int(parts[1])
        return parts[0]
    except:
        # else no extension is present or its not a number -> return
        # full name
        return name



#------------------------------------------------------------------------------
#  Adds an xml element with the specified name to the given parent
#  element.
def addElement(name, parent, xml_doc):
    new = xml_doc.createElement(name)
    parent.appendChild(new)
    return new



#------------------------------------------------------------------------------
def exportTransform(mat, parent_element, xml_doc):

    transform = addElement('Transform', parent_element, xml_doc)

    # x-axis
    transform.setAttribute('_11', str( mat[0][0]))
    transform.setAttribute('_21', str( mat[0][2]))
    transform.setAttribute('_31', str(-mat[0][1]))

    # y-axis
    transform.setAttribute('_12', str( mat[2][0]))
    transform.setAttribute('_22', str( mat[2][2]))
    transform.setAttribute('_32', str(-mat[2][1]))

    # z-axis
    transform.setAttribute('_13', str(-mat[1][0]))
    transform.setAttribute('_23', str(-mat[1][2]))
    transform.setAttribute('_33', str( mat[1][1]))

    # translation
    transform.setAttribute('_14', str( mat[3][0]))
    transform.setAttribute('_24', str( mat[3][2]))
    transform.setAttribute('_34', str(-mat[3][1]))

