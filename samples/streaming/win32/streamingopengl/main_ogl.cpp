//////////////////////////////////////////////////////////////////////////////
// This module contains the main entry point and code specific to Direct3D9.
//////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "../../wavemesh.h"
#include "../../streaming.h"
#include "captureslow_ogl.h"
#include "capturefast_ogl.h"

#include <twitchsdk.h>

#include <gl/glew.h>
#include <gl/glu.h>
#include <gl/glu.h>

#include <GLFW/glfw3.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3native.h>


/**
 * List of possible capture methods used by the sample.
 */
namespace CaptureMethod
{
	enum Enum
	{
		Slow,
		Fast,
	};
}


#pragma region Global Variables


GLFWwindow* gWindow = nullptr;

CaptureMethod::Enum gCaptureMethod = CaptureMethod::Slow;	// The capture method to use for the sample.
char gWindowTitle[128];									// The title bar text

float gRenderFramesPerSecond = 60;							// The number of frames per second to render, 0 if no throttling.
unsigned __int64 gLastFrameTime = 0;						// The last time a frame was rendered.
unsigned __int64 gLastCaptureTime = 0;									// The timestamp of the last frame capture.

// 360p widescreen is 640x360
// 480p widescreen is about 853x480
unsigned int gBroadcastFramesPerSecond = 30;				// The broadcast frames per second.
unsigned int gBroadcastWidth = 640;							// The broadcast width in pixels.
unsigned int gBroadcastHeight = 368;						// The broadcast height in pixels.

unsigned int gWindowWidth = 1024;							// The width of the window.
unsigned int gWindowHeight = 768;							// The height of the window.

unsigned int gWindowFrameBufferWidth  = 0;					// The width of the window framebuffer, will be set at runtime.
unsigned int gWindowFrameBufferHeight = 0;					// The height of the window framebuffer, will be set at runtime.

unsigned int gFullscreen = false;							// Whether or not the app should be fullscreen.

bool gStreamingDesired = true;								// Whether or not the app wants to stream.
bool gPaused = false;										// Whether or not the streaming is paused.
bool gReinitializeRequired = true;							// Whether the device requires reinitialization.

float gCameraFlySpeed = 100.0f;								// The number of units per second to move the camera.
float gCameraRotateSpeed = 90.0f;							// The number of degrees to rotate per second.

struct Point
{
	int x,y;
};
Point gLastMousePos;										// Cached mouse position for calculating deltas.

float gViewMatrix[16];										// The camera view matrix.
float gProjectionMatrix[16];								// The scene projection matrix.

#pragma endregion


#pragma region Forward Declarations

void SetWindowCallbacks();

#pragma endregion


#pragma region Timer Functions

unsigned __int64 GetSystemClockFrequency()
{	
	static unsigned __int64 frequency = 0;
	if (frequency == 0)
	{		
		QueryPerformanceFrequency( reinterpret_cast<LARGE_INTEGER*>(&frequency) );		
	}

	return frequency;
}

unsigned __int64 GetSystemClockTime()
{
	unsigned __int64 counter;
	QueryPerformanceCounter( reinterpret_cast<LARGE_INTEGER*>(&counter) );
	return counter;
}

unsigned __int64 SystemTimeToMs(unsigned __int64 sysTime)
{
	return sysTime * 1000 / GetSystemClockFrequency();
}

/**
 * Determines the current system time in milliseconds.
 */
unsigned __int64 GetSystemTimeMs()
{
	return SystemTimeToMs( GetSystemClockTime() );
}

#pragma endregion


/**
 * Prints the error to the console and shows a message box.
 */
void ReportError(const char* format, ...)
{
	char buffer[256];
	va_list args;
	va_start(args, format);
	vsprintf_s(buffer, sizeof(buffer), format, args);
	perror(buffer);
	va_end(args);
 
	OutputDebugStringA(buffer);
	printf("%s\n", buffer);
	MessageBoxA( glfwGetWin32Window(gWindow), buffer, "Error", MB_OK);	
}


/**
 * Determines the directory that the intel encoder DLL is located.
 */
std::wstring GetIntelDllPath()
{
	return std::wstring(L".\\");
}


/**
 * Resets the view to the default.
 */
