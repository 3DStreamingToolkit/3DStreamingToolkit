#include "pch.h"

#include "opengl_buffer_capturer.h"
#include "plugindefs.h"

using namespace Microsoft::WRL;
using namespace StreamingToolkit;

// function pointers for PBO Extension
// Windows needs to get function pointers from ICD OpenGL drivers,
// because opengl32.dll does not support extensions higher than v1.1.
#ifdef _WIN32
PFNGLGENBUFFERSARBPROC pglGenBuffersARB = 0;                     // VBO Name Generation Procedure
PFNGLBINDBUFFERARBPROC pglBindBufferARB = 0;                     // VBO Bind Procedure
PFNGLBUFFERDATAARBPROC pglBufferDataARB = 0;                     // VBO Data Loading Procedure
PFNGLBUFFERSUBDATAARBPROC pglBufferSubDataARB = 0;               // VBO Sub Data Loading Procedure
PFNGLDELETEBUFFERSARBPROC pglDeleteBuffersARB = 0;               // VBO Deletion Procedure
PFNGLGETBUFFERPARAMETERIVARBPROC pglGetBufferParameterivARB = 0; // return various parameters of VBO
PFNGLMAPBUFFERARBPROC pglMapBufferARB = 0;                       // map VBO procedure
PFNGLUNMAPBUFFERARBPROC pglUnmapBufferARB = 0;                   // unmap VBO procedure
#define glGenBuffersARB           pglGenBuffersARB
#define glBindBufferARB           pglBindBufferARB
#define glBufferDataARB           pglBufferDataARB
#define glBufferSubDataARB        pglBufferSubDataARB
#define glDeleteBuffersARB        pglDeleteBuffersARB
#define glGetBufferParameterivARB pglGetBufferParameterivARB
#define glMapBufferARB            pglMapBufferARB
#define glUnmapBufferARB          pglUnmapBufferARB
#endif

// function pointers for WGL_EXT_swap_control
#ifdef _WIN32
typedef BOOL(WINAPI * PFNWGLSWAPINTERVALEXTPROC) (int interval);
typedef int (WINAPI * PFNWGLGETSWAPINTERVALEXTPROC) (void);
PFNWGLSWAPINTERVALEXTPROC pwglSwapIntervalEXT = 0;
PFNWGLGETSWAPINTERVALEXTPROC pwglGetSwapIntervalEXT = 0;
#define wglSwapIntervalEXT      pwglSwapIntervalEXT
#define wglGetSwapIntervalEXT   pwglGetSwapIntervalEXT
#endif

// global variables
static int index = 0;
int nextIndex = 0;                  // pbo index used for next frame
const GLenum PIXEL_FORMAT = GL_RGBA;
const int PBO_COUNT = 2;
void *font = GLUT_BITMAP_8_BY_13;
GLuint pboIds[PBO_COUNT];           // IDs of PBOs
bool pboSupported;
bool pboUsed;
bool isInitialized;
GLubyte* colorBuffer = 0;

OpenGLBufferCapturer::OpenGLBufferCapturer(int width, int height)
{
	buffer_width = width;
	buffer_height = height;
	isInitialized = false;
}

