/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "webrtc/modules/audio_processing/aec3/render_delay_controller.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "webrtc/base/atomicops.h"
#include "webrtc/base/constructormagic.h"
#include "webrtc/modules/audio_processing/aec3/aec3_common.h"
#include "webrtc/modules/audio_processing/aec3/echo_path_delay_estimator.h"
#include "webrtc/modules/audio_processing/aec3/render_delay_controller_metrics.h"

namespace webrtc {

namespace {

class RenderBuffer {
 public:
  explicit RenderBuffer(size_t size)
      : buffer_(size, std::vector<float>(kBlockSize, 0.f)) {}
  ~RenderBuffer() = default;

  bool Insert(rtc::ArrayView<const float> v) {
    if (size_ >= buffer_.size() - 1) {
      return false;
    }

    last_insert_index_ = (last_insert_index_ + 1) % buffer_.size();
    RTC_DCHECK_EQ(buffer_[last_insert_index_].size(), v.size());

    buffer_[last_insert_index_].clear();
    buffer_[last_insert_index_].insert(buffer_[last_insert_index_].begin(),
                                       v.begin(), v.end());
    ++size_;
    return true;
  }
  rtc::ArrayView<const float> Get() {
    RTC_DCHECK_LT(0, size_);
    --size_;
    return buffer_[(last_insert_index_ - size_ + buffer_.size()) %
                   buffer_.size()];
  }

  size_t Size() { return size_; }

 private:
  std::vector<std::vector<float>> buffer_;
  size_t size_ = 0;
  int last_insert_index_ = 0;
};

class RenderDelayControllerImpl final : public RenderDelayController {
 public:
  RenderDelayControllerImpl(int sample_rate_hz,
                            const RenderDelayBuffer& render_delay_buffer);
  ~RenderDelayControllerImpl() override;
  size_t GetDelay(rtc::ArrayView<const float> capture) override;
  bool AnalyzeRender(rtc::ArrayView<const float> render) override;
  rtc::Optional<size_t> AlignmentHeadroomSamples() const override {
    return headroom_samples_;
  }

 private:
  static int instance_count_;
  std::unique_ptr<ApmDataDumper> data_dumper_;
  const size_t max_delay_;
  size_t delay_;
  RenderBuffer render_buffer_;
  EchoPathDelayEstimator delay_estimator_;
  size_t blocks_since_last_delay_estimate_ = 300000;
  int echo_path_delay_samples_ = 0;
  size_t align_call_counter_ = 0;
  rtc::Optional<size_t> headroom_samples_;
  RenderDelayControllerMetrics metrics_;
  RTC_DISALLOW_IMPLICIT_CONSTRUCTORS(RenderDelayControllerImpl);
};

size_t ComputeNewBufferDelay(size_t current_delay,
                             size_t max_delay,
                             size_t echo_path_delay_samples) {
  // The below division is not exact and the truncation is intended.
  const int echo_path_delay_blocks = echo_path_delay_samples / kBlockSize;
  constexpr int kDelayHeadroomBlocks = 1;

  // Compute the buffer delay increase required to achieve the desired latency.
  size_t new_delay = std::max(echo_path_delay_blocks - kDelayHeadroomBlocks, 0);

  // Add hysteresis.
  if (new_delay == current_delay + 1 || new_delay + 1 == current_delay) {
    new_delay = current_delay;
  }

  // Limit the delay to what is possible.
  new_delay = std::min(new_delay, max_delay);

  return new_delay;
}

int RenderDelayControllerImpl::instance_count_ = 0;

RenderDelayControllerImpl::RenderDelayControllerImpl(
    int sample_rate_hz,
    const RenderDelayBuffer& render_delay_buffer)
    : data_dumper_(
          new ApmDataDumper(rtc::AtomicOps::Increment(&instance_count_))),
      max_delay_(render_delay_buffer.MaxDelay()),
      delay_(render_delay_buffer.Delay()),
      render_buffer_(render_delay_buffer.MaxApiJitter() + 1),
      delay_estimator_(data_dumper_.get()) {
  RTC_DCHECK(ValidFullBandRate(sample_rate_hz));
}

RenderDelayControllerImpl::~RenderDelayControllerImpl() = default;

size_t RenderDelayControllerImpl::GetDelay(
    rtc::ArrayView<const float> capture) {
  RTC_DCHECK_EQ(kBlockSize, capture.size());
  if (render_buffer_.Size() == 0) {
    return delay_;
  }

  ++align_call_counter_;
  rtc::ArrayView<const float> render = render_buffer_.Get();
  rtc::Optional<size_t> echo_path_delay_samples =
      delay_estimator_.EstimateDelay(render, capture);
  if (echo_path_delay_samples) {
    echo_path_delay_samples_ = *echo_path_delay_samples;

    // Compute and set new render delay buffer delay.
    const size_t new_delay =
        ComputeNewBufferDelay(delay_, max_delay_, echo_path_delay_samples_);
    if (new_delay != delay_ && align_call_counter_ > 250) {
      delay_ = new_delay;
    }

    // Update render delay buffer headroom.
    blocks_since_last_delay_estimate_ = 0;
    const int headroom = echo_path_delay_samples_ - delay_ * kBlockSize;
    RTC_DCHECK_LE(0, headroom);
    headroom_samples_ = rtc::Optional<size_t>(headroom);
  } else if (++blocks_since_last_delay_estimate_ > 250 * 20) {
    headroom_samples_ = rtc::Optional<size_t>();
  }

  metrics_.Update(echo_path_delay_samples, delay_);

  data_dumper_->DumpRaw("aec3_render_delay_controller_delay", 1,
                        &echo_path_delay_samples_);
  data_dumper_->DumpRaw("aec3_render_delay_controller_buffer_delay", delay_);

  return delay_;
}

bool RenderDelayControllerImpl::AnalyzeRender(
    rtc::ArrayView<const float> render) {
  return render_buffer_.Insert(render);
}

}  // namespace

RenderDelayController* RenderDelayController::Create(
    int sample_rate_hz,
    const RenderDelayBuffer& render_delay_buffer) {
  return new RenderDelayControllerImpl(sample_rate_hz, render_delay_buffer);
}

}  // namespace webrtc
