#pragma once

#include <freeglut.h>
#include <glut.h>

#include "buffer_capturer.h"
#include "glext.h"
#include <wrl\client.h>
#include <wrl\wrappers\corewrappers.h>

namespace StreamingToolkit
{
	// Provides OpenGL implementation of the BufferCapturer class.
	class OpenGLBufferCapturer : public BufferCapturer
	{
	public:
		OpenGLBufferCapturer();
		virtual ~OpenGLBufferCapturer() {}

		void SendFrame(GLubyte* color_buffer, int width, int height);
	};
}
