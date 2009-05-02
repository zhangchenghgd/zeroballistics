

#ifndef INTRO_TASK_INCLUDED
#define INTRO_TASK_INCLUDED

#include "Datatypes.h"

class CDirectShow;

//------------------------------------------------------------------------------
class IntroTask 
{
public:
    IntroTask(HINSTANCE hInstance, int cmd_show); 
    virtual ~IntroTask();

    void run();

    void onResizeEvent(unsigned width, unsigned height);

    void quit();
 protected:

     void registerClass(HINSTANCE inst);


     bool quit_;

     HINSTANCE hinst_;
    HWND hwnd_;
    CDirectShow * direct_show_;
    int video_id_;
};



#endif
