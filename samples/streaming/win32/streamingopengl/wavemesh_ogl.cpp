//////////////////////////////////////////////////////////////////////////////
// This module contains code which generates and animates a simple mesh.
//////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include "../../wavemesh.h"

#include <gl/glew.h>
#include <gl/gl.h>
#include <FreeImage.h>

#include <string>

struct Vector3
{
	float x,y,z;
};

struct WaveMeshVertex
{
	Vector3 v; 
	float tx, ty; 
};

GLuint gTwitchTexture = 0;
GLuint gVertexBuffer = 0;
GLuint gIndexBuffer = 0;


float gWorldSize = 0;
unsigned int gVertexDim = 0;
unsigned int gNumVertices = 0;
unsigned int gNumIndices = 0;
float gWaveStartTime = 0;

float gScale = 0;


/**
 * Computes the index of the vertex in the vertex buffer for the given vertex coordinates.
 */
inline int GetWaveMeshIndex(unsigned int x, unsigned int y)
{
	return gVertexDim*y + x;
}

/**
 * Create vertex buffer for wave mesh
 */
bool CreateVertexBuffer( float scale, unsigned int vertexDim )
{
	gScale = scale;

	glGenBuffers(1, &gVertexBuffer);
	if ( gVertexBuffer == 0 )
	{
		ReportError("Failed to create vertex buffer");
		return false;
	}

	glBindBuffer(GL_ARRAY_BUFFER, gVertexBuffer);

	glBufferData(GL_ARRAY_BUFFER, gNumVertices*sizeof(WaveMeshVertex), 0, GL_STREAM_DRAW);

	WaveMeshVertex* pMeshVertices = (WaveMeshVertex*)glMapBuffer(GL_ARRAY_BUFFER,GL_WRITE_ONLY);
	{	
		if ( pMeshVertices == nullptr )
		{
			ReportError("Failed to lock vertex buffer");
			return false;
		}

		// shift to center on 0,0,0
		float shift = -(float)scale / 2;

		int index = 0;
		for (unsigned int y=0; y<vertexDim; ++y)
		{
			for (unsigned int x=0; x<vertexDim; ++x)
			{
				pMeshVertices[index].v.x = shift + scale * (float)x / (float)(vertexDim-1);
				pMeshVertices[index].v.y = shift + scale * (float)y / (float)(vertexDim-1);
				pMeshVertices[index].v.z = -100;
				pMeshVertices[index].tx = x / float(vertexDim-1);
				pMeshVertices[index].ty = y / float(vertexDim-1);

				index++;
			}
		}
	}
	GLboolean success = glUnmapBuffer(GL_ARRAY_BUFFER);
	if ( !success )
	{
		ReportError("Failed to unlock vertex buffer");
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		return false;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);

	return true;
}

/**
 * Create index buffer for wave mesh
 */
bool CreateIndexBuffer( unsigned int vertexDim )
{
	// setup the index buffer
	gNumIndices = 3*2*(gVertexDim-1)*(vertexDim-1);
    
	glGenBuffers(1, &gIndexBuffer);
	if ( gIndexBuffer == 0 )
	{
		ReportError("Failed to create index buffer");
		return false;
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIndexBuffer);	

	glBufferData(GL_ELEMENT_ARRAY_BUFFER, gNumIndices*sizeof(unsigned int), 0, GL_STATIC_DRAW);

	unsigned int* pMeshIndices = (unsigned int*)glMapBuffer(GL_ELEMENT_ARRAY_BUFFER, GL_WRITE_ONLY);
	{
		if ( pMeshIndices == nullptr )
		{
			ReportError("Failed to lock index buffer");
			return false; 
		}

		int index = 0;
		for (unsigned int y=0; y<vertexDim-1; ++y)
		{
			for (unsigned int x=0; x<vertexDim-1; ++x)
			{
				pMeshIndices[index++] = GetWaveMeshIndex(x, y);
				pMeshIndices[index++] = GetWaveMeshIndex(x, y+1);
				pMeshIndices[index++] = GetWaveMeshIndex(x+1, y);

				pMeshIndices[index++] = GetWaveMeshIndex(x, y+1);
				pMeshIndices[index++] = GetWaveMeshIndex(x+1, y+1);
				pMeshIndices[index++] = GetWaveMeshIndex(x+1, y);
			}
		}
	}
	GLboolean sucess = glUnmapBuffer(GL_ELEMENT_ARRAY_BUFFER);
	if ( !sucess )
	{
		ReportError("Failed to unlock index buffer");
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
		return false;
	}

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

	return true;
}


/**
 * Create twitch texture
 */
bool CreateTexture()
{
	const char* textureFile = "twitch.png";

	FREE_IMAGE_FORMAT format = FreeImage_GetFileType(textureFile, 0);

	FIBITMAP* bitmap = FreeImage_Load(format, textureFile);

	if ( bitmap == nullptr )
	{
		std::string filePath = std::string("..\\..\\") + textureFile;

		format = FreeImage_GetFileType(filePath.c_str(), 0);

		bitmap = FreeImage_Load(format, filePath.c_str() );

		if ( bitmap == nullptr )
		{
			ReportError("Failed to load texture");
			return false;
		}
	}	
 
	FIBITMAP* bitmap32 = FreeImage_ConvertTo32Bits(bitmap);	
 
	int w = FreeImage_GetWidth(bitmap32);
	int h = FreeImage_GetHeight(bitmap32);

	char* buffer = (char*)FreeImage_GetBits(bitmap32);
 
	glGenTextures(1, &gTwitchTexture);

	glBindTexture(GL_TEXTURE_2D, gTwitchTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER , GL_NEAREST );
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER , GL_NEAREST );

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_BGRA, GL_UNSIGNED_BYTE, (GLvoid*)buffer );

	FreeImage_Unload(bitmap32);
	FreeImage_Unload(bitmap);

	if ( glGetError() != GL_NO_ERROR )
	{
		ReportError("Failed to load texture");
		glBindTexture(GL_TEXTURE_2D, 0);
		return false;
	}
	
	glBindTexture(GL_TEXTURE_2D, 0);

	return true;
}

