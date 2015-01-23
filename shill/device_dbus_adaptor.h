// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_DEVICE_DBUS_ADAPTOR_H_
#define SHILL_DEVICE_DBUS_ADAPTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/dbus_adaptors/org.chromium.flimflam.Device.h"

namespace shill {

class Device;

// Subclass of DBusAdaptor for Device objects
// There is a 1:1 mapping between Device and DeviceDBusAdaptor instances.
// Furthermore, the Device owns the DeviceDBusAdaptor and manages its lifetime,
// so we're OK with DeviceDBusAdaptor having a bare pointer to its owner device.
class DeviceDBusAdaptor : public org::chromium::flimflam::Device_adaptor,
                          public DBusAdaptor,
                          public DeviceAdaptorInterface {
 public:
  static const char kPath[];

  DeviceDBusAdaptor(DBus::Connection *conn, Device *device);
  ~DeviceDBusAdaptor() override;

  // Implementation of DeviceAdaptorInterface.
  virtual const std::string &GetRpcIdentifier();
  virtual const std::string &GetRpcConnectionIdentifier();
  virtual void EmitBoolChanged(const std::string &name, bool value);
  virtual void EmitUintChanged(const std::string &name, uint32_t value);
  virtual void EmitUint16Changed(const std::string &name, uint16_t value);
  virtual void EmitIntChanged(const std::string &name, int value);
  virtual void EmitStringChanged(const std::string &name,
                                 const std::string &value);
  virtual void EmitStringmapChanged(const std::string &name,
                                    const Stringmap &value);
  virtual void EmitStringmapsChanged(const std::string &name,
                                     const Stringmaps &value);
  virtual void EmitStringsChanged(const std::string &name,
                                  const Strings &value);
  virtual void EmitKeyValueStoreChanged(const std::string &name,
                                        const KeyValueStore &value);
  virtual void EmitRpcIdentifierArrayChanged(
      const std::string &name, const std::vector<std::string> &value);

  // Implementation of Device_adaptor.
  virtual std::map<std::string, DBus::Variant> GetProperties(
      DBus::Error &error);  // NOLINT
  virtual void SetProperty(const std::string &name,
                           const DBus::Variant &value,
                           DBus::Error &error);  // NOLINT
  virtual void ClearProperty(const std::string &name,
                             DBus::Error &error);  // NOLINT
  virtual void Enable(DBus::Error &error);  // NOLINT
  virtual void Disable(DBus::Error &error);  // NOLINT
  virtual void ProposeScan(DBus::Error &error);  // NOLINT
  virtual DBus::Path AddIPConfig(const std::string &method,
                                 DBus::Error &error);  // NOLINT
  virtual void Register(const std::string &network_id,
                        DBus::Error &error);  // NOLINT
  virtual void RequirePin(const std::string &pin,
                          const bool &require,
                          DBus::Error &error);  // NOLINT
  virtual void EnterPin(const std::string &pin, DBus::Error &error);  // NOLINT
  virtual void UnblockPin(const std::string &unblock_code,
                          const std::string &pin,
                          DBus::Error &error);  // NOLINT
  virtual void ChangePin(const std::string &old_pin,
                         const std::string &new_pin,
                         DBus::Error &error);  // NOLINT
  virtual std::string PerformTDLSOperation(const std::string &operation,
                                           const std::string &peer,
                                           DBus::Error &error);  // NOLINT
  virtual void Reset(DBus::Error &error);  // NOLINT
  virtual void ResetByteCounters(DBus::Error &error);  // NOLINT
  virtual void SetCarrier(const std::string &carrier,
                          DBus::Error &error);  // NOLINT

 private:
  Device *device_;
  const std::string connection_name_;
  DISALLOW_COPY_AND_ASSIGN(DeviceDBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_DEVICE_DBUS_ADAPTOR_H_