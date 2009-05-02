

import sys,math
import os.path

from sets import Set

from traceback import format_exc


import Blender
from Blender import Types, Object, Draw, Scene, Image



import cmutils
from cmutils import *

import export_utils
from export_utils import *

from zero.registry import config

import zero.material

_materials = []
_quick_n_dirty = False




MAGIC_HEADER = 0xabcafe07


#enum BBM_CREATOR
#{
BC_BLENDER = 1
BC_NATIVE  = 2
#};


#enum BBM_MESH_OPTION
#{
BMO_LIGHTING           = 1
BMO_CULL_FACES         = 2
BMO_ALPHA_TEST         = 4
BMO_ALPHA_BLEND        = 8
BMO_PER_PIXEL_LIGHTING = 16

BMO_LIGHT_MAP          = 32
BMO_BUMP_MAP           = 64
BMO_EMISSIVE_MAP       = 128
#};


#enum BBM_NODE_OPTION
#{
BNO_SHADOW_BLOCKER  = 1
BNO_SHADOW_RECEIVER = 2
BNO_INSTANCED       = 4
#};



#------------------------------------------------------------------------------
# Node types.
(NT_MESH,
 NT_GROUP,
 NT_EFFECT) = range(3)


#------------------------------------------------------------------------------
class Vertex:
    #--------------------------------------------------------
    def __init__(self, v, n, t, index):
        self.v_ = v
        self.n_ = n
        self.t_ = t
        self.index_ = index

    #--------------------------------------------------------
    def equals(self, v):
        assert(self.index_ == v.index_)
        equal = (areVectorsEqual  (self.v_, v.v_) and
                 areVectorsEqual  (self.n_, v.n_))

        if not equal: return False
        if self.t_ and v.t_: return reduce(lambda a,b: a and b, map(areTexcoordsEqual,self.t_,v.t_),True)
        return self.t_ is v.t_




#------------------------------------------------------------------------------
class MeshInfo:
    #--------------------------------------------------------
    def __init__(self):
        self.vertex_list  = []
        self.index_list   = []
        self.vertex_table = {}



    #--------------------------------------------------------
    #  Traverse all vertices from the face and put them into the vertex
    #  array if they aren't already there.
    def addFace(self, f, mesh):
    
        # caculate the face normal vector if the face isn't smooth
        # (cross product between face edges)
        if not f.smooth:
            a = [f.v[1].co[0] - f.v[0].co[0], f.v[1].co[1] - f.v[0].co[1], f.v[1].co[2] - f.v[0].co[2]]
            b = [f.v[2].co[0] - f.v[0].co[0], f.v[2].co[1] - f.v[0].co[1], f.v[2].co[2] - f.v[0].co[2]]
            fn = vecCross(a, b)
            vecNormalize(fn)

        orig_layer = mesh.activeUVLayer

        # traverse face vertices
        for v_number, v in enumerate(f.v):

            if f.smooth: n = v.no
            else:        n = fn

            # store texture coordinates in vertex. meshes must have a
            # texture, and can optionally have a lightmap.
            t=[]
            for layer_name in mesh.getUVLayerNames():
                mesh.activeUVLayer = layer_name
                t.append(f.uv[v_number])

            vertex = Vertex(v.co, n, t, v.index)            

            # get an index for this vertex
            if _quick_n_dirty:
                cur_index = len(self.vertex_list)
                self.vertex_list.append(vertex)
            else:
                cur_index = self._storeVertex(vertex)

            self.index_list.append(cur_index)

        # complete the second triangle if this was a quad
        if len(f.v) == 4:
            self.index_list.append(self.index_list[-4])
            self.index_list.append(self.index_list[-3])


        mesh.activeUVLayer = orig_layer    

        
    #--------------------------------------------------------
    def write(self, file):
        self._writeVertices(file)
        self._writeIndices(file)


    #--------------------------------------------------------
    #
    # Returns the index of vert in vertex_list. Vertices shared
    # between faces can either have the same normal for both faces
    # (smooth) or not. Texture coordinates can be the same or differ,
    # too. We would like to duplicate vertices with different normals
    # and texcoords but share vertices with equal data. So there are
    # two types of indices: Blender indices (every blender vertex has
    # an index) and our own indices, which specify where in list the
    # vertex was put. vertex_table remembers for each blender index
    # where versions of this vertex were stored in self.vertex_list.
    def _storeVertex(self, vert):               
        if vert.index_ in self.vertex_table.keys():
            for possible_index in self.vertex_table[vert.index_]:
                if vert.equals(self.vertex_list[possible_index]):
                    # This vertex is already stored in vertex_list,
                    # return its index.
                    return possible_index
        else:
            # First occurence of this vertex
            self.vertex_table[vert.index_] = []

        new_index = len(self.vertex_list)

        self.vertex_table[vert.index_].append(new_index)
        self.vertex_list.append(vert)

        return new_index

    #--------------------------------------------------------
    #  On the other side, things are read into vector<>s, so writing the
    #  length for every component wastes space but makes things more
    #  comfortable.
    def _writeVertices(self, file):
    
        writeUnsigned32(file, len(self.vertex_list))
        for vertex in self.vertex_list:
            writeVector(file, vertex.v_)

        writeUnsigned32(file, len(self.vertex_list))
        for vertex in self.vertex_list:
            writeVector(file, vertex.n_)

        # tangent & bitangent are not calculated here - store dummy arrays
        writeUnsigned32(file, 0)
        writeUnsigned32(file, 0)

        # write arrays of texcoords (not interleaved)
        if self.vertex_list[0].t_:
            writeUnsigned32(file, len(self.vertex_list[0].t_)) # Number of texture coordinate sets
            for t in range(len(self.vertex_list[0].t_)):
                writeUnsigned32(file, len(self.vertex_list))
                for vertex in self.vertex_list:
                    writeTexcoord(file, vertex.t_[t])

    #--------------------------------------------------------
    def _writeIndices(self, file):
        writeUnsigned32(file, len(self.index_list))
        for i in self.index_list:
            writeUnsigned16(file, i)
                    
        

