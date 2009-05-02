#!BPY
"""
Name: 'Zero Exporter Gui'
Blender: 243
Group: 'Wizards'
Tooltip: 'Various utilities for editing zerobal models'
"""

import profile

import Blender
from Blender import Window, Scene, Types, Object, Draw


from zero import msg
from zero.msg import message
from zero import gui

import zero.registry
from zero.registry import config


from zero.export_utils import makeObjectVisible, getPropsObject, getOrAddProperty, setOrAddProperty, BBM_LAYER, SHAPE_LAYER, addObjectToLayer, removeObjectFromLayer, isObjectInLayer, export



last_selected_object = None
last_edit_mode       = True


add_group_text = None


#------------------------------------------------------------------------------
#  Selects all objects which have the specified texture coordinate set
#  and makes this set the active one.
def selectTexSet(tex_no):
    for o in [o for o in Scene.GetCurrent().objects if type(o.getData()) is Types.NMeshType ]:
        mesh = o.getData(False, True)
        try:
            mesh.activeUVLayer = mesh.getUVLayerNames()[tex_no]
            mesh.renderUVLayer = mesh.getUVLayerNames()[tex_no]

            o.select(True)
            makeObjectVisible(o)

            mesh.update()
        except IndexError:
            o.select(False)

    Blender.Redraw()


#------------------------------------------------------------------------------
#  Copies properties from the first selected object with the specified
#  prefix to all other selected objects.
def copyProps(prefix):

    source_object = Object.GetSelected()[0]
    
    for obj in Object.GetSelected()[1:]:

        if type(obj.getData()) is not Types.NMeshType: continue

        # Slightly hacky...
        if prefix == "s_":

            if isObjectInLayer(source_object, SHAPE_LAYER):
                addObjectToLayer(obj, SHAPE_LAYER)
            else:
                removeObjectFromLayer(obj, SHAPE_LAYER)
                continue

        if prefix == "b_":

            if isObjectInLayer(source_object, BBM_LAYER):
                addObjectToLayer(obj, BBM_LAYER)
            else:
                removeObjectFromLayer(obj, BBM_LAYER)
                continue            
            
        # actually copy properties
        for prop in [p for p in source_object.getAllProperties() if p.getName().find(prefix) == 0]:
            setOrAddProperty(prop.getName(), prop.getData(), obj)





#------------------------------------------------------------------------------
def onMouseMove(val):
    global last_selected_object, last_edit_mode

    cur_selected_object = len(Object.GetSelected()) and Object.GetSelected()[0] or None

    if (cur_selected_object != last_selected_object or
        last_edit_mode):
        
        last_selected_object = cur_selected_object

        gui_handler.updateElements()

        # Update panel visibility
        if (cur_selected_object is None):
            obj_prop_panel.setVisible(False)
        else:
            obj_prop_panel.setVisible(True)
            bbm_button.onChanged()
            ode_button.onChanged()
        
        
        Draw.Redraw(1)
        Blender.Window.Redraw(-1)
        Blender.Draw.Draw()

    last_edit_mode = Blender.Window.EditMode()
   

#------------------------------------------------------------------------------
#  Export bbm and xml for the current object.
def onExportObject(b):

    try:
        export(export_all_scenes_button.getValue(),
               quick_n_dirty_button.getValue())
    except Exception, e:
        message(str(e), msg.V_HIGH)
    else:
        message("Export complete!", msg.V_HIGH)


#------------------------------------------------------------------------------
def onExportImages(b):
    from zero.export_images import exportImages
    exportImages()
    
    message("Image export complete!", msg.V_HIGH)
    
    


#------------------------------------------------------------------------------
def onQuickNDirtyChanged(b):
    config["quick_n_dirty"] = b.getValue()
    zero.registry.save()

#------------------------------------------------------------------------------
def onExportAllScenesChanged(b):
    config["export_all_scenes"] = b.getValue()
    zero.registry.save()

#------------------------------------------------------------------------------
def getCurSelectedObject():
    try: return Object.GetSelected()[0]
    except IndexError: return None    


#------------------------------------------------------------------------------
def onAddGroupPressed(b):

    # Bail if field was empty
    groups_to_add = add_group_text.getValue()
    add_group_text.setValue("")
    if not groups_to_add: return
    

    # traverse selection and add group string
    for sel_obj in Object.GetSelected():
        cur_groups = getOrAddProperty("b_groups", "", sel_obj)

        if cur_groups: cur_groups += ";" + groups_to_add
        else         : cur_groups = groups_to_add

        setOrAddProperty("b_groups", cur_groups, sel_obj)
    

