#include "pch.h"
#include "third_party/nvpipe/nvpipe.h"

NVPIPE_VISIBLE nvpipe*
nvpipe_create_encoder(nvp_codec_t id, uint64_t bitrate, uint64_t frameRate, uint64_t idrPeriod, uint64_t intraRefreshPeriod, bool intraRefreshEnableFlag) {
	return NULL;
}

nvp_err_t
nvpipe_encode(nvpipe* const __restrict cdc,
	const void* const __restrict ibuf,
	const size_t ibuf_sz,
	void* const __restrict obuf,
	size_t* const __restrict obuf_sz,
	const uint32_t width, const uint32_t height, const uint32_t frameRate, nvp_fmt_t format) {
	return NVPIPE_SUCCESS;
}

void
nvpipe_destroy(nvpipe* const __restrict codec) {
}

nvp_err_t
nvpipe_bitrate(nvpipe* const __restrict codec, uint64_t br, uint64_t frameRate) {
	return NVPIPE_SUCCESS;
}

const char*
nvpipe_strerror(nvp_err_t ecode) {
	return "unknown";
}