#------------------------------------------------------------------------------
def checkTexAlphaChannel(img, mat, mesh):
    if img.getDepth() == 32:
        if not mat.alpha_test and not mat.alpha_blend and not mat.bump_map:
            message("mesh " + mesh.name + ":|" +
                    img.getName() + " is used without alphatest/blend or bumpmap but has alpha channel.", msg.V_INFO)
    else:
        if mat.alpha_test or mat.alpha_blend or mat.bump_map:
            message("mesh " + mesh.name + ":|" +
                    img.getName() + " is used with alphatest/blend or bumpmap but has no alpha channel.", msg.V_INFO)
            
    

#------------------------------------------------------------------------------
#  Checks for faces having texcoords but no texture assigned
def checkForUntexturedFaces(object_list):

    for obj in object_list:

        if type(obj.getData()) is not Types.NMeshType: continue
        mesh = obj.getData(False, True)
        if not mesh.faceUV:
            raise Exception("\"" + obj.getName() + "\" has faces without texture. Aborting export.")

        untextured = []

        orig_layer = mesh.activeUVLayer

        for layer_name in mesh.getUVLayerNames():
            mesh.activeUVLayer = layer_name
            for face in mesh.faces:
                if not face.image: untextured.append(face)

            if len(untextured):
                selectFaces(obj, untextured)
                raise Exception("\"" + obj.getName() + "\" has faces without texture. Aborting export.")

        mesh.activeUVLayer = orig_layer
                    


#-----------------------------------------------------------------------------
#  Returns a pair (mat, (tex)), where mat is the material, and (tex)
#  is a tuple of images
def getMatTex(face, mesh):
    tex = []

    mat_name = ""
    if (mesh.materials and face.mat < len(mesh.materials) and mesh.materials[face.mat]):
        mat_name = mesh.materials[face.mat].getName()

    # create material if neccessary
    try:
        m = zero.material.Material()
        m.name = getCanonicalName(str(mat_name))
        ind = _materials.index(m)
        mat = _materials[ind]
    except ValueError:
        if mat_name != "":
            message("Couldn't find description for material \"" +
                    mat_name +
                    "\". Using default material.",
                    msg.V_HIGH)
        mat = zero.material.Material()
        

    assert mesh.faceUV
    
    orig_layer = mesh.activeUVLayer
    for layer_name in mesh.getUVLayerNames():
        mesh.activeUVLayer = layer_name

        assert(face.image) # this should have been checked before.
        tex.append(face.image)

    mesh.activeUVLayer = orig_layer

    return (mat,tuple(tex))


    
    



