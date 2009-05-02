
#include "Gui.h"

#ifdef _WIN32
#include <CEGUI/openglrenderer.h>
#else
#include <CEGUI/RendererModules/OpenGLGUIRenderer/openglrenderer.h>
#endif

#include <CEGUI/CEGUIDefaultResourceProvider.h>

#include "SdlApp.h"
#include "ParameterManager.h"
#include "Utils.h"
#include "Paths.h"
#include "GuiLogger.h"

//------------------------------------------------------------------------------
Gui::Gui() :
    renderer_(NULL),
    cegui_system_(NULL),
    font_manager_(NULL),
    width_(0.0),
    height_(0.0),
    aspect_width_(0.0)
{
}

//------------------------------------------------------------------------------
Gui::~Gui()
{
    s_log << Log::debug('d')
          << "Gui Manager destructor\n";
}

//------------------------------------------------------------------------------
void Gui::frameMove(float dt)
{
	// get current "run-time" in seconds and inject the time that passed since the last call 
	CEGUI::System::getSingleton().injectTimePulse(dt);
}

//------------------------------------------------------------------------------
void Gui::init(int width, int height)
{
    // do basic init of CEGui 
    renderer_.reset(new CEGUI::OpenGLRenderer(0, width, height));

    // will be cleaned up by cegui singleton destruction
    new GuiLogger();
    cegui_system_.reset(new CEGUI::System(renderer_.get(), 0, 0, 0, "", getOrCreateUserDataDir() + "log_cegui.txt"));

    // call this if the gui sheet is changed to resize it correctly
    cegui_system_->subscribeEvent(CEGUI::System::EventGUISheetChanged,
                                  CEGUI::SubscriberSlot(&Gui::onGuiSheetChanged, this));

    // set the default resource groups to be used
    CEGUI::Imageset::setDefaultResourceGroup("imagesets");
    CEGUI::Font::setDefaultResourceGroup("fonts");
    CEGUI::Scheme::setDefaultResourceGroup("schemes");
    CEGUI::WidgetLookManager::setDefaultResourceGroup("looknfeels");
    CEGUI::WindowManager::setDefaultResourceGroup("layouts");
    CEGUI::ScriptModule::setDefaultResourceGroup("lua_scripts");

    // initialise the required dirs for the DefaultResourceProvider
    CEGUI::DefaultResourceProvider* rp = static_cast<CEGUI::DefaultResourceProvider*>
        (CEGUI::System::getSingleton().getResourceProvider());

    rp->setResourceGroupDirectory("schemes",    GUI_SCHEMES_PATH.c_str());
    rp->setResourceGroupDirectory("imagesets",  GUI_IMAGESETS_PATH.c_str());
    rp->setResourceGroupDirectory("fonts",      GUI_FONTS_PATH.c_str());
    rp->setResourceGroupDirectory("layouts",    GUI_LAYOUTS_PATH.c_str());
    rp->setResourceGroupDirectory("looknfeels", GUI_LOOKNFEEL_PATH.c_str());

    CEGUI::ImagesetManager::getSingleton().createImageset(s_params.get<std::string>("client.gui.imageset"));

    // load in the scheme file, which auto-loads the imageset
    CEGUI::SchemeManager::getSingleton().loadScheme(s_params.get<std::string>("client.gui.schema"));

    // load in a font.  The first font loaded automatically becomes the default font.
    font_manager_ = CEGUI::FontManager::getSingletonPtr();
    font_manager_->createFont(s_params.get<std::string>("client.gui.font"));

    // Set the default mouse cursor
    CEGUI::System::getSingleton().setDefaultMouseCursor(
        &(CEGUI::ImagesetManager::getSingleton().getImageset("WindowsLook")->getImage("MouseArrow")));   

    CEGUI::MouseCursor::getSingleton().setConstraintArea(NULL);

}

//------------------------------------------------------------------------------
/***
 * Handles widescreen resolutions differently. On widescreen the viewport for
 * CEGUI rendering is set to a 4:3 aspect ratio window, according to the current 
 * height of the window.
 ***/
