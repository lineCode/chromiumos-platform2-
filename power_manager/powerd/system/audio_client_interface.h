// Copyright 2016 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_INTERFACE_H_
#define POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_INTERFACE_H_

#include <base/macros.h>

namespace power_manager {
namespace system {

class AudioObserver;

// Interface for monitoring system audio activity.
class AudioClientInterface {
 public:
  AudioClientInterface() {}
  virtual ~AudioClientInterface() {}

  // Returns the current state of the headphone jack and of HDMI audio.
  virtual bool GetHeadphoneJackPlugged() const = 0;
  virtual bool GetHdmiActive() const = 0;

  // Adds or removes an observer.
  virtual void AddObserver(AudioObserver* observer) = 0;
  virtual void RemoveObserver(AudioObserver* observer) = 0;

  // Suspends or resumes the audio according to the value of |suspended|.
  virtual void SetSuspended(bool suspended) = 0;

  // Calls Update*() to load the initial state from CRAS.
  virtual void LoadInitialState() = 0;

  // Updates the client's view of connected audio devices.
  virtual void UpdateDevices() = 0;

  // Updates the client's view of the number of active audio streams and
  // notifies observers if the state changed.
  virtual void UpdateNumActiveStreams() = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AudioClientInterface);
};

}  // namespace system
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_SYSTEM_AUDIO_CLIENT_INTERFACE_H_
