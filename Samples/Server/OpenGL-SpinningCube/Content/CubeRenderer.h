#pragma once

#include <functional>
#include <iostream>

#include <freeglut.h>
#include <glut.h>
#include "glext.h"

namespace StreamingToolkitSample
{
	class CubeRenderer
	{
	public:
												CubeRenderer(const std::function<void()>& capture_frame, int width, int height);

		static void								InitGLUT(int argc, char **argv);
		static void								InitGL();
		static void								Render(void);
		static void								FirstRender(void);
		static void								ToPerspective();
		void									UpdateView(float eyeX, float eyeY, float eyeZ, float lookX, float lookY, float lookZ, float upX, float upY, float upZ);
		static void								SetTargetFrameRate(int frameRate);
		
	private:
		static void								InitGraphics();
		static void								SetCamera();
	};
}