void Gui::render()
{
    enableFloatingPointExceptions(false);
        
    cegui_system_->renderGUI(); 
        
    enableFloatingPointExceptions();
}  

//------------------------------------------------------------------------------
void Gui::setMouseCursorVisible(bool v)
{
    CEGUI::MouseCursor::getSingleton().setVisible(v);
}

//------------------------------------------------------------------------------
Vector2d Gui::getMouseCursorPostion() const
{
    return Vector2d(CEGUI::MouseCursor::getSingleton().getPosition().d_x,
                    CEGUI::MouseCursor::getSingleton().getPosition().d_y);
}

//------------------------------------------------------------------------------
/**
 *  only inject ASCII characters into Gui to avoid any unknown symbols
 *  except chat, which is handled inside GameHUD and supports unicode
 *
 */
bool Gui::onKeyDown(SDL_keysym sym)
{
    CEGUI::uint kc = SDLKeyToCEGUIKey(sym.sym);
    bool key_consumed = CEGUI::System::getSingleton().injectKeyDown(kc);

	if ( (sym.unicode & 0xFF80) == 0 && sym.sym != SDLK_RETURN && sym.sym != SDLK_KP_ENTER && sym.sym != SDLK_TAB) {
		key_consumed = key_consumed | CEGUI::System::getSingleton().injectChar(sym.unicode);
	} 
    // else { An international character.. }

    return key_consumed;
}

//------------------------------------------------------------------------------
bool Gui::onKeyUp(SDL_keysym sym)
{
    return CEGUI::System::getSingleton().injectKeyUp(sym.scancode);
}

//------------------------------------------------------------------------------
bool Gui::onMouseButtonDown(const SDL_MouseButtonEvent & event)
{
    switch ( event.button )
	    {
	    // handle real mouse buttons
	    case SDL_BUTTON_LEFT:
		    return CEGUI::System::getSingleton().injectMouseButtonDown(CEGUI::LeftButton);
		    break;
	    case SDL_BUTTON_MIDDLE:
		    return CEGUI::System::getSingleton().injectMouseButtonDown(CEGUI::MiddleButton);
		    break;
	    case SDL_BUTTON_RIGHT:
		    return CEGUI::System::getSingleton().injectMouseButtonDown(CEGUI::RightButton);
		    break;
    	
	    // handle the mouse wheel
	    case SDL_BUTTON_WHEELDOWN:
		    return CEGUI::System::getSingleton().injectMouseWheelChange( -1 );
		    break;
	    case SDL_BUTTON_WHEELUP:
		    return CEGUI::System::getSingleton().injectMouseWheelChange( +1 );
		    break;
        default:
            return false;
	    }
}

//------------------------------------------------------------------------------
bool Gui::onMouseButtonUp(const SDL_MouseButtonEvent & event)
{
	switch ( event.button )
		{
		case SDL_BUTTON_LEFT:
			return CEGUI::System::getSingleton().injectMouseButtonUp(CEGUI::LeftButton);
			break;
		case SDL_BUTTON_MIDDLE:
			return CEGUI::System::getSingleton().injectMouseButtonUp(CEGUI::MiddleButton);
			break;
		case SDL_BUTTON_RIGHT:
			return CEGUI::System::getSingleton().injectMouseButtonUp(CEGUI::RightButton);
			break;
        default:
            return false;
		}
}

//------------------------------------------------------------------------------
bool Gui::onMouseMotion(const SDL_MouseMotionEvent & event)
{
    // inject the mouse position directly.
    return CEGUI::System::getSingleton().injectMousePosition(
      static_cast<float>(event.x),
      static_cast<float>(event.y));
}

