
#ifndef BLUEBEARD_GAME_HUD_INCLUDED
#define BLUEBEARD_GAME_HUD_INCLUDED


#include <string>
#include <memory>

#include <SDL/SDL.h>

#include <osg/ref_ptr>

#include "Datatypes.h"
#include "RegisteredFpGroup.h"
#include "Scheduler.h"


namespace osg
{
    class Geode;
}

class HudNotificationArea;
class PuppetMasterClient;

//------------------------------------------------------------------------------
class GameHud
{
 public:
    GameHud(PuppetMasterClient * master);
    virtual ~GameHud();

    void startChat();
    bool handleChatInput(SDL_keysym sym);
    void addChatLine(const std::string & msg, const Color & color = Color(1.0f,1.0f,1.0f));

    void addMessage(const std::string & msg, const Color & color = Color(1.0f,1.0f,1.0f));
    void appendMessage(const std::string & msg);

    void setStatusLine(const std::string & line);

    void clearChat(void*);
    void clearMessage(void*);

    void onWindowResized();
    
 protected:

    osg::ref_ptr<osg::Geode> geode_;
    
    hTask clear_chat_task_;
    hTask clear_message_task_;
    
    PuppetMasterClient * master_;
    
    std::auto_ptr<HudNotificationArea> chat_area_;
    std::auto_ptr<HudNotificationArea> chat_label_;
    std::auto_ptr<HudNotificationArea> message_area_;
    std::auto_ptr<HudNotificationArea> status_line_;

    std::string chat_msg_;

    RegisteredFpGroup fp_group_;
};

#endif
