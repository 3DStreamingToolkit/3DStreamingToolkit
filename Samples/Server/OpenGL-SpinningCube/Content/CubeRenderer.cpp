#include "pch.h"
#include "CubeRenderer.h"
#ifndef TEST_RUNNER
#include "config_parser.h"
#include "glinfo.h"
#include <thread> 
#endif // TEST_RUNNER

using namespace DirectX;
using namespace DX;
using namespace StreamingToolkitSample;

#ifndef TEST_RUNNER
using namespace StreamingToolkit;
#endif // TEST_RUNNER

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

// Eye is at (0, 0, 1), looking at point (0, 0, 0) with the up-vector along the y-axis.
static XMVECTORF32 eye = { 0.0f, 0.0f, 1.0f, 0.0f };
static XMVECTORF32 at = { 0.0f, 0.0f, 0.0f, 0.0f };
static XMVECTORF32 up = { 0.0f, 1.0f, 0.0f, 0.0f };

static int imgIndex = 0;
static bool offscreen = 1;
static double angle;
static double delta_angle = 0;
static int index = 0;
int nextIndex = 0;                  // pbo index used for next frame
int frame_rate = 30;

// constants
const float CAMERA_DISTANCE = 5.0f;
const int CHANNEL_COUNT = 4;
const int DATA_SIZE = DEFAULT_FRAME_BUFFER_WIDTH * DEFAULT_FRAME_BUFFER_HEIGHT * CHANNEL_COUNT;
const GLenum PIXEL_FORMAT = GL_BGRA;
const int PBO_COUNT = 2;

// global variables
void *font = GLUT_BITMAP_8_BY_13;
GLuint pboIds[PBO_COUNT];           // IDs of PBOs
bool mouseLeftDown;
bool mouseRightDown;
float mouseX, mouseY;
float cameraAngleX;
float cameraAngleY;
float cameraDistance;
bool pboSupported;
bool pboUsed;
int drawMode = 0;
float readTime, processTime;
GLubyte* colorBuffer = 0;

CubeRenderer::CubeRenderer() 
{
	std::thread func1(InitGraphics);
	func1.detach();
}

void CubeRenderer::InitGraphics()
{
	char *argv[1];
	int argc = 1;
	argv[0] = _strdup("SpinningCubeServerGL");

	cameraAngleX = cameraAngleY = 0.0f;
	cameraDistance = CAMERA_DISTANCE;

	mouseX = mouseY = 0;
	mouseLeftDown = mouseRightDown = false;

	drawMode = 0; // 0:fill, 1:wireframe, 2:point
	angle = 0;
	delta_angle = 1;

	// allocate buffers to store frames
	colorBuffer = new GLubyte[DATA_SIZE];
	memset(colorBuffer, 255, DATA_SIZE);

	// init GLUT and GL
	InitGLUT(argc, argv);
	InitGL();

	// get OpenGL info
	glInfo glInfo;
	glInfo.getInfo();
	glInfo.printSelf();

#ifdef _WIN32
	// check PBO is supported by your video card
	if (glInfo.isExtensionSupported("GL_ARB_pixel_buffer_object"))
	{
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
	}

	// check EXT_swap_control is supported
	if (glInfo.isExtensionSupported("WGL_EXT_swap_control"))
	{
		// get pointers to WGL functions
		wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
		wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");
		if (wglSwapIntervalEXT && wglGetSwapIntervalEXT)
		{
			// disable v-sync
			wglSwapIntervalEXT(0);
		}
	}

#else // for linux, do not need to get function pointers, it is up-to-date
	if (glInfo.isExtensionSupported("GL_ARB_pixel_buffer_object"))
	{
		pboSupported = pboUsed = true;
	}
	else
	{
		pboSupported = pboUsed = false;
	}
#endif

	if (pboSupported)
	{
		// create 2 pixel buffer objects, you need to delete them when program exits.
		// glBufferDataARB with NULL pointer reserves only memory space.
		glGenBuffersARB(PBO_COUNT, pboIds);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboIds[0]);
		glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, DATA_SIZE, 0, GL_STREAM_READ_ARB);
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboIds[1]);
		glBufferDataARB(GL_PIXEL_PACK_BUFFER_ARB, DATA_SIZE, 0, GL_STREAM_READ_ARB);

		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, 0);
	}

	// the last GLUT call (LOOP)
	// window will be shown and display callback is triggered by events
	// NOTE: this call never return main().
	glutMainLoop(); /* Start GLUT event-processing loop */
}

