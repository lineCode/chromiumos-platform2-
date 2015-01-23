// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_SUPPLICANT_INTERFACE_PROXY_INTERFACE_H_
#define SHILL_SUPPLICANT_INTERFACE_PROXY_INTERFACE_H_

#include <map>
#include <string>

#include <dbus-c++/dbus.h>

namespace shill {

// SupplicantInterfaceProxyInterface declares only the subset of
// fi::w1::wpa_supplicant1::Interface_proxy that is actually used by WiFi.
class SupplicantInterfaceProxyInterface {
 public:
  virtual ~SupplicantInterfaceProxyInterface() {}

  virtual ::DBus::Path AddNetwork(
      const std::map<std::string, ::DBus::Variant> &args) = 0;
  virtual void EnableHighBitrates() = 0;
  virtual void EAPLogoff() = 0;
  virtual void EAPLogon() = 0;
  virtual void Disconnect() = 0;
  virtual void FlushBSS(const uint32_t &age) = 0;
  virtual void NetworkReply(const ::DBus::Path &network,
                            const std::string &field,
                            const std::string &value) = 0;
  virtual void Reassociate() = 0;
  virtual void Reattach() = 0;
  virtual void RemoveAllNetworks() = 0;
  virtual void RemoveNetwork(const ::DBus::Path &network) = 0;
  virtual void Scan(
      const std::map<std::string, ::DBus::Variant> &args) = 0;
  virtual void SelectNetwork(const ::DBus::Path &network) = 0;
  virtual void SetFastReauth(bool enabled) = 0;
  virtual void SetRoamThreshold(uint16_t seconds) = 0;
  virtual void SetScanInterval(int seconds) = 0;
  virtual void SetDisableHighBitrates(bool disable_high_bitrates) = 0;
  virtual void TDLSDiscover(const std::string &peer) = 0;
  virtual void TDLSSetup(const std::string &peer) = 0;
  virtual std::string TDLSStatus(const std::string &peer) = 0;
  virtual void TDLSTeardown(const std::string &peer) = 0;
  virtual void SetHT40Enable(const ::DBus::Path &network, bool enable) = 0;
};

}  // namespace shill

#endif  // SHILL_SUPPLICANT_INTERFACE_PROXY_INTERFACE_H_