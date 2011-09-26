// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/device_info.h"

#include <glib.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/netlink.h>  // Needs typedefs from sys/socket.h.
#include <linux/rtnetlink.h>

#include <base/callback_old.h>
#include <base/logging.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop.h>
#include <base/stl_util-inl.h>
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "shill/ip_address.h"
#include "shill/manager.h"
#include "shill/mock_control.h"
#include "shill/mock_glib.h"
#include "shill/mock_manager.h"
#include "shill/mock_rtnl_handler.h"
#include "shill/mock_sockets.h"
#include "shill/rtnl_message.h"

using std::map;
using std::string;
using std::vector;
using testing::_;
using testing::Return;
using testing::StrictMock;
using testing::Test;

namespace shill {

class TestEventDispatcher : public EventDispatcher {
 public:
  virtual IOInputHandler *CreateInputHandler(
      int /*fd*/,
      Callback1<InputData*>::Type */*callback*/) {
    return NULL;
  }
};

class DeviceInfoTest : public Test {
 public:
  DeviceInfoTest()
      : manager_(&control_interface_, &dispatcher_, &glib_),
        device_info_(&control_interface_, &dispatcher_, &manager_) {
  }

  virtual void SetUp() {
    device_info_.rtnl_handler_ = &rtnl_handler_;
    EXPECT_CALL(rtnl_handler_, RequestDump(RTNLHandler::kRequestLink |
                                           RTNLHandler::kRequestAddr));
  }

 protected:
  static const int kTestDeviceIndex;
  static const char kTestDeviceName[];
  static const char kTestMACAddress[];
  static const char kTestIPAddress0[];
  static const int kTestIPAddressPrefix0;
  static const char kTestIPAddress1[];
  static const int kTestIPAddressPrefix1;
  static const char kTestIPAddress2[];
  static const char kTestIPAddress3[];
  static const char kTestIPAddress4[];

  RTNLMessage *BuildLinkMessage(RTNLMessage::Mode mode);
  RTNLMessage *BuildAddressMessage(RTNLMessage::Mode mode,
                                   const IPAddress &address,
                                   unsigned char flags,
                                   unsigned char scope);
  void SendMessageToDeviceInfo(const RTNLMessage &message);

