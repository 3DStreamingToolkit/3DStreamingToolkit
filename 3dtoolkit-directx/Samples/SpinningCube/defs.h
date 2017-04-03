#pragma once

// Static define here, also defined as a build configuration
//#define TEST_RUNNER

#define STEREO_OUTPUT_MODE

#define SERVER_APP
#define STEREO_OUTPUT_MODE

#ifdef STEREO_OUTPUT_MODE
#define IPD 0.02f
#define FRAME_BUFFER_WIDTH	1280
#else // STEREO_OUTPUT_MODE
#define FRAME_BUFFER_WIDTH	640
#endif // STEREO_OUTPUT_MODE
#define FRAME_BUFFER_HEIGHT	480
