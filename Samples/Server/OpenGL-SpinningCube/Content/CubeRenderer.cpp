#include "pch.h"
#include "CubeRenderer.h"
#ifndef TEST_RUNNER
#include "config_parser.h"
#include "glinfo.h"
#include <thread> 
#endif // TEST_RUNNER

using namespace StreamingToolkitSample;

#ifndef TEST_RUNNER
using namespace StreamingToolkit;
#endif // TEST_RUNNER

std::function<void()> CaptureFrame;

static double _angle;
static double _delta_angle = 0;
int _frameRate = 60;
float _eyeX = 0, _eyeY = 0, _eyeZ = 10, _lookAtX = 0, _lookAtY = 0, _lookAtZ = 0, _upX = 0, _upY = 1, _upZ = 0;
int _width, _height;
int scaleCorrection = 10;

CubeRenderer::CubeRenderer(const std::function<void()>& capture_frame, int width, int height)
{
	CaptureFrame = capture_frame;
	_width = width;
	_height = height;
	std::thread func1(InitGraphics);
	func1.detach();
}

void CubeRenderer::InitGraphics()
{
	char *argv[1];
	int argc = 1;
	argv[0] = _strdup("SpinningCubeServerGL");

	_angle = 0;
	_delta_angle = 1;

	// init GLUT and GL
	InitGLUT(argc, argv);
	InitGL();
	glutMainLoop(); /* Start GLUT event-processing loop */
}

void StreamingToolkitSample::CubeRenderer::SetCamera()
{
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(_eyeX, _eyeY, _eyeZ, _lookAtX, _lookAtY, _lookAtZ, _upX, _upY, _upZ);
}

void timerCB(int millisec)
{
	millisec = 1000 / _frameRate;
	_angle += _delta_angle;
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

	glutInitWindowSize(_width, _height);    // window size

	glutInitWindowPosition(100, 100);           // window location

												// finally, create a window with openGL context
												// Window will not displayed until glutMainLoop() is called
												// it returns a unique ID
	int handle = glutCreateWindow(argv[0]);     // param is the title of window

												// register GLUT callback functions
	glutDisplayFunc(FirstRender);            // we will render normal drawing first
											 // so, the first frame has a valid content
	unsigned int updateInMs = 1000 / _frameRate;
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
	glRotatef(_angle, 0.0f, 1.0f, 0.0f);
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

	// Capture frame
	CaptureFrame();
	glutSwapBuffers();
}

void StreamingToolkitSample::CubeRenderer::ToPerspective()
{
	// set viewport to be the entire window
	glViewport(0, 0, (GLsizei)_width, (GLsizei)_height);

	// set perspective viewing frustum
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0f, (float)(_width) / _height, 0.1f, 1000.0f); // FOV, AspectRatio, NearClip, FarClip

																	 // switch to modelview matrix in order to set scene
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void CubeRenderer::UpdateView(float eyeX, float eyeY, float eyeZ, float lookX, float lookY, float lookZ, float upX, float upY, float upZ)
{
	// Modify incoming values to match scene scaling. 
	_eyeX = eyeX * scaleCorrection;
	_eyeY = eyeY * scaleCorrection;
	_eyeZ = eyeZ * scaleCorrection;
	_lookAtX = lookX * scaleCorrection;
	_lookAtY = lookY * scaleCorrection;
	_lookAtZ = lookZ * scaleCorrection;
	_upX = upX;
	_upY = upY;
	_upZ = upZ;
}

void StreamingToolkitSample::CubeRenderer::SetTargetFrameRate(int frameRate)
{
	_frameRate = frameRate;
}