void StreamingToolkitSample::CubeRenderer::SetCamera()
{
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(eye[0] * CAMERA_DISTANCE, eye[1] * CAMERA_DISTANCE, eye[2] * CAMERA_DISTANCE, at[0] * CAMERA_DISTANCE, at[1] * CAMERA_DISTANCE, at[2] * CAMERA_DISTANCE, up[0] * CAMERA_DISTANCE, up[1] * CAMERA_DISTANCE, up[2] * CAMERA_DISTANCE);
}

void timerCB(int millisec)
{
	millisec = 1000 / frame_rate;
	angle += delta_angle;
	glutTimerFunc(millisec, timerCB, millisec);
	glutPostRedisplay();
}

void StreamingToolkitSample::CubeRenderer::InitGLUT(int argc, char ** argv)
{
	// GLUT stuff for windowing
	// initialization openGL window.
	// it is called before any other GLUT routine
	glutInit(&argc, argv);

	glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE | GLUT_ALPHA); // display mode

	glutInitWindowSize(DEFAULT_FRAME_BUFFER_WIDTH, DEFAULT_FRAME_BUFFER_HEIGHT);    // window size

	glutInitWindowPosition(100, 100);           // window location

												// finally, create a window with openGL context
												// Window will not displayed until glutMainLoop() is called
												// it returns a unique ID
	int handle = glutCreateWindow(argv[0]);     // param is the title of window

												// register GLUT callback functions
	glutDisplayFunc(FirstRender);            // we will render normal drawing first
												// so, the first frame has a valid content
	unsigned int updateInMs = 1000 / frame_rate;
	glutTimerFunc(updateInMs, timerCB, updateInMs);             // redraw based on desired framerate
}

void StreamingToolkitSample::CubeRenderer::InitGL()
{
	glShadeModel(GL_SMOOTH);                    // shading mathod: GL_SMOOTH or GL_FLAT
	glPixelStorei(GL_UNPACK_ALIGNMENT, 4);      // 4-byte pixel alignment

												// enable /disable features
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	//glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	//glHint(GL_POLYGON_SMOOTH_HINT, GL_NICEST);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_LIGHTING);
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_CULL_FACE);

	// track material ambient and diffuse from surface color, call it before glEnable(GL_COLOR_MATERIAL)
	glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
	glEnable(GL_COLOR_MATERIAL);

	glClearColor(0, 0, 0, 0);                   // background color
	glClearStencil(0);                          // clear stencil buffer
	glClearDepth(1.0f);                         // 0 is near, 1 is far
	glDepthFunc(GL_LEQUAL);

	// set up light colors (ambient, diffuse, specular)
	GLfloat lightKa[] = { .2f, .2f, .2f, 1.0f };  // ambient light
	GLfloat lightKd[] = { .7f, .7f, .7f, 1.0f };  // diffuse light
	GLfloat lightKs[] = { 1, 1, 1, 1 };           // specular light
	glLightfv(GL_LIGHT0, GL_AMBIENT, lightKa);
	glLightfv(GL_LIGHT0, GL_DIFFUSE, lightKd);
	glLightfv(GL_LIGHT0, GL_SPECULAR, lightKs);

	// position the light
	float lightPos[4] = { 0, 0, 20, 1 }; // positional light
	glLightfv(GL_LIGHT0, GL_POSITION, lightPos);

	glEnable(GL_LIGHT0);                        // MUST enable each light source after configuration
}

