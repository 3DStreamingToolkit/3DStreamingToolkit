/*
 *  Copyright 2012 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_FLAGDEFS_H_
#define WEBRTC_FLAGDEFS_H_

#include "webrtc/base/flags.h"

extern const uint16_t kDefaultServerPort;  // From defaults.[h|cc]
extern const int kDefaultHeartbeat;  // From defaults.[h|cc]

// Define flags for the peerconnect_client testing tool, in a separate
// header file so that they can be shared across the different main.cc's
// for each platform.

DEFINE_bool(help, false, "Prints this message");
DEFINE_bool(autoconnect, false, "Connect to the server without user "
                                "intervention.");
DEFINE_string(server, "signalingserver.centralus.cloudapp.azure.com", "The server to connect to.");
DEFINE_int(port, kDefaultServerPort,
           "The port on which the server is listening.");
DEFINE_bool(autocall, false, "Call the first available other client on "
  "the server without user intervention.  Note: this flag should only be set "
  "to true on one of the two clients.");
DEFINE_int(heartbeat, kDefaultHeartbeat, "The interval (in ms) at which heartbeat requests will be issued");

#endif  // WEBRTC_FLAGDEFS_H_
