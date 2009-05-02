
#include "GUIProfiler.h"

#include "Profiler.h"
#include "Utils.cpp"

#include "SceneManager.h" ///< used for triangle count

//------------------------------------------------------------------------------
GUIProfiler::GUIProfiler(CEGUI::Window * parent) :
            profiler_window_(NULL),
            profiler_text_(NULL)
{
    enableFloatingPointExceptions(false);

    CEGUI::WindowManager& wm = CEGUI::WindowManager::getSingleton();
    
    // Use parent window name as prefix to avoid name clashes
    wm.loadWindowLayout("profiler.layout", parent->getName());
    profiler_window_ = (CEGUI::FrameWindow*) wm.getWindow(parent->getName() + "profiler/window");
    profiler_text_   = (CEGUI::Window*)      wm.getWindow(parent->getName() + "profiler/text");

    if (!profiler_window_ || !profiler_text_ )
    {
        throw Exception("GUI Profiler is missing a widget!");
    }

    // add console to widget tree
    parent->addChildWindow(profiler_window_);

    profiler_window_->setVisible(false);
    profiler_window_->setEnabled(false);

    s_scheduler.addTask(PeriodicTaskCallback(this, &GUIProfiler::refresh),
                        1.0f,
                        "GUIProfiler::refresh",
                        &fp_group_);

    profiler_window_->subscribeEvent(CEGUI::FrameWindow::EventCloseClicked,
                                     CEGUI::Event::Subscriber(&GUIProfiler::closeClicked, this));
    profiler_window_->subscribeEvent(CEGUI::FrameWindow::EventKeyDown,
                                     CEGUI::Event::Subscriber(&GUIProfiler::onKeyDown, this));
    profiler_text_->subscribeEvent  (CEGUI::Window::EventKeyDown,
                                     CEGUI::Event::Subscriber(&GUIProfiler::onKeyDown, this));

    profiler_text_->subscribeEvent(CEGUI::Window::EventMouseButtonDown, CEGUI::Event::Subscriber(&GUIProfiler::onMouseDown, this));

    enableFloatingPointExceptions();
}

//------------------------------------------------------------------------------
GUIProfiler::~GUIProfiler()
{
}

//------------------------------------------------------------------------------
void GUIProfiler::toggle()
{
    if (!profiler_window_) return;
    profiler_window_->setVisible(!profiler_window_->isVisible());

    if (profiler_window_->isVisible()) 
    {
        profiler_window_->enable();
        profiler_text_->enable();
        profiler_window_->activate();
        profiler_text_->activate(); 
        refresh(1.0f);
    }
    else
    {
        profiler_window_->disable();
        profiler_text_->disable();
        profiler_window_->deactivate();
        profiler_text_->deactivate();
    }
}


//------------------------------------------------------------------------------
bool GUIProfiler::isActive() const
{
    if (!profiler_window_) return false;
    return profiler_window_->isVisible();
}

//------------------------------------------------------------------------------
void GUIProfiler::refresh(float dt)
{
    if(!profiler_window_->isVisible()) return;

    s_profiler.refresh(dt);
    profiler_text_->setText(std::string("triangles: ") + toString(s_scene_manager.calculateStats())
                            + "\n" + s_profiler.getString());
}

//------------------------------------------------------------------------------
bool GUIProfiler::closeClicked(const CEGUI::EventArgs& e)
{
    toggle();
    return true;
}

//------------------------------------------------------------------------------
bool GUIProfiler::onKeyDown(const CEGUI::EventArgs& e)
{
	CEGUI::KeyEventArgs* ek=(CEGUI::KeyEventArgs*)&e;

    switch(ek->scancode)
    {
    case CEGUI::Key::Numpad0:
        s_profiler.enterParent();
        refresh(1.0f);
        return true;
    case CEGUI::Key::Numpad1:
        s_profiler.enterChild0();
        refresh(1.0f);
        return true;
    case CEGUI::Key::Numpad2:
        s_profiler.enterChild1();
        refresh(1.0f);
        return true;
    case CEGUI::Key::Numpad3:
        s_profiler.enterChild2();
        refresh(1.0f);
        return true;
    case CEGUI::Key::Numpad4:
        s_profiler.enterChild3();
        refresh(1.0f);
        return true;
    case CEGUI::Key::Numpad5:
        s_profiler.enterChild4();
        refresh(1.0f);
        return true;
    case CEGUI::Key::Numpad6:
        s_profiler.enterChild5();
        refresh(1.0f);
        return true;
    case CEGUI::Key::Numpad7:
        s_profiler.enterChild6();
        refresh(1.0f);
        return true;
    case CEGUI::Key::Numpad8:
        s_profiler.enterChild7();
        refresh(1.0f);
        return true;
    case CEGUI::Key::Numpad9:
        s_profiler.enterChild8();
        refresh(1.0f);
        return true;
    default:
        return false;

    }
}

/*
 *  consume mouse event when profiler window has been clicked
 */
//------------------------------------------------------------------------------
bool GUIProfiler::onMouseDown(const CEGUI::EventArgs& e)
{
    return true;
}
