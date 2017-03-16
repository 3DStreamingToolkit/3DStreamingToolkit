/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/modules/audio_processing/aec3/residual_echo_estimator.h"

#include "webrtc/base/random.h"
#include "webrtc/modules/audio_processing/aec3/aec_state.h"
#include "webrtc/modules/audio_processing/aec3/aec3_fft.h"
#include "webrtc/modules/audio_processing/test/echo_canceller_test_tools.h"
#include "webrtc/test/gtest.h"

namespace webrtc {

#if RTC_DCHECK_IS_ON && GTEST_HAS_DEATH_TEST && !defined(WEBRTC_ANDROID)

// Verifies that the check for non-null output gains works.
TEST(ResidualEchoEstimator, NullOutputGains) {
  AecState aec_state;
  FftBuffer X_buffer(Aec3Optimization::kNone, 10, std::vector<size_t>(1, 10));
  std::vector<std::array<float, kFftLengthBy2Plus1>> H2;
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;
  std::array<float, kFftLengthBy2Plus1> S2_linear;
  std::array<float, kFftLengthBy2Plus1> S2_fallback;
  std::array<float, kFftLengthBy2Plus1> Y2;

  EXPECT_DEATH(ResidualEchoEstimator().Estimate(true, aec_state, X_buffer, H2,
                                                E2_main, E2_shadow, S2_linear,
                                                S2_fallback, Y2, nullptr),
               "");
}

#endif

TEST(ResidualEchoEstimator, BasicTest) {
  ResidualEchoEstimator estimator;
  AecState aec_state;
  FftBuffer X_buffer(Aec3Optimization::kNone, 10, std::vector<size_t>(1, 10));
  std::array<float, kFftLengthBy2Plus1> E2_main;
  std::array<float, kFftLengthBy2Plus1> E2_shadow;
  std::array<float, kFftLengthBy2Plus1> S2_linear;
  std::array<float, kFftLengthBy2Plus1> S2_fallback;
  std::array<float, kFftLengthBy2Plus1> Y2;
  std::array<float, kFftLengthBy2Plus1> R2;
  EchoPathVariability echo_path_variability(false, false);
  std::array<float, kBlockSize> x;
  std::vector<std::array<float, kFftLengthBy2Plus1>> H2(10);
  Random random_generator(42U);
  FftData X;
  std::array<float, kBlockSize> x_old;
  Aec3Fft fft;

  for (auto& H2_k : H2) {
    H2_k.fill(0.01f);
  }
  H2[2].fill(10.f);

  constexpr float kLevel = 10.f;
  E2_shadow.fill(kLevel);
  E2_main.fill(kLevel);
  S2_linear.fill(kLevel);
  S2_fallback.fill(kLevel);
  Y2.fill(kLevel);

  for (int k = 0; k < 100; ++k) {
    RandomizeSampleVector(&random_generator, x);
    fft.PaddedFft(x, x_old, &X);
    X_buffer.Insert(X);

    aec_state.Update(H2, rtc::Optional<size_t>(2), X_buffer, E2_main, E2_shadow,
                     Y2, x, echo_path_variability, false);

    estimator.Estimate(true, aec_state, X_buffer, H2, E2_main, E2_shadow,
                       S2_linear, S2_fallback, Y2, &R2);
  }
  std::for_each(R2.begin(), R2.end(),
                [&](float a) { EXPECT_NEAR(kLevel, a, 0.1f); });
}

}  // namespace webrtc
