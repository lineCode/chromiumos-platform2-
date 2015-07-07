// Copyright 2015 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "settingsd/settings_keys.h"

namespace settingsd {
namespace keys {

const char kSettingsdPrefix[] = "org.chromium.settings";

const char kSources[] = "sources";

namespace sources {

const char kName[] = "name";
const char kStatus[] = "status";
const char kType[] = "type";
const char kAccess[] = "access";

}  // namespace sources
}  // namespace keys
}  // namespace settingsd