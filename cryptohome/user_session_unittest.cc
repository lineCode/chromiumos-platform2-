// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Unit tests for UserSession.

#include "cryptohome/user_session.h"

#include <brillo/secure_blob.h>
#include <gtest/gtest.h>
#include <string>

#include "cryptohome/cryptolib.h"
#include "cryptohome/mock_platform.h"
#include "cryptohome/username_passkey.h"

using brillo::SecureBlob;

namespace cryptohome {

class UserSessionTest : public ::testing::Test {
 public:
  UserSessionTest()
      : salt() { }
  virtual ~UserSessionTest() { }

  void SetUp() {
    salt.resize(16);
    CryptoLib::GetSecureRandom(salt.data(), salt.size());
  }

 protected:
  SecureBlob salt;

 private:
  DISALLOW_COPY_AND_ASSIGN(UserSessionTest);
};

TEST_F(UserSessionTest, InitTest) {
  UsernamePasskey up("username", SecureBlob("password"));
  UserSession session;
  session.Init(salt);
  EXPECT_TRUE(session.SetUser(up));
}

TEST_F(UserSessionTest, CheckUserTest) {
  UsernamePasskey up("username", SecureBlob("password"));
  UserSession session;
  session.Init(salt);
  EXPECT_TRUE(session.SetUser(up));
  EXPECT_TRUE(session.CheckUser(up));
}

TEST_F(UserSessionTest, ReInitTest) {
  UsernamePasskey up("username", SecureBlob("password"));
  UsernamePasskey up_new("username2", SecureBlob("password2"));
  UserSession session;
  session.Init(salt);
  EXPECT_TRUE(session.SetUser(up));
  EXPECT_TRUE(session.SetUser(up_new));
  EXPECT_FALSE(session.CheckUser(up));
  EXPECT_TRUE(session.CheckUser(up_new));
}

TEST_F(UserSessionTest, ResetTest) {
  UsernamePasskey up("username", SecureBlob("password"));
  UserSession session;
  session.Init(salt);
  EXPECT_TRUE(session.SetUser(up));
  session.Reset();
  EXPECT_FALSE(session.CheckUser(up));
}

TEST_F(UserSessionTest, VerifyTest) {
  UsernamePasskey up("username", SecureBlob("password"));
  UserSession session;
  session.Init(salt);
  EXPECT_TRUE(session.SetUser(up));
  EXPECT_TRUE(session.Verify(up));
}

}  // namespace cryptohome