void CubeRenderer::Render(void)
{
	glRotatef(angle, 0.0f, 1.0f, 0.0f);
	glBegin(GL_QUADS);
		// face v0-v1-v2-v3
		glNormal3f(0, 0, 1);
		glColor3f(1, 1, 1);
		glVertex3f(1, 1, 1);
		glColor3f(1, 1, 0);
		glVertex3f(-1, 1, 1);
		glColor3f(1, 0, 0);
		glVertex3f(-1, -1, 1);
		glColor3f(1, 0, 1);
		glVertex3f(1, -1, 1);

		// face v0-v3-v4-v6
		glNormal3f(1, 0, 0);
		glColor3f(1, 1, 1);
		glVertex3f(1, 1, 1);
		glColor3f(1, 0, 1);
		glVertex3f(1, -1, 1);
		glColor3f(0, 0, 1);
		glVertex3f(1, -1, -1);
		glColor3f(0, 1, 1);
		glVertex3f(1, 1, -1);

		// face v0-v5-v6-v1
		glNormal3f(0, 1, 0);
		glColor3f(1, 1, 1);
		glVertex3f(1, 1, 1);
		glColor3f(0, 1, 1);
		glVertex3f(1, 1, -1);
		glColor3f(0, 1, 0);
		glVertex3f(-1, 1, -1);
		glColor3f(1, 1, 0);
		glVertex3f(-1, 1, 1);

		// face  v1-v6-v7-v2
		glNormal3f(-1, 0, 0);
		glColor3f(1, 1, 0);
		glVertex3f(-1, 1, 1);
		glColor3f(0, 1, 0);
		glVertex3f(-1, 1, -1);
		glColor3f(0, 0, 0);
		glVertex3f(-1, -1, -1);
		glColor3f(1, 0, 0);
		glVertex3f(-1, -1, 1);

		// face v7-v4-v3-v2
		glNormal3f(0, -1, 0);
		glColor3f(0, 0, 0);
		glVertex3f(-1, -1, -1);
		glColor3f(0, 0, 1);
		glVertex3f(1, -1, -1);
		glColor3f(1, 0, 1);
		glVertex3f(1, -1, 1);
		glColor3f(1, 0, 0);
		glVertex3f(-1, -1, 1);

		// face v4-v7-v6-v5
		glNormal3f(0, 0, -1);
		glColor3f(0, 0, 1);
		glVertex3f(1, -1, -1);
		glColor3f(0, 0, 0);
		glVertex3f(-1, -1, -1);
		glColor3f(0, 1, 0);
		glVertex3f(-1, 1, -1);
		glColor3f(0, 1, 1);
		glVertex3f(1, 1, -1);
	glEnd();
}

void StreamingToolkitSample::CubeRenderer::FirstRender(void)
{
	// clear buffer
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

	// Set camera lookat
	SetCamera();

	// draw a cube
	glPushMatrix();
	Render();
	glPopMatrix();

	ToPerspective();
	glRasterPos2i(0, 0);

	// set the framebuffer to read
	glReadBuffer(GL_FRONT);

	// increment current index first then get the next index
	// "index" is used to read pixels from a framebuffer to a PBO
	// "nextIndex" is used to process pixels in the other PBO
	index = (index + 1) % 2;
	nextIndex = (index + 1) % 2;

	if (pboUsed) // with PBO
	{
		// OpenGL should perform asynch DMA transfer, so glReadPixels() will return immediately.
		glBindBufferARB(GL_PIXEL_PACK_BUFFER_ARB, pboIds[index]);
		glReadPixels(0, 0, DEFAULT_FRAME_BUFFER_WIDTH, DEFAULT_FRAME_BUFFER_HEIGHT, PIXEL_FORMAT, GL_UNSIGNED_BYTE, 0);

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
		glReadPixels(0, 0, DEFAULT_FRAME_BUFFER_WIDTH, DEFAULT_FRAME_BUFFER_HEIGHT, PIXEL_FORMAT, GL_UNSIGNED_BYTE, colorBuffer);
	}

	glutSwapBuffers();
}

void StreamingToolkitSample::CubeRenderer::ToPerspective()
{
	// set viewport to be the entire window
	glViewport(0, 0, (GLsizei)DEFAULT_FRAME_BUFFER_WIDTH, (GLsizei)DEFAULT_FRAME_BUFFER_HEIGHT);

	// set perspective viewing frustum
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0f, (float)(DEFAULT_FRAME_BUFFER_WIDTH) / DEFAULT_FRAME_BUFFER_HEIGHT, 0.1f, 1000.0f); // FOV, AspectRatio, NearClip, FarClip

	// switch to modelview matrix in order to set scene
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void CubeRenderer::UpdateView(const XMVECTORF32& eyeVec, const XMVECTORF32& atVec, const XMVECTORF32& upVec)
{
	eye = eyeVec;
	at = atVec;
	up = upVec;
}

byte * StreamingToolkitSample::CubeRenderer::GrabRGBFrameBuffer()
{
	return colorBuffer;
}

void StreamingToolkitSample::CubeRenderer::SetTargetFrameRate(int frameRate)
{
	frame_rate = frameRate;
}
