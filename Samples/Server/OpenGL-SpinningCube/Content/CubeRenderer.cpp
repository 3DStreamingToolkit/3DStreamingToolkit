#include "pch.h"

#include "config_parser.h"
#include "CubeRenderer.h"
#include "glinfo.h"

using namespace DirectX;
using namespace StreamingToolkit;
using namespace StreamingToolkitSample;

static double _angle;
static double _delta_angle = 0;
float _eyeX = 0, _eyeY = 0, _eyeZ = 10, _lookAtX = 0, _lookAtY = 0, _lookAtZ = 0, _upX = 0, _upY = 1, _upZ = 0;
int _width, _height;
int scaleCorrection = 10;

CubeRenderer::CubeRenderer(int width, int height)
{
	_width = width;
	_height = height;
	_angle = 0;
	_delta_angle = 1;
}

void CubeRenderer::SetCamera()
{
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	gluLookAt(_eyeX, _eyeY, _eyeZ, _lookAtX, _lookAtY, _lookAtZ, _upX, _upY, _upZ);
}

void CubeRenderer::Render()
{
	// Rotates the cube.
	_angle += _delta_angle;
	glRotatef(_angle, 0.0f, 1.0f, 0.0f);

	// Renders the cube.
	glBegin(GL_QUADS);

	// face v0-v1-v2-v3
	glColor3f(1, 1, 1);
	glVertex3f(1, 1, 1);
	glColor3f(1, 1, 0);
	glVertex3f(-1, 1, 1);
	glColor3f(1, 0, 0);
	glVertex3f(-1, -1, 1);
	glColor3f(1, 0, 1);
	glVertex3f(1, -1, 1);

	// face v0-v3-v4-v6
	glColor3f(1, 1, 1);
	glVertex3f(1, 1, 1);
	glColor3f(1, 0, 1);
	glVertex3f(1, -1, 1);
	glColor3f(0, 0, 1);
	glVertex3f(1, -1, -1);
	glColor3f(0, 1, 1);
	glVertex3f(1, 1, -1);

	// face v0-v5-v6-v1
	glColor3f(1, 1, 1);
	glVertex3f(1, 1, 1);
	glColor3f(0, 1, 1);
	glVertex3f(1, 1, -1);
	glColor3f(0, 1, 0);
	glVertex3f(-1, 1, -1);
	glColor3f(1, 1, 0);
	glVertex3f(-1, 1, 1);

	// face  v1-v6-v7-v2
	glColor3f(1, 1, 0);
	glVertex3f(-1, 1, 1);
	glColor3f(0, 1, 0);
	glVertex3f(-1, 1, -1);
	glColor3f(0, 0, 0);
	glVertex3f(-1, -1, -1);
	glColor3f(1, 0, 0);
	glVertex3f(-1, -1, 1);

	// face v7-v4-v3-v2
	glColor3f(0, 0, 0);
	glVertex3f(-1, -1, -1);
	glColor3f(0, 0, 1);
	glVertex3f(1, -1, -1);
	glColor3f(1, 0, 1);
	glVertex3f(1, -1, 1);
	glColor3f(1, 0, 0);
	glVertex3f(-1, -1, 1);

	// face v4-v7-v6-v5
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

void CubeRenderer::ToPerspective()
{
	// Set viewport to be the entire window.
	glViewport(0, 0, (GLsizei)_width, (GLsizei)_height);

	// Set perspective viewing frustum.
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(60.0f, (float)(_width) / _height, 0.1f, 1000.0f); // FOV, AspectRatio, NearClip, FarClip

	// Switch to modelview matrix in order to set scene.
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void CubeRenderer::UpdateView(const XMVECTORF32& eye, const XMVECTORF32& at, const XMVECTORF32& up)
{
	// Modify incoming values to match scene scaling. 
	_eyeX = eye[0] * scaleCorrection;
	_eyeY = eye[1] * scaleCorrection;
	_eyeZ = eye[2] * scaleCorrection;
	_lookAtX = at[0] * scaleCorrection;
	_lookAtY = at[1] * scaleCorrection;
	_lookAtZ = at[2] * scaleCorrection;
	_upX = up[0];
	_upY = up[1];
	_upZ = up[2];
}