//------------------------------------------------------------------------------
void Gui::onResizeEvent(int w, int h)
{
    enableFloatingPointExceptions(false);

    if(renderer_.get())
    {
        renderer_->setDisplaySize(CEGUI::Size((float)w,(float)h));
    }


    // we want the gui to look good on widescreen resolutions, therefore
    // an aspect ratio of 4/3 must be preserved for gui windows. to achieve this
    // the gui root sheets are sized and positioned according to the resolution
    // present.

    width_ = (float)w;
    height_ = (float)h;
    aspect_width_ = height_ * (4.0/3.0);

    if (width_ > aspect_width_)
    {
        float unified_aspect_width = aspect_width_ / width_;
        cegui_system_->getGUISheet()->setPosition(
            CEGUI::UVector2(CEGUI::UDim((1.0-unified_aspect_width)/2.0,0), CEGUI::UDim(0.0,0)));

        cegui_system_->getGUISheet()->setSize(
            CEGUI::UVector2(CEGUI::UDim(unified_aspect_width,0), CEGUI::UDim(1.0,0)));       

        // additionally notify all fonts of the new aspect resolution they should use
        CEGUI::FontManager::FontIterator fi = font_manager_->getIterator();
        while(!fi.isAtEnd())
        {
            (*fi)->notifyScreenResolution(CEGUI::Size(aspect_width_,height_));
            ++fi;
        }
    } 
    else // on non-widescreen resolutions do normal rendering
    {
        cegui_system_->getGUISheet()->setPosition(CEGUI::UVector2(CEGUI::UDim(0.0,0), CEGUI::UDim(0.0,0)));
        cegui_system_->getGUISheet()->setSize(CEGUI::UVector2(CEGUI::UDim(1.0,0), CEGUI::UDim(1.0,0))); 
    }

    enableFloatingPointExceptions();
}

//------------------------------------------------------------------------------
/***
 *  If a resize event happend to another gui sheet before, the new gui sheet
 *  is not informed, so set it to the new dimensions here at the change callback.
 ***/
bool Gui::onGuiSheetChanged(const CEGUI::EventArgs &ea)
{
    if (width_ > aspect_width_)
    {
        float unified_aspect_width = aspect_width_ / width_;
        cegui_system_->getGUISheet()->setPosition(
            CEGUI::UVector2(CEGUI::UDim((1.0-unified_aspect_width)/2.0,0), CEGUI::UDim(0.0,0)));

        cegui_system_->getGUISheet()->setSize(
            CEGUI::UVector2(CEGUI::UDim(unified_aspect_width,0), CEGUI::UDim(1.0,0)));       
    } 
    else // on non-widescreen resolutions do normal rendering
    {
        cegui_system_->getGUISheet()->setPosition(CEGUI::UVector2(CEGUI::UDim(0.0,0), CEGUI::UDim(0.0,0)));
        cegui_system_->getGUISheet()->setSize(CEGUI::UVector2(CEGUI::UDim(1.0,0), CEGUI::UDim(1.0,0))); 
    }
    return true;
}


//------------------------------------------------------------------------------
bool Gui::onWindowActivated(const CEGUI::EventArgs &ea)
{
    try
    {
        const CEGUI::WindowEventArgs& we = static_cast<const CEGUI::WindowEventArgs&>(ea);

        // If it is the same window as before, ignore it.
        if(!activated_windows_.empty())
        {
            if(activated_windows_.back() == we.window) return true;
        }


        if(!activated_windows_.empty())
        {
            deactivated_window_ = activated_windows_.back();
            activated_windows_.back()->setVisible(false); ///< triggers onWindowDeactivated
            deactivated_window_ = NULL;
        }
        
        // add window to activation stack
        activated_windows_.push_back(we.window);

        // only keep ACTIVE_WINDOW_STACK_SIZE on the stack
        if(activated_windows_.size() > ACTIVE_WINDOW_STACK_SIZE)
        {
            activated_windows_.erase(activated_windows_.begin());
        }

        // special case handling for console window
        if(we.window->getName() == "TankApp_root/console/window")
        {
            s_app.setAllowMouseCapturing(true);
        }
        else
        {
            s_app.setAllowMouseCapturing(false);           
        }

        // free mouse as gui window opens
        s_app.captureMouse(false);


    } catch( CEGUI::Exception& e )
    {
        s_log << Log::error <<  "Gui::onWindowActivated: " + e.getMessage( );
        return true;
    }

    return true;
}