void ResetView()
{
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glGetFloatv(GL_MODELVIEW_MATRIX, gViewMatrix);
}


/**
 * Handles user input.
 */
void HandleInput()
{
	float timeDelta = (GetSystemTimeMs() - gLastFrameTime) / 1000.0f;

	// handle camera rotation
	if ( glfwGetMouseButton(gWindow, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS )
	{
		Point last = gLastMousePos;

		double mx, my;
		glfwGetCursorPos( gWindow, &mx, &my );
		gLastMousePos.x = (int)mx;
		gLastMousePos.y = (int)my;		

		const float dampening = 10.0f;
		float dx = (gLastMousePos.x - last.x) / dampening;
		float dy = (gLastMousePos.y - last.y) / dampening;

		if (dx != 0)
		{
			glLoadIdentity();
			glRotatef( dx * timeDelta * gCameraRotateSpeed, 0, 1, 0 );			
			glMultMatrixf(gViewMatrix);

			glGetFloatv( GL_MODELVIEW_MATRIX, gViewMatrix);
		}

		if (dy != 0)
		{
			glLoadIdentity();
			glRotatef( dy * timeDelta * gCameraRotateSpeed, 1, 0, 0 );			
			glMultMatrixf(gViewMatrix);

			glGetFloatv( GL_MODELVIEW_MATRIX, gViewMatrix);
		}
	}

	// handle camera fly through
	float x = 0;
	float y = 0;
	float z = 0;
	if ( glfwGetKey(gWindow, GLFW_KEY_A) == GLFW_PRESS )
	{
		x += gCameraFlySpeed * timeDelta;
	}
	if ( glfwGetKey(gWindow, GLFW_KEY_D) == GLFW_PRESS )
	{
		x -= gCameraFlySpeed * timeDelta;
	}
	if ( glfwGetKey(gWindow, GLFW_KEY_E) == GLFW_PRESS )
	{
		y -= gCameraFlySpeed * timeDelta;
	}
	if ( glfwGetKey(gWindow, GLFW_KEY_Q) == GLFW_PRESS )
	{
		y += gCameraFlySpeed * timeDelta;
	}
    if ( glfwGetKey(gWindow, GLFW_KEY_W) == GLFW_PRESS )
	{
		z += gCameraFlySpeed * timeDelta;
	}
	if ( glfwGetKey(gWindow, GLFW_KEY_S) == GLFW_PRESS ) 
 	{
		z -= gCameraFlySpeed * timeDelta;
	}

	glLoadIdentity();
	glTranslatef(x,y,z);
	glMultMatrixf(gViewMatrix);
	glGetFloatv(GL_MODELVIEW_MATRIX, gViewMatrix);

	// Reset the view
	if ( glfwGetKey(gWindow, GLFW_KEY_R) == GLFW_PRESS )
	{
		ResetView();
	}

	// Get the latest mouse position
	double mx, my;
	glfwGetCursorPos( gWindow, &mx, &my );
	gLastMousePos.x = (int)mx;
	gLastMousePos.y = (int)my;		

	// play a commercial
    if ( glfwGetKey(gWindow, GLFW_KEY_P) == GLFW_PRESS )
	{
		RunCommercial();
	}
}


/**
 * Retrieves the size of the screen.
 */
void GetScreenSize(unsigned int& width, unsigned int& height)
{
	const GLFWvidmode* vidMod = glfwGetVideoMode( glfwGetPrimaryMonitor() );
	
	width = vidMod->width;
	height = vidMod->height;	
}

/**
 * Init window and make opengl context current
 */
bool InitWindow()
{
	if ( gWindow )
	{
		glfwDestroyWindow( gWindow );
		gWindow = nullptr;
	}
	
	glfwWindowHint(GLFW_RESIZABLE, false);
	glfwWindowHint(GLFW_VISIBLE, true);

	if (gFullscreen)
	{
		GetScreenSize(gWindowWidth, gWindowHeight);

		gWindow = glfwCreateWindow( gWindowWidth, gWindowHeight, gWindowTitle, glfwGetPrimaryMonitor(), nullptr);
	}
	else
	{
		gWindowWidth = 1024;
		gWindowHeight = 768;

		gWindow = glfwCreateWindow( gWindowWidth, gWindowHeight, gWindowTitle, nullptr, nullptr);
	}

	if ( !gWindow )
		return false;

	// get frame buffer size, may bot be the same as window size
	int frameWidth, frameHeight;
	glfwGetFramebufferSize(gWindow, &frameWidth, &frameHeight);

	gWindowFrameBufferWidth = frameWidth;
	gWindowFrameBufferHeight = frameHeight;
	
	// set window callbacks
	SetWindowCallbacks();

	// bind opengl context
	glfwMakeContextCurrent(gWindow);

	// init glew, must be called after 	glfwMakeContextCurrent
	glewInit();

	return true;
}


/**
 * Initializes the rendering using the appropriate rendering method.
 */
bool InitializeRendering()
{
	InitWindow();

	DestroyWaveMesh();
	
	switch (gCaptureMethod)
	{
	case CaptureMethod::Slow:
		DeinitRendering_Slow();
		break;
	case CaptureMethod::Fast:
		DeinitRendering_Fast();
		break;
	}		

		
	switch (gCaptureMethod)
	{
	case CaptureMethod::Slow:
		InitRendering_Slow(gWindowFrameBufferWidth, gWindowFrameBufferHeight);
		break;
	case CaptureMethod::Fast:
		InitRendering_Fast(gWindowFrameBufferWidth, gWindowFrameBufferHeight, gBroadcastWidth, gBroadcastHeight);
		break;
	}

	glClearColor(0,0,1,1);
	
	glViewport(0, 0, gWindowFrameBufferWidth, gWindowFrameBufferHeight);

	// Setup the projection matrix
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60, (float)gWindowFrameBufferWidth/(float)gWindowFrameBufferHeight, 1, 1000);

	glMatrixMode(GL_MODELVIEW);

	// Create the mesh that will be rendered
	CreateWaveMesh(64, 20);

	return true;
}


