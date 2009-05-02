/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// File: DirectShow.cpp	
//
// Author: Allen Danklefsen
//
// Creation Date: July 24th , 2004
//
// Purpose: Direct Show for us to use...
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "DirectShow.h"

#define WM_GRAPHNOTIFY WM_APP + 1 //message handler case.

//convert from decibel lvl to logarithmic
#define DECTOLOG(vol) (-15 * (100 - vol))

//convert to scale of -10k to 10k left to right
#define CONVPAN(pan) (pan * 100)

CDirectShow *CDirectShow::pInstance = 0;

//default constructor
CDirectShow::CDirectShow()
{}

//delete instance
void CDirectShow::DeleteInstance(void)
{
	if(pInstance)
	{
		delete pInstance;
		pInstance = 0;
	}
}

CDirectShow *CDirectShow::GetInstance(void)
{
	if(pInstance ==0)
		pInstance = new CDirectShow;

	return pInstance;
}

CDirectShow::~CDirectShow(void)
{
}

void CDirectShow::InitOnce()
{
	NameId.empty();
    CoInitialize(NULL);
}

void CDirectShow::ShutDown()
{
	for(int i = 0; i < NameId.size(); i++)
	{
		if(NameId[i].pMediaSeeking)
		{
			NameId[i].pMediaSeeking->Release();
			NameId[i].pMediaSeeking = NULL;
		}

		if(NameId[i].pBasicAudio)
		{
			NameId[i].pBasicAudio->Release();
			NameId[i].pBasicAudio = NULL;
		}

		if(NameId[i].pMediaControl)
		{
			NameId[i].pMediaControl->Stop();
			NameId[i].pMediaControl->Release();
			NameId[i].pMediaControl = NULL;
		}

		if(NameId[i].pVideoWindow)
		{
			NameId[i].pVideoWindow->put_Visible(OAFALSE);
			NameId[i].pVideoWindow->put_Owner(NULL);
			NameId[i].pVideoWindow->Release();
			NameId[i].pVideoWindow = NULL;
		}

		if(NameId[i].pMediaEvent)
		{
			NameId[i].pMediaEvent->Release();
			NameId[i].pMediaEvent = NULL;
		}

		if(NameId[i].pGraphBuilder)
		{
			NameId[i].pGraphBuilder->Release();
			NameId[i].pGraphBuilder = NULL;
		}

		if(NameId[i].pMediaPosition)
		{
            NameId[i].pMediaPosition->Release();
            NameId[i].pMediaPosition = NULL;
        }
	}

	CoUninitialize();
}

void CDirectShow::PlayFileMovie(int nID, HWND hWnd, bool fullscreen)
{	
	WCHAR wszName[UNLEN+1];
	
	MultiByteToWideChar(CP_ACP, 0, NameId[nID].name,
        strlen(NameId[nID].name)+1, wszName,   
     sizeof(wszName)/sizeof(wszName[0]) );

	//specify the owner window
	NameId[nID].pVideoWindow->put_Owner((OAHWND)hWnd);
	NameId[nID].pVideoWindow->put_WindowStyle(WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN);
	
	if(fullscreen)
		NameId[nID].pVideoWindow->put_FullScreenMode(OATRUE);

	//make the movie take up the entire window.
	RECT rect;
	GetClientRect(hWnd, &rect);
	NameId[nID].pVideoWindow->SetWindowPosition(rect.left, rect.top, rect.right, rect.bottom);

	//set the owner window to receive event notifications.
	NameId[nID].pMediaEvent->SetNotifyWindow((OAHWND)hWnd, WM_GRAPHNOTIFY, 0);

    // forward events (mouse clicks...) to the window hWnd
    NameId[nID].pVideoWindow->put_MessageDrain((OAHWND)hWnd);

    // hide the cursor when playing videos
    hideMouse(nID);

	//run the graph
	NameId[nID].pMediaControl->Run(); 
}

