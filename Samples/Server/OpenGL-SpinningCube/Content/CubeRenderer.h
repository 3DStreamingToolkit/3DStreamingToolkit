#pragma once

#include "DeviceResources.h"

#include <freeglut.h>
#include <glut.h>
#include "glext.h"

namespace StreamingToolkitSample
{
	class CubeRenderer
	{
	public:
												CubeRenderer();

		static void								InitGLUT(int argc, char **argv);
		static void								InitGL();
		static void								Render(void);
		static void								FirstRender(void);
		static void								ToPerspective();
		void									UpdateView(const DirectX::XMVECTORF32& eye, const DirectX::XMVECTORF32& lookAt, const DirectX::XMVECTORF32& up);
		static byte*							GrabRGBFrameBuffer();
		static void								SetTargetFrameRate(int frameRate);
		
	private:
		static void								InitGraphics();
		static void								SetCamera();
	};
}
