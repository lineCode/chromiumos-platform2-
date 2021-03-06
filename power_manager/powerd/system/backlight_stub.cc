// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/system/backlight_stub.h"

namespace power_manager {
namespace system {

BacklightStub::BacklightStub(int64_t max_level, int64_t current_level)
    : clock_(nullptr),
      max_level_(max_level),
      current_level_(current_level),
      resume_level_(-1),
      transition_in_progress_(false),
      should_fail_(false) {
}

BacklightStub::~BacklightStub() {}

int64_t BacklightStub::GetMaxBrightnessLevel() {
  return max_level_;
}

int64_t BacklightStub::GetCurrentBrightnessLevel() {
  return current_level_;
}

bool BacklightStub::SetBrightnessLevel(int64_t level,
                                       base::TimeDelta interval) {
  if (level != current_level_) {
    last_set_brightness_level_time_ =
        clock_ ? clock_->GetCurrentTime() : base::TimeTicks::Now();
  }
  if (should_fail_)
    return false;
  current_level_ = level;
  current_interval_ = interval;
  return true;
}

bool BacklightStub::SetResumeBrightnessLevel(int64_t level) {
  if (should_fail_)
    return false;
  resume_level_ = level;
  return true;
}

bool BacklightStub::TransitionInProgress() const {
  return transition_in_progress_;
}

}  // namespace system
}  // namespace power_manager