/**
 * Creates a wave mesh of the desired world size and number of vertices wide.
 */
void CreateWaveMesh(float scale, unsigned int vertexDim)
{
	DestroyWaveMesh();

	if (scale < 1)
	{
		scale = 1;
	}

	if (vertexDim < 2)
	{
		vertexDim = 2;
	}

	gWorldSize = scale;
	gVertexDim = vertexDim;
	gNumVertices = gVertexDim*gVertexDim;

	bool success;
	success = CreateVertexBuffer( scale, vertexDim );
	if ( success == false )
		return;

	success = CreateIndexBuffer( vertexDim );
	if ( success == false )
		return;

	success = CreateTexture();
	if ( success == false )
		return;

	gWaveStartTime = static_cast<float>(GetSystemTimeMs());
}


/**
 * Frees resources used by the mesh.
 */
void DestroyWaveMesh()
{
	if ( gVertexBuffer != 0 )
	{
		glDeleteBuffers(1, &gVertexBuffer);
		gVertexBuffer = 0;
	}

	if ( gIndexBuffer != 0 )
	{
		glDeleteBuffers(1, &gIndexBuffer);
		gIndexBuffer = 0;
	}
	
	if ( gTwitchTexture != 0 )
	{
		glDeleteTextures(1, &gTwitchTexture);
	    gTwitchTexture = 0;
	}
	
	gNumVertices = 0;
	gNumIndices = 0;
	gWorldSize = 0;
	gVertexDim = 0;
}


/**
 * Animate the mesh.
 */
void UpdateWaveMesh()
{
	if (gVertexBuffer == 0)
		return;

	// This is a horrible way to animate the mesh and it should be done in a simple vertex shader.  However, it's a simple
	// sample and this keeps things simpler.
	float totalTime = GetSystemTimeMs() - gWaveStartTime;

	const float amp = 3.0f;
	const float freq = 2;
	float shift = freq * 6.28f * totalTime / 1000.0f;

	glBindBuffer(GL_ARRAY_BUFFER, gVertexBuffer);

	long long s = GetSystemTimeMs();

	// We drop the old buffer data here to avoid a GPU sync.
	// The drawback is we have to refill the whole mesh but should be much faster when mesh is small.
	glBufferData(GL_ARRAY_BUFFER, gNumVertices*sizeof(WaveMeshVertex), 0, GL_STREAM_DRAW);

    WaveMeshVertex* pMeshVertices = (WaveMeshVertex*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	{
		if ( pMeshVertices == nullptr )
		{
			ReportError("Failed to lock vertex buffer");
			return;
		}

		float offset = -(float)gWorldSize / 2;

		int index = 0;
		float shiftxy = -(float)gScale / 2;
		for (unsigned int y=0; y<gVertexDim; ++y)
		{
			for (unsigned int x=0; x<gVertexDim; ++x)
			{	
				float c = cos(shift + 6.28f*(x+y)/(float)(gVertexDim-1));

				pMeshVertices[index].v.x = shiftxy + gScale * (float)x / (float)(gVertexDim-1);
				pMeshVertices[index].v.y = shiftxy + gScale * (float)y / (float)(gVertexDim-1);
				pMeshVertices[index].v.z = -100 + amp * c;
				pMeshVertices[index].tx = x / float(gVertexDim-1);
				pMeshVertices[index].ty = y / float(gVertexDim-1);
				
				index++;
			}
		}
	}

	GLenum success = glUnmapBuffer(GL_ARRAY_BUFFER);
	if ( !success )
	{
		ReportError("Failed to unlock vertex buffer");
		glBindBuffer(GL_ARRAY_BUFFER, 0);
		return;
	}

	glBindBuffer(GL_ARRAY_BUFFER, 0);
}


/**
 * Draws the mesh.
 */
void DrawWaveMesh()
{
	glDepthMask(true);

	glEnable(GL_DEPTH_TEST);

	glEnable(GL_ALPHA_TEST);

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glEnable(GL_TEXTURE_2D);

	glBindBuffer(GL_ARRAY_BUFFER, gVertexBuffer);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gIndexBuffer);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	
	glBindTexture(GL_TEXTURE_2D, gTwitchTexture);    

	glVertexPointer(  3, GL_FLOAT, sizeof(WaveMeshVertex), 0);
	glTexCoordPointer(2, GL_FLOAT, sizeof(WaveMeshVertex), (GLvoid*)offsetof(WaveMeshVertex, tx));
	
	glDrawElements(GL_TRIANGLES, gNumIndices, GL_UNSIGNED_INT, 0);

	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);

	glBindTexture(GL_TEXTURE_2D, 0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}
