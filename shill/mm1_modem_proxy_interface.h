// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SHILL_MM1_MODEM_PROXY_INTERFACE_H_
#define SHILL_MM1_MODEM_PROXY_INTERFACE_H_

#include <string>
#include <vector>

#include "shill/callbacks.h"

namespace shill {
class Error;

namespace mm1 {

typedef base::Callback<void(int32_t,
                            int32_t, uint32_t)> ModemStateChangedSignalCallback;

// These are the methods that a org.freedesktop.ModemManager1.Modem
// proxy must support. The interface is provided so that it can be
// mocked in tests. All calls are made asynchronously. Call completion
// is signalled via the callbacks passed to the methods.
class ModemProxyInterface {
 public:
  virtual ~ModemProxyInterface() {}

  virtual void Enable(bool enable,
                      Error *error,
                      const ResultCallback &callback,
                      int timeout) = 0;
  virtual void CreateBearer(const DBusPropertiesMap &properties,
                            Error *error,
                            const DBusPathCallback &callback,
                            int timeout) = 0;
  virtual void DeleteBearer(const ::DBus::Path &bearer,
                            Error *error,
                            const ResultCallback &callback,
                            int timeout) = 0;
  virtual void Reset(Error *error,
                     const ResultCallback &callback,
                     int timeout) = 0;
  virtual void FactoryReset(const std::string &code,
                            Error *error,
                            const ResultCallback &callback,
                            int timeout) = 0;
  virtual void SetCurrentCapabilities(const uint32_t &capabilities,
                                      Error *error,
                                      const ResultCallback &callback,
                                      int timeout) = 0;
  virtual void SetCurrentModes(const ::DBus::Struct<uint32_t, uint32_t> &modes,
                               Error *error,
                               const ResultCallback &callback,
                               int timeout) = 0;
  virtual void SetCurrentBands(const std::vector<uint32_t> &bands,
                               Error *error,
                               const ResultCallback &callback,
                               int timeout) = 0;
  virtual void Command(const std::string &cmd,
                       const uint32_t &user_timeout,
                       Error *error,
                       const StringCallback &callback,
                       int timeout) = 0;
  virtual void SetPowerState(const uint32_t &power_state,
                             Error *error,
                             const ResultCallback &callback,
                             int timeout) = 0;


  virtual void set_state_changed_callback(
      const ModemStateChangedSignalCallback &callback) = 0;
};

}  // namespace mm1
}  // namespace shill

#endif  // SHILL_MM1_MODEM_PROXY_INTERFACE_H_