int CDirectShow::LoadFile(char *cpFileName, HWND hWnd)
{
	SNameId temp;

	//create the filter graph manager
	CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC, IID_IGraphBuilder,
		(void **)&temp.pGraphBuilder);

	temp.pGraphBuilder->QueryInterface(IID_IBasicAudio,	 (void**)&temp.pBasicAudio);
	temp.pGraphBuilder->QueryInterface(IID_IMediaSeeking, (void**)&temp.pMediaSeeking);
	temp.pGraphBuilder->QueryInterface(IID_IVideoWindow,  (void **)&temp.pVideoWindow);
	temp.pGraphBuilder->QueryInterface(IID_IMediaControl, (void **)&temp.pMediaControl);
	temp.pGraphBuilder->QueryInterface(IID_IMediaEvent,   (void**)&temp.pMediaEvent);
    temp.pGraphBuilder->QueryInterface(IID_IMediaPosition,   (void**)&temp.pMediaPosition);

	strcpy(temp.name, cpFileName);

	WCHAR wszName[UNLEN+1];
	
	MultiByteToWideChar(CP_ACP, 0, temp.name,
        strlen(temp.name)+1, wszName,   
     sizeof(wszName)/sizeof(wszName[0]) );
	
	//load this file and build a filter graph for it.
	HRESULT err = temp.pGraphBuilder->RenderFile(wszName, NULL);

    // if video cannot be rendered, return an invalid id
    if(err == S_OK)
    {	
    	NameId.push_back(temp);
	    return NameId.size() - 1;
    }
    else
    {
        return -1;
    }

}

void CDirectShow::HandleEvent(int nID)
{
	long evCode, param1, param2; 
	HRESULT result;

	//call getevent until it return a failure code, indicating that the queue is empty.
	while(result = NameId[nID].pMediaEvent->GetEvent(&evCode, &param1, &param2, 0), SUCCEEDED(result))
	{
		result = NameId[nID].pMediaEvent->FreeEventParams(evCode, param1, param2);

		//if the sample stops playing or is stopped by the user then call cleanup.
		if((evCode == EC_COMPLETE) || (evCode == EC_USERABORT))
		{
			ShutDown();
			break;
		}
	}
}

void CDirectShow::PlaySound(int nID)
{
	//run the graph
	NameId[nID].pMediaControl->Run();
}

void CDirectShow::RestartStream(int nID)
{
	Stop(nID);

	if(NameId[nID].pMediaSeeking)
	{
		LONGLONG llPos = 0;
		NameId[nID].pMediaSeeking->SetPositions(&llPos, AM_SEEKING_AbsolutePositioning, 
			&llPos, AM_SEEKING_NoPositioning);
	}
}

void CDirectShow::Stop(int nID)
{
	NameId[nID].pMediaControl->Stop();
}

void CDirectShow::Pause(int nID)
{
	NameId[nID].pMediaControl->Pause();
}

void CDirectShow::ResumeFromPause(int nID)
{
	NameId[nID].pMediaControl->Run();
}

void CDirectShow::Checkforwindow(int nID,HWND hWnd)
{
	//make the movie take up the entire window.
	RECT rect;
	if(GetClientRect(hWnd, &rect))
    {
        if(NameId[nID].pVideoWindow)
        {
            NameId[nID].pVideoWindow->SetWindowPosition(rect.left, rect.top, rect.right, rect.bottom);
        }
    }
}

void CDirectShow::ChangeWindow_Full(int nID)
{
	long *check;

	NameId[nID].pVideoWindow->get_FullScreenMode(check);
	if(check == (LONG*)OATRUE)
		NameId[nID].pVideoWindow->put_FullScreenMode(OAFALSE);
	else if(check == (LONG*)OAFALSE)
		NameId[nID].pVideoWindow->put_FullScreenMode(OATRUE);

}

bool CDirectShow::isFinished(int nID)
{
    REFTIME current = 0.0;
    REFTIME end = 0.0;

    NameId[nID].pMediaPosition->get_CurrentPosition(&current);
    NameId[nID].pMediaPosition->get_Duration(&end);

    if(current >= end)
        return true;
    else
        return false;

}

void CDirectShow::hideMouse(int nID)
{
    NameId[nID].pVideoWindow->HideCursor(OATRUE);
}

void CDirectShow::setVolume(int nID, long volume)
{
    NameId[nID].pBasicAudio->put_Volume(volume);
}
