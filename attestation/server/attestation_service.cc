// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "attestation/server/attestation_service.h"

#include <base/callback.h>

#include "attestation/server/database_impl.h"

namespace attestation {

bool AttestationService::Initialize() {
  if (!database_) {
    default_database_.reset(new DatabaseImpl(crypto_));
    database_ = default_database_.get();
  }
  LOG(INFO) << "Attestation service started.";
  return true;
}

void AttestationService::CreateGoogleAttestedKey(
    const std::string& key_label,
    KeyType key_type,
    KeyUsage key_usage,
    CertificateProfile certificate_profile,
    const base::Callback<CreateGoogleAttestedKeyCallback>& callback) {
  callback.Run(NOT_AVAILABLE, std::string(), std::string());
}

}  // namespace attestation
