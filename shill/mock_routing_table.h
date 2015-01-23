// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MOCK_ROUTING_TABLE_H_
#define SHILL_MOCK_ROUTING_TABLE_H_

#include <base/macros.h>
#include <gmock/gmock.h>

#include "shill/routing_table.h"

namespace shill {

class MockRoutingTable : public RoutingTable {
 public:
  MockRoutingTable();
  ~MockRoutingTable() override;

  MOCK_METHOD0(Start, void());
  MOCK_METHOD0(Stop, void());
  MOCK_METHOD2(AddRoute, bool(int interface_index,
                              const RoutingTableEntry &entry));
  MOCK_METHOD3(GetDefaultRoute, bool(int interface_index,
                                     IPAddress::Family family,
                                     RoutingTableEntry *entry));
  MOCK_METHOD3(SetDefaultRoute, bool(int interface_index,
                                     const IPAddress &gateway_address,
                                     uint32_t metric));
  MOCK_METHOD3(ConfigureRoutes, bool(int interface_index,
                                     const IPConfigRefPtr &ipconfig,
                                     uint32_t metric));
  MOCK_METHOD3(CreateBlackholeRoute, bool(int interface_index,
                                          IPAddress::Family family,
                                          uint32_t metric));
  MOCK_METHOD3(CreateLinkRoute, bool(int interface_index,
                                     const IPAddress &local_address,
                                     const IPAddress &remote_address));
  MOCK_METHOD1(FlushRoutes, void(int interface_index));
  MOCK_METHOD1(FlushRoutesWithTag, void(int tag));
  MOCK_METHOD0(FlushCache, bool());
  MOCK_METHOD1(ResetTable, void(int interface_index));
  MOCK_METHOD2(SetDefaultMetric, void(int interface_index, uint32_t metric));
  MOCK_METHOD4(RequestRouteToHost, bool(const IPAddress &addresss,
                                        int interface_index,
                                        int tag,
                                        const Query::Callback &callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockRoutingTable);
};

}  // namespace shill

#endif  // SHILL_MOCK_ROUTING_TABLE_H_