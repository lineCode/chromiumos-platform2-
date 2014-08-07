// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_USB_DEVICE_H_
#define MIST_USB_DEVICE_H_

#include <stdint.h>

#include <string>

#include <base/basictypes.h>
#include <base/memory/scoped_ptr.h>
#include <base/memory/weak_ptr.h>

#include "mist/usb_constants.h"
#include "mist/usb_error.h"

struct libusb_device;
struct libusb_device_descriptor;
struct libusb_device_handle;

namespace mist {

class UsbConfigDescriptor;
class UsbDeviceDescriptor;

// A USB device, which wraps a libusb_device C struct from libusb 1.0 and
// related libusb library functions into a C++ object.
class UsbDevice : public base::SupportsWeakPtr<UsbDevice> {
 public:
  // Constructs a UsbDevice object by taking a raw pointer to a libusb_device
  // struct as |device|. The ownership of |device| is not transferred, but its
  // reference count is increased by one during the lifetime of this object.
  explicit UsbDevice(libusb_device* device);

  // Constructs a UsbDevice object by taking a raw pointer to a
  // libusb_device_handle struct as |device_handle|. The device is considered
  // open and the corresponding libusb_device struct is obtained via
  // |device_handle| and has its reference count increased by one during the
  // lifetime of this object. |device_handle| is closed when this object is
  // destructed.
  explicit UsbDevice(libusb_device_handle* device_handle);

  // Destructs this UsbDevice object, closes any open libusb_device_handle, and
  // decreases the reference count of the underlying libusb_device struct by
  // one.
  ~UsbDevice();

  // Returns true if the device is open.
  bool IsOpen() const;

  // Opens the device. Returns true on success. It is a no-op if the device is
  // already open.
  bool Open();

  // Closes the device. It is a no-op if the device is not open.
  void Close();

  // Reinitializes the device by performing a USB port reset. Returns true on
  // success.
  bool Reset();

  uint8_t GetBusNumber() const;
  uint8_t GetDeviceAddress() const;
  UsbSpeed GetDeviceSpeed() const;

  bool GetConfiguration(int* configuration);
  bool SetConfiguration(int configuration);

  bool ClaimInterface(int interface_number);
  bool ReleaseInterface(int interface_number);
  bool SetInterfaceAlternateSetting(int interface_number,
                                    int alternate_setting);
  bool IsKernelDriverActive(int interface_number);
  bool AttachKernelDriver(int interface_number);
  bool DetachKernelDriver(int interface_number);

  bool ClearHalt(uint8_t endpoint_address);

  // Returns a pointer to a UsbConfigDescriptor object for the descriptor of the
  // currently active configuration, or a NULL pointer on error. The returned
  // object should be deleted by the caller after used and should not be held
  // beyond the lifetime of this object.
  UsbConfigDescriptor* GetActiveConfigDescriptor();

  // Returns a pointer to a UsbConfigDescriptor object for the configuration
  // descriptor indexed at |index|, or a NULL pointer if the index is invalid.
  // The returned object should be deleted by the caller after use and should
  // not be held beyond the lifetime of this object.
  UsbConfigDescriptor* GetConfigDescriptor(uint8_t index);

  // Returns a pointer to a UsbConfigDescriptor object for the
  // configuration descriptor with configuration value |configuration_value|,
  // or a NULL pointer if the configuration value is invalid. The returned
  // object should be deleted by the caller after use and should not be held
  // beyond the lifetime of this object.
  UsbConfigDescriptor* GetConfigDescriptorByValue(
      uint8_t configuration_value);

  // Returns a pointer to a UsbDeviceDescriptor object for the descriptor
  // of this device, or a NULL pointer on error. The returned object should be
  // deleted by the caller after use and should not be held beyond the lifetime
  // of this object.
  UsbDeviceDescriptor* GetDeviceDescriptor();

  // Returns a string value, in ASCII, of string descriptor indexed at |index|,
  // or an empty string if the index is invalid.
  std::string GetStringDescriptorAscii(uint8_t index);

  libusb_device_handle* device_handle() const { return device_handle_; }
  const UsbError& error() const { return error_; }

 private:
  // Verifies that the device is open, and if so, returns true. Otherwise, set
  // |error_| to UsbError::kErrorDeviceNotOpen and returns false.
  bool VerifyOpen();

  libusb_device* device_;
  libusb_device_handle* device_handle_;
  scoped_ptr<libusb_device_descriptor> device_descriptor_;
  UsbError error_;

  DISALLOW_COPY_AND_ASSIGN(UsbDevice);
};

}  // namespace mist

#endif  // MIST_USB_DEVICE_H_
