// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "libweave/src/storage_impls.h"

#include <string>

#include <base/files/important_file_writer.h>
#include <base/json/json_writer.h>

#include "libweave/src/utils.h"

namespace weave {

FileStorage::FileStorage(const base::FilePath& file_path)
    : file_path_(file_path) {
}

std::unique_ptr<base::DictionaryValue> FileStorage::Load() {
  return LoadJsonDict(file_path_, nullptr);
}

bool FileStorage::Save(const base::DictionaryValue& config) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      config, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return base::ImportantFileWriter::WriteFileAtomically(file_path_, json);
}

std::unique_ptr<base::DictionaryValue> MemStorage::Load() {
  return std::unique_ptr<base::DictionaryValue>(cache_.DeepCopy());
}

bool MemStorage::Save(const base::DictionaryValue& config) {
  cache_.Clear();
  cache_.MergeDictionary(&config);
  return true;
}

}  // namespace weave
