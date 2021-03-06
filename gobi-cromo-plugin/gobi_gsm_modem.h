// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// GSM specialization of Gobi Modem

#ifndef GOBI_CROMO_PLUGIN_GOBI_GSM_MODEM_H_
#define GOBI_CROMO_PLUGIN_GOBI_GSM_MODEM_H_

#include <map>
#include <string>
#include <vector>

#include <cromo/dbus_adaptors/org.freedesktop.ModemManager.Modem.Gsm.h>
#include <cromo/dbus_adaptors/org.freedesktop.ModemManager.Modem.Gsm.Card.h>
#include <cromo/dbus_adaptors/org.freedesktop.ModemManager.Modem.Gsm.Network.h>
#include <cromo/dbus_adaptors/org.freedesktop.ModemManager.Modem.Gsm.SMS.h>
#include <cromo/sms_cache.h>

#include "gobi-cromo-plugin/gobi_modem.h"

class GobiModem;

typedef std::vector<std::map<std::string, std::string>> ScannedNetworkList;

class GobiGsmModem
    : public GobiModem,
      public cromo::SmsModemOperations,
      public org::freedesktop::ModemManager::Modem::Gsm_adaptor,
      public org::freedesktop::ModemManager::Modem::Gsm::Card_adaptor,
      public org::freedesktop::ModemManager::Modem::Gsm::Network_adaptor,
      public org::freedesktop::ModemManager::Modem::Gsm::SMS_adaptor {
 public:
  GobiGsmModem(DBus::Connection& connection,
               const DBus::Path& path,
               const gobi::DeviceElement& device,
               gobi::Sdk *sdk,
                GobiModemHelper *modem_helper)
      : GobiModem(connection, path, device, sdk, modem_helper) {}
  virtual ~GobiGsmModem();

  // DBUS Methods: Modem.Gsm.Network
  virtual void Register(const std::string& network_id, DBus::Error& error);
  virtual ScannedNetworkList Scan(DBus::Error& error);
  virtual void SetApn(const std::string& apn, DBus::Error& error);
  virtual uint32_t GetSignalQuality(DBus::Error& error);
  virtual void SetBand(const uint32_t& band, DBus::Error& error);
  virtual uint32_t GetBand(DBus::Error& error);
  virtual void SetNetworkMode(const uint32_t& mode, DBus::Error& error);
  virtual uint32_t GetNetworkMode(DBus::Error& error);
  virtual DBus::Struct<uint32_t, std::string, std::string> GetRegistrationInfo(
      DBus::Error& error);
  virtual void SetAllowedMode(const uint32_t& mode, DBus::Error& error);

  // DBUS Methods: Modem.Gsm.Card
  virtual std::string GetImei(DBus::Error& error);
  virtual std::string GetImsi(DBus::Error& error);
  virtual void SendPuk(const std::string& puk,
                       const std::string& pin,
                       DBus::Error& error);
  virtual void SendPin(const std::string& pin, DBus::Error& error);
  virtual void EnablePin(const std::string& pin,
                         const bool& enabled,
                         DBus::Error& error);
  virtual void ChangePin(const std::string& old_pin,
                         const std::string& new_pin,
                         DBus::Error& error);
  virtual std::string GetOperatorId(DBus::Error& error);
  virtual std::string GetSpn(DBus::Error& error);
  virtual std::string GetMsIsdn(DBus::Error& error);

  // DBUS Methods: Modem.Gsm.SMS
  virtual void Delete(const uint32_t& index, DBus::Error &error);
  virtual utilities::DBusPropertyMap Get(const uint32_t& index,
                                         DBus::Error &error);
  virtual uint32_t GetFormat(DBus::Error &error);
  virtual void SetFormat(const uint32_t& format, DBus::Error &error);
  virtual std::string GetSmsc(DBus::Error &error);
  virtual void SetSmsc(const std::string& smsc, DBus::Error &error);
  virtual std::vector<utilities::DBusPropertyMap> List(DBus::Error &error);
  virtual std::vector<uint32_t> Save(
      const utilities::DBusPropertyMap& properties,
      DBus::Error &error);
  virtual std::vector<uint32_t> Send(
      const utilities::DBusPropertyMap& properties,
      DBus::Error &error);
  virtual void SendFromStorage(const uint32_t& index, DBus::Error &error);
  virtual void SetIndication(const uint32_t& mode,
                             const uint32_t& mt,
                             const uint32_t& bm,
                             const uint32_t& ds,
                             const uint32_t& bfr,
                             DBus::Error &error);

  // Low-level implementation routines for the SMS code
  virtual cromo::SmsMessageFragment* GetSms(int index, DBus::Error& error);
  virtual void DeleteSms(int index, DBus::Error& error);
  virtual std::vector<int>* ListSms(DBus::Error& error);

 protected:
  void GetGsmRegistrationInfo(uint32_t* registration_state,
                              std::string* operator_code,
                              std::string* operator_name,
                              DBus::Error& error);
  // Overrides and extensions of GobiModem functions
  virtual void RegisterCallbacks();
  virtual void RegistrationStateHandler();
  virtual void DataCapabilitiesHandler(BYTE num_data_caps,
                                       ULONG* data_caps);
  virtual void DataBearerTechnologyHandler(ULONG technology);
  virtual void SignalStrengthHandler(INT8 signal_strength,
                                     ULONG radio_interface);
  virtual void SetTechnologySpecificProperties();
  virtual void GetTechnologySpecificStatus(
      utilities::DBusPropertyMap* properties);
  virtual bool CheckEnableOk(DBus::Error &error);

  struct NewSmsArgs : public CallbackArgs {
    NewSmsArgs(ULONG storage_type,
               ULONG message_index)
      : storage_type(storage_type),
        message_index(message_index) { }
    ULONG storage_type;
    ULONG message_index;
  };
  static void NewSmsCallbackTrampoline(ULONG storage_type,
                                       ULONG message_index) {
    PostCallbackRequest(NewSmsCallback,
                        new NewSmsArgs(storage_type,
                                       message_index));
  }
  static gboolean NewSmsCallback(gpointer data);

 private:
  void SendNetworkTechnologySignal(uint32_t mm_access_tech);
  bool GetPinStatus(bool* enabled, std::string* status, uint32_t* retries_left);
  void UpdatePinStatus();
  uint32_t GetMmAccessTechnology();

  static gboolean CheckDataCapabilities(gpointer data);

  cromo::SmsCache sms_cache_;
};

#endif  // GOBI_CROMO_PLUGIN_GOBI_GSM_MODEM_H_
