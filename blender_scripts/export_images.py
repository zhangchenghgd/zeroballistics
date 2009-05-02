
import os
import os.path


from sets import Set


from Blender import Image, Window

from zero.export_utils import * 
from zero.registry import config





#------------------------------------------------------------------------------
# Traverse all objects in bbm layer to find used images. Returns a
# list with their names.
def getUsedImages():

    ret = Set()
    
    bbm_objects = [ o for o in Scene.GetCurrent().objects if (isObjectInLayer(o, BBM_LAYER) and
                                                              type(o.getData()) is Types.NMeshType) ]

    
    for o in bbm_objects:
        mesh = o.getData(False, True)

        if mesh.faceUV:
            orig_layer = mesh.activeUVLayer
            for layer_name in mesh.getUVLayerNames():
                mesh.activeUVLayer = layer_name

                for f in mesh.faces:
                    if f.image: ret.add(f.image.getName())

            mesh.activeUVLayer = orig_layer

    message("Used textures: " + str(list(ret)), msg.V_DEBUG)

    return list(ret)



#------------------------------------------------------------------------------
def exportImage(filename, dest_path, format):

    dest_path = os.path.join(os.path.join(config["export_dir"], "textures", "models"),
                             dest_path)

    dds_filename = os.path.join(dest_path,
                                os.path.splitext(os.path.basename(filename))[0] + ".dds")

    # if dds already exists and is newer than our file, go on to next file
    try:
        if os.stat(dds_filename).st_mtime > os.stat(filename).st_mtime: return False
    except: pass

    message("Format for " + filename + " is " + format, msg.V_DEBUG)

    Window.WaitCursor(True)
    
    execCmd(config["nvdxt_executable_file"],
            " -force4" +
            " -rescale nearest" +
            " -outdir \"" + dest_path.replace("\\", "/") + "\"" +
            format +
            " -file " + filename)

    return True


#------------------------------------------------------------------------------
#  First compiles a list of all used images by traversing all objects
#  in BBM_LAYER.
#
#  24-bpp images are compressed with dxt1, 32-bpp with dxt5.
def exportImages():

    try:
        num_converted_images = 0
        
        used_images = getUsedImages()

        for img_ind,img_name in enumerate(used_images):        
            DrawProgressBarIfInteractive(float(img_ind) / len(used_images), img_name)
            img = Image.Get(img_name)
            assert not img.packed, "Images must not be packed"
            

            if img.getDepth() == 24: format = " -dxt1c"
            else:                    format = " -dxt5"

            found_file = True
            try:
                components = splitPath(img.getFilename())


                if len(components) > 2:subdir = os.path.join(*components[1:-1])
                else: subdir = ""

                name = components[-1]
                print subdir + " " + name
                filename = os.path.join(os.path.dirname(Blender.Get("filename")),
                                        "textures",
                                        subdir,
                                        name)
            except:
                found_file = False

            if found_file and not os.path.exists(filename): found_file = False
            if not found_file: raise Exception("Image " + img.getFilename() +
                                               " is not saved or is not relative to blend file")

            
            if exportImage(filename, subdir, format): num_converted_images += 1

            em_filename = os.path.join(os.path.dirname(filename),
                                       "em_" + os.path.basename(filename))
            if os.path.exists(em_filename):
                message("Found emissive map " + em_filename, msg.V_INFO)
                if exportImage(em_filename, subdir, " -a8"): num_converted_images += 1
            
    finally:
        Window.WaitCursor(False)
        DrawProgressBarIfInteractive(1.0, "")


