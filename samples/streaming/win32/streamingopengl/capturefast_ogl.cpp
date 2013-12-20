#include "stdafx.h"

#include "capturefast_ogl.h"
#include "../../wavemesh.h"
#include "../../streaming.h"

#include <gl/glew.h>
#include <gl/gl.h>
#include <gl/glu.h>


#define NUM_CAPTURE_SURFACES 4

extern unsigned int gBroadcastWidth;
extern unsigned int gBroadcastHeight;

static unsigned int gWindowWidth = 0;
static unsigned int gWindowHeight = 0;

static GLuint gSceneFBO = 0;
static GLuint gSceneTexture = 0;
static GLuint gDepthBuffer = 0;

static GLuint gCapturePBO[NUM_CAPTURE_SURFACES] = { 0, 0, 0, 0 };
static GLsync gCaptureSync[NUM_CAPTURE_SURFACES] = { 0, 0, 0, 0};

static GLuint gResizeFBO = 0;
static GLuint gResizeTexture = 0;

static int gCaptureWidth = 0;			// The desired width of the output buffer.
static int gCaptureHeight = 0;			// The desired height of the output buffer.
static unsigned int gCapturePut = 0;	// The current request to render the resized texture to the destination render target.
static unsigned int gCaptureGet = 0;	// The current request for the pixel data.

struct Rect
{
	int left, right;
	int top, bottom;
};

/**
 * Create texture for render target.
 */
static void CreateRenderTexture( GLuint& texid, int width, int height)
{
	glGenTextures(1, &texid);

	glBindTexture(GL_TEXTURE_2D, texid);			

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);			

	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 
				  width, height, 0, 
				  GL_RGBA, GL_UNSIGNED_BYTE, 0);

	glBindTexture(GL_TEXTURE_2D, 0);	
}


/**
 * Initializes the render method.
 */
void InitRendering_Fast(unsigned int windowWidth, unsigned int windowHeight, unsigned int broadcastWidth, unsigned int broadcastHeight)
{
	DeinitRendering_Fast();

	gWindowWidth = windowWidth;
	gWindowHeight = windowHeight;

	glGenFramebuffers(1, &gSceneFBO);

	glBindFramebuffer(GL_FRAMEBUFFER, gSceneFBO);

	// create render target
	CreateRenderTexture( gSceneTexture, windowWidth, windowHeight );

	// bind render target
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gSceneTexture, 0);

	// create depth buffer
	{
		glGenRenderbuffers(1, &gDepthBuffer);
		glBindRenderbuffer(GL_RENDERBUFFER, gDepthBuffer);
		glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, windowWidth, windowHeight);
		glBindRenderbuffer(GL_RENDERBUFFER, 0);
	}

	// bind depth buffer
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, gDepthBuffer);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


/**
 * Cleans up the render method.
 */
void DeinitRendering_Fast()
{
	if ( gSceneFBO != 0 )
	{
		glDeleteFramebuffers(1, &gSceneFBO);
		gSceneFBO = 0;
	}

	if ( gSceneTexture != 0 )
	{
		glDeleteTextures(1, &gSceneTexture);
		gSceneTexture = 0;
	}

	if ( gDepthBuffer != 0 )
	{
		glDeleteRenderbuffers(1, &gDepthBuffer);
		gDepthBuffer = 0;
	}
	
	if ( gResizeFBO != 0 )
	{
		glDeleteFramebuffers(1, &gResizeFBO);
		gResizeFBO = 0;
	}	

	if ( gResizeTexture != 0 )
	{
		glDeleteTextures(1, &gResizeTexture);
		gResizeTexture = 0;
	}	

	for ( int i = 0; i < NUM_CAPTURE_SURFACES; i++)
	{
		if ( gCapturePBO[i] != 0 )
		{
			glDeleteBuffers(1, &gCapturePBO[i]);
			gCapturePBO[i] = 0;
		}
	}	

	for ( int i = 0; i < NUM_CAPTURE_SURFACES; i++)
	{
		if ( gCaptureSync[i] != 0 )
		{
			glDeleteSync( gCaptureSync[i] );
			gCaptureSync[i] = 0;
		}
	}
}


/**
 * Renders the scene to a texture.
 */
