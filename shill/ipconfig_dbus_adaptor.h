// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_IPCONFIG_DBUS_ADAPTOR_H_
#define SHILL_IPCONFIG_DBUS_ADAPTOR_H_

#include <map>
#include <string>
#include <vector>

#include <base/macros.h>
#include <dbus-c++/dbus.h>

#include "shill/adaptor_interfaces.h"
#include "shill/dbus_adaptor.h"
#include "shill/dbus_adaptors/org.chromium.flimflam.IPConfig.h"

namespace shill {

class IPConfig;

// Subclass of DBusAdaptor for IPConfig objects
// There is a 1:1 mapping between IPConfig and IPConfigDBusAdaptor
// instances.  Furthermore, the IPConfig owns the IPConfigDBusAdaptor
// and manages its lifetime, so we're OK with IPConfigDBusAdaptor
// having a bare pointer to its owner ipconfig.
class IPConfigDBusAdaptor : public org::chromium::flimflam::IPConfig_adaptor,
                            public DBusAdaptor,
                            public IPConfigAdaptorInterface {
 public:
  static const char kInterfaceName[];
  static const char kPath[];

  IPConfigDBusAdaptor(DBus::Connection *conn, IPConfig *ipconfig);
  ~IPConfigDBusAdaptor() override;

  // Implementation of IPConfigAdaptorInterface.
  virtual const std::string &GetRpcIdentifier() { return path(); }
  virtual void EmitBoolChanged(const std::string &name, bool value);
  virtual void EmitUintChanged(const std::string &name, uint32_t value);
  virtual void EmitIntChanged(const std::string &name, int value);
  virtual void EmitStringChanged(const std::string &name,
                                 const std::string &value);
  virtual void EmitStringsChanged(const std::string &name,
                                  const std::vector<std::string> &value);

  // Implementation of IPConfig_adaptor
  virtual std::map<std::string, DBus::Variant> GetProperties(
      DBus::Error &error);  // NOLINT
  virtual void SetProperty(const std::string &name,
                           const DBus::Variant &value,
                           DBus::Error &error);  // NOLINT
  virtual void ClearProperty(const std::string &name,
                             DBus::Error &error);  // NOLINT
  virtual void Remove(DBus::Error &error);  // NOLINT
  virtual void Refresh(DBus::Error &error);  // NOLINT

 private:
  IPConfig *ipconfig_;
  DISALLOW_COPY_AND_ASSIGN(IPConfigDBusAdaptor);
};

}  // namespace shill

#endif  // SHILL_IPCONFIG_DBUS_ADAPTOR_H_