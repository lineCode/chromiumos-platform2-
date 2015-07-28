// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DBUS_CHROMEOS_SUPPLICANT_INTERFACE_PROXY_H_
#define SHILL_DBUS_CHROMEOS_SUPPLICANT_INTERFACE_PROXY_H_

#include <string>
#include <vector>

#include <base/macros.h>

#include "shill/refptr_types.h"
#include "shill/supplicant/supplicant_interface_proxy_interface.h"
#include "supplicant/dbus-proxies.h"

namespace shill {

class SupplicantEventDelegateInterface;

// ChromeosSupplicantInterfaceProxy. provides access to wpa_supplicant's
// network-interface APIs via D-Bus.  This takes a delegate, which
// is an interface that is used to send notifications of supplicant
// events.  This pointer is not owned by ChromeosSupplicantInterfaceProxy
// and must outlive the proxy.
class ChromeosSupplicantInterfaceProxy
    : public SupplicantInterfaceProxyInterface {
 public:
  ChromeosSupplicantInterfaceProxy(
      const scoped_refptr<dbus::Bus>& bus,
      const std::string& object_path,
      SupplicantEventDelegateInterface* delegate);
  ~ChromeosSupplicantInterfaceProxy() override;

  // Implementation of SupplicantInterfaceProxyInterface.
  bool AddNetwork(const KeyValueStore& args, std::string* network) override;
  bool EnableHighBitrates() override;
  bool EAPLogon() override;
  bool EAPLogoff() override;
  bool Disconnect() override;
  bool FlushBSS(const uint32_t& age) override;
  bool NetworkReply(const std::string& network,
                    const std::string& field,
                    const std::string& value) override;
  bool Reassociate() override;
  bool Reattach() override;
  bool RemoveAllNetworks() override;
  bool RemoveNetwork(const std::string& network) override;
  bool Scan(const KeyValueStore& args) override;
  bool SelectNetwork(const std::string& network) override;
  bool TDLSDiscover(const std::string& peer) override;
  bool TDLSSetup(const std::string& peer) override;
  bool TDLSStatus(const std::string& peer, std::string* status) override;
  bool TDLSTeardown(const std::string& peer) override;
  bool SetHT40Enable(const std::string& network, bool enable) override;
  // The below set functions will always return true, since PropertySet::Set
  // is an async method. Any failures will be logged in the callback.
  bool SetFastReauth(bool enabled) override;
  bool SetRoamThreshold(uint16_t threshold) override;
  bool SetScanInterval(int seconds) override;
  bool SetDisableHighBitrates(bool disable_high_bitrates) override;
  bool SetSchedScan(bool enable) override;
  bool SetScan(bool enable) override;

 private:
  class PropertySet : public dbus::PropertySet {
   public:
    PropertySet(dbus::ObjectProxy* object_proxy,
                const std::string& interface_name,
                const PropertyChangedCallback& callback);
    dbus::Property<uint32_t> ap_scan;
    dbus::Property<std::vector<std::string>> blobs;
    dbus::Property<std::string> bridge_ifname;
    dbus::Property<std::vector<dbus::ObjectPath>> bsss;
    dbus::Property<chromeos::VariantDictionary> capabilities;
    dbus::Property<dbus::ObjectPath> current_bss;
    dbus::Property<dbus::ObjectPath> current_network;
    dbus::Property<bool> disable_high_bitrates;
    dbus::Property<std::string> driver;
    dbus::Property<bool> fast_reauth;
    dbus::Property<std::string> ifname;
    dbus::Property<std::vector<dbus::ObjectPath>> networks;
    dbus::Property<uint16_t> roam_threshold;
    dbus::Property<bool> scan;
    dbus::Property<uint32_t> scan_interval;
    dbus::Property<bool> scanning;
    dbus::Property<bool> sched_scan;
    dbus::Property<std::string> state;

   private:
    DISALLOW_COPY_AND_ASSIGN(PropertySet);
  };

  static const char kInterfaceName[];
  static const char kPropertyApScan[];
  static const char kPropertyBlobs[];
  static const char kPropertyBridgeIfname[];
  static const char kPropertyBSSs[];
  static const char kPropertyCapabilities[];
  static const char kPropertyCurrentBss[];
  static const char kPropertyCurrentNetwork[];
  static const char kPropertyDisableHighBitrates[];
  static const char kPropertyDriver[];
  static const char kPropertyFastReauth[];
  static const char kPropertyIfname[];
  static const char kPropertyNetworks[];
  static const char kPropertyRoamThreshold[];
  static const char kPropertyScan[];
  static const char kPropertyScanInterval[];
  static const char kPropertyScanning[];
  static const char kPropertySchedScan[];
  static const char kPropertyState[];

  // Signal handlers.
  void BlobAdded(const std::string& blobname);
  void BlobRemoved(const std::string& blobname);
  void BSSAdded(const dbus::ObjectPath& BSS,
                const chromeos::VariantDictionary& properties);
  void BSSRemoved(const dbus::ObjectPath& BSS);
  void Certification(const chromeos::VariantDictionary& properties);
  void EAP(const std::string& status, const std::string& parameter);
  void NetworkAdded(const dbus::ObjectPath& network,
                    const chromeos::VariantDictionary& properties);
  void NetworkRemoved(const dbus::ObjectPath& network);
  void NetworkSelected(const dbus::ObjectPath& network);
  void PropertiesChanged(
      const chromeos::VariantDictionary& properties);
  void ScanDone(bool success);
  void TDLSDiscoverResponse(const std::string& peer_address);

  // Callback invoked when the value of property |property_name| is changed.
  void OnPropertyChanged(const std::string& property_name);

  // Called when signal is connected to the ObjectProxy.
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool success);

  // Callback invoked when property |property_name| set completed.
  void OnPropertySet(const std::string& property_name, bool success);

  std::unique_ptr<fi::w1::wpa_supplicant1::InterfaceProxy> interface_proxy_;
  std::unique_ptr<PropertySet> properties_;

  // This pointer is owned by the object that created |this|.  That object
  // MUST destroy |this| before destroying itself.
  SupplicantEventDelegateInterface* delegate_;

  base::WeakPtrFactory<ChromeosSupplicantInterfaceProxy> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(ChromeosSupplicantInterfaceProxy);
};

}  // namespace shill

#endif  // SHILL_DBUS_CHROMEOS_SUPPLICANT_INTERFACE_PROXY_H_