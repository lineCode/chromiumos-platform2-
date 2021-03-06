// Copyright (c) 2014 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LOGIN_MANAGER_FAKE_GENERATOR_JOB_H_
#define LOGIN_MANAGER_FAKE_GENERATOR_JOB_H_

#include "login_manager/generator_job.h"

#include <signal.h>
#include <sys/types.h>

#include <string>

#include <base/memory/scoped_ptr.h>
#include <base/time/time.h>
#include <gmock/gmock.h>

namespace login_manager {
class FakeGeneratorJob : public GeneratorJobInterface {
 public:
  class Factory : public GeneratorJobFactoryInterface {
   public:
    Factory(pid_t pid,
            const std::string& name,
            const std::string& key_contents);
    virtual ~Factory();
    scoped_ptr<GeneratorJobInterface> Create(
        const std::string& filename,
        const base::FilePath& user_path,
        uid_t desired_uid,
        SystemUtils* utils) override;
   private:
    pid_t pid_;
    const std::string name_;
    const std::string key_contents_;
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  FakeGeneratorJob(pid_t pid,
                   const std::string& name,
                   const std::string& key_contents,
                   const std::string& filename);
  virtual ~FakeGeneratorJob();

  bool RunInBackground() override;
  MOCK_METHOD2(KillEverything, void(int, const std::string&));
  MOCK_METHOD2(Kill, void(int, const std::string&));
  MOCK_METHOD1(WaitAndAbort, void(base::TimeDelta));

  const std::string GetName() const override { return name_; }
  pid_t CurrentPid() const override { return pid_; }

 private:
  pid_t pid_;
  const std::string name_;
  const std::string key_contents_;
  const std::string filename_;

  DISALLOW_COPY_AND_ASSIGN(FakeGeneratorJob);
};
}  // namespace login_manager

#endif  // LOGIN_MANAGER_FAKE_GENERATOR_JOB_H_