/**
 * Render the scene using the appropriate rendering method.
 */
void RenderScene()
{	
	switch (gCaptureMethod)
	{
	case CaptureMethod::Slow:
		RenderScene_Slow();
		break;
	case CaptureMethod::Fast:
		RenderScene_Fast();
		break;
	}

	glfwSwapBuffers(gWindow);
}


/**
 * Callback function for key event.
 */
void KeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	if ( action != GLFW_PRESS )
		return;

	switch ( key )
	{
		case GLFW_KEY_F1:
		{
			bool streaming = IsStreaming();
			if (streaming)
			{
				StopStreaming();
			}

			if (gBroadcastWidth == 640)
			{
				gBroadcastWidth = 1024;
				gBroadcastHeight = 768;
			}
			else
			{
				gBroadcastWidth = 640;
				gBroadcastHeight = 368;
			}

			if (streaming)
			{
				StartStreaming(gBroadcastWidth, gBroadcastHeight, gBroadcastFramesPerSecond, TTV_PF_RGBA);
			}
			break;
		}

		case GLFW_KEY_F5:
		{
			if (IsStreaming())
			{
				gStreamingDesired = false;
				StopStreaming();
			}
			else
			{
				gStreamingDesired = true;
				StartStreaming(gBroadcastWidth, gBroadcastHeight, gBroadcastFramesPerSecond, TTV_PF_RGBA);
			}
			break;
		}
	
		case GLFW_KEY_F12:
		{
			gFullscreen = !gFullscreen;
			gReinitializeRequired = true;
			break;
		}

		default:
			break;
	}
}


/**
 * Callback function for repaint event.
 */
void RepaintCallback( GLFWwindow* window )
{
	if (!gReinitializeRequired)
	{
		RenderScene();
	}
}


/**
 * Callback function for iconify event.
 * Pause streaming when minimized since the back buffer might not be available.
 */
void IconifyCallback( GLFWwindow* window, int iconified )
{
	// Update the pause state
	if (iconified)
	{
		gPaused = true;
		Pause();
	}
	else
	{
		gPaused = false;
	}
}

/**
 * Set all window callback functions.
 */
void SetWindowCallbacks()
{
	glfwSetKeyCallback( gWindow, KeyCallback );

	glfwSetWindowRefreshCallback( gWindow, RepaintCallback );

	glfwSetWindowIconifyCallback( gWindow,  IconifyCallback);
}

