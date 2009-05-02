


from Blender import Draw,BGL


X_SPACING = 20
Y_SPACING = 30

PANEL_MARGIN = 5


DEFAULT_HEIGHT = 19
DEFAULT_WIDTH = 170



#------------------------------------------------------------------------------
class GuiHandler:
    #--------------------------------------------------------
    def __init__(self):
        self.element = []
        self.bg_color = (1.0, 0.0, 1.0, 1.0)
        self.event_handler = {}

    #--------------------------------------------------------
    def setEventHandler(self, evt, handler):
        self.event_handler[evt] = handler

    #--------------------------------------------------------
    def addElement(self, el):
        el.setEvent(len(self.element))
        el.setGuiHandler(self)
        self.element.append(el)
        return el

    #--------------------------------------------------------
    def updateElements(self):
        for el in self.element:
            el.update()            
        Draw.Redraw()

    #--------------------------------------------------------
    def start(self):
        
        def d()        : self._draw()
        def ec(evt,val): self._eventCallback(evt, val)
        def bc(evt)    : self._buttonCallback(evt)

        self.updateElements()
            
        Draw.Register(d, ec, bc)

    #--------------------------------------------------------
    def _eventCallback(self, evt, val):
        try: self.event_handler[evt](val)
        except KeyError: pass
    
    #--------------------------------------------------------
    def _buttonCallback(self, evt):
        self.element[evt].onChanged()
        Draw.Redraw()
        
    #--------------------------------------------------------
    def _draw(self):
        BGL.glClearColor(*self.bg_color)
        BGL.glClear(BGL.GL_COLOR_BUFFER_BIT)

        for el in self.element:
            el.draw()



#------------------------------------------------------------------------------
class Element:

    #--------------------------------------------------------
    def __init__(self, title, pos_x, pos_y,
                 changed_callback = None,
                 update_callback = None,
                 tooltip=""):
        self.visible = True
        self.title   = title
        self.tooltip = tooltip
        self.setDimensions(pos_x, pos_y)

        self.changed_callback = []
        
        self.addChangedCallback(changed_callback)
        self.setUpdateCallback (update_callback)

        # so we don't have to create a real button here...
        class dummy: pass
        self.button = dummy()
        self.button.val = 0

        self.gui_handler = None
        
        

    #--------------------------------------------------------
    def setTitle(self, title):
        self.title = title


    #--------------------------------------------------------
    def setEvent(self, e):
        self.event = e

    #--------------------------------------------------------
    def setValue(self, v):
        self.button.val = v

    #--------------------------------------------------------
    def getValue(self):
        return self.button.val

    #--------------------------------------------------------
    def update(self):
        if self.update_callback:
            self.update_callback(self)

    #--------------------------------------------------------
    def onChanged(self):
        for cb in self.changed_callback:
            cb(self)

    #--------------------------------------------------------
    #  The buttons value will be set to the return value of the
    #  callback every time the button values are updated.
    def setUpdateCallback(self, callback):
        self.update_callback = callback


    #--------------------------------------------------------
    #  callback will be called with the new button value every time
    #  the button changes.
    def addChangedCallback(self, callback):
        if callback: self.changed_callback.append(callback)


    #--------------------------------------------------------
    def setDimensions(self, x, y, w=DEFAULT_WIDTH, h=DEFAULT_HEIGHT):
        self.pos_x  = x
        self.pos_y  = y
        self.width  = w
        self.height = h    

    #--------------------------------------------------------
    def setTooltip(self,tt):
        self.tooltip = tt


    #--------------------------------------------------------
    def setVisible(self, v):
        self.visible = v

    #--------------------------------------------------------
    def draw(self):
        if self.visible: self.doDraw()


    #--------------------------------------------------------
    def setGuiHandler(self, handler):
        self.gui_handler = handler
        

#------------------------------------------------------------------------------
class Panel(Element):

    #--------------------------------------------------------
    def __init__(self, color):
        Element.__init__(self, "", -1, -1)
        self.color = color
        self.element = []

    #--------------------------------------------------------
    def addElement(self, el):
        self.gui_handler.addElement(el)
        self.element.append(el)

        def maxDims(dim1, dim2):            
            return (min(dim1[0], dim2[0]),
                    max(dim1[1], dim2[1]),
                    min(dim1[2], dim2[2]),
                    max(dim1[3], dim2[3]))
            
        (self.pos_x, max_x,
         self.pos_y, max_y) = reduce(maxDims,
                                     [(e.pos_x, e.pos_x+e.width,
                                       e.pos_y, e.pos_y+e.height) for e in self.element])

        self.pos_x  -= PANEL_MARGIN
        self.pos_y  -= PANEL_MARGIN
        self.width  = max_x - self.pos_x + PANEL_MARGIN
        self.height = max_y - self.pos_y + PANEL_MARGIN

        return el

    #--------------------------------------------------------
    def setVisible(self, v):
        for e in self.element:
            e.setVisible(v)


    #--------------------------------------------------------
    def doDraw(self):
        BGL.glColor3f(*self.color)
        BGL.glRecti(self.pos_x, self.pos_y,
                    self.pos_x + self.width,
                    self.pos_y + self.height)
        
    

