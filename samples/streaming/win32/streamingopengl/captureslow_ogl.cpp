#include "stdafx.h"

#include "captureslow_ogl.h"
#include "../../wavemesh.h"
#include "../../streaming.h"

#include <gl/glew.h>
#include <gl/gl.h>
#include <gl/glu.h>


static GLuint gSceneFBO = 0;
static GLuint gSceneTexture = 0;
static GLuint gDepthBuffer = 0;

static GLuint gResizeFBO = 0;
static GLuint gResizeTexture = 0;

static GLuint gCapturePBO = 0;

static unsigned int gWindowWidth = 0;
static unsigned int gWindowHeight = 0;
static unsigned int gBroadcastWidth = 0;
static unsigned int gBroadcastHeight = 0;

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
void InitRendering_Slow(unsigned int windowWidth, unsigned int windowHeight)
{
	DeinitRendering_Slow();

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
void DeinitRendering_Slow()
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

	if ( gCapturePBO != 0 )
	{
		glDeleteBuffers(1, &gCapturePBO);
		gCapturePBO = 0;
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
void RenderScene_Slow()
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
	if ( gCapturePBO != 0 )
	{
		glDeleteBuffers(1, &gCapturePBO);
		gCapturePBO = 0;
	}

	glGenBuffers(1, &gCapturePBO);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, gCapturePBO);
	glBufferData(GL_PIXEL_PACK_BUFFER, captureWidth*captureHeight*4, 0, GL_STREAM_READ);
	glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

/**
 * Renders to resize FBO
 */
static void RenderToResizeFBO( int captureWidth, int captureHeight )
{
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

	glBindFramebuffer(GL_FRAMEBUFFER, 0);

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
}

/**
 * Captures the frame in RGBA format from the backbuffer.  This method works but is really slow because it locks the backbuffer
 * during the copy and prevents any furthur rendering until the copy is complete.  It also locks the backbuffer surface 
 * immediately after requesting the render target data which is bad because it doesn't give the GPU time to prepare the data
 * for locking asynchronously.
 */
bool CaptureFrame_Slow(int captureWidth, int captureHeight, unsigned char*& outRgbaFrame)
{
	// Create/recreate the target textures and surfaces if needed
	if (gResizeFBO == 0 || captureHeight != gBroadcastHeight || captureWidth != gBroadcastWidth)		
	{
		// recreate resize FBO
		CreateResizeFBO(captureWidth, captureHeight);

		// recreate capture PBO
		CreateCapturePBO(captureWidth, captureHeight);

		gBroadcastHeight = captureHeight;
		gBroadcastWidth = captureWidth;
	}

	// strect to resize FBO
	RenderToResizeFBO(captureWidth, captureHeight);

	// read resize FBO to capture PBO
	{
		glBindFramebuffer(GL_FRAMEBUFFER, gResizeFBO);

		glBindBuffer(GL_PIXEL_PACK_BUFFER, gCapturePBO);
	
		glReadPixels(0, 0, captureWidth, captureHeight, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
	}	

	// lock PBO for reading
	void* buffer = glMapBuffer(GL_PIXEL_PACK_BUFFER , GL_READ_ONLY);
	if ( buffer == nullptr )
	{
		ReportError("Error locking PBO");
		return false;
	}

	const int kPixelSize = 4;
	const int rgbaWidthBytes = captureWidth * kPixelSize;
	const int bgraFrameBytes = captureWidth * captureHeight * kPixelSize;

	// Grab the free buffer from the streaming pool
	outRgbaFrame = GetNextFreeBuffer();

	if ( outRgbaFrame != nullptr )
	{
		memcpy(outRgbaFrame, buffer, bgraFrameBytes);
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

	return true;
}
