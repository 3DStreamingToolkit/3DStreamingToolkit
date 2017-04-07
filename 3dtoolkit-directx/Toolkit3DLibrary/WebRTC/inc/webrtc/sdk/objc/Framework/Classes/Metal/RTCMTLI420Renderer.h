/*
 *  Copyright 2017 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#import <Foundation/Foundation.h>
#import <MetalKit/MTKView.h>

#import "WebRTC/RTCVideoFrame.h"

/**
 * Protocol defining ability to render RTCVideoFrame in Metal enabled views.
 */
@protocol RTCMTLRenderer <NSObject>

/**
 * Method to be implemented to perform actual rendering of the provided frame.
 *
 * @param frame The frame to be rendered.
 */
- (void)drawFrame:(RTCVideoFrame *)frame;
@end

NS_AVAILABLE_MAC(10.11)
/**
 * Implementation of RTCMTLRenderer protocol for rendering native nv12 video frames.
 */
@interface RTCMTLI420Renderer : NSObject <RTCMTLRenderer>

/**
 * Sets the provided view as rendering destination if possible.
 *
 * If not possible method returns NO and callers of the method are responisble for performing
 * cleanups.
 */
- (BOOL)addRenderingDestination:(__kindof MTKView *)view;
@end