//------------------------------------------------------------------------------
bool Gui::onWindowDeactivated(const CEGUI::EventArgs &ea)
{
    try
    {
        const CEGUI::WindowEventArgs& we = static_cast<const CEGUI::WindowEventArgs&>(ea);

        // if this is the window that has been made invisible previously at
        // Gui::onWindowActivated -> bail out
        if(we.window == deactivated_window_)
        {            
            return true;
        }

        // take the last window from activation stack and make the
        // window below visible and focused
        if(!activated_windows_.empty()) activated_windows_.pop_back();
        

        // Now we get the window that was active before the current one:
        // re-activate it
        if(!activated_windows_.empty())
        {
            activated_windows_.back()->setVisible(true);
            activated_windows_.back()->activate();
        }

        // if the last window has been closed, mouse capturing is allowed again
        // and mouse is automatically captured
        if(activated_windows_.empty()) 
        {
            s_app.setAllowMouseCapturing(true);

            // if window gets deactivated and we are already on another GUISheet
            // do not capture mouse
            Tokenizer t(we.window->getName().c_str(), '/');
            CEGUI::String gui_sheet(t.getNextWord());
            gui_sheet += "/";

            if(cegui_system_.get() && 
               cegui_system_->getGUISheet() &&
               cegui_system_->getGUISheet()->getName() == gui_sheet)
            {
                // also do a special case handling for console window to keep cursor active
                if(we.window->getName() == "TankApp_root/console/window") return true;
                
                s_app.captureMouse(true);
            }
        }
        else // if there is still a window present in the stack, mouse capturing
             // is disabled
        {
            s_app.setAllowMouseCapturing(false);
        }


    } catch( CEGUI::Exception& e )
    {
        s_log << Log::error <<  "Gui::onWindowDeactivated error: " + e.getMessage( );
        return true;
    }

    return true;
}

//------------------------------------------------------------------------------
bool Gui::onWindowDestructed(const CEGUI::EventArgs &ea)
{
    try
    {
        const CEGUI::WindowEventArgs& we = static_cast<const CEGUI::WindowEventArgs&>(ea);

        // remove destructed window from activated window stack
        std::vector<CEGUI::Window*>::iterator it = activated_windows_.begin();
        while(it != activated_windows_.end())
        {
            if(we.window == (*it))
            {
                it = activated_windows_.erase(it);                
            }
            else
            {
                ++it;
            }
        }


    } catch( CEGUI::Exception& e )
    {
        s_log << Log::error <<  "Gui::onWindowDestructed error: " + e.getMessage( );
        return true;
    }

    return true;
}

/************************************************************************
     Translate a CEGUI::Key to the proper SDLKey 
 *************************************************************************/