#------------------------------------------------------------------------------
def writeMeshes(obj, file):
    mesh = obj.getData(False, True)
    assert type(mesh) == Types.MeshType


    # stores data for each material / texture combination
    mesh_info = {}

    if len(mesh.faces)==0:
        message("Mesh " + mesh.name + "  has no faces", msg.V_HIGH)

    for face_ind, f in enumerate(mesh.faces):

        if face_ind % PROGRESS_UPDATE_INTERVAL == 0:
            DrawProgressBarIfInteractive(float(face_ind) / len(mesh.faces),
                            Scene.GetCurrent().getName() + " (collecting vertices): " + mesh.name)

        # get the face's material and texture names
        (mat, tex_images) = getMatTex(f, mesh)

        # If mat/tex combination hasn't shown up before, create the appropriate lists in our dictionaries.
        try:
            cur_info = mesh_info[(mat,tex_images)]
        except KeyError:
            # sanity check: if tex has alpha channel, either bumpmapping
            # or alpha blending/testing must be enabled, and vice versa
            for t in tex_images: checkTexAlphaChannel(t, mat, mesh)
            
            cur_info = MeshInfo()
            mesh_info[(mat,tex_images)] = cur_info

        cur_info.addFace(f, mesh)


    DrawProgressBarIfInteractive(1.0, "BBM: " + mesh.name)

    DrawProgressBarIfInteractive(0.0, "BBM (writing data): " + mesh.name)

    # traverse meshes (= all mat/tex combinations)

    if len(mesh_info) == 0:
        raise Exception("object " + obj.getName() + " has no meshes!")
    
    writeUnsigned32(file, len(mesh_info))
    for (mat,tex_images), cur_info in mesh_info.iteritems():
        

        tex_names = [ os.path.join(*splitPath(i.getFilename())[1:]) for i in tex_images ]

        # See whether an emissive map exists for the given base
        # texture.
        em_filename = os.path.join(os.path.dirname(Blender.Get("filename")),
                                   "textures",
                                   "em_" + os.path.basename(tex_names[0]))
        if os.path.exists(em_filename):
            has_em = True
            em_filename = os.path.basename(em_filename)
        else: has_em = False

        #write per-mesh options
        flags = BMO_PER_PIXEL_LIGHTING  # enable ppl by default for now...

        if len(tex_images) == 2     : flags += BMO_LIGHT_MAP
        if has_em                   : flags += BMO_EMISSIVE_MAP
        if mat.lighting    : flags += BMO_LIGHTING
        if mat.cull_faces  : flags += BMO_CULL_FACES
        if mat.alpha_test  : flags += BMO_ALPHA_TEST
        if mat.alpha_blend : flags += BMO_ALPHA_BLEND
        if mat.bump_map    : flags += BMO_BUMP_MAP
        writeUnsigned16(file, flags)

        writeString(file, mat.shader)

        cur_info.write(file)

        # Write texture names

        # Replace extension with "dds"
        writeString                        (file, os.path.splitext(tex_names[0])[0] + ".dds")
        if len(tex_names) == 2: writeString(file, os.path.splitext(tex_names[1])[0] + ".dds")
        if has_em             : writeString(file, os.path.splitext(em_filename) [0] + ".dds")

        #write bumpmap options
        if mat.bump_map :
            writeFloat(file, mat.parallax_strength)
            writeFloat(file, mat.normal_strength)

        # write specularity values
        writeFloat     (file, mat.specularity)
        writeUnsigned32(file, mat.hardness)

        message("%s : %d vertices, %d faces exported" %
                (str((mat.name, tex_names)), len(cur_info.vertex_list), len(cur_info.index_list)/3),
                msg.V_DEBUG)
    DrawProgressBarIfInteractive(1.0, "BBM (writing data): " + mesh.name)
    

#------------------------------------------------------------------------------
def exportGroups(obj, fid):
    groups = getPropertySafe(obj, "b_groups")
    if groups: writeString(fid, groups)
    else:      writeString(fid, "")


#------------------------------------------------------------------------------
def exportMeshObject( obj, fid):

    message("Exporting bbm mesh %s ..." % obj.getName())

    writeUnsigned32(fid, NT_MESH)
    writeString(fid, getCanonicalName(obj.getName()))

    exportGroups(obj, fid)

    writeMatrix(fid, obj.getMatrix())
    writeMeshes(obj, fid)

    #traverse children
    exportObjects([c for c in getChildren(obj) if isObjectInLayer(c, BBM_LAYER) ], fid)


