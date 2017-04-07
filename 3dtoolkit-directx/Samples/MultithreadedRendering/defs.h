#pragma once

#ifndef TEST_RUNNER
#define REMOTE_RENDERING
#ifdef REMOTE_RENDERING
#define SERVER_APP
#endif // REMOTE_RENDERING
#endif // TEST_RUNNER

//#define STEREO_OUTPUT_MODE
#ifdef STEREO_OUTPUT_MODE
#define IPD 2.0f
#define FRAME_BUFFER_WIDTH	2560
#else // STEREO_OUTPUT_MODE
#define FRAME_BUFFER_WIDTH	1280
#endif // STEREO_OUTPUT_MODE
#define FRAME_BUFFER_HEIGHT	720

#define MOVING_CAMERA
#ifdef MOVING_CAMERA
#define CAMERA_SPEED 5
#endif // MOVING_CAMERA
