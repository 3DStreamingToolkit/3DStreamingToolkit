#pragma once

// This requires DirectX 11.4
#define MULTITHREAD_PROTECTION

#define MAX_ENCODE_QUEUE 32
#define BITSTREAM_BUFFER_SIZE 2 * 1024 * 1024
#define DEPLOYED_SERVICE

#ifndef TEST_RUNNER
#define USE_WEBRTC_NVENCODE
#endif // TEST_RUNNER
