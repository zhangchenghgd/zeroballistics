


import math
from math import degrees, sqrt, cos, acos

from traceback import format_exc

import xml.dom.minidom
from xml.dom.minidom import getDOMImplementation

import Blender
from Blender import Object, Mathutils, Library, Types, Scene, Mesh
from Blender.Window import DrawProgressBar
from Blender.Mathutils import Vector


import export_utils
from export_utils import *

from zero import msg
from zero.msg import message
from zero.registry import config


xml_doc = None


#------------------------------------------------------------------------------
def distance(p1, p2):
    sqrDist = pow(p2[0]-p1[0], 2) + pow(p2[1]-p1[1], 2) + pow(p2[2]-p1[2], 2)
    return (sqrt(sqrDist))


	
#------------------------------------------------------------------------------
def calcABCD(point, normal):
    origin = [0,0,0]
    normal.normalize()
    v = Blender.Mathutils.Vector([0,0,0])
    v[0] = point[0] - origin[0]
    v[1] = point[1] - origin[1]
    v[2] = point[2] - origin[2]	
    angle = 0
    if 0 != v.length:
            v.normalize()
            angle = acos(Blender.Mathutils.DotVecs(normal, v))
    length = distance(point, origin)
    d = length * cos(angle)
    abcd = [normal[0], normal[1], normal[2], d]
    return abcd


#------------------------------------------------------------------------------
def getLocalExtents(object):

    if object.getData() is None: return (1.0, 1.0, 1.0)
    
    euler = object.getEuler()
    pos = object.getLocation()
    zeroEuler = Blender.Mathutils.Euler([0,0,0])
    object.setEuler(zeroEuler)
    Blender.Redraw()

    corners = object.getBoundBox()
    # now we have 8 points defining the corners of the object
    # calculate the range for each dimension
    bbox_min = map(lambda *row: min(row), *corners)
    bbox_max = map(lambda *row: max(row), *corners)

    x = bbox_max[0] - bbox_min[0]
    y = bbox_max[1] - bbox_min[1]
    z = bbox_max[2] - bbox_min[2]

    # under some circumstances, the returned BB consists of inf and
    # nan values. Detect this here.
    if 2*(x+y+z) == x+y+z:
        raise Exception("Bounding box extents are (%f,%f,%f). Blender Bug!?!" % (x,y,z) )

    object.setLocation(pos[0], pos[1], pos[2])
    object.setEuler(euler)

    if x<0 or y<0 or z<0:
        raise Exception("Negative dimensions in getLocalExtents. Negative scale factor?")

    return x, y, z

 
        
#------------------------------------------------------------------------------
def exportShape(obj, parent_element):

    message("Exporting shape " + obj.name, msg.V_DEBUG)
    
    shape_element = createShapeElement(obj, parent_element)

    transform = Blender.Mathutils.Matrix(obj.getMatrix())

    if obj.getProperty("s_type").getData() == "continuous":
        pass
    else:
        dimensions = addElement("Dimensions", shape_element, xml_doc)
        size = getLocalExtents(obj)

        if obj.getProperty("s_type").getData() == "box":
            dimensions.setAttribute("x", str(size[0]))
            dimensions.setAttribute("y", str(size[2]))
            dimensions.setAttribute("z", str(size[1]))
        elif obj.getProperty("s_type").getData() == "sphere":
            dimensions.setAttribute("radius", str(size[0]*0.5))
        elif obj.getProperty("s_type").getData() == "ccylinder":

            radius = size[0]*0.5
            length = size[2]# - 2*radius

            if length < 0:
                raise Exception("Negative length for capped cylinder. Check dims.")

            dimensions.setAttribute("radius", str(radius))
            dimensions.setAttribute("length", str(length))

            # rotate around x axis to match ode ccylinder with blender cylinder bounds
            rot = Blender.Mathutils.RotationMatrix(90, 4, "x")
            transform = rot*transform
            
        elif obj.getProperty("s_type").getData() == "plane":
            # planes take a lot of extra work, especially since we don"t
            # already have a, b, c, and d for the plane equation
            pos = obj.getLocation()
            rotmat = obj.getMatrix("worldspace").rotationPart()
            initialNormal = Blender.Mathutils.Vector([0,0,1])
            normal = initialNormal * rotmat
            normal.normalize()
            # this doesn"t work because normal doesn"t always get updated :(
            #normal = obj.getData().faces[0].normal

            abcd = calcABCD(pos, normal)
            dimensions.setAttribute("a", str(abcd[0]))
            dimensions.setAttribute("b", str(abcd[2]))
            dimensions.setAttribute("c", str(abcd[1]))
            dimensions.setAttribute("d", str(abcd[3]))
        elif obj.getProperty("s_type").getData() == "ray":

            length = size[2]
            dimensions.setAttribute("length", str(length))

            # The origin of the object is in the middle of the ray;
            # should be at the start, so translate along ray dir
            offset = Blender.Mathutils.Vector(transform[1][0:3])
            transform[3][0:3] = list(transform.translationPart() + offset*length*0.5)
        else:
            raise Exception("Unknown shape type in object %s" % obj.getName())



    if obj.getProperty("s_type").getData() != "plane":
        exportTransform(transform, shape_element, xml_doc)


                