  MockGLib glib_;
  MockControl control_interface_;
  StrictMock<MockManager> manager_;
  DeviceInfo device_info_;
  TestEventDispatcher dispatcher_;
  StrictMock<MockRTNLHandler> rtnl_handler_;
};

const int DeviceInfoTest::kTestDeviceIndex = 123456;
const char DeviceInfoTest::kTestDeviceName[] = "test-device";
const char DeviceInfoTest::kTestMACAddress[] = {
  0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff };
const char DeviceInfoTest::kTestIPAddress0[] = "192.168.1.1";
const int DeviceInfoTest::kTestIPAddressPrefix0 = 24;
const char DeviceInfoTest::kTestIPAddress1[] = "fe80::1aa9:5ff:abcd:1234";
const int DeviceInfoTest::kTestIPAddressPrefix1 = 64;
const char DeviceInfoTest::kTestIPAddress2[] = "fe80::1aa9:5ff:abcd:1235";
const char DeviceInfoTest::kTestIPAddress3[] = "fe80::1aa9:5ff:abcd:1236";
const char DeviceInfoTest::kTestIPAddress4[] = "fe80::1aa9:5ff:abcd:1237";

RTNLMessage *DeviceInfoTest::BuildLinkMessage(RTNLMessage::Mode mode) {
  RTNLMessage *message = new RTNLMessage(
      RTNLMessage::kTypeLink,
      mode,
      0,
      0,
      0,
      kTestDeviceIndex,
      IPAddress::kFamilyIPv4);
  message->SetAttribute(static_cast<uint16>(IFLA_IFNAME),
                        ByteString(kTestDeviceName, true));
  ByteString test_address(kTestMACAddress, sizeof(kTestMACAddress));
  message->SetAttribute(IFLA_ADDRESS, test_address);
  return message;
}

RTNLMessage *DeviceInfoTest::BuildAddressMessage(RTNLMessage::Mode mode,
                                                 const IPAddress &address,
                                                 unsigned char flags,
                                                 unsigned char scope) {
  RTNLMessage *message = new RTNLMessage(
      RTNLMessage::kTypeAddress,
      mode,
      0,
      0,
      0,
      kTestDeviceIndex,
      address.family());
  message->SetAttribute(IFA_ADDRESS, address.address());
  message->set_address_status(
      RTNLMessage::AddressStatus(address.prefix(), flags, scope));
  return message;
}

void DeviceInfoTest::SendMessageToDeviceInfo(const RTNLMessage &message) {
  if (message.type() == RTNLMessage::kTypeLink) {
    device_info_.LinkMsgHandler(message);
  } else if (message.type() == RTNLMessage::kTypeAddress) {
    device_info_.AddressMsgHandler(message);
  } else {
    NOTREACHED();
  }
}

MATCHER_P(IsIPAddress, address, "") {
  // NB: IPAddress objects don't support the "==" operator as per style, so
  // we need a custom matcher.
  return address.Equals(arg);
}

TEST_F(DeviceInfoTest, DeviceEnumeration) {
  // Start our own private device_info
  device_info_.Start();
  scoped_ptr<RTNLMessage> message(BuildLinkMessage(RTNLMessage::kModeAdd));
  message->set_link_status(RTNLMessage::LinkStatus(0, IFF_LOWER_UP, 0));
  EXPECT_FALSE(device_info_.GetDevice(kTestDeviceIndex).get());
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetDevice(kTestDeviceIndex).get());
  unsigned int flags = 0;
  EXPECT_TRUE(device_info_.GetFlags(kTestDeviceIndex, &flags));
  EXPECT_EQ(IFF_LOWER_UP, flags);
  ByteString address;
  EXPECT_TRUE(device_info_.GetMACAddress(kTestDeviceIndex, &address));
  EXPECT_FALSE(address.IsEmpty());
  EXPECT_TRUE(address.Equals(ByteString(kTestMACAddress,
                                        sizeof(kTestMACAddress))));

  message.reset(BuildLinkMessage(RTNLMessage::kModeAdd));
  message->set_link_status(RTNLMessage::LinkStatus(0, IFF_UP | IFF_RUNNING, 0));
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetFlags(kTestDeviceIndex, &flags));
  EXPECT_EQ(IFF_UP | IFF_RUNNING, flags);

  message.reset(BuildLinkMessage(RTNLMessage::kModeDelete));
  SendMessageToDeviceInfo(*message);
  EXPECT_FALSE(device_info_.GetDevice(kTestDeviceIndex).get());
  EXPECT_FALSE(device_info_.GetFlags(kTestDeviceIndex, NULL));

  device_info_.Stop();
}

TEST_F(DeviceInfoTest, DeviceBlackList) {
  device_info_.AddDeviceToBlackList(kTestDeviceName);
  device_info_.Start();
  scoped_ptr<RTNLMessage> message(BuildLinkMessage(RTNLMessage::kModeAdd));
  SendMessageToDeviceInfo(*message);

  DeviceRefPtr device = device_info_.GetDevice(kTestDeviceIndex);
  ASSERT_TRUE(device.get());
  EXPECT_TRUE(device->TechnologyIs(Technology::kBlacklisted));

  device_info_.Stop();
}