#------------------------------------------------------------------------------
class Label(Element):

    #--------------------------------------------------------
    def __init__(self, title, pos_x, pos_y,
                 update_callback = None):
        Element.__init__(self, title, pos_x, pos_y,
                         None,
                         update_callback,
                         "")
        self.color = (1.0, 1.0, 1.0)

    #--------------------------------------------------------
    def doDraw(self):
        # this has black color...
 #       Draw.Label(self.title,
#                   self.pos_x, self.pos_y, self.width, self.height)
        BGL.glColor3f(*self.color)        
        BGL.glRasterPos2i(self.pos_x, self.pos_y)
        Draw.Text(self.title + self.button.val)

#------------------------------------------------------------------------------
class PushButton(Element):

    #--------------------------------------------------------
    def __init__(self, title, pos_x, pos_y,
                 changed_callback = None,
                 update_callback = None,
                 tooltip=""):
        Element.__init__(self, title, pos_x, pos_y,
                         changed_callback,
                         update_callback,
                         tooltip)

    #--------------------------------------------------------
    def doDraw(self):
        Draw.PushButton(self.title, self.event,
                        self.pos_x, self.pos_y, self.width, self.height,
                        self.tooltip)


#------------------------------------------------------------------------------
class ToggleButton(Element):

    #--------------------------------------------------------
    def __init__(self, title, pos_x, pos_y,
                 changed_callback = None,
                 update_callback = None,
                 tooltip=""):
        Element.__init__(self, title, pos_x, pos_y,
                         changed_callback,
                         update_callback,
                         tooltip)


    #--------------------------------------------------------
    def doDraw(self):
        self.button = Draw.Toggle(self.title, self.event,
                                  self.pos_x, self.pos_y, self.width, self.height,
                                  self.button.val,
                                  self.tooltip)

#------------------------------------------------------------------------------
class StringButton(Element):


    #--------------------------------------------------------
    def __init__(self, title, pos_x, pos_y,
                 changed_callback = None,
                 update_callback = None,
                 tooltip=""):
        Element.__init__(self, title, pos_x, pos_y,
                         changed_callback,
                         update_callback,
                         tooltip)
        self.max_length = 120
        self.button.val = ""


    #--------------------------------------------------------
    def doDraw(self):
        self.button = Draw.String(self.title + ": ", self.event,
                                  self.pos_x, self.pos_y, self.width, self.height,
                                  self.button.val,
                                  self.max_length,
                                  self.tooltip)

#------------------------------------------------------------------------------
class Slider(Element):

    #--------------------------------------------------------
    def __init__(self, min_val, max_val, title, pos_x, pos_y,
                 changed_callback = None,
                 update_callback = None,
                 tooltip=""):
        Element.__init__(self, title, pos_x, pos_y,
                         changed_callback,
                         update_callback,
                         tooltip)
        self.min = min_val
        self.max = max_val

    #--------------------------------------------------------
    def doDraw(self):
        self.button = Draw.Slider(self.title, self.event,
                                  self.pos_x, self.pos_y, self.width, self.height,
                                  self.button.val,
                                  self.min, self.max,
                                  0, self.tooltip)

#------------------------------------------------------------------------------
class ColorPicker(Element):

    #--------------------------------------------------------
    def __init__(self, title, pos_x, pos_y,
                 changed_callback = None,
                 update_callback = None,
                 tooltip=""):
        Element.__init__(self, title, pos_x, pos_y,
                         changed_callback,
                         update_callback,
                         tooltip)
        self.button.val = (1.0,1.0,1.0)


    #--------------------------------------------------------
    def doDraw(self):
        self.button = Draw.ColorPicker(self.event,
                                       self.pos_x, self.pos_y, self.width, self.height,
                                       self.button.val, self.tooltip)


#------------------------------------------------------------------------------
class ComboBox(Element):

    #--------------------------------------------------------
    def __init__(self, title, pos_x, pos_y,
                 changed_callback = None,
                 update_callback = None,
                 tooltip=""):
        Element.__init__(self, title, pos_x, pos_y,
                         changed_callback,
                         update_callback,
                         tooltip)
        self.items = []

    #--------------------------------------------------------
    def setItems(self, *items):
        self.items = list(items)

    #--------------------------------------------------------
    def setValue(self, v):
        self.button.val = self.items.index(v)

    #--------------------------------------------------------
    def getValue(self):
        return self.items[self.button.val]

    #--------------------------------------------------------
    def setCurIndex(self, i):
        self.button.val = i

    #--------------------------------------------------------
    def getCurIndex(self):
        return self.button.val


    #--------------------------------------------------------
    def getItem(self, num):
        return self.items[num]

    #--------------------------------------------------------
    def doDraw(self):
        self.button = Draw.Menu(self.title + "%t|" + "|".join([ s+"%%x%d"%k for k,s in enumerate(self.items)]),
                                self.event,
                                self.pos_x, self.pos_y, self.width, self.height,
                                self.button.val, self.tooltip)