#------------------------------------------------------------------------------
#  Exports the specified objects as trimeshes (merge them all
#  together, assuming that their properties are identical
def exportTrimeshes(trimeshes, parent_element):

    message("exporting trimeshes", msg.V_DEBUG)

    num_vertices = 0

    vert_element = xml_doc.createTextNode("")
    face_element = xml_doc.createTextNode("")

    parent_element.appendChild(vert_element)
    parent_element.appendChild(face_element)

    vert_element.data += "["
    face_element.data += "["

    for tri_obj in trimeshes:
        mesh = tri_obj.getData(False, True)



        if not mesh:
            message(tri_obj.getName() + " is no mesh but has export shape prop.")
            continue
        
        for v_no, vert in enumerate(mesh.verts):

            if v_no % PROGRESS_UPDATE_INTERVAL == 0:
                DrawProgressBar(float(v_no)/(len(mesh.verts)+len(mesh.faces)-2), "Trimesh Verts: " + mesh.name)

            
            vec = Blender.Mathutils.Vector(vert.co).resize4D();
            vec *= tri_obj.getMatrix()

            vert_element.data += ("[" +
                                  str( vec[0]) + ";" +
                                  str( vec[2]) + ";" +
                                  str(-vec[1]) + "];")

        for face_no, face in enumerate(mesh.faces):

            if face_no % PROGRESS_UPDATE_INTERVAL == 0:
                DrawProgressBar(float(v_no + face_no)/(len(mesh.verts)+len(mesh.faces)-2), "Trimesh Faces: " + mesh.name)

            face_element.data += ("[" +
                                  str(face.v[0].index + num_vertices) + ";" +
                                  str(face.v[1].index + num_vertices) + ";" +
                                  str(face.v[2].index + num_vertices) + "];" )            


            if len(face.v) == 4:
                face_element.data += ("[" +
                                      str(face.v[2].index + num_vertices) + ";" +
                                      str(face.v[3].index + num_vertices) + ";" +
                                      str(face.v[0].index + num_vertices) + "];" )                           

        DrawProgressBar(1.0, "Trimesh Faces: " + mesh.name)

        num_vertices += len(mesh.verts)

    vert_element.data = vert_element.data[:-1]
    vert_element.data += "]\n"
    face_element.data = face_element.data[:-1]
    face_element.data += "]"




#------------------------------------------------------------------------------
#  Export COG, properties stored in PROPS
def exportProperties(element):
    for prop in getPropsObject().getAllProperties():
        new_element = addElement(str(prop.getName()), element, xml_doc)
        new_element.setAttribute("value", str(prop.getData()))


    cog_element = addElement("COG", element, xml_doc)

    try:
        cog = Object.Get("COG").getLocation("worldspace")
        cog_element.setAttribute("x", str( cog[0]))
        cog_element.setAttribute("y", str( cog[2]))
        cog_element.setAttribute("z", str(-cog[1]))
    except:
        cog_element.setAttribute("x", "0")
        cog_element.setAttribute("y", "0")
        cog_element.setAttribute("z", "0")   




