//////////////////////////////////////////////////////////////////////////
// File: DirectShow.h	
//
// Author: Allen Danklefsen
//
// Creation Date: July 24th , 2004
//
// Purpose: 
//////////////////////////////////////////////////////////////////////////
#ifndef _DIRECT_SHOW_
#define _DIRECT_SHOW_

#include "Datatypes.h"

#include <dshow.h>
#include <lm.h>
#include <vector>

using namespace std;


class CDirectShow
{
   private:
	   //instance of the class.
	   static CDirectShow *pInstance;
      ////////////////////////////////////////////////////////////////////
      // Function: CDirectShow
      //
      // Last Modified:  AUgust 26, 04
      // Input: 
      // Output: 
      // Returns: 
      //
      // Purpose: Constructor for our class
      ////////////////////////////////////////////////////////////////////
      CDirectShow();
      ////////////////////////////////////////////////////////////////////
      // Function: CDirectShow
      //
      // Last Modified:  August 26, 2004
      // Input: CDirectShow&
      // Output: 
      // Returns: 
      //
      // Purpose: copy constructor
      ////////////////////////////////////////////////////////////////////
      CDirectShow(const CDirectShow&);
      //assignment operator.
      CDirectShow &operator=(const CDirectShow&);
      ////////////////////////////////////////////////////////////////////
      // Function: ~CDirectShow
      //
      // Last Modified:   August 26, 2004
      // Input: 
      // Output: 
      // Returns: 
      //
      // Purpose: Destructor
      ////////////////////////////////////////////////////////////////////
      ~CDirectShow();
   protected:
	   struct SNameId
	   {
		   IGraphBuilder *pGraphBuilder;
		   IMediaControl *pMediaControl;
           IMediaPosition *pMediaPosition;
		   IVideoWindow  *pVideoWindow;
		   IMediaEventEx *pMediaEvent;
		   IBasicAudio   *pBasicAudio;
		   IMediaSeeking *pMediaSeeking;
		   char name[256];
	   };
	   vector<SNameId> NameId;
   public:
      ////////////////////////////////////////////////////////////////////
      // Function: DeleteInstance
      //
      // Last Modified:  Aug 26, 2004
      // Input: 
      // Output: 
      // Returns: 
      //
      // Purpose: Delete the memory used
      ////////////////////////////////////////////////////////////////////
      static void DeleteInstance(void);

      ////////////////////////////////////////////////////////////////////
      // Function: GetInstance
      //
      // Last Modified:  Aug 26, 2004
      // Input: 
      // Output: 
      // Returns: 
      //
      // Purpose: return the instance of our class
      ////////////////////////////////////////////////////////////////////
      static CDirectShow *GetInstance(void);

      ////////////////////////////////////////////////////////////////////
      // Function: InitOnce
      //
      // Last Modified:  Aug , 26, 2004 
      // Input: 
      // Output: 
      // Returns: 
      //
      // Purpose: Init some variables up for us once
      ////////////////////////////////////////////////////////////////////
      void InitOnce();

      ////////////////////////////////////////////////////////////////////
      // Function: Shutdown
      //
      // Last Modified:  Aug 26, 2004
      // Input: 
      // Output: 
      // Returns: 
      //
      // Purpose: Shutdown all things created for dx
      ////////////////////////////////////////////////////////////////////
      void ShutDown();

      ////////////////////////////////////////////////////////////////////
      // Function: PlayFileMovie
      //
      // Last Modified:  
      // Input: id - to the file to play, hWnd - window handle
      // Output: A played file is going
      // Returns: 
      //
      // Purpose: Load and Plays a file
      ////////////////////////////////////////////////////////////////////
      void PlayFileMovie(int nID, HWND hWnd, bool fullscreen);

      ////////////////////////////////////////////////////////////////////
      // Function: LoadFile
      //
      // Last Modified:  
      // Input: cpFileName - filename, hWnd - window handle
      // Output: A played file is going
      // Returns: 
      //
      // Purpose: load a file to an id number
      ////////////////////////////////////////////////////////////////////
      int LoadFile(char *cpFileName, HWND hWnd);

      ////////////////////////////////////////////////////////////////////
      // Function: HandleEvent
      //
      // Last Modified:  
      // Input: none
      // Output: The handled message
      // Returns: 
      //
      // Purpose: 
      ////////////////////////////////////////////////////////////////////
      void HandleEvent(int nID);

      ////////////////////////////////////////////////////////////////////
      // Function: PlaySound
      //
      // Last Modified:  
      // Input: none
      // Output: You should start hearing sounds
      // Returns: 
      //
      // Purpose: play a sound / music / etc
      ////////////////////////////////////////////////////////////////////
      void PlaySound(int nID);

      ////////////////////////////////////////////////////////////////////
      // Function: RestartStream
      //
      // Last Modified:  
      // Input: id
      // Output: 
      // Returns: 
      //
      // Purpose: Resets the stream starting point 
      ////////////////////////////////////////////////////////////////////
      void RestartStream(int nID);

      ////////////////////////////////////////////////////////////////////
      // Function:Stop
      //
      // Last Modified: August 26, 04
      // Input:int nID
      // OutPut:
      // Returns:
      //
      // Purpose:stop the movie or music
      ////////////////////////////////////////////////////////////////////
      void Stop(int nID);

      ////////////////////////////////////////////////////////////////////
      // Function: Pause
      //
      // Last Modified:August 26, 04
      // Input:int nID
      // OutPut:
      // Returns:
      //
      // Purpose: pause the movie
      ////////////////////////////////////////////////////////////////////
      void Pause(int nID);

      ////////////////////////////////////////////////////////////////////
      // Function:ResumeFromPause
      //
      // Last Modified:August 26, 04
      // Input:int nID
      // OutPut:
      // Returns:
      //
      // Purpose: REsume the id from pause
      ////////////////////////////////////////////////////////////////////
      void ResumeFromPause(int nID);

      ////////////////////////////////////////////////////////////////////
      // Function: Checkforwindow
      //
      // Last Modified:August 26, 04
      // Input:int nID,HWND hWnd
      // OutPut:
      // Returns:
      //
      // Purpose: see if the window is big or small
      ////////////////////////////////////////////////////////////////////
      void Checkforwindow(int nID,HWND hWnd);

      ////////////////////////////////////////////////////////////////////
      // Function:ChangeWindow_Full
      //
      // Last Modified: August 26, 2004
      // Input: id to change in the vector
      // OutPut:
      // Returns:
      //
      // Purpose: if the user maximizes window make the movie big also
      ////////////////////////////////////////////////////////////////////
      void ChangeWindow_Full(int nID);

      bool isFinished(int nID);

      void hideMouse(int nID);

      void setVolume(int nID, long volume);
};

#endif