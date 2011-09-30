// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHAPS_CHAPS_ADAPTOR_H
#define CHAPS_CHAPS_ADAPTOR_H

#include <base/basictypes.h>

#include "chaps/chaps_adaptor_generated.h"

namespace chaps {

class ChapsInterface;

// The ChapsAdaptor class implements the dbus-c++ generated adaptor interface
// and redirects IPC calls to a ChapsInterface instance.  All dbus-c++ specific
// logic, error handling, etc. is implemented here.  Specifically, the
// ChapsInterface instance need not be aware of dbus-c++ or IPC.  This class
// exists because we don't want to couple dbus-c++ with the Chaps service
// implementation.
class ChapsAdaptor : public org::chromium::Chaps_adaptor,
                     public DBus::ObjectAdaptor {
public:
  ChapsAdaptor(ChapsInterface* service);
  virtual ~ChapsAdaptor();

  virtual void GetSlotList(const bool& token_present,
                           std::vector<uint32_t>& slot_list,
                           uint32_t& result,
                           ::DBus::Error &error);
  virtual void GetSlotInfo(const uint32_t& slot_id,
                           std::string& slot_description,
                           std::string& manufacturer_id,
                           uint32_t& flags,
                           uint8_t& hardware_version_major,
                           uint8_t& hardware_version_minor,
                           uint8_t& firmware_version_major,
                           uint8_t& firmware_version_minor,
                           uint32_t& result,
                           ::DBus::Error &error);
  virtual void GetTokenInfo(const uint32_t& slot_id,
                            std::string& label,
                            std::string& manufacturer_id,
                            std::string& model,
                            std::string& serial_number,
                            uint32_t& flags,
                            uint32_t& max_session_count,
                            uint32_t& session_count,
                            uint32_t& max_session_count_rw,
                            uint32_t& session_count_rw,
                            uint32_t& max_pin_len,
                            uint32_t& min_pin_len,
                            uint32_t& total_public_memory,
                            uint32_t& free_public_memory,
                            uint32_t& total_private_memory,
                            uint32_t& free_private_memory,
                            uint8_t& hardware_version_major,
                            uint8_t& hardware_version_minor,
                            uint8_t& firmware_version_major,
                            uint8_t& firmware_version_minor,
                            uint32_t& result,
                            ::DBus::Error &error);
  virtual void GetMechanismList(const uint32_t& slot_id,
                                std::vector<uint32_t>& mechanism_list,
                                uint32_t& result,
                                ::DBus::Error &error);
  virtual void GetMechanismInfo(const uint32_t& slot_id,
                                const uint32_t& mechanism_type,
                                uint32_t& min_key_size,
                                uint32_t& max_key_size,
                                uint32_t& flags,
                                uint32_t& result,
                                ::DBus::Error &error);

private:
  ChapsInterface* service_;

  DISALLOW_COPY_AND_ASSIGN(ChapsAdaptor);
};

}  // namespace
#endif  // CHAPS_CHAPS_ADAPTOR_H
