

#include "IntroTask.h"

#include "Paths.h"
#include "DirectShow.h"
#include "ParameterManager.h"



#pragma comment(lib, "strmiids_video.lib")
#pragma comment(lib, "quartz_video.lib")

const char * wnd_class_name = "zb_intro_wnd";

IntroTask * g_task = NULL;

//------------------------------------------------------------------------------
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	int wmId, wmEvent;
	HDC hdc;

	switch (message) 
	{	
    case WM_SIZE:
        g_task->onResizeEvent(LOWORD(lParam), HIWORD(lParam));
        break;
    case WM_KEYDOWN:
    case WM_MBUTTONDOWN:
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
	case WM_DESTROY:
        g_task->quit();
		break;
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}




//------------------------------------------------------------------------------
IntroTask::IntroTask(HINSTANCE hinst, int cmd_show) :
    quit_(false),
    hinst_(hinst),
    hwnd_(0),
    direct_show_(NULL),
    video_id_(-1)
{   
    g_task = this;

    registerClass(hinst);
    

    DWORD style;
    DWORD style_ex = 0;
    if (s_params.get<bool>("client.app.fullscreen"))
    {
        style_ex = WS_EX_TOPMOST;
        style =  WS_POPUP;
    } else
    {
        style = WS_OVERLAPPEDWINDOW;
    }
    hwnd_ = CreateWindowEx(style_ex, wnd_class_name, "Zero Ballistics Intro", style,
        0, 0, CW_USEDEFAULT, CW_USEDEFAULT, NULL, NULL, hinst, NULL);

   if (!hwnd_) return;

   ShowWindow(hwnd_, s_params.get<bool>("client.app.fullscreen") ? SW_MAXIMIZE : cmd_show);
   UpdateWindow(hwnd_);




    std::string movie = s_params.get<std::string>("client.intro.movie");

    direct_show_ = CDirectShow::GetInstance();
    direct_show_->InitOnce();
    video_id_ = direct_show_->LoadFile( const_cast<char *>(movie.c_str()),
                                        hwnd_);

    // check if video was loaded correctly, otherwise quit on video_finished_
    if(video_id_ != -1)
    {
        direct_show_->PlayFileMovie(video_id_, hwnd_, false);
        direct_show_->setVolume(video_id_, 200000);      
    }
}

//------------------------------------------------------------------------------
IntroTask::~IntroTask()
{
   if (direct_show_)
   {
       if(video_id_ != -1) direct_show_->Stop(video_id_);

       direct_show_->ShutDown();
       direct_show_->DeleteInstance();      
   }

   DestroyWindow(hwnd_);
   UnregisterClass(wnd_class_name, hinst_);
}

//------------------------------------------------------------------------------
void IntroTask::run()
{
    if (!direct_show_ || video_id_ == -1) 
    {
        return;
    }

    while (!direct_show_->isFinished(video_id_) && !quit_)
    {
	    MSG msg;
        if (PeekMessage( &msg, hwnd_, 0, 0, PM_REMOVE ))
        { 
            TranslateMessage(&msg); 
            DispatchMessage(&msg); 
        }

        ::Sleep((DWORD)20);
    }
}


//------------------------------------------------------------------------------
void IntroTask::quit()
{
    quit_ = true;
}

//------------------------------------------------------------------------------
void IntroTask::registerClass(HINSTANCE inst)
{
	WNDCLASSEX wcex;
    memset(&wcex, 0, sizeof(wcex));

	wcex.cbSize = sizeof(WNDCLASSEX); 

	wcex.style			= CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc	= (WNDPROC)WndProc;
	wcex.cbClsExtra		= 0;
	wcex.cbWndExtra		= 0;
	wcex.hInstance		= inst;
	wcex.hIcon			= NULL;//LoadIcon(hInstance, (LPCTSTR)IDI_DIRECTSHOWTRY2);
	wcex.hCursor		= NULL;//LoadCursor(NULL, IDC_ARROW);
	wcex.hbrBackground	= NULL;//(HBRUSH)(COLOR_WINDOW+1);
	wcex.lpszMenuName	= NULL;//(LPCTSTR)IDC_DIRECTSHOWTRY2;
	wcex.lpszClassName	= wnd_class_name;
	wcex.hIconSm		= NULL;//LoadIcon(wcex.hInstance, (LPCTSTR)IDI_SMALL);

	RegisterClassEx(&wcex);
}


//------------------------------------------------------------------------------
void IntroTask::onResizeEvent(unsigned width, unsigned height)
{    
    if(!s_params.get<bool>("client.app.fullscreen"))
    {
        if(video_id_ != -1) direct_show_->Checkforwindow(video_id_, hwnd_);
    }
}
