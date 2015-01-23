// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "shill/byte_string.h"

#include <netinet/in.h>
#include <string.h>

#include <algorithm>

#include <base/strings/string_number_conversions.h>

using std::distance;
using std::min;
using std::string;
using std::vector;

namespace shill {

ByteString::ByteString(const ByteString &b) {
  data_.assign(Vector::const_iterator(b.begin_), b.data_.end());
  begin_ = data_.begin();
}

ByteString &ByteString::operator=(const ByteString &b) {
  data_.assign(Vector::const_iterator(b.begin_), b.data_.end());
  begin_ = data_.begin();
  return *this;
}

unsigned char *ByteString::GetData() {
  return (GetLength() == 0) ? nullptr : &*begin_;
}

const unsigned char *ByteString::GetConstData() const {
  return (GetLength() == 0) ? nullptr : &*begin_;
}

size_t ByteString::GetLength() const {
  return distance(Vector::const_iterator(begin_), data_.end());
}

ByteString ByteString::GetSubstring(size_t offset, size_t length) const {
  if (offset > GetLength()) {
    offset = GetLength();
  }
  if (length > GetLength() - offset) {
    length = GetLength() - offset;
  }
  return ByteString(GetConstData() + offset, length);
}

// static
ByteString ByteString::CreateFromCPUUInt32(uint32_t val) {
  return ByteString(reinterpret_cast<unsigned char *>(&val), sizeof(val));
}

// static
ByteString ByteString::CreateFromNetUInt32(uint32_t val) {
  return CreateFromCPUUInt32(ntohl(val));
}

// static
ByteString ByteString::CreateFromHexString(const string &hex_string) {
  vector<uint8_t> bytes;
  if (!base::HexStringToBytes(hex_string, &bytes)) {
    return ByteString();
  }
  return ByteString(&bytes.front(), bytes.size());
}

bool ByteString::ConvertToCPUUInt32(uint32_t *val) const {
  if (val == nullptr || GetLength() != sizeof(*val)) {
    return false;
  }
  memcpy(val, GetConstData(), sizeof(*val));

  return true;
}

bool ByteString::ConvertToNetUInt32(uint32_t *val) const {
  if (!ConvertToCPUUInt32(val)) {
    return false;
  }
  *val = ntohl(*val);
  return true;
}

template <typename T>
bool ByteString::ConvertByteOrderAsUIntArray(T (*converter)(T)) {
  size_t length = GetLength();
  if ((length % sizeof(T)) != 0) {
    return false;
  }
  for (Vector::iterator i = begin_; i != data_.end(); i += sizeof(T)) {
    // Take care of word alignment.
    T val;
    memcpy(&val, &(*i), sizeof(T));
    val = converter(val);
    memcpy(&(*i), &val, sizeof(T));
  }
  return true;
}

bool ByteString::ConvertFromNetToCPUUInt32Array() {
  return ConvertByteOrderAsUIntArray(ntohl);
}

bool ByteString::ConvertFromCPUToNetUInt32Array() {
  return ConvertByteOrderAsUIntArray(htonl);
}

bool ByteString::IsZero() const {
  for (Vector::const_iterator i = begin_; i != data_.end(); ++i) {
    if (*i != 0) {
      return false;
    }
  }
  return true;
}

bool ByteString::BitwiseAnd(const ByteString &b) {
  if (GetLength() != b.GetLength()) {
    return false;
  }
  Vector::iterator lhs(begin_);
  Vector::const_iterator rhs(b.begin_);
  while (lhs != data_.end()) {
    *lhs++ &= *rhs++;
  }
  return true;
}

bool ByteString::BitwiseOr(const ByteString &b) {
  if (GetLength() != b.GetLength()) {
    return false;
  }
  Vector::iterator lhs(begin_);
  Vector::const_iterator rhs(b.begin_);
  while (lhs != data_.end()) {
    *lhs++ |= *rhs++;
  }
  return true;
}

void ByteString::BitwiseInvert() {
  for (Vector::iterator i = begin_; i != data_.end(); ++i) {
    *i = ~*i;
  }
}

bool ByteString::Equals(const ByteString &b) const {
  if (GetLength() != b.GetLength()) {
    return false;
  }
  Vector::const_iterator lhs(begin_);
  Vector::const_iterator rhs(b.begin_);
  while (lhs != data_.end()) {
    if (*lhs++ != *rhs++) {
      return false;
    }
  }
  return true;
}

void ByteString::Append(const ByteString &b) {
  // Save and reapply offset since |insert| may reallocate the memory and
  // invalidate the iterator.
  size_t offset = distance(data_.begin(), begin_);
  data_.insert(data_.end(), Vector::const_iterator(b.begin_), b.data_.end());
  begin_ = data_.begin() + offset;
}

void ByteString::Clear() {
  data_.clear();
  begin_ = data_.begin();
}

void ByteString::Resize(int size) {
  // Save and reapply offset since |resize| may reallocate the memory and
  // invalidate the iterator.
  size_t offset = distance(data_.begin(), begin_);
  data_.resize(size + offset, 0);
  begin_ = data_.begin() + offset;
}

string ByteString::HexEncode() const {
  return base::HexEncode(GetConstData(), GetLength());
}

void ByteString::RemovePrefix(size_t offset) {
  if (offset >= GetLength()) {
    begin_ = data_.end();
  } else {
    begin_ += offset;
  }
}

// static
bool ByteString::IsLessThan(const ByteString &lhs, const ByteString &rhs) {
  size_t byte_count = min(lhs.GetLength(), rhs.GetLength());
  int result = memcmp(lhs.GetConstData(), rhs.GetConstData(), byte_count);
  if (result == 0) {
    return lhs.GetLength() < rhs.GetLength();
  }
  return result < 0;
}

}  // namespace shill