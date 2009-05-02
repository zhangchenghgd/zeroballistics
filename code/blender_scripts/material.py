
import os

import Blender

#------------------------------------------------------------------------------
class Material:
    #--------------------------------------------------------
    def __init__(self):
        self.name = "default"
        self.color = (1.0, 1.0, 1.0)
        self.cull_faces = True
        self.lighting = True
        self.alpha_test = False
        self.alpha_blend = False
        self.specularity = 0.2
        self.hardness = 30

#         self.gloss_map = False
#         self.emissive_map = False
        self.bump_map = False
        self.parallax_strength = 0.07
        self.normal_strength = 1.5

        self.shader = ""

    #------------------------------------------------------------------------------
    def __hash__(self):
        return hash(self.name)

    #--------------------------------------------------------
    def __cmp__(self, other):
        if self.name > other.name: return -1
        elif self.name == other.name: return 0
        else: return 1

    #--------------------------------------------------------
    def save(self):
        f = open(self._getFilename(), "w")
        f.write(repr(self.__dict__))

    #--------------------------------------------------------
    def load(self):
        f = open(self._getFilename(), "r")

        loaded_dict = eval(f.read())

        # Transfer loaded values into own dict, ignore items no longer
        # present
        for key, val in loaded_dict.iteritems():
            if self.__dict__.has_key(key):
                self.__dict__[key] = loaded_dict[key];

    #--------------------------------------------------------
    def delete(self):
        os.remove(self._getFilename())

    #--------------------------------------------------------
    def createBlendMaterial(self):
        try:
            blend_mat = Blender.Material.Get(self.name)
        except NameError:
            blend_mat = Blender.Material.New(self.name)

        blend_mat.setRGBCol(list(self.color))
        return blend_mat

    #--------------------------------------------------------
    def rename(self, name):
        self.delete()
        self.name = name
        self.save()
        self.createBlendMaterial()
        
        

    #--------------------------------------------------------
    @staticmethod
    def loadAll():
        ret = []

        mat_dir = os.path.join(os.path.dirname(Blender.Get("filename")),
                               "materials")
        
        for m in os.listdir(mat_dir):
            if os.path.splitext(m)[1] != ".txt": continue

            new_mat = Material()
            new_mat.name = os.path.splitext(m)[0]
            new_mat.load()

            ret.append(new_mat)

            new_mat.createBlendMaterial()

        ret.sort()    
        return ret

    #--------------------------------------------------------
    def _getFilename(self):
        base_dir = os.path.dirname(Blender.Get("filename"))
        return os.path.join(base_dir, "materials", self.name + ".txt")
        
