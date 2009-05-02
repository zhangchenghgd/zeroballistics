
#ifndef TANKS_SDL_KEY_NAMES_INCLUDED
#define TANKS_SDL_KEY_NAMES_INCLUDED


#include <SDL/SDL_keysym.h>

#include "Datatypes.h"

namespace input_handler
{

const uint16_t INVALID_KEY_CODE = (uint16_t)-1;    

//------------------------------------------------------------------------------
struct Key
{
    Key(const std::string & name, uint16_t code) : name_(name), code_(code) {}

    std::string name_;
    uint16_t code_;
}; 

//------------------------------------------------------------------------------
const Key MODIFIER_NAMES[] = {
Key("NONE",         KMOD_NONE),

Key("SHIFT",        KMOD_SHIFT), 
Key("LSHIFT",       KMOD_LSHIFT), 
Key("RSHIFT",       KMOD_RSHIFT),

Key("CONTROL",      KMOD_CTRL),  
Key("LCONTROL",     KMOD_LCTRL),  
Key("RCONTROL",     KMOD_RCTRL),  

Key("ALT",          KMOD_ALT),   
Key("LALT",         KMOD_LALT),   
Key("RALT",         KMOD_RALT),   

Key("CAPSLOCK",     KMOD_CAPS),  
Key("META",         KMOD_META),  
Key("NUMLOCK",      KMOD_NUM)    
};

const unsigned NUM_MODIFIERS = sizeof(MODIFIER_NAMES) / sizeof(Key);


//------------------------------------------------------------------------------
/**
 *  Fake codes for mouse events.
 */
enum
{
    KEY_LeftButton   = 0xAA00,
    KEY_MiddleButton = 0xAA01,
    KEY_RightButton  = 0xAA02,
    KEY_WheelUp      = 0xAA03,
    KEY_WheelDown    = 0xAA04,
    KEY_Mouse        = 0xAA05
};
 
//------------------------------------------------------------------------------ 
const Key KEY_NAMES[] = {
    
    // Mouse Buttons, wheel (added)
    Key("LeftButton",		KEY_LeftButton),
    Key("RightButton",		KEY_RightButton),
    Key("MiddleButton",		KEY_MiddleButton),

    Key("WheelUp",		KEY_WheelUp),
    Key("WheelDown",		KEY_WheelDown),

    Key("Mouse",		KEY_Mouse),

    
// Copied & modified from SDL_keysym.h
    Key("BACKSPACE",		SDLK_BACKSPACE),
    Key("TAB",			SDLK_TAB),
    Key("CLEAR",		SDLK_CLEAR),
    Key("RETURN",		SDLK_RETURN),
    Key("PAUSE",		SDLK_PAUSE),
    Key("ESCAPE",		SDLK_ESCAPE),
    Key("SPACE",		SDLK_SPACE),
    Key("EXCLAIM",		SDLK_EXCLAIM),
    Key("QUOTEDBL",		SDLK_QUOTEDBL),
    Key("HASH",			SDLK_HASH),
    Key("DOLLAR",		SDLK_DOLLAR),
    Key("AMPERSAND",		SDLK_AMPERSAND),
    Key("QUOTE",		SDLK_QUOTE),
    Key("LEFTPAREN",		SDLK_LEFTPAREN),
    Key("RIGHTPAREN",		SDLK_RIGHTPAREN),
    Key("ASTERISK",		SDLK_ASTERISK),
    Key("PLUS",			SDLK_PLUS),
    Key("COMMA",		SDLK_COMMA),
    Key("MINUS",		SDLK_MINUS),
    Key("PERIOD",		SDLK_PERIOD),
    Key("SLASH",		SDLK_SLASH),
    Key("0",			SDLK_0),
    Key("1",			SDLK_1),
    Key("2",			SDLK_2),
    Key("3",			SDLK_3),
    Key("4",			SDLK_4),
    Key("5",			SDLK_5),
    Key("6",			SDLK_6),
    Key("7",			SDLK_7),
    Key("8",			SDLK_8),
    Key("9",			SDLK_9),
    Key("COLON",		SDLK_COLON),
    Key("SEMICOLON",		SDLK_SEMICOLON),
    Key("LESS",			SDLK_LESS),
    Key("EQUALS",		SDLK_EQUALS),
    Key("GREATER",		SDLK_GREATER),
    Key("QUESTION",		SDLK_QUESTION),
    Key("AT",			SDLK_AT),
/*                      
                        Skip uppercase letters
*/                     
    Key("LEFTBRACKET",		SDLK_LEFTBRACKET),
    Key("BACKSLASH",		SDLK_BACKSLASH),
    Key("RIGHTBRACKET",		SDLK_RIGHTBRACKET),
    Key("CARET",		SDLK_CARET),
    Key("UNDERSCORE",		SDLK_UNDERSCORE),
    Key("BACKQUOTE",		SDLK_BACKQUOTE),
    Key("a",			SDLK_a),
    Key("b",			SDLK_b),
    Key("c",			SDLK_c),
    Key("d",			SDLK_d),
    Key("e",			SDLK_e),
    Key("f",			SDLK_f),
    Key("g",			SDLK_g),
    Key("h",			SDLK_h),
    Key("i",			SDLK_i),
    Key("j",			SDLK_j),
    Key("k",			SDLK_k),
    Key("l",			SDLK_l),
    Key("m",			SDLK_m),
    Key("n",			SDLK_n),
    Key("o",			SDLK_o),
    Key("p",			SDLK_p),
    Key("q",			SDLK_q),
    Key("r",			SDLK_r),
    Key("s",			SDLK_s),
    Key("t",			SDLK_t),
    Key("u",			SDLK_u),
    Key("v",			SDLK_v),
    Key("w",			SDLK_w),
    Key("x",			SDLK_x),
    Key("y",			SDLK_y),
    Key("z",			SDLK_z),
    Key("DELETE",		SDLK_DELETE),
/* End of ASCII mapped keysyms */
                        
/* International keyboard syms */
    Key("WORLD_0",		SDLK_WORLD_0),
    Key("WORLD_1",		SDLK_WORLD_1),
    Key("WORLD_2",		SDLK_WORLD_2),
    Key("WORLD_3",		SDLK_WORLD_3),
    Key("WORLD_4",		SDLK_WORLD_4),
    Key("WORLD_5",		SDLK_WORLD_5),
    Key("WORLD_6",		SDLK_WORLD_6),
    Key("WORLD_7",		SDLK_WORLD_7),
    Key("WORLD_8",		SDLK_WORLD_8),
    Key("WORLD_9",		SDLK_WORLD_9),
    Key("WORLD_10",		SDLK_WORLD_10),
    Key("WORLD_11",		SDLK_WORLD_11),
    Key("WORLD_12",		SDLK_WORLD_12),
    Key("WORLD_13",		SDLK_WORLD_13),
    Key("WORLD_14",		SDLK_WORLD_14),
    Key("WORLD_15",		SDLK_WORLD_15),
    Key("WORLD_16",		SDLK_WORLD_16),
    Key("WORLD_17",		SDLK_WORLD_17),
    Key("WORLD_18",		SDLK_WORLD_18),
    Key("WORLD_19",		SDLK_WORLD_19),
    Key("WORLD_20",		SDLK_WORLD_20),
    Key("WORLD_21",		SDLK_WORLD_21),
    Key("WORLD_22",		SDLK_WORLD_22),
    Key("WORLD_23",		SDLK_WORLD_23),
    Key("WORLD_24",		SDLK_WORLD_24),
    Key("WORLD_25",		SDLK_WORLD_25),
    Key("WORLD_26",		SDLK_WORLD_26),
    Key("WORLD_27",		SDLK_WORLD_27),
    Key("WORLD_28",		SDLK_WORLD_28),
    Key("WORLD_29",		SDLK_WORLD_29),
    Key("WORLD_30",		SDLK_WORLD_30),
    Key("WORLD_31",		SDLK_WORLD_31),
    Key("WORLD_32",		SDLK_WORLD_32),
    Key("WORLD_33",		SDLK_WORLD_33),
    Key("WORLD_34",		SDLK_WORLD_34),
    Key("WORLD_35",		SDLK_WORLD_35),
    Key("WORLD_36",		SDLK_WORLD_36),
    Key("WORLD_37",		SDLK_WORLD_37),
    Key("WORLD_38",		SDLK_WORLD_38),
    Key("WORLD_39",		SDLK_WORLD_39),
    Key("WORLD_40",		SDLK_WORLD_40),
    Key("WORLD_41",		SDLK_WORLD_41),
    Key("WORLD_42",		SDLK_WORLD_42),
    Key("WORLD_43",		SDLK_WORLD_43),
    Key("WORLD_44",		SDLK_WORLD_44),
    Key("WORLD_45",		SDLK_WORLD_45),
    Key("WORLD_46",		SDLK_WORLD_46),
    Key("WORLD_47",		SDLK_WORLD_47),
    Key("WORLD_48",		SDLK_WORLD_48),
    Key("WORLD_49",		SDLK_WORLD_49),
    Key("WORLD_50",		SDLK_WORLD_50),
    Key("WORLD_51",		SDLK_WORLD_51),
    Key("WORLD_52",		SDLK_WORLD_52),
    Key("WORLD_53",		SDLK_WORLD_53),
    Key("WORLD_54",		SDLK_WORLD_54),
    Key("WORLD_55",		SDLK_WORLD_55),
    Key("WORLD_56",		SDLK_WORLD_56),
    Key("WORLD_57",		SDLK_WORLD_57),
    Key("WORLD_58",		SDLK_WORLD_58),
    Key("WORLD_59",		SDLK_WORLD_59),
    Key("WORLD_60",		SDLK_WORLD_60),
    Key("WORLD_61",		SDLK_WORLD_61),
    Key("WORLD_62",		SDLK_WORLD_62),
    Key("WORLD_63",		SDLK_WORLD_63),
    Key("WORLD_64",		SDLK_WORLD_64),
    Key("WORLD_65",		SDLK_WORLD_65),
    Key("WORLD_66",		SDLK_WORLD_66),
    Key("WORLD_67",		SDLK_WORLD_67),
    Key("WORLD_68",		SDLK_WORLD_68),
    Key("WORLD_69",		SDLK_WORLD_69),
    Key("WORLD_70",		SDLK_WORLD_70),
    Key("WORLD_71",		SDLK_WORLD_71),
    Key("WORLD_72",		SDLK_WORLD_72),
    Key("WORLD_73",		SDLK_WORLD_73),
    Key("WORLD_74",		SDLK_WORLD_74),
    Key("WORLD_75",		SDLK_WORLD_75),
    Key("WORLD_76",		SDLK_WORLD_76),
    Key("WORLD_77",		SDLK_WORLD_77),
    Key("WORLD_78",		SDLK_WORLD_78),
    Key("WORLD_79",		SDLK_WORLD_79),
    Key("WORLD_80",		SDLK_WORLD_80),
    Key("WORLD_81",		SDLK_WORLD_81),
    Key("WORLD_82",		SDLK_WORLD_82),
    Key("WORLD_83",		SDLK_WORLD_83),
    Key("WORLD_84",		SDLK_WORLD_84),
    Key("WORLD_85",		SDLK_WORLD_85),
    Key("WORLD_86",		SDLK_WORLD_86),
    Key("WORLD_87",		SDLK_WORLD_87),
    Key("WORLD_88",		SDLK_WORLD_88),
    Key("WORLD_89",		SDLK_WORLD_89),
    Key("WORLD_90",		SDLK_WORLD_90),
    Key("WORLD_91",		SDLK_WORLD_91),
    Key("WORLD_92",		SDLK_WORLD_92),
    Key("WORLD_93",		SDLK_WORLD_93),
    Key("WORLD_94",		SDLK_WORLD_94),
    Key("WORLD_95",		SDLK_WORLD_95),
                                                
/* Numeric keypad */    
    Key("KP0",			SDLK_KP0),
    Key("KP1",			SDLK_KP1),
    Key("KP2",			SDLK_KP2),
    Key("KP3",			SDLK_KP3),
    Key("KP4",			SDLK_KP4),
    Key("KP5",			SDLK_KP5),
    Key("KP6",			SDLK_KP6),
    Key("KP7",			SDLK_KP7),
    Key("KP8",			SDLK_KP8),
    Key("KP9",			SDLK_KP9),
    Key("KP_PERIOD",		SDLK_KP_PERIOD),
    Key("KP_DIVIDE",		SDLK_KP_DIVIDE),
    Key("KP_MULTIPLY",		SDLK_KP_MULTIPLY),
    Key("KP_MINUS",		SDLK_KP_MINUS),
    Key("KP_PLUS",		SDLK_KP_PLUS),
    Key("KP_ENTER",		SDLK_KP_ENTER),
    Key("KP_EQUALS",		SDLK_KP_EQUALS),
                                                
/* Arrows + Home/End pad */
    Key("UP",			SDLK_UP),
    Key("DOWN",			SDLK_DOWN),
    Key("RIGHT",		SDLK_RIGHT),
    Key("LEFT",			SDLK_LEFT),
    Key("INSERT",		SDLK_INSERT),
    Key("HOME",			SDLK_HOME),
    Key("END",			SDLK_END),
    Key("PAGEUP",		SDLK_PAGEUP),
    Key("PAGEDOWN",		SDLK_PAGEDOWN),
                                                
/* Function keys */  
    Key("F1",			SDLK_F1),
    Key("F2",			SDLK_F2),
    Key("F3",			SDLK_F3),
    Key("F4",			SDLK_F4),
    Key("F5",			SDLK_F5),
    Key("F6",			SDLK_F6),
    Key("F7",			SDLK_F7),
    Key("F8",			SDLK_F8),
    Key("F9",			SDLK_F9),
    Key("F10",			SDLK_F10),
    Key("F11",			SDLK_F11),
    Key("F12",			SDLK_F12),
    Key("F13",			SDLK_F13),
    Key("F14",			SDLK_F14),
    Key("F15",			SDLK_F15),
                                                
/* Key state modifier keys */
    Key("NUMLOCK",		SDLK_NUMLOCK),
    Key("CAPSLOCK",		SDLK_CAPSLOCK),
    Key("SCROLLOCK",		SDLK_SCROLLOCK),
    Key("RSHIFT",		SDLK_RSHIFT),
    Key("LSHIFT",		SDLK_LSHIFT),
    Key("RCTRL",		SDLK_RCTRL),
    Key("LCTRL",		SDLK_LCTRL),
    Key("RALT",			SDLK_RALT),
    Key("LALT",			SDLK_LALT),
    Key("RMETA",		SDLK_RMETA),
    Key("LMETA",		SDLK_LMETA),
    Key("LSUPER",		SDLK_LSUPER),
    Key("RSUPER",		SDLK_RSUPER),
    Key("MODE",			SDLK_MODE),
    Key("COMPOSE",		SDLK_COMPOSE),
                                                
/* Miscellaneous function keys */
    Key("HELP",			SDLK_HELP),
    Key("PRINT",		SDLK_PRINT),
    Key("SYSREQ",		SDLK_SYSREQ),
    Key("BREAK",		SDLK_BREAK),
    Key("MENU",			SDLK_MENU),
    Key("POWER",		SDLK_POWER),
    Key("EURO",			SDLK_EURO),
    Key("UNDO",			SDLK_UNDO),


    Key("INVALID",              INVALID_KEY_CODE)
};


const unsigned NUM_KEYS = sizeof(KEY_NAMES) / sizeof(Key);


//------------------------------------------------------------------------------
inline const Key & getKeyWithCode(uint16_t code)
{
    for (unsigned key=0; key<NUM_KEYS; ++key)
    {
        if (KEY_NAMES[key].code_ == code) return KEY_NAMES[key];
    }

    return KEY_NAMES[NUM_KEYS-1];
}


} // namespace input_handler

#endif // #ifndef TANKS_SDL_KEY_NAMES_INCLUDED
