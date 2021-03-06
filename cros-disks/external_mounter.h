// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_EXTERNAL_MOUNTER_H_
#define CROS_DISKS_EXTERNAL_MOUNTER_H_

#include <string>

#include "cros-disks/mounter.h"

namespace cros_disks {

// A class for mounting a device file using an external mount program.
class ExternalMounter : public Mounter {
 public:
  // A unique type identifier of this derived mounter class.
  static const char kMounterType[];

  ExternalMounter(const std::string& source_path,
                  const std::string& target_path,
                  const std::string& filesystem_type,
                  const MountOptions& mount_options);

  // Returns the full path of an external mount program if it is found in
  // some predefined locations. Otherwise, an empty string is returned.
  std::string GetMountProgramPath() const;

 protected:
  // Mounts a device file using an external mount program.
  MountErrorType MountImpl() override;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_EXTERNAL_MOUNTER_H_