/**
 * main function
 */
int main(void)
{
    if ( glfwInit() == false )
        return -1;
	
	InitializeRendering();
	gReinitializeRequired = false;
		
	// Set the view to the default position
	ResetView();

	// Cache the last mouse position
	double mx,my;
	glfwGetCursorPos(gWindow, &mx, &my);
	gLastMousePos.x = (int)mx;
	gLastMousePos.y = (int)my;
		
	// Initialize the Twitch SDK
	InitializeStreaming("<username>", "<password>", "<clientId>", "<clientSecret>", GetIntelDllPath());
    
	while ( !glfwWindowShouldClose(gWindow) )
    {
		if (gReinitializeRequired)
		{
			gReinitializeRequired = false;
			InitializeRendering();
		}
		
		RenderScene();
		
		UpdateWaveMesh();
		
		// Process user input independent of the event queue
		if ( glfwGetWindowAttrib(gWindow, GLFW_FOCUSED) )
		{
			HandleInput();
		}
		
		// Record the frame time
		unsigned __int64 curTime = GetSystemTimeMs();
		
		// Begin streaming when ready
		if (gStreamingDesired && 
			!IsStreaming() &&
			IsReadyToStream())
		{
			StartStreaming(gBroadcastWidth, gBroadcastHeight, gBroadcastFramesPerSecond, TTV_PF_RGBA);

			gLastCaptureTime = 0;
		}

		// If you send frames too quickly to the SDK (based on the broadcast FPS you configured) it will not be able 
		// to make use of them all.  In that case, it will simply release buffers without using them which means the
		// game wasted time doing the capture.  To mitigate this, the app should pace the captures to the broadcast FPS.
		unsigned __int64 captureDelta = curTime - gLastCaptureTime;
		bool isTimeForNextCapture = (captureDelta / 1000.0) >= (1.0 / gBroadcastFramesPerSecond);

		// streaming is in progress so try and capture a frame
		if (IsStreaming() && 
			!gPaused &&
			isTimeForNextCapture)
		{
			// capture a snapshot of the back buffer
			unsigned char* pBgraFrame = nullptr;
			int width = 0;
			int height = 0;
			bool gotFrame = false;

			switch (gCaptureMethod)
			{
			case CaptureMethod::Slow:
				gotFrame = CaptureFrame_Slow(gBroadcastWidth, gBroadcastHeight, pBgraFrame);
				break;
			case CaptureMethod::Fast:
				gotFrame = CaptureFrame_Fast(gBroadcastWidth, gBroadcastHeight, pBgraFrame, width, height);
				break;
			}

			// send a frame to the stream
			if (gotFrame)
			{
				SubmitFrame(pBgraFrame);
			}
		}

		// The SDK may generate events that need to be handled by the main thread so we should handle them
		FlushStreamingEvents();		

		unsigned __int64 timePerFrame = curTime - gLastFrameTime;
		unsigned int fps = 0;
		if (timePerFrame > 0)
		{
			fps = static_cast<int>(1000 / timePerFrame);
		}
		gLastFrameTime = curTime;

		// Update the window title to show the state
		#undef STREAM_STATE
		#define STREAM_STATE(__state__) #__state__,

		char buffer[128];
		const char* streamStates[] = 
		{
			STREAM_STATE_LIST
		};
		#undef STREAM_STATE

		sprintf_s(buffer, sizeof(buffer), "Twitch OpenGL Streaming Sample - %s - %s    FPS=%d", GetUsername().c_str(), streamStates[GetStreamState()], fps);		
		glfwSetWindowTitle(gWindow, buffer);		

		// poll window events
        glfwPollEvents();   
	}

	// Shutdown the Twitch SDK
	StopStreaming();
	ShutdownStreaming();

	// Cleanup the rendering method
	switch (gCaptureMethod)
	{
	case CaptureMethod::Slow:
		DeinitRendering_Slow();
		break;
	case CaptureMethod::Fast:
		DeinitRendering_Fast();
		break;
	}

	// Cleanup the mesh
	DestroyWaveMesh();
	
	// glfw cleanup 
    glfwTerminate();

    return 0;
}