static void RenderOffscreen()
{
	glBindFramebuffer(GL_FRAMEBUFFER, gSceneFBO);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	DrawWaveMesh();

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


/**
 * Renders the scene texture to the screen.
 */
static void RenderToScreen()
{
	                     /*pos*/  /*texcoord*/
	const float quad[] = { 0,1,    0,1,
	                       0,0,    0,0,
	                       1,1,    1,1,
	                       1,0,    1,0 };

	glDepthMask(false);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);

	// setup matrix
	{
		// set modeview to identity
		glPushMatrix();
		glLoadIdentity();

		// set 2D projection
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		gluOrtho2D(0,1,0,1);
	}
		
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gSceneTexture);

	glVertexPointer(  2, GL_FLOAT, sizeof(float)*4, quad);
	glTexCoordPointer(2, GL_FLOAT, sizeof(float)*4, quad+2);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glBindTexture(GL_TEXTURE_2D, 0);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDepthMask(true);

	// restore matrix
	{
		// restore projection
		glPopMatrix();

		// restore modelview
	    glMatrixMode(GL_MODELVIEW);	
		glPopMatrix();
	}	
}


/**
 * Renders the scene using the render method.
 */
void RenderScene_Fast()
{
	glDisable(GL_LIGHTING);
		
    RenderOffscreen();

	RenderToScreen();
}


/**
 * Create resize FBO.
 */
static void CreateResizeFBO( int captureWidth, int captureHeight )
{
	if ( gResizeFBO != 0 )
	{
		glDeleteFramebuffers(1, &gResizeFBO);
		gResizeFBO = 0;
	}

	if ( gResizeTexture != 0 )
	{
		glDeleteTextures(1, &gResizeTexture);
		gResizeTexture = 0;
	}

	glGenFramebuffers(1, &gResizeFBO);

	glBindFramebuffer(GL_FRAMEBUFFER, gResizeFBO);
				
	CreateRenderTexture( gResizeTexture, captureWidth, captureHeight );

	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gResizeTexture, 0);		

	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


/**
 * Create capture PBO for reading from resize FBO.
 */
static void CreateCapturePBO( int captureWidth, int captureHeight )
{
	for ( int i = 0; i < NUM_CAPTURE_SURFACES; i++ )
	{
		if ( gCapturePBO[i] != 0 )
		{
			glDeleteBuffers(1, &gCapturePBO[i]);
			gCapturePBO[i] = 0;
		}

		glGenBuffers(1, &gCapturePBO[i]);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, gCapturePBO[i]);
		glBufferData(GL_PIXEL_PACK_BUFFER, captureWidth*captureHeight*4, 0, GL_STREAM_READ);
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
	}	
}

/**
 * Renders the scene texture to resize FBO and read asynchronously from resize FBO to capture PBO.
 */
static void CaptureFrame( int captureWidth, int captureHeight )
{
	if (gCapturePut - gCaptureGet >= NUM_CAPTURE_SURFACES)
	    return;

	// Stretch and copy the image to the correct area of the destination (black-bordering if necessary)
	float captureAspect = (float)captureHeight / (float)captureWidth;
	float srcAspect = (float)gWindowHeight / (float)gWindowWidth;

	Rect destRect;

	// Determine the destination rectangle
	if (captureAspect >= srcAspect)
	{
		float scale = (float)captureWidth / (float)gWindowWidth;

		destRect.left = 0;
		destRect.right = captureWidth-1;
		destRect.top = (int)( ((float)captureHeight - (float)gWindowHeight*scale) / 2 );
		destRect.bottom = (int)( ((float)captureHeight + (float)gWindowHeight*scale) / 2 );
	}
	else
	{
		float scale = (float)captureHeight / (float)gWindowHeight;

		destRect.top = 0;
		destRect.bottom = captureHeight-1;
		destRect.left = (int)( ((float)captureWidth - (float)gWindowWidth*scale) / 2 );
		destRect.right = (int)( ((float)captureWidth + (float)gWindowWidth*scale) / 2 );
	}

	// render to resize FBO
    glBindFramebuffer(GL_FRAMEBUFFER, gResizeFBO);
	
	// push viewport & clear color
	glPushAttrib(GL_VIEWPORT_BIT | GL_COLOR_BUFFER_BIT);

	glClearColor(0,0,0,0);
	glClear(GL_COLOR_BUFFER_BIT);
	
	// set viewport to dest rect
	glViewport( destRect.left, destRect.top, 
				destRect.right - destRect.left + 1, 
				destRect.bottom - destRect.top + 1);

    // invert texcoord because opengl's backbuffer starts from bottom
                          /*pos*/  /*texcoord*/
	const float quad[] = { 0,1,    0,0,
	                       0,0,    0,1,
	                       1,1,    1,0,
	                       1,0,    1,1 };

	glDepthMask(false);
	glDisable(GL_DEPTH_TEST);
	glDisable(GL_ALPHA_TEST);
	glDisable(GL_BLEND);

	// setup matrix
	{
		// set modeview to identity
		glPushMatrix();
		glLoadIdentity();

		// set 2D projection
		glMatrixMode(GL_PROJECTION);
		glPushMatrix();
		glLoadIdentity();
		gluOrtho2D(0,1,0,1);
	}
		
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);

	glEnable(GL_TEXTURE_2D);
	glBindTexture(GL_TEXTURE_2D, gSceneTexture);

	glVertexPointer(  2, GL_FLOAT, sizeof(float)*4, quad);
	glTexCoordPointer(2, GL_FLOAT, sizeof(float)*4, quad+2);

	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	glBindTexture(GL_TEXTURE_2D, 0);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glDepthMask(true);

	// restore matrix
	{
		// restore projection
		glPopMatrix();

		// restore modelview
	    glMatrixMode(GL_MODELVIEW);	
		glPopMatrix();
	}	

	// restor viewport & clear color
	glPopAttrib();


	// asynchronously read resize FBO to capture PBO
	int idx = gCapturePut % NUM_CAPTURE_SURFACES;
	++gCapturePut;	

	glBindBuffer(GL_PIXEL_PACK_BUFFER, gCapturePBO[idx]);
	
	glReadPixels(0, 0, captureWidth, captureHeight, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

	// insert a sync
	{
		if ( gCaptureSync[idx] != 0 )
			glDeleteSync(gCaptureSync[idx]);

		gCaptureSync[idx] = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
	}	

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

	glBindFramebuffer(GL_FRAMEBUFFER, 0);	
}

