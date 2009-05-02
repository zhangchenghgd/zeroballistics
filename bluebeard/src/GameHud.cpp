

#include "GameHud.h"

#include <osg/Geode>


#include "HudNotificationArea.h"
#include "PuppetMasterClient.h"
#include "HudTextElement.h"
#include "Scheduler.h"
#include "Paths.h"
#include "Log.h"
#include "SceneManager.h"
#include "HudTextureElement.h"



const float HUD_CLEAR_DELAY = 12.0f;


//------------------------------------------------------------------------------
GameHud::GameHud(PuppetMasterClient * master) :
    geode_(new osg::Geode),
    clear_chat_task_(INVALID_TASK_HANDLE),
    clear_message_task_(INVALID_TASK_HANDLE),
    master_(master),
    chat_area_   (new HudNotificationArea("chat_area",    geode_.get())),
    chat_label_  (new HudNotificationArea("chat_label",   geode_.get())),
    message_area_(new HudNotificationArea("message_area", geode_.get())),
    status_line_ (new HudNotificationArea("status_line",  geode_.get()))
{
    geode_->setName("GameHud geode");
    s_scene_manager.addHudNode(geode_.get());

    s_scene_manager.addObserver(ObserverCallbackFun0(this, &GameHud::onWindowResized),
                                SOE_RESOLUTION_CHANGED,
                                &fp_group_);
}


//------------------------------------------------------------------------------
GameHud::~GameHud()
{
    DeleteNodeVisitor delete_visual(geode_.get());
    s_scene_manager.getRootNode()->accept(delete_visual);    
}

//------------------------------------------------------------------------------
void GameHud::startChat()
{
    chat_label_->addLine("say:|");
}


//------------------------------------------------------------------------------
/**
 *  \return false if chat window should remain active, true otherwise.
 */
bool GameHud::handleChatInput(SDL_keysym sym)
{
    switch(sym.sym)
    {
    case SDLK_ESCAPE:
        chat_label_->addLine("");
        chat_msg_.clear();
        return true;

    case SDLK_RETURN:
        if (!chat_msg_.empty())
        {
            master_->sendChat(chat_msg_);
        }

        chat_label_->addLine("");
        chat_msg_.clear();
        
        return true;

    case SDLK_BACKSPACE:
        if (!chat_msg_.empty())
        {
            chat_msg_ = chat_msg_.substr(0, chat_msg_.size()-1);
            chat_label_->addLine(std::string("say:") + chat_msg_ + "|");
        }
        return false;
            
    default:
        if (sym.unicode != 0)
        {
            chat_msg_ += sym.unicode;
            chat_label_->addLine(std::string("say:") + chat_msg_ + "|");
        } 
        return false;
    }
}


//------------------------------------------------------------------------------
void GameHud::addChatLine(const std::string & msg, const Color & color)
{
    chat_area_->addLine(msg, color);

    s_scheduler.removeTask(clear_chat_task_, &fp_group_);
    clear_chat_task_ = s_scheduler.addEvent(SingleEventCallback(this, &GameHud::clearChat),
                                            HUD_CLEAR_DELAY,
                                            NULL,
                                            "GameHud::clearChat",
                                            &fp_group_);
}

//------------------------------------------------------------------------------
void GameHud::addMessage(const std::string & msg, const Color & color)
{
    s_log << "MSG: " << msg << "\n";
    
    message_area_->addLine(msg, color);

    s_scheduler.removeTask(clear_message_task_, &fp_group_);
    clear_message_task_ = s_scheduler.addEvent(SingleEventCallback(this, &GameHud::clearMessage),
                                               HUD_CLEAR_DELAY,
                                               NULL,
                                               "GameHud::clearMessage",
                                               &fp_group_);
}


//------------------------------------------------------------------------------
void GameHud::appendMessage(const std::string & msg)
{
    message_area_->appendToLine(msg);
}


//------------------------------------------------------------------------------
void GameHud::setStatusLine(const std::string & line)
{
    status_line_->addLine(line);
}


//------------------------------------------------------------------------------
void GameHud::clearChat(void*)
{
    clear_chat_task_ = INVALID_TASK_HANDLE;
    chat_area_->clear();
}

//------------------------------------------------------------------------------
void GameHud::clearMessage(void*)
{
    clear_message_task_ = INVALID_TASK_HANDLE;
    message_area_->clear();
}

//------------------------------------------------------------------------------
void GameHud::onWindowResized()
{
    chat_area_   ->recalcTextPos();
    chat_label_  ->recalcTextPos();
    message_area_->recalcTextPos();
    status_line_ ->recalcTextPos();
}