void OpenGLBufferCapturer::Initialize(bool headless, int width, int height)
{
	auto data_size = buffer_width * buffer_height * 4;
	// allocate buffers to store frames
	colorBuffer = new GLubyte[data_size];
	memset(colorBuffer, 255, data_size);

	// get pointers to GL functions
	glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)wglGetProcAddress("glGenBuffersARB");
	glBindBufferARB = (PFNGLBINDBUFFERARBPROC)wglGetProcAddress("glBindBufferARB");
	glBufferDataARB = (PFNGLBUFFERDATAARBPROC)wglGetProcAddress("glBufferDataARB");
	glBufferSubDataARB = (PFNGLBUFFERSUBDATAARBPROC)wglGetProcAddress("glBufferSubDataARB");
	glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)wglGetProcAddress("glDeleteBuffersARB");
	glGetBufferParameterivARB = (PFNGLGETBUFFERPARAMETERIVARBPROC)wglGetProcAddress("glGetBufferParameterivARB");
	glMapBufferARB = (PFNGLMAPBUFFERARBPROC)wglGetProcAddress("glMapBufferARB");
	glUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)wglGetProcAddress("glUnmapBufferARB");

	// check once again PBO extension
	if (glGenBuffersARB && glBindBufferARB && glBufferDataARB && glBufferSubDataARB &&
		glMapBufferARB && glUnmapBufferARB && glDeleteBuffersARB && glGetBufferParameterivARB)
	{
		pboSupported = pboUsed = true;
	}
	else
	{
		pboSupported = pboUsed = false;
	}

	// get pointers to WGL functions
	wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
	wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");
	if (wglSwapIntervalEXT && wglGetSwapIntervalEXT)
	{
		// disable v-sync
		wglSwapIntervalEXT(0);
	}

	if (pboSupported)
	{
		// create 2 pixel buffer objects, you need to delete them when program exits.
		// glBufferDataARB with NULL pointer reserves only memory space.
		glGenBuffersARB(PBO_COUNT, pboIds);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboIds[0]);
		glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, data_size, 0, GL_STREAM_READ_ARB);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboIds[1]);
		glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, data_size, 0, GL_STREAM_READ_ARB);

		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}

	isInitialized = true;
}

void OpenGLBufferCapturer::SendFrame()
{
	rtc::CritScope cs(&lock_);

	if (!isInitialized)
	{
		Initialize();
	}

	if (!running_)
	{
		return;
	}

	// increment current index first then get the next index
	// "index" is used to read pixels from a framebuffer to a PBO
	// "nextIndex" is used to process pixels in the other PBO
	index = (index + 1) % 2;
	nextIndex = (index + 1) % 2;

	if (pboUsed) // with PBO
	{
		// OpenGL should perform asynch DMA transfer, so glReadPixels() will return immediately.
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboIds[index]);
		glReadPixels(0, 0, buffer_width, buffer_height, PIXEL_FORMAT, GL_UNSIGNED_BYTE, 0);

		// map the PBO that contain framebuffer pixels before processing it
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboIds[nextIndex]);
		GLubyte* src = (GLubyte*)glMapBufferARB(GL_PIXEL_PACK_BUFFER_ARB, GL_READ_ONLY_ARB);
		if (src)
		{
			// copy buffer 
			memcpy(&colorBuffer, &src, sizeof(src));

			// release pointer to the mapped buffer
			glUnmapBufferARB(GL_PIXEL_PACK_BUFFER_ARB);
		}

		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}
	else
	{
		// read framebuffer 
		glReadPixels(0, 0, buffer_width, buffer_height, PIXEL_FORMAT, GL_UNSIGNED_BYTE, colorBuffer);
	}

	if (buffer_width > 4096 || buffer_height > 4096)
	{
		return;
	}

	rtc::scoped_refptr<webrtc::I420Buffer> buffer;
	buffer = webrtc::I420Buffer::Create(buffer_width, buffer_height);

	if (use_software_encoder_)
	{
		libyuv::ABGRToI420(
			(uint8_t*)colorBuffer,
			buffer_width * 4,
			buffer.get()->MutableDataY(),
			buffer.get()->StrideY(),
			buffer.get()->MutableDataU(),
			buffer.get()->StrideU(),
			buffer.get()->MutableDataV(),
			buffer.get()->StrideV(),
			buffer_width,
			buffer_height);
	}

	auto frame = webrtc::VideoFrame(buffer, kVideoRotation_0, 0);

	if (!use_software_encoder_)
	{
		frame.set_frame_buffer((uint8_t*)colorBuffer);
	}

	frame.set_ntp_time_ms(clock_->CurrentNtpInMilliseconds());

	// Sending video frame.
	BufferCapturer::SendFrame(frame);
}
