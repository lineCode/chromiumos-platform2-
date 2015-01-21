// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_PASSIVE_LINK_MONITOR_H_
#define SHILL_MOCK_PASSIVE_LINK_MONITOR_H_

#include "shill/passive_link_monitor.h"

#include <base/macros.h>
#include <gmock/gmock.h>

namespace shill {

class MockPassiveLinkMonitor : public PassiveLinkMonitor {
 public:
  MockPassiveLinkMonitor();
  ~MockPassiveLinkMonitor() override;

  MOCK_METHOD1(Start, bool(int));
  MOCK_METHOD0(Stop, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPassiveLinkMonitor);
};

}  // namespace shill

#endif  // SHILL_MOCK_PASSIVE_LINK_MONITOR_H_