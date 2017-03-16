/*
 *  Copyright 2015 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import "RTCVideoFrame+Private.h"

#include "webrtc/common_video/include/corevideo_frame_buffer.h"

@implementation RTCVideoFrame {
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> _videoBuffer;
  RTCVideoRotation _rotation;
  int64_t _timeStampNs;
}

- (int)width {
  return _videoBuffer->width();
}

- (int)height {
  return _videoBuffer->height();
}

- (RTCVideoRotation)rotation {
  return _rotation;
}

- (const uint8_t *)dataY {
  return _videoBuffer->DataY();
}

- (const uint8_t *)dataU {
  return _videoBuffer->DataU();
}

- (const uint8_t *)dataV {
  return _videoBuffer->DataV();
}

- (int)strideY {
  return _videoBuffer->StrideY();
}

- (int)strideU {
  return _videoBuffer->StrideU();
}

- (int)strideV {
  return _videoBuffer->StrideV();
}

- (int64_t)timeStampNs {
  return _timeStampNs;
}

- (CVPixelBufferRef)nativeHandle {
  return static_cast<CVPixelBufferRef>(_videoBuffer->native_handle());
}

- (RTCVideoFrame *)newI420VideoFrame {
  return [[RTCVideoFrame alloc]
      initWithVideoBuffer:_videoBuffer->NativeToI420Buffer()
                 rotation:_rotation
              timeStampNs:_timeStampNs];
}

- (instancetype)initWithPixelBuffer:(CVPixelBufferRef)pixelBuffer
                           rotation:(RTCVideoRotation)rotation
                        timeStampNs:(int64_t)timeStampNs {
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> videoBuffer(
      new rtc::RefCountedObject<webrtc::CoreVideoFrameBuffer>(pixelBuffer));
  return [self initWithVideoBuffer:videoBuffer
                          rotation:rotation
                       timeStampNs:timeStampNs];
}

- (instancetype)initWithPixelBuffer:(CVPixelBufferRef)pixelBuffer
                        scaledWidth:(int)scaledWidth
                       scaledHeight:(int)scaledHeight
                          cropWidth:(int)cropWidth
                         cropHeight:(int)cropHeight
                              cropX:(int)cropX
                              cropY:(int)cropY
                           rotation:(RTCVideoRotation)rotation
                        timeStampNs:(int64_t)timeStampNs {
  rtc::scoped_refptr<webrtc::VideoFrameBuffer> videoBuffer(
      new rtc::RefCountedObject<webrtc::CoreVideoFrameBuffer>(
          pixelBuffer,
          scaledWidth, scaledHeight,
          cropWidth, cropHeight,
          cropX, cropY));
  return [self initWithVideoBuffer:videoBuffer
                          rotation:rotation
                       timeStampNs:timeStampNs];
}

#pragma mark - Private

- (instancetype)initWithVideoBuffer:
                    (rtc::scoped_refptr<webrtc::VideoFrameBuffer>)videoBuffer
                           rotation:(RTCVideoRotation)rotation
                        timeStampNs:(int64_t)timeStampNs {
  if (self = [super init]) {
    _videoBuffer = videoBuffer;
    _rotation = rotation;
    _timeStampNs = timeStampNs;
  }
  return self;
}

@end
