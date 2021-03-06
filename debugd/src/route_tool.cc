// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "debugd/src/route_tool.h"

#include <base/files/file_util.h>
#include <brillo/process.h>

#include "debugd/src/process_with_output.h"

namespace debugd {

const char* kIpTool = "/bin/ip";

std::vector<std::string> RouteTool::GetRoutes(
    const std::map<std::string, DBus::Variant>& options, DBus::Error* error) {
  std::vector<std::string> result;
  ProcessWithOutput p;
  if (!p.Init())
    return result;
  p.AddArg(kIpTool);

  auto option_iter = options.find("v6");
  if (option_iter != options.end() && option_iter->second.reader().get_bool())
    p.AddArg("-6");
  p.AddArg("r");  // route
  p.AddArg("s");  // show

  if (p.Run())
    return result;
  p.GetOutputLines(&result);
  return result;
}

}  // namespace debugd
