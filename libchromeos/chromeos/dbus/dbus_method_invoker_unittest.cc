// Copyright 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <chromeos/dbus/dbus_method_invoker.h>

#include <string>

#include <dbus/mock_bus.h>
#include <dbus/mock_object_proxy.h>
#include <dbus/scoped_dbus_error.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

using testing::AnyNumber;
using testing::InSequence;
using testing::Invoke;
using testing::Return;
using testing::_;

using dbus::MessageReader;
using dbus::MessageWriter;
using dbus::Response;

namespace chromeos {
namespace dbus_utils {

const char kTestPath[] = "/test/path";
const char kTestServiceName[] = "org.test.Object";
const char kTestInterface[] = "org.test.Object.TestInterface";
const char kTestMethod1[] = "TestMethod1";
const char kTestMethod2[] = "TestMethod2";

class DBusMethodInvokerTest: public testing::Test {
 public:
  void SetUp() override {
    dbus::Bus::Options options;
    options.bus_type = dbus::Bus::SYSTEM;
    bus_ = new dbus::MockBus(options);
    // By default, don't worry about threading assertions.
    EXPECT_CALL(*bus_, AssertOnOriginThread()).Times(AnyNumber());
    EXPECT_CALL(*bus_, AssertOnDBusThread()).Times(AnyNumber());
    // Use a mock exported object.
    mock_object_proxy_ = new dbus::MockObjectProxy(
        bus_.get(), kTestServiceName, dbus::ObjectPath(kTestPath));
    EXPECT_CALL(*bus_, GetObjectProxy(kTestServiceName,
                                      dbus::ObjectPath(kTestPath)))
        .WillRepeatedly(Return(mock_object_proxy_.get()));
    int def_timeout_ms = dbus::ObjectProxy::TIMEOUT_USE_DEFAULT;
    EXPECT_CALL(*mock_object_proxy_,
                MockCallMethodAndBlockWithErrorDetails(_, def_timeout_ms, _))
        .WillRepeatedly(Invoke(this, &DBusMethodInvokerTest::CreateResponse));
  }

  void TearDown() override {
    bus_ = nullptr;
  }

  Response* CreateResponse(dbus::MethodCall* method_call,
                           int timeout_ms,
                           dbus::ScopedDBusError* dbus_error) {
    if (method_call->GetInterface() == kTestInterface) {
      if (method_call->GetMember() == kTestMethod1) {
        MessageReader reader(method_call);
        int v1, v2;
        // Input: two ints.
        // Output: sum of the ints converted to string.
        if (reader.PopInt32(&v1) && reader.PopInt32(&v2)) {
          auto response = Response::CreateEmpty();
          MessageWriter writer(response.get());
          writer.AppendString(std::to_string(v1 + v2));
          return response.release();
        }
      } else if (method_call->GetMember() == kTestMethod2) {
        method_call->SetSerial(123);
        dbus_set_error(dbus_error->get(), "org.MyError", "My error message");
        return nullptr;
      }
    }

    LOG(ERROR) << "Unexpected method call: " << method_call->ToString();
    return nullptr;
  }

  std::string CallTestMethod(int v1, int v2) {
    std::unique_ptr<dbus::Response> response =
        chromeos::dbus_utils::CallMethodAndBlock(mock_object_proxy_.get(),
                                                 kTestInterface,
                                                 kTestMethod1,
                                                 nullptr,
                                                 v1, v2);
    EXPECT_NE(nullptr, response.get());
    std::string result;
    using chromeos::dbus_utils::ExtractMethodCallResults;
    EXPECT_TRUE(ExtractMethodCallResults(response.get(), nullptr, &result));
    return result;
  }

  scoped_refptr<dbus::MockBus> bus_;
  scoped_refptr<dbus::MockObjectProxy> mock_object_proxy_;
};

TEST_F(DBusMethodInvokerTest, TestSuccess) {
  EXPECT_EQ("4", CallTestMethod(2, 2));
  EXPECT_EQ("10", CallTestMethod(3, 7));
  EXPECT_EQ("-4", CallTestMethod(13, -17));
}

TEST_F(DBusMethodInvokerTest, TestFailure) {
  chromeos::ErrorPtr error;
  std::unique_ptr<dbus::Response> response =
      chromeos::dbus_utils::CallMethodAndBlock(mock_object_proxy_.get(),
                                               kTestInterface,
                                               kTestMethod2,
                                               &error);
  EXPECT_EQ(nullptr, response.get());
  EXPECT_EQ(chromeos::errors::dbus::kDomain, error->GetDomain());
  EXPECT_EQ("org.MyError", error->GetCode());
  EXPECT_EQ("My error message", error->GetMessage());
}

}  // namespace dbus_utils
}  // namespace chromeos