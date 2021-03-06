// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gobi-cromo-plugin/gobi_modem_factory.h"

#include <memory>

#include <base/logging.h>

#include "gobi-cromo-plugin/gobi_2k_modem.h"
#include "gobi-cromo-plugin/gobi_3k_modem.h"
#include "gobi-cromo-plugin/gobi_cdma_modem.h"
#include "gobi-cromo-plugin/gobi_gsm_modem.h"

GobiModem* GobiModemFactory::CreateModem(DBus::Connection& connection,
                                         const DBus::Path& path,
                                         gobi::DeviceElement& device,
                                         gobi::Sdk* sdk) {
  // Determine whether modem is configured for CDMA or
  // GSM, and instantiate an GobiModem object of the
  // corresponding type.
  ULONG rc = sdk->QCWWANConnect(device.deviceNode, device.deviceKey);
  if (rc != 0) {
    LOG(ERROR) << "CreateModem: QCWWANConnect() failed: " << rc;
    return nullptr;
  }

  ULONG firmware_id;
  ULONG technology;
  ULONG carrier;
  ULONG region;
  ULONG gps_capability;
  rc = sdk->GetFirmwareInfo(&firmware_id,
                            &technology,
                            &carrier,
                            &region,
                            &gps_capability);
  if (rc != 0) {
    LOG(ERROR) << "CreateModem: cannot get firmware info: " << rc;
    (void)sdk->QCWWANDisconnect();
    return nullptr;
  }
  GobiType devtype = GetDeviceType();
  std::unique_ptr<GobiModemHelper> helper;
  switch (devtype) {
    case GOBITYPE_2K:
      helper.reset(new Gobi2KModemHelper(sdk));
      break;
    case GOBITYPE_3K:
      helper.reset(new Gobi3KModemHelper(sdk));
      break;
    default:
      LOG(ERROR) << "CreateModem: cannot get device type: " << devtype;
      (void)sdk->QCWWANDisconnect();
      return nullptr;
  }

  GobiModem *modem = nullptr;
  switch (technology) {
    case gobi::kConfigurationCdma:
      modem = new GobiCdmaModem(connection, path, device, sdk,
                                helper.release());
      LOG(INFO) << "CreateModem: CDMA modem";
      break;
    case gobi::kConfigurationUmts:
      modem = new GobiGsmModem(connection, path, device, sdk, helper.release());
      LOG(INFO) << "CreateModem: GSM modem";
      break;
    case gobi::kConfigurationUnknownTechnology:
      LOG(ERROR) << "CreateModem: technology is unknown";
      break;
    default:
      LOG(ERROR) << "CreateModem: invalid technology value " << technology;
      break;
  }
  rc = sdk->QCWWANDisconnect();
  if (rc != 0)
    LOG(WARNING) << "CreateModem: QCWWANDisconnect failed: " << rc;
  return modem;
}