/**
 * Try to read the capture PBO, if its not ready yet then returns.
 */
static bool ReadCapturePBO(int captureWidth, int captureHeight, unsigned char*& outRgbaFrame, int& outWidth, int& outHeight)
{	
	int idx = gCaptureGet % NUM_CAPTURE_SURFACES;

	if ( gCaptureGet == gCapturePut || 
		 (gCaptureSync[idx] != 0 && glClientWaitSync(gCaptureSync[idx], 0, 0) ==  GL_TIMEOUT_EXPIRED) )
	{
		return false;
	}
	
	// lock PBO for reading
	glBindBuffer(GL_PIXEL_PACK_BUFFER, gCapturePBO[idx]);

	void* buffer = glMapBuffer(GL_PIXEL_PACK_BUFFER , GL_READ_ONLY);
	if ( buffer == nullptr )
	{
		ReportError("Error locking PBO");
		return false;
	}

	// Grab the free buffer from the streaming pool
	outRgbaFrame = GetNextFreeBuffer();

	if ( outRgbaFrame != nullptr )
	{
		memcpy(outRgbaFrame, buffer, captureWidth*captureHeight*4);
	}
	
	GLboolean success;
	success = glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
	if ( !success )
	{
		ReportError("Error unlocking PBO");
		glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
		return false;
	}

	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
			
	++gCaptureGet;

	outWidth = captureWidth;
	outHeight = captureHeight;

	return true;
		
}


/**
 * Captures the frame asynchronously in RGBA format from the current scene render target.  Since it is asynchronous, the buffer will be returned when a 
 * request from a previous call is ready.  Because it is asynchronous it blocks as little as possible to ensure there is a 
 * minimal hit to the rendering pipeline.
 *
 * This implementation black-boxes the captured buffer in case the broadcast aspect ratio and game aspect ratio do not match.
 */
bool CaptureFrame_Fast(int captureWidth, int captureHeight, unsigned char*& outRgbaFrame, int& outWidth, int& outHeight)
{
	// Clear the outputs until we have confirmed a capture
	outRgbaFrame = nullptr;
	outWidth = 0;
	outHeight = 0;

	// Check for valid parameters
	if ( gSceneFBO == 0 || 
		 captureWidth <= 0 || 
		 captureHeight <= 0 ||
		 captureWidth % 16 != 0 ||
		 captureHeight % 16 != 0)
	{
		return false;
	}

	// Cancel all outstanding captures and re-allocate the capture textures
	if ( gResizeFBO == 0 ||
		 captureWidth != gCaptureWidth || 
		 captureHeight != gCaptureHeight)
	{
		// recreate resize FBO
		CreateResizeFBO(captureWidth, captureHeight);

		// recreate capture PBO
		CreateCapturePBO(captureWidth, captureHeight);		

		gCaptureGet = 0;
		gCapturePut = 0;

		gCaptureWidth = captureWidth;
		gCaptureHeight = captureHeight;
	}

	// capture frame asynchronously
	CaptureFrame( captureWidth, captureHeight );

	// try to read from capture PBO
	return ReadCapturePBO(captureWidth, captureHeight, outRgbaFrame, outWidth, outHeight);
}
