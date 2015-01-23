// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/mock_dbus_objectmanager_proxy.h"

#include "shill/testing.h"

using ::testing::_;
using ::testing::AnyNumber;

namespace shill {
MockDBusObjectManagerProxy::MockDBusObjectManagerProxy() {
  ON_CALL(*this, GetManagedObjects(_, _, _))
      .WillByDefault(SetOperationFailedInArgumentAndWarn<0>());
}

MockDBusObjectManagerProxy::~MockDBusObjectManagerProxy() {}

void MockDBusObjectManagerProxy::IgnoreSetCallbacks() {
  EXPECT_CALL(*this, set_interfaces_added_callback(_)).Times(AnyNumber());
  EXPECT_CALL(*this, set_interfaces_removed_callback(_)).Times(AnyNumber());
}
}  // namespace shill