#------------------------------------------------------------------------------
def updateObjectName(button):
    try:
        button.setValue(getCurSelectedObject().getName())
    except:
        button.setValue("None")
        

#------------------------------------------------------------------------------
#  Returns a function which sets the currently selected object's
#  property to the buttons value.
#  Returns a function which updates the button from a property of the
#  currently selected object.
def getPropButtonFuncs(prop_name, obj_getter):
    def onChanged(button):
        obj = obj_getter()
        if obj: obj.getProperty(prop_name).setData(button.getValue())
    def updater(button):
        obj = obj_getter()
        if obj: button.setValue(getOrAddProperty(prop_name, button.getValue(), obj))

    return (onChanged,updater)



#------------------------------------------------------------------------------
#  Returns functions to enable / disable membership of a specified
#  object in a specified layer
def getLayerButtonFuncs(layer_number, obj_getter):
    def onChanged(button):
        obj = obj_getter()
        if not obj: return
        if button.getValue():
            addObjectToLayer(obj, layer_number)
        else:
            removeObjectFromLayer(obj, layer_number)
    def updater(button):
        obj = obj_getter()
        if not obj: return
        button.setValue(isObjectInLayer(obj, layer_number))

    return (onChanged, updater)
        


if __name__ == "__main__":
        
    

    gui_handler = gui.GuiHandler()
    gui_handler.bg_color = (0.0, 0.0, 0.5, 1.0)



    # -------------------- Export Button --------------------
    cur_x = gui.X_SPACING
    cur_y = gui.Y_SPACING
    
    gui_handler.addElement(gui.PushButton("Export Object", cur_x, cur_y,
                                          onExportObject))
    cur_y += gui.Y_SPACING

    gui_handler.addElement(gui.PushButton("Export Images", cur_x, cur_y,
                                          onExportImages))
    cur_y += gui.Y_SPACING

    quick_n_dirty_button = gui_handler.addElement(gui.ToggleButton("Quick'n'dirty", cur_x, cur_y,
                                                  onQuickNDirtyChanged))
    quick_n_dirty_button.setValue(bool(config["quick_n_dirty"]))
    cur_y += gui.Y_SPACING

    export_all_scenes_button = gui_handler.addElement(gui.ToggleButton("Export all scenes", cur_x, cur_y,
                                                                       onExportAllScenesChanged))
    export_all_scenes_button.setValue(bool(config["export_all_scenes"]))
    cur_y += gui.Y_SPACING


    # -------------------- Utility Buttons --------------------
    cur_x = gui.DEFAULT_WIDTH + 2*gui.X_SPACING
    cur_y = gui.Y_SPACING

    gui_handler.addElement(gui.PushButton("Select TexSet 2", cur_x, cur_y,
                                          lambda b: selectTexSet(1)))
    cur_y += gui.Y_SPACING

    gui_handler.addElement(gui.PushButton("Select TexSet 1", cur_x, cur_y,
                                          lambda b: selectTexSet(0)))
    cur_y += gui.Y_SPACING
    cur_y += gui.Y_SPACING

    add_group_text = gui.StringButton("Groups", cur_x, cur_y)
    add_group_text.width = gui.DEFAULT_WIDTH*0.5 - gui.PANEL_MARGIN
    gui_handler.addElement(add_group_text)
    add_group_button = gui.PushButton("Add!", cur_x+gui.DEFAULT_WIDTH*0.5, cur_y,
                                      onAddGroupPressed)
    add_group_button.width = gui.DEFAULT_WIDTH*0.5
    gui_handler.addElement(add_group_button)

    # -------------------- Per-Object Properties --------------------

    cur_x = gui.X_SPACING
    cur_y = 6*gui.Y_SPACING

    glob_prop_panel = gui.Panel((0.0, 0.0, 0.6))
    gui_handler.addElement(glob_prop_panel)
    
    glob_prop_panel.addElement(gui.ToggleButton("Instanced", cur_x, cur_y,
                                                *getPropButtonFuncs("Instanced", getPropsObject)))
    cur_y += gui.Y_SPACING

    glob_prop_panel.addElement(gui.ToggleButton("Static", cur_x, cur_y,
                                                *getPropButtonFuncs("Static", getPropsObject))).setValue(True)
    cur_y += gui.Y_SPACING

    glob_prop_panel.addElement(gui.ToggleButton("Shadow Caster", cur_x, cur_y,
                                                *getPropButtonFuncs("Blocker", getPropsObject)))
    cur_y += gui.Y_SPACING


    glob_prop_panel.addElement(gui.ToggleButton("Shadow Receiver", cur_x, cur_y,
                                                *getPropButtonFuncs("Receiver", getPropsObject))).setValue(True)
    cur_y += gui.Y_SPACING

    combo = glob_prop_panel.addElement(gui.ComboBox("LOD class", cur_x, cur_y,
                                                    *getPropButtonFuncs("LodClass", getPropsObject)))
    combo.setItems("building", "tree", "gameobject", "nolod")
    cur_y += gui.Y_SPACING




    obj_prop_panel = gui.Panel((0.0, 0.0, 0.7))
    gui_handler.addElement(obj_prop_panel)


    # -------------------- BBM Buttons --------------------
    
    cur_x = gui.X_SPACING
    cur_y = 16*gui.Y_SPACING

    bbm_prop_panel = gui.Panel((0.0, 0.0, 0.6))
    obj_prop_panel.addElement(bbm_prop_panel)


    bbm_prop_panel.addElement(gui.PushButton("Copy to selection", cur_x, cur_y,
                                          lambda b: copyProps("b_")))
    cur_y += gui.Y_SPACING
    

    bbm_prop_panel.addElement(gui.StringButton("Groups", cur_x, cur_y,
                                            *getPropButtonFuncs("b_groups", getCurSelectedObject)))
    cur_y += gui.Y_SPACING
    
    cur_y += gui.Y_SPACING
    bbm_button = obj_prop_panel.addElement(
        gui.ToggleButton("Export as BBM (Layer %d)" % BBM_LAYER, cur_x, cur_y,
                         *getLayerButtonFuncs(BBM_LAYER, getCurSelectedObject)))
    bbm_button.addChangedCallback(lambda b: bbm_prop_panel.setVisible(b.getValue()))
    cur_y += gui.Y_SPACING

    obj_prop_panel.addElement(gui.Label("Object: ", cur_x, cur_y,
                                     updateObjectName))
    cur_y += gui.Y_SPACING

    
    
    # -------------------- Ode Buttons --------------------
    cur_x = 2*gui.X_SPACING + gui.DEFAULT_WIDTH
    cur_y = 12*gui.Y_SPACING

    ode_prop_panel = gui.Panel((0.0, 0.0, 0.6))
    obj_prop_panel.addElement(ode_prop_panel)
    
    
    ode_prop_panel.addElement(gui.PushButton("Copy to selection", cur_x, cur_y,
                                          lambda b: copyProps("s_")))
    cur_y += gui.Y_SPACING

    ode_prop_panel.addElement(gui.ToggleButton("Sensor", cur_x, cur_y,
                                            *getPropButtonFuncs("s_sensor", getCurSelectedObject)))
    cur_y += gui.Y_SPACING
    
    ode_prop_panel.addElement(gui.Slider(0.0, 1.0, "Friction", cur_x, cur_y,
                                      *getPropButtonFuncs("s_friction", getCurSelectedObject))).setValue(1.0)
    cur_y += gui.Y_SPACING

    ode_prop_panel.addElement(gui.Slider(0.0, 1.0, "Bounciness", cur_x, cur_y,
                                      *getPropButtonFuncs("s_bounciness", getCurSelectedObject))).setValue(0.0)
    cur_y += gui.Y_SPACING
    
    ode_prop_panel.addElement(gui.Slider(0.0, 1.0, "Mass", cur_x, cur_y,
                                      *getPropButtonFuncs("s_mass", getCurSelectedObject))).setValue(1.0)
    cur_y += gui.Y_SPACING

    combo = ode_prop_panel.addElement(gui.ComboBox("Shape Type", cur_x, cur_y,
                                                *getPropButtonFuncs("s_type", getCurSelectedObject)))
    combo.setItems("sphere", "ccylinder", "box", "ray", "plane", "trimesh", "continuous")
    
    cur_y += gui.Y_SPACING
    cur_y += gui.Y_SPACING
    ode_button = obj_prop_panel.addElement(
        gui.ToggleButton("Export as Shape (Layer %d)" % SHAPE_LAYER, cur_x, cur_y,
                         *getLayerButtonFuncs(SHAPE_LAYER, getCurSelectedObject)))
    ode_button.addChangedCallback(lambda b: ode_prop_panel.setVisible(b.getValue()))
    cur_y += gui.Y_SPACING
    





    gui_handler.setEventHandler(Draw.MOUSEX, onMouseMove)
    gui_handler.setEventHandler(Draw.MOUSEY, onMouseMove)
    gui_handler.start()

