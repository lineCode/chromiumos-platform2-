// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CROS_DISKS_FILESYSTEM_H_
#define CROS_DISKS_FILESYSTEM_H_

#include <string>
#include <vector>

namespace cros_disks {

class Filesystem {
 public:
  explicit Filesystem(const std::string& type);

  ~Filesystem() = default;

  bool accepts_user_and_group_id() const {
    return accepts_user_and_group_id_;
  }
  void set_accepts_user_and_group_id(bool accepts_user_and_group_id) {
    accepts_user_and_group_id_ = accepts_user_and_group_id;
  }

  const std::vector<std::string>& extra_mount_options() const {
    return extra_mount_options_;
  }
  void set_extra_mount_options(
      const std::vector<std::string>& extra_mount_options) {
    extra_mount_options_ = extra_mount_options;
  }

  // Adds an option to the list of extra mount options.
  void AddExtraMountOption(const std::string& option) {
    extra_mount_options_.push_back(option);
  }

  bool is_mounted_read_only() const { return is_mounted_read_only_; }
  void set_is_mounted_read_only(bool is_mounted_read_only) {
    is_mounted_read_only_ = is_mounted_read_only;
  }

  const std::string& mount_type() const {
    return mount_type_.empty() ? type_ : mount_type_;
  }
  void set_mount_type(const std::string& mount_type) {
    mount_type_ = mount_type;
  }

  const std::string& mounter_type() const {
    return mounter_type_;
  }
  void set_mounter_type(const std::string& mounter_type) {
    mounter_type_ = mounter_type;
  }

  const std::string& type() const { return type_; }
  void set_type(const std::string& type) { type_ = type; }

 private:
  // This variable is set to true if default user and group ID can be
  // specified for mounting the filesystem.
  bool accepts_user_and_group_id_;

  // Extra mount options to specify when mounting the filesystem.
  std::vector<std::string> extra_mount_options_;

  // This variable is set to true if the filesystem should be mounted
  // as read-only.
  bool is_mounted_read_only_;

  // Filesystem type that is passed to the mounter when mounting the
  // filesystem.
  std::string mount_type_;

  // Type of mounter to use for mounting the filesystem.
  std::string mounter_type_;

  // Filesystem type.
  std::string type_;
};

}  // namespace cros_disks

#endif  // CROS_DISKS_FILESYSTEM_H_
