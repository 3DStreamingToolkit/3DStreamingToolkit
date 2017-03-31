#pragma once

#define STEREO_OUTPUT_MODE
//Static define here, also defined as a build configuration
//#define TEST_RUNNER
#define IPD 0.02f
#define SERVER_APP

#ifdef STEREO_OUTPUT_MODE
#define FRAME_BUFFER_WIDTH	1280
#else
#define FRAME_BUFFER_WIDTH	640
#endif // STEREO_OUTPUT_MODE
#define FRAME_BUFFER_HEIGHT	480