SDLKey Gui::CEGUIKeyToSDLKey(CEGUI::uint key)
{
     using namespace CEGUI;
     switch (key)
     {
     case  Key::Backspace:            return SDLK_BACKSPACE;   
     case  Key::Tab:                  return SDLK_TAB;        
     case  Key::Return:               return SDLK_RETURN;      
     case  Key::Pause:                return SDLK_PAUSE;       
     case  Key::Escape:               return SDLK_ESCAPE;      
     case  Key::Space:                return SDLK_SPACE;       
     case  Key::Comma:                return SDLK_COMMA;       
     case  Key::Minus:                return SDLK_MINUS;       
     case  Key::Period:               return SDLK_PERIOD;      
     case  Key::Slash:                return SDLK_SLASH;       
     case  Key::Zero:                 return SDLK_0;         
     case  Key::One:                  return SDLK_1;        
     case  Key::Two:                  return SDLK_2;        
     case  Key::Three:                return SDLK_3;          
     case  Key::Four:                 return SDLK_4;         
     case  Key::Five:                 return SDLK_5;         
     case  Key::Six:                  return SDLK_6;        
     case  Key::Seven:                return SDLK_7;          
     case  Key::Eight:                return SDLK_8;          
     case  Key::Nine:                 return SDLK_9;           
     case  Key::Colon:                return SDLK_COLON;       
     case  Key::Semicolon:            return SDLK_SEMICOLON;   
     case  Key::Equals:               return SDLK_EQUALS;      
     case  Key::LeftBracket:          return SDLK_LEFTBRACKET; 
     case  Key::Backslash:            return SDLK_BACKSLASH;   
     case  Key::RightBracket:         return SDLK_RIGHTBRACKET;
     case  Key::A:                    return SDLK_a;           
     case  Key::B:                    return SDLK_b;           
     case  Key::C:                    return SDLK_c;           
     case  Key::D:                    return SDLK_d;           
     case  Key::E:                    return SDLK_e;           
     case  Key::F:                    return SDLK_f;           
     case  Key::G:                    return SDLK_g;           
     case  Key::H:                    return SDLK_h;           
     case  Key::I:                    return SDLK_i;           
     case  Key::J:                    return SDLK_j;           
     case  Key::K:                    return SDLK_k;           
     case  Key::L:                    return SDLK_l;           
     case  Key::M:                    return SDLK_m;           
     case  Key::N:                    return SDLK_n;           
     case  Key::O:                    return SDLK_o;           
     case  Key::P:                    return SDLK_p;           
     case  Key::Q:                    return SDLK_q;           
     case  Key::R:                    return SDLK_r;           
     case  Key::S:                    return SDLK_s;           
     case  Key::T:                    return SDLK_t;           
     case  Key::U:                    return SDLK_u;           
     case  Key::V:                    return SDLK_v;           
     case  Key::W:                    return SDLK_w;           
     case  Key::X:                    return SDLK_x;           
     case  Key::Y:                    return SDLK_y;           
     case  Key::Z:                    return SDLK_z;           
     case  Key::Delete:                return SDLK_DELETE;    
     case  Key::Numpad0:               return SDLK_KP0;     
     case  Key::Numpad1:               return SDLK_KP1;     
     case  Key::Numpad2:               return SDLK_KP2;     
     case  Key::Numpad3:               return SDLK_KP3;     
     case  Key::Numpad4:               return SDLK_KP4;     
     case  Key::Numpad5:               return SDLK_KP5;     
     case  Key::Numpad6:               return SDLK_KP6;     
     case  Key::Numpad7:               return SDLK_KP7;     
     case  Key::Numpad8:               return SDLK_KP8;     
     case  Key::Numpad9:               return SDLK_KP9;     
     case  Key::Decimal:               return SDLK_KP_PERIOD;   
     case  Key::Divide:                return SDLK_KP_DIVIDE;   
     case  Key::Multiply:              return SDLK_KP_MULTIPLY; 
     case  Key::Subtract:              return SDLK_KP_MINUS;    
     case  Key::Add:                   return SDLK_KP_PLUS; 
     case  Key::NumpadEnter:           return SDLK_KP_ENTER;    
     case  Key::NumpadEquals:          return SDLK_KP_EQUALS;   
     case  Key::ArrowUp:               return SDLK_UP;     
     case  Key::ArrowDown:             return SDLK_DOWN;       
     case  Key::ArrowRight:            return SDLK_RIGHT;       
     case  Key::ArrowLeft:             return SDLK_LEFT;       
     case  Key::Insert:                return SDLK_INSERT;    
     case  Key::Home:                  return SDLK_HOME;  
     case  Key::End:                   return SDLK_END; 
     case  Key::PageUp:                return SDLK_PAGEUP;    
     case  Key::PageDown:              return SDLK_PAGEDOWN;    
     case  Key::F1:                    return SDLK_F1;          
     case  Key::F2:                    return SDLK_F2;          
     case  Key::F3:                    return SDLK_F3;          
     case  Key::F4:                    return SDLK_F4;          
     case  Key::F5:                    return SDLK_F5;          
     case  Key::F6:                    return SDLK_F6;          
     case  Key::F7:                    return SDLK_F7;          
     case  Key::F8:                    return SDLK_F8;          
     case  Key::F9:                    return SDLK_F9;          
     case  Key::F10:                    return SDLK_F10;         
     case  Key::F11:                    return SDLK_F11;         
     case  Key::F12:                    return SDLK_F12;         
     case  Key::F13:                    return SDLK_F13;         
     case  Key::F14:                    return SDLK_F14;         
     case  Key::F15:                    return SDLK_F15;         
     case  Key::NumLock:                return SDLK_NUMLOCK;     
     case  Key::ScrollLock:             return SDLK_SCROLLOCK;   
     case  Key::RightShift:             return SDLK_RSHIFT;      
     case  Key::LeftShift:              return SDLK_LSHIFT;      
     case  Key::RightControl:           return SDLK_RCTRL;       
     case  Key::LeftControl:            return SDLK_LCTRL;       
     case  Key::RightAlt:               return SDLK_RALT;        
     case  Key::LeftAlt:                return SDLK_LALT;        
     case  Key::LeftWindows:            return SDLK_LSUPER;      
     case  Key::RightWindows:           return SDLK_RSUPER;      
     case  Key::SysRq:                  return SDLK_SYSREQ;      
     case  Key::AppMenu:                return SDLK_MENU;        
     case  Key::Power:                  return SDLK_POWER;       
     case  Key::Grave:                  return SDLK_CARET;       
     default:                           return SDLK_UNKNOWN;
     }
     return SDLK_UNKNOWN;
}


