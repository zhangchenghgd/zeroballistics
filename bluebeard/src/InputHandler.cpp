
#include "InputHandler.h"


#include "Log.h"
#include "ParameterManager.h"
#include "SdlKeyNames.h"
#include "Exception.h"
#include "Console.h"

namespace input_handler
{

//------------------------------------------------------------------------------
InputHandlerFp::InputHandlerFp(const std::string & name) :
    name_(name)
{
}

//------------------------------------------------------------------------------
const std::string & InputHandlerFp::toString() const
{
    return name_;
}

//------------------------------------------------------------------------------
bool InputHandlerFp::operator==(const RegisteredFp & other) const
{
    const InputHandlerFp * other_ihf = dynamic_cast<const InputHandlerFp*>(&other);
    if (!other_ihf) return false;

    return name_ == other_ihf->name_;    
}


//------------------------------------------------------------------------------
void InputHandlerFp::deregisterPointer() const
{
    s_input_handler.deregisterInputCallback(name_);
}
    

    
//------------------------------------------------------------------------------
InputHandler::InputHandler() :
    mouse_pos_  (Vector2d(0.0f, 0.0f)),
    mouse_delta_(Vector2d(0.0f, 0.0f)),
    modifiers_(0)
{
    s_log << Log::debug('i') << "InputHandler constructor\n";


#ifdef ENABLE_DEV_FEATURES
    s_console.addFunction("printRegisteredInputHandlers",
                          ConsoleFun(this, &InputHandler::printRegisteredFunctions),
                          &fp_group_);
    s_console.addFunction("printKeymap",
                          ConsoleFun(this, &InputHandler::printKeymap),
                          &fp_group_);
#endif
}

//------------------------------------------------------------------------------
InputHandler::~InputHandler()
{
    s_log << Log::debug('d') << "InputHandler destructor\n";
}

    
//------------------------------------------------------------------------------
/**
 *  Load or reloads the keymap from the specified file. A keymap is an
 *  array of string arrays. The first element is the action to be
 *  performed, following are a key and optional modifiers.
 */
void InputHandler::loadKeymap(const std::string & file, const std::string & super_section)
{
    if (isKeymapLoaded(file, super_section))
    {
        unloadKeymap(file, super_section);
        s_log << Log::debug('k')
              << "REloading keymap ";
    } else
    {
        s_log << Log::debug('k')
              << "Loading keymap ";
    }
    s_log << super_section
          << " in file "
          << file
          << "\n";

    loaded_keymaps_.insert(std::make_pair(file, super_section));

    
    LocalParameters keymap_params;
    keymap_params.loadParameters(file, super_section);

    const StringParamMap & param_map =
        keymap_params.getParameterMap<std::vector<std::string> >();
    
    for (StringParamMap::const_iterator it = param_map.begin();
         it != param_map.end();
         ++it)
    {
        // Skip headers
        if (it->second.empty()) continue;

        if (it->first.length() <= ACTION_PREFIX_LENGTH )
        {
            s_log << Log::warning
                  << "Invalid keymap entry in supersection "
                  << super_section
                  << ", "
                  << file
                  << ": "
                  << it->first
                  << "\n";

            continue;
        }

        addKey(it->first.substr(ACTION_PREFIX_LENGTH),
               it->second[0],
               it->second.size() > 1 ? it->second[1] : "NONE",
               it->second.size() > 2 ? it->second[2] : "NONE");
    }
}

//------------------------------------------------------------------------------
/**
 *  Removes all keymappings that are assigned to functions in the
 *  specified keymap.
 */
void InputHandler::unloadKeymap(const std::string & file, const std::string & super_section)
{
    if (!isKeymapLoaded(file, super_section)) return;

    loaded_keymaps_.erase(loaded_keymaps_.find(std::make_pair(file, super_section)));
    
    s_log << Log::debug('k')
          << "Unloading keymap "
          << super_section
          << " in file "
          << file
          << "\n";

    LocalParameters keymap_params;
    keymap_params.loadParameters(file, super_section);

    const StringParamMap & param_map =
        keymap_params.getParameterMap<std::vector<std::string> >();
    
    for (StringParamMap::const_iterator it = param_map.begin();
         it != param_map.end();
         ++it)
    {
        // Traverse registered keys and see whether a function was
        // registered under this name.
        for (KeyMap::iterator key_it = key_map_.begin();
             key_it != key_map_.end(); /* do nothing */ )
        {
            if (it->first.length() > ACTION_PREFIX_LENGTH &&
                key_it->second.name_ == it->first.substr(ACTION_PREFIX_LENGTH))
            {
                key_it = key_map_.erase(key_it);
            } else ++key_it;
        }
    }    
}


//------------------------------------------------------------------------------
bool InputHandler::isKeymapLoaded(const std::string & file, const std::string & supersection) const
{
    return loaded_keymaps_.find(std::make_pair(file, supersection)) != loaded_keymaps_.end();
}


//------------------------------------------------------------------------------
/**
 *  Registers the specified callback function with the given name.
 */
void InputHandler::registerInputCallback(const std::string & name,
                                         SingleInputHandler handler,
                                         RegisteredFpGroup * group)
{
    if (registered_callbacks_.find(name) != registered_callbacks_.end())
        throwTwiceRegistered(name);
    
    registered_callbacks_.insert(std::make_pair(name, RegisteredCallback(name, handler)));
    group->addFunctionPointer(new InputHandlerFp(name));
    
    s_log << Log::debug('k') << "Registered " << name << ".\n";
}


//------------------------------------------------------------------------------
void InputHandler::registerInputCallback(const std::string & name,
                                         ContinuousInputHandler handler,
                                         RegisteredFpGroup * group)
{
    if (registered_callbacks_.find(name) != registered_callbacks_.end())
        throwTwiceRegistered(name);
    
    registered_callbacks_.insert(std::make_pair(name, RegisteredCallback(name, handler)));
    group->addFunctionPointer(new InputHandlerFp(name));

    s_log << Log::debug('k') << "Registered " << name << ".\n";
}


//------------------------------------------------------------------------------
void InputHandler::registerInputCallback(const std::string & name,
                                         MouseInputHandler handler,
                                         RegisteredFpGroup * group)
{
    if (registered_callbacks_.find(name) != registered_callbacks_.end())
        throwTwiceRegistered(name);
    
    registered_callbacks_.insert(std::make_pair(name, RegisteredCallback(name, handler)));
    group->addFunctionPointer(new InputHandlerFp(name));

    s_log << Log::debug('k') << "Registered " << name << ".\n";
}




//------------------------------------------------------------------------------
/**
 *  \return Whether the key was consumed.
 */
bool InputHandler::onKeyDown(uint16_t key)
{    
    uint32_t total_code = getTotalKeyCode(key, modifiers_);

    RegisteredCallback * callback = NULL;
    for (KeyMap::iterator it = key_map_.begin();
         it != key_map_.end();
         ++it)
    {
        if (it->first == total_code)
        {
            // found perfect match, execute it
            callback = &it->second;
            break;
        } else if (getKey(it->first) == key &&
                   ((getModifier(it->first) & modifiers_) == getModifier(it->first)))
        {
            // too many modifiers down, but we will execute the
            // callback if we don't find anything better.
            callback = &it->second;
        }
    }

    if (callback)
    {
        try
        {
            if(callback->type_ == CT_MOUSE)
            {
                callback->mouse_(mouse_pos_, mouse_delta_);
            } else if (callback->type_ == CT_CONTINUOUS)
            {
                callback->continuous_(true);
            } else
            {
                callback->single_();
            }
        } catch (Exception & e)
        {
            e.addHistory("\"" + callback->name_ + "\"");
            e.addHistory("InputHandler::onKeyDown()");
            s_log << Log::error
                  << e.getTotalErrorString()
                  << "\n";
        }

        return true;
    }

    return false;
}

//------------------------------------------------------------------------------
/**
 *  Handles a keyup. Keyups are only relevant for continuous callback
 *  functions (they will be called with argument 0). Modifiers are
 *  ignored so the keyup message will be handled even if modifiers
 *  have been pressed during the key was pressed.
 */
void InputHandler::onKeyUp(uint16_t key)
{
    // Traverse all registered keys and skip all but continuous types.
    for (KeyMap::iterator it = key_map_.begin();
         it != key_map_.end();
         ++it)
    {
        if (getKey(it->first) != key ||
            it->second.type_  != CT_CONTINUOUS) continue;

        it->second.continuous_(false);
    }
}


    
//------------------------------------------------------------------------------
/**
 *  \return Whether the mouse cursor should be rewarped to the center.
 */
bool InputHandler::setMousePos(Vector2d pos, Vector2d delta)
{
    mouse_pos_   = pos;
    mouse_delta_ = delta;

    return onKeyDown(KEY_Mouse);
}


//------------------------------------------------------------------------------
void InputHandler::setModifiers(uint16_t state)
{
    modifiers_ = state;
}

//------------------------------------------------------------------------------
std::string InputHandler::getKeyForFunction(const std::string & fun_name) const
{
    for (unsigned fun=0; fun<key_map_.size(); ++fun)
    {
        if (key_map_[fun].second.name_ == fun_name)
        {
            uint16_t key_code = getKey(key_map_[fun].first);
            return getKeyWithCode(key_code).name_;
        }
    }

    assert(false);
    return "INVALID";
}

//------------------------------------------------------------------------------
/**
 *  Deregisters the function with the given name.
 */
void InputHandler::deregisterInputCallback(const std::string & name)
{
    std::map<std::string, RegisteredCallback>::iterator it = registered_callbacks_.find(name);
    assert(it != registered_callbacks_.end());
    registered_callbacks_.erase(it);

    // Remove all keys registered for this function, too, but issue a
    // warning if any exists.
    for (KeyMap::iterator it = key_map_.begin();
         it != key_map_.end(); /* do nothing */)
    {
        if (it->second.name_ == name)
        {
            s_log << Log::debug('k')
                  << "Removing input function '"
                  << name
                  << "', but key "
                  << getKeyWithCode(getKey(it->first)).name_
                  << " is still registered for that function.\n";
            
            it = key_map_.erase(it);
        } else ++it;
    }
}


//------------------------------------------------------------------------------
void InputHandler::addKey(const std::string & callback_name,
                          const std::string & key_name,
                          const std::string & modifier1_name,
                          const std::string & modifier2_name)
{
    std::map<std::string, RegisteredCallback>::iterator callback;
        
    callback = registered_callbacks_.find(callback_name);
    if (callback == registered_callbacks_.end())
    {
        s_log << Log::debug('k')
              << Log::warning
              << "Input handler '"
              << callback_name
              << "' was not registered, but a key ('"
              << key_name
              <<"') was assigned to it.\n";
        return;
    }

    uint32_t total_code = getKeyCode(key_name, modifier1_name, modifier2_name);

    // check for and remove duplicate assignments
    KeyMap::iterator it = key_map_.begin();
    while (it != key_map_.end())
    {
        if (it->first == total_code)
        {
            s_log << Log::debug('k')
                  << Log::warning
                  << "Key " << modifier1_name
                  << "+" << modifier2_name << "+"
                  << key_name << " was registered more than once. Old: "
                  << it->second.name_
                  << ", new: "
                  << callback_name
                  << "\n";
            it = key_map_.erase(it);
        } else ++it;
    }

    // add key
    key_map_.push_back(std::make_pair(total_code, callback->second));
    s_log << Log::debug('k')
          << (modifier2_name == "NONE" ? "" : modifier2_name + "+")
          << (modifier1_name == "NONE" ? "" : modifier1_name + " + ")
          << key_name
          << " mapped to "
          << callback_name << ".\n";        
}

//------------------------------------------------------------------------------
/**
 *  Searches KEY_NAMES and MODIFIER_NAMES for the specified key and
 *  encodes the corresponding key codes into a 32-bit number.
 */
uint32_t InputHandler::getKeyCode(const std::string & key_name,
                                  const std::string & modifier1_name,
                                  const std::string & modifier2_name) const
{
    uint16_t key_code      = 0;
    uint16_t modifier_code = 0;
    
    for (unsigned key = 0; key < NUM_KEYS; ++key)
    {
        if (KEY_NAMES[key].name_ == key_name)
        {
            key_code = KEY_NAMES[key].code_;
            break;
        }

        if (key == NUM_KEYS-1)
        {
            Exception e;
            e << "Key \"" << key_name << "\" is unknown.";
            throw e;
        }
    }


    bool found1=false, found2=false;
    for (unsigned key = 0; key < NUM_MODIFIERS; ++key)
    {
        if (MODIFIER_NAMES[key].name_ == modifier1_name)
        {
            found1 = true;
            modifier_code |= MODIFIER_NAMES[key].code_;
        }

        if (MODIFIER_NAMES[key].name_ == modifier2_name)
        {
            found2 = true;
            modifier_code |= MODIFIER_NAMES[key].code_;
        }
    }
    
    if (!found1 || !found2)
    {
        Exception e;
        e << "Modifier \""
          << (found1 ? "" : modifier1_name);

        if (!found2) e << "\", \"" << modifier2_name;
        e << "\" unknown.";
            
        throw e;
    }

    return getTotalKeyCode(key_code, modifier_code);
}

//------------------------------------------------------------------------------
/**
 *  Returns the given code and modifier encoded into a 32-bit number.
 */
uint32_t InputHandler::getTotalKeyCode(uint16_t code, uint16_t modifiers) const
{
    return (unsigned)modifiers | (((unsigned)code) << 16);
}

//------------------------------------------------------------------------------
uint16_t InputHandler::getKey(uint32_t code) const
{
    return code >> 16;
}

//------------------------------------------------------------------------------
uint16_t InputHandler::getModifier(uint32_t code) const
{
    return code & 0xffff;
}


//------------------------------------------------------------------------------
void InputHandler::throwTwiceRegistered(const std::string & name) const
{
    Exception e;
    e << "Input Action " << name << " was registered twice.";
    throw e;
}



//------------------------------------------------------------------------------
std::string InputHandler::printRegisteredFunctions(const std::vector<std::string>&) const
{
    std::string ret;

    for (std::map<std::string, RegisteredCallback>::const_iterator it = registered_callbacks_.begin();
         it != registered_callbacks_.end();
         ++it)
    {
        ret += it->first + "\n";
    }

    return ret;
}



//------------------------------------------------------------------------------
std::string InputHandler::printKeymap(const std::vector<std::string>&) const
{
    std::ostringstream strstr;

    for (KeyMap::const_iterator it = key_map_.begin();
         it != key_map_.end();
         ++it)
    {
        strstr << (void*)it->first
               << " : "
               << it->second.name_
               << "\n";
    }

    return strstr.str();
}



    
} // namespace input_handler