TEST_F(DeviceInfoTest, DeviceAddressList) {
  device_info_.Start();
  scoped_ptr<RTNLMessage> message(BuildLinkMessage(RTNLMessage::kModeAdd));
  SendMessageToDeviceInfo(*message);

  vector<DeviceInfo::AddressData> addresses;
  EXPECT_TRUE(device_info_.GetAddresses(kTestDeviceIndex, &addresses));
  EXPECT_TRUE(addresses.empty());

  // Add an address to the device address list
  IPAddress ip_address0(IPAddress::kFamilyIPv4);
  EXPECT_TRUE(ip_address0.SetAddressFromString(kTestIPAddress0));
  ip_address0.set_prefix(kTestIPAddressPrefix0);
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd, ip_address0, 0, 0));
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetAddresses(kTestDeviceIndex, &addresses));
  EXPECT_EQ(1, addresses.size());
  EXPECT_TRUE(ip_address0.Equals(addresses[0].address));

  // Re-adding the same address shouldn't cause the address list to change
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetAddresses(kTestDeviceIndex, &addresses));
  EXPECT_EQ(1, addresses.size());
  EXPECT_TRUE(ip_address0.Equals(addresses[0].address));

  // Adding a new address should expand the list
  IPAddress ip_address1(IPAddress::kFamilyIPv6);
  EXPECT_TRUE(ip_address1.SetAddressFromString(kTestIPAddress1));
  ip_address1.set_prefix(kTestIPAddressPrefix1);
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd, ip_address1, 0, 0));
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetAddresses(kTestDeviceIndex, &addresses));
  EXPECT_EQ(2, addresses.size());
  EXPECT_TRUE(ip_address0.Equals(addresses[0].address));
  EXPECT_TRUE(ip_address1.Equals(addresses[1].address));

  // Deleting an address should reduce the list
  message.reset(BuildAddressMessage(RTNLMessage::kModeDelete,
                                    ip_address0,
                                    0,
                                    0));
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetAddresses(kTestDeviceIndex, &addresses));
  EXPECT_EQ(1, addresses.size());
  EXPECT_TRUE(ip_address1.Equals(addresses[0].address));

  // Delete last item
  message.reset(BuildAddressMessage(RTNLMessage::kModeDelete,
                                    ip_address1,
                                    0,
                                    0));
  SendMessageToDeviceInfo(*message);
  EXPECT_TRUE(device_info_.GetAddresses(kTestDeviceIndex, &addresses));
  EXPECT_TRUE(addresses.empty());

  // Delete device
  message.reset(BuildLinkMessage(RTNLMessage::kModeDelete));
  SendMessageToDeviceInfo(*message);

  // Should be able to handle message for interface that doesn't exist
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd, ip_address0, 0, 0));
  SendMessageToDeviceInfo(*message);
  EXPECT_FALSE(device_info_.GetDevice(kTestDeviceIndex).get());

  device_info_.Stop();
}

TEST_F(DeviceInfoTest, FlushAddressList) {
  device_info_.Start();
  scoped_ptr<RTNLMessage> message(BuildLinkMessage(RTNLMessage::kModeAdd));
  SendMessageToDeviceInfo(*message);

  IPAddress address1(IPAddress::kFamilyIPv6);
  EXPECT_TRUE(address1.SetAddressFromString(kTestIPAddress1));
  address1.set_prefix(kTestIPAddressPrefix1);
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd,
                                    address1,
                                    0,
                                    RT_SCOPE_UNIVERSE));
  SendMessageToDeviceInfo(*message);
  IPAddress address2(IPAddress::kFamilyIPv6);
  EXPECT_TRUE(address2.SetAddressFromString(kTestIPAddress2));
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd,
                                    address2,
                                    IFA_F_TEMPORARY,
                                    RT_SCOPE_UNIVERSE));
  SendMessageToDeviceInfo(*message);
  IPAddress address3(IPAddress::kFamilyIPv6);
  EXPECT_TRUE(address3.SetAddressFromString(kTestIPAddress3));
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd,
                                    address3,
                                    0,
                                    RT_SCOPE_LINK));
  SendMessageToDeviceInfo(*message);
  IPAddress address4(IPAddress::kFamilyIPv6);
  EXPECT_TRUE(address4.SetAddressFromString(kTestIPAddress4));
  message.reset(BuildAddressMessage(RTNLMessage::kModeAdd,
                                    address4,
                                    IFA_F_PERMANENT,
                                    RT_SCOPE_UNIVERSE));
  SendMessageToDeviceInfo(*message);

  // DeviceInfo now has 4 addresses associated with it, but only two of
  // them are valid for flush.
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(kTestDeviceIndex,
                                                    IsIPAddress(address1)));
  EXPECT_CALL(rtnl_handler_, RemoveInterfaceAddress(kTestDeviceIndex,
                                                    IsIPAddress(address2)));
  device_info_.FlushAddresses(kTestDeviceIndex);
  device_info_.Stop();
}

}  // namespace shill
