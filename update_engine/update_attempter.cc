// Copyright (c) 2010 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "update_engine/update_attempter.h"

// From 'man clock_gettime': feature test macro: _POSIX_C_SOURCE >= 199309L
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 199309L
#endif  // _POSIX_C_SOURCE
#include <time.h>

#include <tr1/memory>
#include <string>
#include <vector>
#include <glib.h>
#include "update_engine/dbus_service.h"
#include "update_engine/download_action.h"
#include "update_engine/filesystem_copier_action.h"
#include "update_engine/libcurl_http_fetcher.h"
#include "update_engine/omaha_request_prep_action.h"
#include "update_engine/omaha_response_handler_action.h"
#include "update_engine/postinstall_runner_action.h"
#include "update_engine/set_bootable_flag_action.h"
#include "update_engine/update_check_action.h"

using std::tr1::shared_ptr;
using std::string;
using std::vector;

namespace chromeos_update_engine {

namespace {
// Returns true on success.
bool GetCPUClockTime(struct timespec* out) {
  return clock_gettime(CLOCK_REALTIME, out) == 0;
}
// Returns stop - start.
struct timespec CPUClockTimeElapsed(const struct timespec& start,
                                    const struct timespec& stop) {
  CHECK(start.tv_sec >= 0);
  CHECK(stop.tv_sec >= 0);
  CHECK(start.tv_nsec >= 0);
  CHECK(stop.tv_nsec >= 0);

  const int64_t kOneBillion = 1000000000L;
  const int64_t start64 = start.tv_sec * kOneBillion + start.tv_nsec;
  const int64_t stop64 = stop.tv_sec * kOneBillion + stop.tv_nsec;

  const int64_t result64 = stop64 - start64;

  struct timespec ret;
  ret.tv_sec = result64 / kOneBillion;
  ret.tv_nsec = result64 % kOneBillion;

  return ret;
}
bool CPUClockTimeGreaterThanHalfSecond(const struct timespec& spec) {
  if (spec.tv_sec >= 1)
    return true;
  return (spec.tv_nsec > 500000000);
}
}

const char* UpdateStatusToString(UpdateStatus status) {
  switch (status) {
    case UPDATE_STATUS_IDLE:
      return "UPDATE_STATUS_IDLE";
    case UPDATE_STATUS_CHECKING_FOR_UPDATE:
      return "UPDATE_STATUS_CHECKING_FOR_UPDATE";
    case UPDATE_STATUS_UPDATE_AVAILABLE:
      return "UPDATE_STATUS_UPDATE_AVAILABLE";
    case UPDATE_STATUS_DOWNLOADING:
      return "UPDATE_STATUS_DOWNLOADING";
    case UPDATE_STATUS_VERIFYING:
      return "UPDATE_STATUS_VERIFYING";
    case UPDATE_STATUS_FINALIZING:
      return "UPDATE_STATUS_FINALIZING";
    case UPDATE_STATUS_UPDATED_NEED_REBOOT:
      return "UPDATE_STATUS_UPDATED_NEED_REBOOT";
    default:
      return "unknown status";
  }
}

void UpdateAttempter::Update(bool force_full_update) {
  full_update_ = force_full_update;
  CHECK(!processor_.IsRunning());
  processor_.set_delegate(this);

  // Actions:
  shared_ptr<OmahaRequestPrepAction> request_prep_action(
      new OmahaRequestPrepAction(force_full_update));
  shared_ptr<UpdateCheckAction> update_check_action(
      new UpdateCheckAction(new LibcurlHttpFetcher));
  shared_ptr<OmahaResponseHandlerAction> response_handler_action(
      new OmahaResponseHandlerAction);
  shared_ptr<FilesystemCopierAction> filesystem_copier_action(
      new FilesystemCopierAction(false));
  shared_ptr<FilesystemCopierAction> kernel_filesystem_copier_action(
      new FilesystemCopierAction(true));
  shared_ptr<DownloadAction> download_action(
      new DownloadAction(new LibcurlHttpFetcher));
  shared_ptr<PostinstallRunnerAction> postinstall_runner_action_precommit(
      new PostinstallRunnerAction(true));
  shared_ptr<SetBootableFlagAction> set_bootable_flag_action(
      new SetBootableFlagAction);
  shared_ptr<PostinstallRunnerAction> postinstall_runner_action_postcommit(
      new PostinstallRunnerAction(false));
  
  download_action->set_delegate(this);
  response_handler_action_ = response_handler_action;

  actions_.push_back(shared_ptr<AbstractAction>(request_prep_action));
  actions_.push_back(shared_ptr<AbstractAction>(update_check_action));
  actions_.push_back(shared_ptr<AbstractAction>(response_handler_action));
  actions_.push_back(shared_ptr<AbstractAction>(filesystem_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(
      kernel_filesystem_copier_action));
  actions_.push_back(shared_ptr<AbstractAction>(download_action));
  actions_.push_back(shared_ptr<AbstractAction>(
      postinstall_runner_action_precommit));
  actions_.push_back(shared_ptr<AbstractAction>(set_bootable_flag_action));
  actions_.push_back(shared_ptr<AbstractAction>(
      postinstall_runner_action_postcommit));
  
  // Enqueue the actions
  for (vector<shared_ptr<AbstractAction> >::iterator it = actions_.begin();
       it != actions_.end(); ++it) {
    processor_.EnqueueAction(it->get());
  }

  // Bond them together. We have to use the leaf-types when calling
  // BondActions().
  BondActions(request_prep_action.get(),
              update_check_action.get());
  BondActions(update_check_action.get(),
              response_handler_action.get());
  BondActions(response_handler_action.get(),
              filesystem_copier_action.get());
  BondActions(filesystem_copier_action.get(),
              kernel_filesystem_copier_action.get());
  BondActions(kernel_filesystem_copier_action.get(),
              download_action.get());
  BondActions(download_action.get(),
              postinstall_runner_action_precommit.get());
  BondActions(postinstall_runner_action_precommit.get(),
              set_bootable_flag_action.get());
  BondActions(set_bootable_flag_action.get(),
              postinstall_runner_action_postcommit.get());

  SetStatusAndNotify(UPDATE_STATUS_CHECKING_FOR_UPDATE);
  processor_.StartProcessing();
}

void UpdateAttempter::CheckForUpdate() {
  if (status_ != UPDATE_STATUS_IDLE) {
    LOG(INFO) << "Check for update requested, but status is "
              << UpdateStatusToString(status_) << ", so not checking.";
    return;
  }
  Update(false);
}

// Delegate methods:
void UpdateAttempter::ProcessingDone(const ActionProcessor* processor,
                                     bool success) {
  CHECK(response_handler_action_);
  LOG(INFO) << "Processing Done.";
  if (success) {
    SetStatusAndNotify(UPDATE_STATUS_UPDATED_NEED_REBOOT);
  } else {
    LOG(INFO) << "Update failed.";
    SetStatusAndNotify(UPDATE_STATUS_IDLE);
  }
}

void UpdateAttempter::ProcessingStopped(const ActionProcessor* processor) {
  download_progress_ = 0.0;
  SetStatusAndNotify(UPDATE_STATUS_IDLE);
}

// Called whenever an action has finished processing, either successfully
// or otherwise.
void UpdateAttempter::ActionCompleted(ActionProcessor* processor,
                                      AbstractAction* action,
                                      bool success) {
  // Reset download progress regardless of whether or not the download action
  // succeeded.
  const string type = action->Type();
  if (type == DownloadAction::StaticType())
    download_progress_ = 0.0;
  if (!success)
    return;
  // Find out which action completed.
  if (type == OmahaResponseHandlerAction::StaticType()) {
    SetStatusAndNotify(UPDATE_STATUS_DOWNLOADING);
    OmahaResponseHandlerAction* omaha_response_handler_action = 
        dynamic_cast<OmahaResponseHandlerAction*>(action);
    CHECK(omaha_response_handler_action);
    const InstallPlan& plan = omaha_response_handler_action->install_plan();
    last_checked_time_ = time(NULL);
    // TODO(adlr): put version in InstallPlan
    new_version_ = "0.0.0.0";
    new_size_ = plan.size;
  } else if (type == DownloadAction::StaticType()) {
    SetStatusAndNotify(UPDATE_STATUS_FINALIZING);
  }
}

// Stop updating. An attempt will be made to record status to the disk
// so that updates can be resumed later.
void UpdateAttempter::Terminate() {
  // TODO(adlr): implement this method.
  NOTIMPLEMENTED();
}

// Try to resume from a previously Terminate()d update.
void UpdateAttempter::ResumeUpdating() {
  // TODO(adlr): implement this method.
  NOTIMPLEMENTED();
}

void UpdateAttempter::BytesReceived(uint64_t bytes_received, uint64_t total) {
  if (status_ != UPDATE_STATUS_DOWNLOADING) {
    LOG(ERROR) << "BytesReceived called while not downloading.";
    return;
  }
  download_progress_ = static_cast<double>(bytes_received) /
      static_cast<double>(total);
  // We self throttle here
  timespec now;
  now.tv_sec = 0;
  now.tv_nsec = 0;
  if (GetCPUClockTime(&now) &&
      CPUClockTimeGreaterThanHalfSecond(
          CPUClockTimeElapsed(last_notify_time_, now))) {
    SetStatusAndNotify(UPDATE_STATUS_DOWNLOADING);
  }
}

bool UpdateAttempter::GetStatus(int64_t* last_checked_time,
                                double* progress,
                                std::string* current_operation,
                                std::string* new_version,
                                int64_t* new_size) {
  *last_checked_time = last_checked_time_;
  *progress = download_progress_;
  *current_operation = UpdateStatusToString(status_);
  *new_version = new_version_;
  *new_size = new_size_;
  return true;
}

void UpdateAttempter::SetStatusAndNotify(UpdateStatus status) {
  status_ = status;
  if (!dbus_service_)
    return;
  GetCPUClockTime(&last_notify_time_);
  update_engine_service_emit_status_update(
      dbus_service_,
      last_checked_time_,
      download_progress_,
      UpdateStatusToString(status_),
      new_version_.c_str(),
      new_size_);
}

}  // namespace chromeos_update_engine
