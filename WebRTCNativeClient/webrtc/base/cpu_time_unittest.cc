/*
 *  Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <memory>
#include <algorithm>
#include "webrtc/base/cpu_time.h"
#include "webrtc/base/platform_thread.h"
#include "webrtc/base/timeutils.h"
#include "webrtc/test/gtest.h"
#include "webrtc/system_wrappers/include/cpu_info.h"
#include "webrtc/system_wrappers/include/sleep.h"

namespace {
const int kAllowedErrorMillisecs = 30;
const int kProcessingTimeMillisecs = 300;
const int kWorkingThreads = 2;

// Consumes approximately kProcessingTimeMillisecs of CPU time in single thread.
bool WorkingFunction(void* counter_pointer) {
  int64_t* counter = reinterpret_cast<int64_t*>(counter_pointer);
  *counter = 0;
  int64_t stop_cpu_time =
      rtc::GetThreadCpuTimeNanos() +
      kProcessingTimeMillisecs * rtc::kNumNanosecsPerMillisec;
  while (rtc::GetThreadCpuTimeNanos() < stop_cpu_time) {
    (*counter)++;
  }
  return false;
}
}  // namespace

namespace rtc {

TEST(CpuTimeTest, TwoThreads) {
  int64_t process_start_time_nanos = GetProcessCpuTimeNanos();
  int64_t thread_start_time_nanos = GetThreadCpuTimeNanos();
  int64_t counter1;
  int64_t counter2;
  PlatformThread thread1(WorkingFunction, reinterpret_cast<void*>(&counter1),
                         "Thread1");
  PlatformThread thread2(WorkingFunction, reinterpret_cast<void*>(&counter2),
                         "Thread2");
  thread1.Start();
  thread2.Start();
  thread1.Stop();
  thread2.Stop();

  EXPECT_GE(counter1, 0);
  EXPECT_GE(counter2, 0);
  int64_t process_duration_nanos =
      GetProcessCpuTimeNanos() - process_start_time_nanos;
  int64_t thread_duration_nanos =
      GetThreadCpuTimeNanos() - thread_start_time_nanos;
  // This thread did almost nothing.
  // Therefore GetThreadCpuTime is not a wall clock.
  EXPECT_LE(thread_duration_nanos,
            kAllowedErrorMillisecs * kNumNanosecsPerMillisec);
  // Total process time is at least twice working threads' CPU time.
  // Therefore process and thread times are correctly related.
  EXPECT_GE(
      process_duration_nanos,
      kWorkingThreads * (kProcessingTimeMillisecs - kAllowedErrorMillisecs)
      * kNumNanosecsPerMillisec);
}

TEST(CpuTimeTest, Sleeping) {
  int64_t process_start_time_nanos = GetProcessCpuTimeNanos();
  webrtc::SleepMs(kProcessingTimeMillisecs);
  int64_t process_duration_nanos =
      GetProcessCpuTimeNanos() - process_start_time_nanos;
  // Sleeping should not introduce any additional CPU time.
  // Therefore GetProcessCpuTime is not a wall clock.
  EXPECT_LE(process_duration_nanos,
            kWorkingThreads * kAllowedErrorMillisecs * kNumNanosecsPerMillisec);
}

}  // namespace rtc
