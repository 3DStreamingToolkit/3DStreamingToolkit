#pragma once

#pragma warning(disable : 4100)

#ifdef STEREO_OUTPUT_MODE
#define IPD 2.0f
#define FRAME_BUFFER_WIDTH	2560
#else // STEREO_OUTPUT_MODE
#define FRAME_BUFFER_WIDTH	1280
#endif // STEREO_OUTPUT_MODE
#define FRAME_BUFFER_HEIGHT	720

//#define MOVING_CAMERA
#define CAMERA_SPEED 5
