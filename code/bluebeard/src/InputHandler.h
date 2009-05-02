
#ifndef TANKS_INPUT_HANDLER_INCLUDED
#define TANKS_INPUT_HANDLER_INCLUDED

#include <map>
#include <string>
#include <set>

#include <loki/Functor.h>


#include "Singleton.h"
#include "Datatypes.h"
#include "Vector2d.h"
#include "RegisteredFpGroup.h"
#include "Exception.h"


namespace input_handler
{

    
typedef Loki::Functor<void>                                      SingleInputHandler;
typedef Loki::Functor<void, LOKI_TYPELIST_1(bool)>               ContinuousInputHandler;
typedef Loki::Functor<void, LOKI_TYPELIST_2(Vector2d, Vector2d)> MouseInputHandler;


/// The prefix determines the order of the keys in the options menu. 
const unsigned ACTION_PREFIX_LENGTH = 4; 


//------------------------------------------------------------------------------
/**
 *  \see RegisteredFpGroup.
 */
class InputHandlerFp : public RegisteredFp
{
 public:
    InputHandlerFp(const std::string & name);
    
    virtual const std::string & toString() const;
    virtual bool operator==(const RegisteredFp & other) const;
    virtual void deregisterPointer() const;

 protected:
    std::string name_;
};
 
 
//------------------------------------------------------------------------------ 
enum CALLBACK_TYPE
{
    CT_SINGLE,
    CT_CONTINUOUS,
    CT_MOUSE
};



//------------------------------------------------------------------------------
class KeyNotRegisteredException : public Exception
{
 public:
    KeyNotRegisteredException(const std::string & key) :
        Exception(key + " was not registered.")
        {
        }
};

 
//------------------------------------------------------------------------------
class RegisteredCallback
{
 public:
    RegisteredCallback(const std::string & name, SingleInputHandler handler) :
        name_(name), type_(CT_SINGLE), single_(handler) {}
    
    RegisteredCallback(const std::string & name, ContinuousInputHandler handler) :
        name_(name), type_(CT_CONTINUOUS), continuous_(handler) {}
    
    RegisteredCallback(const std::string & name, MouseInputHandler handler) :
        name_(name), type_(CT_MOUSE), mouse_(handler) {}

    std::string name_; ///< The name of the action. This is stored so
                       ///actions can be deregistered.
    
    CALLBACK_TYPE type_; ///< Defines the type for callback_.

    /// The callback function for a single action. Check type_ to see
    /// whether this is valid.
    SingleInputHandler single_;

    /// The callback function for a continuous action. Check type_ to
    /// see whether this is valid.
    ContinuousInputHandler continuous_;

    /// The callback function for a mouse action. Check type_ to see
    /// whether this is valid.
    MouseInputHandler mouse_;
};



#define s_input_handler Loki::SingletonHolder<input_handler::InputHandler, Loki::CreateUsingNew, SingletonInputHandlerLifetime >::Instance()
//------------------------------------------------------------------------------
/**
 *  Does the job of mapping input to game actions. Three different
 *  type of game actions can be registered with a descriptive
 *  name. The loaded keymap determines the action an input event is
 *  mapped to.
 *
 *  The three different types of input are:
 *
 *  - single: A single keypress exectues the action. Subject to
 *  autorepeat.
 *
 *  - continuous: Currently only implemented for keyboard, so this is
 *  not really continuous. If the relevant key is pressed, the
 *  registered function is called with an argument of 1.0, if the key
 *  is released the argument is 0.0.
 *
 *  - mouse: Upon action, the registered function is called with the
 *  mouse coordinates.
 */
class InputHandler
{
    friend class InputHandlerFp;
    
    DECLARE_SINGLETON(InputHandler);
 public:

    virtual ~InputHandler();

    void loadKeymap  (const std::string & file, const std::string & supersection);
    void unloadKeymap(const std::string & file, const std::string & supersection);
    bool isKeymapLoaded(const std::string & file, const std::string & supersection) const;
    
    void registerInputCallback(const std::string & name, SingleInputHandler     handler, RegisteredFpGroup * group);
    void registerInputCallback(const std::string & name, ContinuousInputHandler handler, RegisteredFpGroup * group);
    void registerInputCallback(const std::string & name, MouseInputHandler      handler, RegisteredFpGroup * group);

    bool onKeyDown(uint16_t key);
    void onKeyUp  (uint16_t key);
    
    bool setMousePos(Vector2d pos, Vector2d delta);
    void setModifiers(uint16_t state);

    std::string getKeyForFunction(const std::string & fun) const;
    
 protected:

    typedef std::map<std::string, std::vector<std::string> >      StringParamMap;
    typedef std::vector<std::pair<uint32_t, RegisteredCallback> > KeyMap;

    void deregisterInputCallback(const std::string & name);
    
    
    void addKey(const std::string & callback_name,
                const std::string & key_name,
                const std::string & modifier1_name,
                const std::string & modifier2_name);

    uint32_t getKeyCode(const std::string & key_name,
                        const std::string & modifier1_name,
                        const std::string & modifier2_name) const;

    uint32_t getTotalKeyCode(uint16_t code, uint16_t modifiers) const;
    uint16_t getKey(uint32_t code) const;
    uint16_t getModifier(uint32_t code) const;
    
    void throwTwiceRegistered(const std::string & name) const;
    

    std::string printRegisteredFunctions(const std::vector<std::string>&) const;
    std::string printKeymap             (const std::vector<std::string>&) const;
    
    
    /// Maintains a list of all possible function a key can be mapped to.
    std::map<std::string, RegisteredCallback> registered_callbacks_;

    KeyMap key_map_; ///< Maps keycodes to registered callback function.

    Vector2d mouse_pos_;    ///< The current mouse position.
    Vector2d mouse_delta_;  ///< The delta in mouse pos from the last movement.
    
    uint16_t modifiers_;    ///< The currently active modifier keys.

    std::set<std::pair<std::string, std::string> > loaded_keymaps_;
    
    RegisteredFpGroup fp_group_;
};


} // namespace input_handler

#endif // #ifndef TANKS_INPUT_HANDLER_INCLUDED