//------------------------------------------------------------------------------
void Gui::subscribeActivationEvents(CEGUI::Window * win)
{
    win->subscribeEvent(CEGUI::Window::EventShown,              CEGUI::SubscriberSlot(&Gui::onWindowActivated,   this));
    win->subscribeEvent(CEGUI::Window::EventHidden,             CEGUI::SubscriberSlot(&Gui::onWindowDeactivated, this));
    win->subscribeEvent(CEGUI::Window::EventDestructionStarted, CEGUI::SubscriberSlot(&Gui::onWindowDestructed, this));
}



/************************************************************************
     Translate a SDLKey to the proper CEGUI::Key
 *************************************************************************/
CEGUI::uint Gui::SDLKeyToCEGUIKey(SDLKey key)
{
     using namespace CEGUI;
     switch (key)
     {
     case SDLK_BACKSPACE:    return Key::Backspace;
     case SDLK_TAB:          return Key::Tab;
     case SDLK_RETURN:       return Key::Return;
     case SDLK_PAUSE:        return Key::Pause;
     case SDLK_ESCAPE:       return Key::Escape;
     case SDLK_SPACE:        return Key::Space;
     case SDLK_COMMA:        return Key::Comma;
     case SDLK_MINUS:        return Key::Minus;
     case SDLK_PERIOD:       return Key::Period;
     case SDLK_SLASH:        return Key::Slash;
     case SDLK_0:            return Key::Zero;
     case SDLK_1:            return Key::One;
     case SDLK_2:            return Key::Two;
     case SDLK_3:            return Key::Three;
     case SDLK_4:            return Key::Four;
     case SDLK_5:            return Key::Five;
     case SDLK_6:            return Key::Six;
     case SDLK_7:            return Key::Seven;
     case SDLK_8:            return Key::Eight;
     case SDLK_9:            return Key::Nine;
     case SDLK_COLON:        return Key::Colon;
     case SDLK_SEMICOLON:    return Key::Semicolon;
     case SDLK_EQUALS:       return Key::Equals;
     case SDLK_LEFTBRACKET:  return Key::LeftBracket;
     case SDLK_BACKSLASH:    return Key::Backslash;
     case SDLK_RIGHTBRACKET: return Key::RightBracket;
     case SDLK_a:            return Key::A;
     case SDLK_b:            return Key::B;
     case SDLK_c:            return Key::C;
     case SDLK_d:            return Key::D;
     case SDLK_e:            return Key::E;
     case SDLK_f:            return Key::F;
     case SDLK_g:            return Key::G;
     case SDLK_h:            return Key::H;
     case SDLK_i:            return Key::I;
     case SDLK_j:            return Key::J;
     case SDLK_k:            return Key::K;
     case SDLK_l:            return Key::L;
     case SDLK_m:            return Key::M;
     case SDLK_n:            return Key::N;
     case SDLK_o:            return Key::O;
     case SDLK_p:            return Key::P;
     case SDLK_q:            return Key::Q;
     case SDLK_r:            return Key::R;
     case SDLK_s:            return Key::S;
     case SDLK_t:            return Key::T;
     case SDLK_u:            return Key::U;
     case SDLK_v:            return Key::V;
     case SDLK_w:            return Key::W;
     case SDLK_x:            return Key::X;
     case SDLK_y:            return Key::Y;
     case SDLK_z:            return Key::Z;
     case SDLK_DELETE:       return Key::Delete;
     case SDLK_KP0:          return Key::Numpad0;
     case SDLK_KP1:          return Key::Numpad1;
     case SDLK_KP2:          return Key::Numpad2;
     case SDLK_KP3:          return Key::Numpad3;
     case SDLK_KP4:          return Key::Numpad4;
     case SDLK_KP5:          return Key::Numpad5;
     case SDLK_KP6:          return Key::Numpad6;
     case SDLK_KP7:          return Key::Numpad7;
     case SDLK_KP8:          return Key::Numpad8;
     case SDLK_KP9:          return Key::Numpad9;
     case SDLK_KP_PERIOD:    return Key::Decimal;
     case SDLK_KP_DIVIDE:    return Key::Divide;
     case SDLK_KP_MULTIPLY:  return Key::Multiply;
     case SDLK_KP_MINUS:     return Key::Subtract;
     case SDLK_KP_PLUS:      return Key::Add;
     case SDLK_KP_ENTER:     return Key::NumpadEnter;
     case SDLK_KP_EQUALS:    return Key::NumpadEquals;
     case SDLK_UP:           return Key::ArrowUp;
     case SDLK_DOWN:         return Key::ArrowDown;
     case SDLK_RIGHT:        return Key::ArrowRight;
     case SDLK_LEFT:         return Key::ArrowLeft;
     case SDLK_INSERT:       return Key::Insert;
     case SDLK_HOME:         return Key::Home;
     case SDLK_END:          return Key::End;
     case SDLK_PAGEUP:       return Key::PageUp;
     case SDLK_PAGEDOWN:     return Key::PageDown;
     case SDLK_F1:           return Key::F1;
     case SDLK_F2:           return Key::F2;
     case SDLK_F3:           return Key::F3;
     case SDLK_F4:           return Key::F4;
     case SDLK_F5:           return Key::F5;
     case SDLK_F6:           return Key::F6;
     case SDLK_F7:           return Key::F7;
     case SDLK_F8:           return Key::F8;
     case SDLK_F9:           return Key::F9;
     case SDLK_F10:          return Key::F10;
     case SDLK_F11:          return Key::F11;
     case SDLK_F12:          return Key::F12;
     case SDLK_F13:          return Key::F13;
     case SDLK_F14:          return Key::F14;
     case SDLK_F15:          return Key::F15;
     case SDLK_NUMLOCK:      return Key::NumLock;
     case SDLK_SCROLLOCK:    return Key::ScrollLock;
     case SDLK_RSHIFT:       return Key::RightShift;
     case SDLK_LSHIFT:       return Key::LeftShift;
     case SDLK_RCTRL:        return Key::RightControl;
     case SDLK_LCTRL:        return Key::LeftControl;
     case SDLK_RALT:         return Key::RightAlt;
     case SDLK_LALT:         return Key::LeftAlt;
     case SDLK_LSUPER:       return Key::LeftWindows;
     case SDLK_RSUPER:       return Key::RightWindows;
     case SDLK_SYSREQ:       return Key::SysRq;
     case SDLK_MENU:         return Key::AppMenu;
     case SDLK_POWER:        return Key::Power;
     case SDLK_CARET:        return Key::Grave;
     default:                return 0;
     }
     return 0;
}

