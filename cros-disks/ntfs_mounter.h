// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_NTFS_MOUNTER_H_
#define CROS_DISKS_NTFS_MOUNTER_H_

#include <string>

#include "cros-disks/fuse_mounter.h"

namespace cros_disks {

// A class for mounting a device file using ntfs-3g.
class NTFSMounter : public FUSEMounter {
 public:
  // A unique type identifier of this derived mounter class.
  static const char kMounterType[];

  NTFSMounter(const std::string& source_path,
              const std::string& target_path,
              const std::string& filesystem_type,
              const MountOptions& mount_options,
              const Platform* platform);
};

}  // namespace cros_disks

#endif  // CROS_DISKS_NTFS_MOUNTER_H_