#------------------------------------------------------------------------------
def createShapeElement(obj, parent_element):

    # Names are limited to 21 characters. If the object uses the full
    # name but has a .xxx extension added, it may have happend that
    # parts of the name got lost when copying the scene.
    if len(obj.getName()) == 17:
        message(obj.getName() + "|has possibly been capped during export.|Check and rename!", msg.V_HIGH)
        

    message("Creating Shape element for " + obj.getName(), msg.V_DEBUG)

    obj.getProperty("s_type") # make sure this prop is present
    
    shape_element = addElement("Shape", parent_element, xml_doc)
    shape_element.setAttribute("name", getCanonicalName(obj.getName()))

    # Export remaining properties
    for prop in [p for p in obj.getAllProperties() if p.getName().find("s_") == 0]:
        shape_element.setAttribute(prop.getName()[2:], str(prop.getData()))

    return shape_element




#------------------------------------------------------------------------------
def exportOde():
    global xml_doc

    obj_list = [o for o in Scene.GetCurrent().getChildren() if (isObjectInLayer(o, SHAPE_LAYER) and
                                                            type(o.getData()) is Types.NMeshType)]

    # if no objects, bail only if bbm layer is empty too.
    if len(obj_list) == 0:
        bbm_objects = [ o for o in Scene.GetCurrent().getChildren() if isObjectInLayer(o, BBM_LAYER) ]
        if len(bbm_objects) == 0: return
    
    checkForDegenerates(obj_list)



    filename = os.path.join(config["export_dir"], "models",
                             getCanonicalName(Scene.GetCurrent().getName()) + ".xml")
    message("exporting ode xml to " + filename)


    # Delete prev and create new export scene
    current_scene = Scene.GetCurrent()
    try:
        export_scene = Scene.Get('ode_export_scene')
        Scene.Unlink(export_scene)
    except: pass
    export_scene = current_scene.copy(2) # full copy to avoid multi-user issue
    export_scene.setName('ode_export_scene')
    export_scene.update()


        
    dom_impl = getDOMImplementation()
    xml_doc = dom_impl.createDocument(None, "RigidBody", None)
    root_element = xml_doc.documentElement

    exportProperties(root_element)


    # Find all objects which are to be exported as shape
    shape_objects = [o for o in export_scene.getChildren() if isObjectInLayer(o, SHAPE_LAYER) ]

    # ... and move the COG into the origin for all parent shapes
    try:
        cog = Vector(Object.Get("COG").getLocation("worldspace"))
    except:
        cog = Blender.Mathutils.Vector([0,0,0])

    # We must transform COG to the origin. Clear all parent / child
    # relations because of bad side effects otherwise (ray
    # translation...)
    for obj in [o for o in export_scene.getChildren() ]:
        obj.clrParent()
        obj.setLocation(Vector(obj.getLocation()) - cog)

    export_scene.update(1)


    # Trimesh shapes are merged
    trimesh_objects = [o for o in shape_objects if getPropertySafe(o, "s_type") == "trimesh"]

    # Other shapes are exported separately
    other_objects =   [o for o in shape_objects if o not in trimesh_objects ]
    



    failed_objects = []
    try:
        if len(trimesh_objects):
            exportTrimeshes(trimesh_objects,
                            createShapeElement(trimesh_objects[0], root_element))


        for o in other_objects:
            try:
                exportShape(o, root_element)
            except Exception, m:
                message(format_exc())
                message("Removing " + getCanonicalName(o.getName()) +
                        " from exported shapes because of error \"" + str(m) + "\"",
                        msg.V_HIGH)
                failed_objects.append(export_scene.getChildren().index(o))


        f = open(filename, "w")
        xml_doc.writexml(f)
#        f.write(xml_doc.toprettyxml()) # Cannot do this until more
#        robust vector parsing is implemented (whitespace problems...)
        f.close()

       
    finally:        
        # Restore Original Scene
        message("Restoring Scene...", msg.V_DEBUG)
        current_scene.makeCurrent()
        Scene.Unlink(export_scene)

        # Remove failed objects from shape layer
        for ind in failed_objects:
            removeObjectFromLayer(current_scene.getChildren()[ind], SHAPE_LAYER)

        
        Blender.Redraw()
