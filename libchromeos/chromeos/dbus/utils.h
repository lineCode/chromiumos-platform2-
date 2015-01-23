// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LIBCHROMEOS_CHROMEOS_DBUS_UTILS_H_
#define LIBCHROMEOS_CHROMEOS_DBUS_UTILS_H_

#include <memory>
#include <string>

#include <chromeos/chromeos_export.h>
#include <chromeos/errors/error.h>
#include <dbus/exported_object.h>
#include <dbus/message.h>
#include <dbus/scoped_dbus_error.h>

namespace chromeos {
namespace dbus_utils {

using MethodCallHandler =
    base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall*)>;

CHROMEOS_EXPORT dbus::ExportedObject::MethodCallCallback
    GetExportableDBusMethod(const MethodCallHandler& handler);

// A helper function to create a D-Bus error response object as unique_ptr<>.
CHROMEOS_EXPORT std::unique_ptr<dbus::Response> CreateDBusErrorResponse(
    dbus::MethodCall* method_call,
    const std::string& code,
    const std::string& message);

// Create a D-Bus error response object from chromeos::Error. If the last
// error in the error chain belongs to "dbus" error domain, its error code
// and message are directly translated to D-Bus error code and message.
// Any inner errors are formatted as "domain/code:message" string and appended
// to the D-Bus error message, delimited by semi-colons.
CHROMEOS_EXPORT std::unique_ptr<dbus::Response> GetDBusError(
    dbus::MethodCall* method_call,
    const chromeos::Error* error);

// TODO(avakulenko): Until dbus::ScopedDBusError has inline dbus-1 function
// calls removed from its header file, we need to create a wrapper around it.
// This way we can hide calls to low-level dbus API calls from the call sites.
// See http://crbug.com/416628
class CHROMEOS_EXPORT ScopedDBusErrorWrapper : public dbus::ScopedDBusError {
 public:
  // Do not inline constructor/destructor.
  ScopedDBusErrorWrapper();
  ~ScopedDBusErrorWrapper();
  // Hide the function of ScopedDBusError and provide our own...
  bool is_set() const;
};

}  // namespace dbus_utils
}  // namespace chromeos

#endif  // LIBCHROMEOS_CHROMEOS_DBUS_UTILS_H_