#------------------------------------------------------------------------------
def exportGroupObject( obj, fid):

    message("Exporting group %s ..." % obj.getName())

    writeUnsigned32(fid, NT_GROUP)
    writeString(fid, getCanonicalName(obj.getName()))

    exportGroups(obj, fid)

    writeMatrix(fid, obj.getMatrix())

    #traverse children
    exportObjects([c for c in getChildren(obj) if isObjectInLayer(c, BBM_LAYER) ], fid)


#------------------------------------------------------------------------------
def exportEffectObject( obj, fid):

    writeUnsigned32(fid, NT_EFFECT)
    writeString(fid, getCanonicalName(obj.getName()[3:]))
    exportGroups(obj, fid)
    writeMatrix(fid, obj.getMatrix())
    
    

#------------------------------------------------------------------------------
def exportObjects( objects, fid):

    message("number of (child) objects: %d" % len(objects), msg.V_DEBUG)

    writeUnsigned32(fid, len(objects))

    for o in objects:

        # this can happen if an object is still in a group but not
        # linked to the scene.
        if not o in Scene.GetCurrent().getChildren(): 
            raise Exception("Warning: " + o.name + " is not linked to scene! Aborting export.")
        
        if o.getData():                exportMeshObject  ( o, fid )
        elif o.getName()[:3] == "ef:": exportEffectObject( o, fid )
        else:                          exportGroupObject ( o, fid )


#------------------------------------------------------------------------------
#  If quick_n_dirty is set, export three unique vertices for every
#  face and don't merge them (saves export time).
def exportBbm(quick_n_dirty = False):

    global _materials
    _materials = zero.material.Material.loadAll()

    global _quick_n_dirty
    _quick_n_dirty = quick_n_dirty


    model_dir = os.path.join(config["export_dir"], "models")
    # If there is more than one property object, create subdir for scene
    if len(getAllPropsObjects()) > 1:
        model_dir = os.path.join(model_dir, getCanonicalName(Scene.GetCurrent().getName()))


    for (prop_obj, obj_name) in getAllPropsObjects():

        filename = os.path.join(model_dir, obj_name + ".bbm")

        message("exporting bbm to " + filename)


        # Find all  mesh objects which are to be exported
        bbm_objects = [ o for o in Scene.GetCurrent().getChildren() if isObjectInLayer(o, BBM_LAYER) ]

        # only use chidlren of current PROPS object if there is more than one.
        if len(getAllPropsObjects()) > 1:
            bbm_objects = [ o for o in bbm_objects if isChildOf(o, prop_obj)]



        # Make sure no bbm object has non-bbm as parent
        for o in bbm_objects:
            assert (not o.getParent() or isObjectInLayer(o.getParent(), BBM_LAYER),
                    "Object \"" +  o.getName() + "\" has non-bbm parent." )

        checkForDegenerates(bbm_objects)
        checkForUntexturedFaces(bbm_objects)

        # Traverse only top-level objects which are either mesh or
        # empty. Empties will be exported as groups.
        bbm_objects = [o for o in bbm_objects if ((type(o.getData()) is Types.NMeshType or o.getData() is None)
                                                  and (o.getParent() == prop_obj or
                                                       o.getParent() == None     or
                                                       not o.getParent() in Scene.GetCurrent().getChildren()))]
            
        if len(bbm_objects) == 0:
            message(prop_obj.getName() + " has no bbm objects. Skipping.", msg.V_HIGH)
            continue



        fid = open(filename, "wb")

        # First, export magic header and properties pertaining to the whole object
        writeUnsigned32(fid, MAGIC_HEADER)

        # Blender was used to create this bbm...
        writeUnsigned32(fid, BC_BLENDER)

        flags=0
        if getPropertySafe(prop_obj, "Blocker")  : flags += BNO_SHADOW_BLOCKER
        if getPropertySafe(prop_obj, "Instanced"): flags += BNO_INSTANCED
        if getPropertySafe(prop_obj, "Receiver") : flags += BNO_SHADOW_RECEIVER
        writeUnsigned16(fid, flags)

        lod_class = getPropertySafe(prop_obj, "LodClass")
        if not lod_class: lod_class = "building"
        writeString(fid, lod_class)

        # Now, export all objects
        message("Exporting %d top-level objects" % len(bbm_objects), msg.V_DEBUG)
        exportObjects(bbm_objects, fid)




