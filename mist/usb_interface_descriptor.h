// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIST_USB_INTERFACE_DESCRIPTOR_H_
#define MIST_USB_INTERFACE_DESCRIPTOR_H_

#include <stdint.h>

#include <ostream>  // NOLINT(readability/streams)
#include <string>

#include <base/basictypes.h>
#include <base/memory/weak_ptr.h>

#include "mist/usb_constants.h"

struct libusb_interface_descriptor;

namespace mist {

class UsbDevice;
class UsbEndpointDescriptor;

// A USB interface descriptor, which wraps a libusb_interface_descriptor C
// struct from libusb 1.0 into a C++ object.
class UsbInterfaceDescriptor {
 public:
  // Constructs a UsbInterfaceDescriptor object by taking a weak pointer to a
  // UsbDevice object as |device| and a raw pointer to a
  // libusb_interface_descriptor struct as |interface_descriptor|. |device| is
  // used for getting USB string descriptors related to this object. The
  // ownership of |interface_descriptor| is not transferred, and thus it should
  // outlive this object.
  UsbInterfaceDescriptor(
      const base::WeakPtr<UsbDevice>& device,
      const libusb_interface_descriptor* interface_descriptor);

  ~UsbInterfaceDescriptor();

  // Getters for retrieving fields of the libusb_interface_descriptor struct.
  uint8_t GetLength() const;
  uint8_t GetDescriptorType() const;
  uint8_t GetInterfaceNumber() const;
  uint8_t GetAlternateSetting() const;
  uint8_t GetNumEndpoints() const;
  uint8_t GetInterfaceClass() const;
  uint8_t GetInterfaceSubclass() const;
  uint8_t GetInterfaceProtocol() const;
  std::string GetInterfaceDescription() const;

  // Returns a pointer to a UsbEndpointDescriptor object for the endpoint
  // descriptor indexed at |index|, or a NULL pointer if |index| is invalid.
  // The returned should be deleted by the caller after use and should not be
  // held beyond the lifetime of this object.
  UsbEndpointDescriptor* GetEndpointDescriptor(uint8_t index) const;

  // Returns a pointer to a UsbEndpointDescriptor object for the first endpoint
  // descriptor with its transfer type equal to |transfer_type| and its
  // direction equal to |direction|, or a NULL pointer if not matching endpoint
  // descriptor is found. The returned object should be deleted by the caller
  // after use and should not be held beyond the lifetime of this object.
  UsbEndpointDescriptor* GetEndpointDescriptorByTransferTypeAndDirection(
      UsbTransferType transfer_type, UsbDirection direction) const;

  // Returns a string describing the properties of this object for logging
  // purpose.
  std::string ToString() const;

 private:
  base::WeakPtr<UsbDevice> device_;
  const libusb_interface_descriptor* const interface_descriptor_;

  DISALLOW_COPY_AND_ASSIGN(UsbInterfaceDescriptor);
};

}  // namespace mist

// Output stream operator provided to facilitate logging.
std::ostream& operator<<(
    std::ostream& stream,
    const mist::UsbInterfaceDescriptor& interface_descriptor);

#endif  // MIST_USB_INTERFACE_DESCRIPTOR_H_
