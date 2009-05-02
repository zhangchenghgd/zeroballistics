#!BPY
"""
Name: 'Material Editor'
Blender: 243
Group: 'Wizards'
Tooltip: 'Create and edit materials'
"""
import copy
import random

import Blender
from Blender import Draw

from zero import gui
from zero.material import Material




#------------------------------------------------------------------------------
def onBumpmapChanged(v):
    normal_strength_button.setVisible(v.getValue())
    parallax_strength_button.setVisible(v.getValue())
    

#------------------------------------------------------------------------------
def getCurrentMaterial():
    if len(material) == 0: return Material()
    else: return material[cur_material_button.getCurIndex()]


#------------------------------------------------------------------------------
def getMaterialPropUpdater(name):
    def updater(b):
        b.setValue(getCurrentMaterial().__dict__[name])
    return updater

#------------------------------------------------------------------------------
def getMaterialOnChanged(name):
    def onChanged(b):
        getCurrentMaterial().__dict__[name] = b.getValue()
        getCurrentMaterial().save()
    return onChanged


#------------------------------------------------------------------------------
def onMaterialColorChanged(b):
    cur_mat = getCurrentMaterial()
    cur_mat.color = b.getValue()
    cur_mat.save()

    cur_mat.createBlendMaterial().setRGBCol(cur_mat.color)
    


#------------------------------------------------------------------------------
def updateMaterialName(b):
    b.setValue(getCurrentMaterial().name)

#------------------------------------------------------------------------------
def onMaterialNameChanged(b):
    if len(material) == 0:
        b.setValue("")
    else:
        getCurrentMaterial().rename(b.getValue())
        material.sort()

        # sorting placed the current material someplace else...
        # find it
        for i,m in enumerate(material):
            if m.name == b.getValue():
                cur_material_button.setCurIndex(i)
                break
    gui_handler.updateElements()
    bump_button.onChanged()
    

#------------------------------------------------------------------------------
def updateCurMaterial(b):
    mats = [m.name for m in material]
    mats.sort()
    mats.reverse()
    b.setItems(*mats)


#------------------------------------------------------------------------------
def onCurMaterialChanged(b):
    gui_handler.updateElements()
    bump_button.onChanged()


#------------------------------------------------------------------------------
def onDelMaterial(v):
    if len(material)==0 : return

    cur_mat = getCurrentMaterial()
    
    result = Draw.PupMenu("Really delete material \"" + cur_mat.name + "\"?%t|Do as I say!")
    if result==-1: return

    cur_mat.delete()
    del material[cur_material_button.getCurIndex()]
    cur_material_button.setCurIndex(0)
    gui_handler.updateElements()
    bump_button.onChanged()



#------------------------------------------------------------------------------
def onNewMaterial(v):
    if len(material) == 0:
        new_mat = Material()
    else:        
        new_mat = copy.deepcopy(getCurrentMaterial())
        new_mat.name += "(copy)"

    new_mat.color = (random.random(),
                     random.random(),
                     random.random())
    new_mat.createBlendMaterial()

    material.append(new_mat)
    new_mat.save()
        
    material.sort()

    gui_handler.updateElements()
    bump_button.onChanged()



material = Material.loadAll()


#------------------------------------------------------------------------------
def addPropButton(gui_handler, prop_name, element):

    element.addChangedCallback(getMaterialOnChanged  (prop_name))
    element.setUpdateCallback (getMaterialPropUpdater(prop_name))
    gui_handler.addElement(element)
    
    globals()[prop_name+"_button"] = element

    return element
    


if __name__ == "__main__":

    gui_handler = gui.GuiHandler()
    gui_handler.bg_color = (0.0, 0.5, 0.0, 1.0)

    cur_x = gui.X_SPACING
    cur_y = gui.Y_SPACING

    prop_panel = gui.Panel((0.0, 0.0, 0.6))
    gui_handler.addElement(prop_panel)
    

    addPropButton(prop_panel, "parallax_strength",
                  gui.Slider(0.0, 0.1, "Parallax: ", cur_x, cur_y))
    cur_y += gui.Y_SPACING

    addPropButton(prop_panel, "normal_strength",
                  gui.Slider(1.0, 1.95, "Norm: ", cur_x, cur_y))
    cur_y += gui.Y_SPACING
    
    bump_button = addPropButton(prop_panel, "bump_map",
                                gui.ToggleButton("Bump Map", cur_x, cur_y))
    bump_button.addChangedCallback(onBumpmapChanged)
    cur_y += gui.Y_SPACING

#     addPropButton(prop_panel, "gloss_map",
#                   gui.ToggleButton("Gloss Map", cur_x, cur_y))
#     cur_y += gui.Y_SPACING

#     addPropButton(prop_panel, "emissive_map",
#                   gui.ToggleButton("Emissive Map", cur_x, cur_y))
#     cur_y += gui.Y_SPACING

    cur_y += gui.Y_SPACING


    addPropButton(prop_panel, "hardness",
                  gui.Slider(1.0,255.0, "Hard: ", cur_x, cur_y))
    cur_y += gui.Y_SPACING

    addPropButton(prop_panel, "specularity",
                  gui.Slider(0.0, 1.0, "Spec: ", cur_x, cur_y))
    cur_y += gui.Y_SPACING

    addPropButton(prop_panel, "alpha_blend",
                  gui.ToggleButton("Alpha Blending", cur_x, cur_y))
    cur_y += gui.Y_SPACING

    addPropButton(prop_panel, "alpha_test",
                  gui.ToggleButton("Alpha Testing", cur_x, cur_y))
    cur_y += gui.Y_SPACING

    addPropButton(prop_panel, "lighting",
                  gui.ToggleButton("Lighting", cur_x, cur_y))
    cur_y += gui.Y_SPACING

    addPropButton(prop_panel, "cull_faces",
                  gui.ToggleButton("Hide Backfaces", cur_x, cur_y))
    cur_y += gui.Y_SPACING

    addPropButton(prop_panel, "shader",
                  gui.StringButton("Shader", cur_x, cur_y))
    



    cur_y += gui.Y_SPACING
    cur_y += gui.Y_SPACING


    control_panel = gui.Panel((0.0, 0.0, 0.6))
    gui_handler.addElement(control_panel)

    control_panel.addElement(gui.ColorPicker("", cur_x, cur_y,
                                           onMaterialColorChanged,
                                           getMaterialPropUpdater("color")))
    cur_y += gui.Y_SPACING


    control_panel.addElement(gui.StringButton("Name", cur_x, cur_y,
                                            onMaterialNameChanged,
                                            updateMaterialName))
    cur_y += gui.Y_SPACING


    cur_material_button = control_panel.addElement(gui.ComboBox("Material", cur_x, cur_y,
                                                              onCurMaterialChanged,
                                                              updateCurMaterial))
    cur_y += gui.Y_SPACING

    control_panel.addElement(gui.PushButton("Delete Material", cur_x, cur_y,
                                          onDelMaterial))
    cur_y += gui.Y_SPACING

    control_panel.addElement(gui.PushButton("New Material", cur_x, cur_y,
                                          onNewMaterial))

    gui_handler.start()
    bump_button.onChanged()
