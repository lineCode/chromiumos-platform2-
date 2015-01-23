// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/dhcp_provider.h"

#include <base/files/file_util.h>
#include <base/strings/stringprintf.h>

#include "shill/control_interface.h"
#include "shill/dhcp_config.h"
#include "shill/dhcpcd_proxy.h"
#include "shill/logging.h"
#include "shill/proxy_factory.h"

using base::FilePath;
using std::string;

namespace shill {

namespace {
base::LazyInstance<DHCPProvider> g_dhcp_provider = LAZY_INSTANCE_INITIALIZER;
}  // namespace

constexpr char DHCPProvider::kDHCPCDPathFormatLease[];

DHCPProvider::DHCPProvider()
    : proxy_factory_(ProxyFactory::GetInstance()),
      root_("/"),
      control_interface_(nullptr),
      dispatcher_(nullptr),
      glib_(nullptr),
      metrics_(nullptr) {
  SLOG(DHCP, 2) << __func__;
}

DHCPProvider::~DHCPProvider() {
  SLOG(DHCP, 2) << __func__;
}

DHCPProvider* DHCPProvider::GetInstance() {
  return g_dhcp_provider.Pointer();
}

void DHCPProvider::Init(ControlInterface *control_interface,
                        EventDispatcher *dispatcher,
                        GLib *glib,
                        Metrics *metrics) {
  SLOG(DHCP, 2) << __func__;
  listener_.reset(new DHCPCDListener(proxy_factory_->connection(), this));
  glib_ = glib;
  control_interface_ = control_interface;
  dispatcher_ = dispatcher;
  metrics_ = metrics;
}

DHCPConfigRefPtr DHCPProvider::CreateConfig(const string &device_name,
                                            const string &host_name,
                                            const string &lease_file_suffix,
                                            bool arp_gateway) {
  SLOG(DHCP, 2) << __func__ << " device: " << device_name;
  return new DHCPConfig(control_interface_,
                        dispatcher_,
                        this,
                        device_name,
                        host_name,
                        lease_file_suffix,
                        arp_gateway,
                        glib_,
                        metrics_);
}

DHCPConfigRefPtr DHCPProvider::GetConfig(int pid) {
  SLOG(DHCP, 2) << __func__ << " pid: " << pid;
  PIDConfigMap::const_iterator it = configs_.find(pid);
  if (it == configs_.end()) {
    return nullptr;
  }
  return it->second;
}

void DHCPProvider::BindPID(int pid, const DHCPConfigRefPtr &config) {
  SLOG(DHCP, 2) << __func__ << " pid: " << pid;
  configs_[pid] = config;
}

void DHCPProvider::UnbindPID(int pid) {
  SLOG(DHCP, 2) << __func__ << " pid: " << pid;
  configs_.erase(pid);
}

void DHCPProvider::DestroyLease(const string &name) {
  SLOG(DHCP, 2) << __func__ << " name: " << name;
  base::DeleteFile(root_.Append(
      base::StringPrintf(kDHCPCDPathFormatLease,
                         name.c_str())), false);
}

}  // namespace shill