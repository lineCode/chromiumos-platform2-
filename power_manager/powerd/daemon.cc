// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "power_manager/powerd/daemon.h"

#include <algorithm>
#include <cmath>
#include <map>

#include <base/bind.h>
#include <base/files/file_util.h>
#include <base/format_macros.h>
#include <base/logging.h>
#include <base/strings/string_number_conversions.h>
#include <base/strings/string_split.h>
#include <base/strings/string_util.h>
#include <base/strings/stringprintf.h>
#include <base/threading/platform_thread.h>
#include <chromeos/dbus/service_constants.h>
#include <dbus/message.h>

#include "cryptohome/proto_bindings/rpc.pb.h"
#include "power_manager/common/metrics_sender.h"
#include "power_manager/common/power_constants.h"
#include "power_manager/common/prefs.h"
#include "power_manager/common/util.h"
#if USE_BUFFET
#include "power_manager/powerd/buffet/command_handlers.h"
#endif
#include "power_manager/powerd/daemon_delegate.h"
#include "power_manager/powerd/metrics_collector.h"
#include "power_manager/powerd/policy/backlight_controller.h"
#include "power_manager/powerd/policy/keyboard_backlight_controller.h"
#include "power_manager/powerd/policy/state_controller.h"
#include "power_manager/powerd/policy/wakeup_controller.h"
#include "power_manager/powerd/system/acpi_wakeup_helper_interface.h"
#include "power_manager/powerd/system/ambient_light_sensor.h"
#include "power_manager/powerd/system/audio_client_interface.h"
#include "power_manager/powerd/system/backlight_interface.h"
#include "power_manager/powerd/system/dark_resume.h"
#include "power_manager/powerd/system/dbus_wrapper.h"
#include "power_manager/powerd/system/display/display_power_setter.h"
#include "power_manager/powerd/system/display/display_watcher.h"
#include "power_manager/powerd/system/ec_wakeup_helper_interface.h"
#include "power_manager/powerd/system/event_device_interface.h"
#include "power_manager/powerd/system/input_watcher_interface.h"
#include "power_manager/powerd/system/peripheral_battery_watcher.h"
#include "power_manager/powerd/system/power_supply.h"
#include "power_manager/powerd/system/udev.h"
#include "power_manager/proto_bindings/policy.pb.h"
#include "power_manager/proto_bindings/power_supply_properties.pb.h"

#if USE_BUFFET
namespace dbus {
class Bus;
}  // namespace dbus
#endif  // USE_BUFFET

namespace power_manager {
namespace {

// Default values for |*_path_| members (which can be overridden for tests).
const char kDefaultSuspendedStatePath[] =
    "/var/lib/power_manager/powerd_suspended";
const char kDefaultWakeupCountPath[] = "/sys/power/wakeup_count";
const char kDefaultOobeCompletedPath[] = "/home/chronos/.oobe_completed";
const char kDefaultFlashromLockPath[] = "/run/lock/flashrom_powerd.lock";
const char kDefaultBatteryToolLockPath[] = "/run/lock/battery_tool_powerd.lock";
const char kDefaultProcPath[] = "/proc";

// Basename appended to |run_dir| (see Daemon's c'tor) to produce
// |suspend_announced_path_|.
const char kSuspendAnnouncedFile[] = "suspend_announced";

// Strings for states that powerd cares about from the session manager's
// SessionStateChanged signal.
const char kSessionStarted[] = "started";

// When noticing that the firmware is being updated while suspending, wait up to
// this long for the update to finish before reporting a suspend failure. The
// event loop is blocked during this period.
const int kFirmwareUpdateTimeoutMs = 500;

// Interval between successive polls during kFirmwareUpdateTimeoutMs.
const int kFirmwareUpdatePollMs = 100;

// Interval between attempts to retry shutting the system down while
// |flashrom_lock_path_| or |battery_lock_path_| exist, in seconds.
const int kRetryShutdownForFirmwareUpdateSec = 5;

// Interval between log messages while audio is active, in seconds.
const int kLogAudioSec = 180;

// Maximum amount of time to wait for responses to D-Bus method calls to other
// processes.
const int kSessionManagerDBusTimeoutMs = 3000;
const int kUpdateEngineDBusTimeoutMs = 3000;
const int kCryptohomedDBusTimeoutMs = 2 * 60 * 1000;  // Two minutes.

// Passes |method_call| to |handler| and passes the response to
// |response_sender|. If |handler| returns NULL, an empty response is created
// and sent.
void HandleSynchronousDBusMethodCall(
    base::Callback<std::unique_ptr<dbus::Response>(dbus::MethodCall*)> handler,
    dbus::MethodCall* method_call,
    dbus::ExportedObject::ResponseSender response_sender) {
  std::unique_ptr<dbus::Response> response = handler.Run(method_call);
  if (!response)
    response = dbus::Response::FromMethodCall(method_call);
  response_sender.Run(std::move(response));
}

// Creates a new "not supported" reply to |method_call|.
std::unique_ptr<dbus::Response> CreateNotSupportedError(
    dbus::MethodCall* method_call,
    std::string message) {
  return std::unique_ptr<dbus::Response>(
      dbus::ErrorResponse::FromMethodCall(
          method_call, DBUS_ERROR_NOT_SUPPORTED, message));
}

// Creates a new "invalid args" reply to |method_call|.
std::unique_ptr<dbus::Response> CreateInvalidArgsError(
    dbus::MethodCall* method_call,
    std::string message) {
  return std::unique_ptr<dbus::Response>(
      dbus::ErrorResponse::FromMethodCall(
          method_call, DBUS_ERROR_INVALID_ARGS, message));
}

// Intended to aid in debugging: if powerd only logs when audio starts, people
// often get confused about why their system is blocked from suspending.
void LogAudioActivity() {
  LOG(INFO) << "Audio is still active";
}

}  // namespace

// Performs actions requested by |state_controller_|.  The reason that
// this is a nested class of Daemon rather than just being implemented as
// part of Daemon is to avoid method naming conflicts.
class Daemon::StateControllerDelegate
    : public policy::StateController::Delegate {
 public:
  explicit StateControllerDelegate(Daemon* daemon) : daemon_(daemon) {}
  virtual ~StateControllerDelegate() {
    daemon_ = NULL;
  }

  // Overridden from policy::StateController::Delegate:
  bool IsUsbInputDeviceConnected() override {
    return daemon_->input_watcher_->IsUSBInputDeviceConnected();
  }

  bool IsOobeCompleted() override {
    return base::PathExists(base::FilePath(daemon_->oobe_completed_path_));
  }

  bool IsHdmiAudioActive() override {
    return daemon_->audio_client_ ?
        daemon_->audio_client_->GetHdmiActive() : false;
  }

  bool IsHeadphoneJackPlugged() override {
    return daemon_->audio_client_ ?
        daemon_->audio_client_->GetHeadphoneJackPlugged() : false;
  }

  LidState QueryLidState() override {
    return daemon_->input_watcher_->QueryLidState();
  }

  void DimScreen() override {
    daemon_->SetBacklightsDimmedForInactivity(true);
  }

  void UndimScreen() override {
    daemon_->SetBacklightsDimmedForInactivity(false);
  }

  void TurnScreenOff() override {
    daemon_->SetBacklightsOffForInactivity(true);
  }

  void TurnScreenOn() override {
    daemon_->SetBacklightsOffForInactivity(false);
  }

  void LockScreen() override {
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerLockScreen);
    daemon_->dbus_wrapper_->CallMethodSync(
        daemon_->session_manager_dbus_proxy_, &method_call,
        base::TimeDelta::FromMilliseconds(kSessionManagerDBusTimeoutMs));
  }

  void Suspend() override {
    daemon_->Suspend(false, 0);
  }

  void StopSession() override {
    // This session manager method takes a string argument, although it
    // doesn't currently do anything with it.
    dbus::MethodCall method_call(
        login_manager::kSessionManagerInterface,
        login_manager::kSessionManagerStopSession);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString("");
    daemon_->dbus_wrapper_->CallMethodSync(
        daemon_->session_manager_dbus_proxy_, &method_call,
        base::TimeDelta::FromMilliseconds(kSessionManagerDBusTimeoutMs));
  }

  void ShutDown() override {
    daemon_->ShutDown(SHUTDOWN_MODE_POWER_OFF,
                      SHUTDOWN_REASON_STATE_TRANSITION);
  }

  void UpdatePanelForDockedMode(bool docked) override {
    daemon_->SetBacklightsDocked(docked);
  }

  void EmitIdleActionImminent(
      base::TimeDelta time_until_idle_action) override {
    IdleActionImminent proto;
    proto.set_time_until_idle_action(time_until_idle_action.ToInternalValue());
    daemon_->dbus_wrapper_->EmitSignalWithProtocolBuffer(
        kIdleActionImminentSignal, proto);
  }

  void EmitIdleActionDeferred() override {
    daemon_->dbus_wrapper_->EmitBareSignal(kIdleActionDeferredSignal);
  }

  void ReportUserActivityMetrics() override {
    daemon_->metrics_collector_->GenerateUserActivityMetrics();
  }

 private:
  Daemon* daemon_;  // weak

  DISALLOW_COPY_AND_ASSIGN(StateControllerDelegate);
};

Daemon::Daemon(DaemonDelegate* delegate, const base::FilePath& run_dir)
    : delegate_(delegate),
      session_manager_dbus_proxy_(nullptr),
      update_engine_dbus_proxy_(nullptr),
      cryptohomed_dbus_proxy_(nullptr),
      state_controller_delegate_(new StateControllerDelegate(this)),
      state_controller_(new policy::StateController),
      input_controller_(new policy::InputController),
      wakeup_controller_(new policy::WakeupController),
      suspender_(new policy::Suspender),
      metrics_collector_(new MetricsCollector),
      shutting_down_(false),
      retry_shutdown_for_firmware_update_timer_(false /* retain_user_task */,
                                                true /* is_repeating */),
      log_audio_timer_(false /* retain_user_task */, true /* is_repeating */),
      wakeup_count_path_(kDefaultWakeupCountPath),
      oobe_completed_path_(kDefaultOobeCompletedPath),
      flashrom_lock_path_(kDefaultFlashromLockPath),
      battery_tool_lock_path_(kDefaultBatteryToolLockPath),
      proc_path_(kDefaultProcPath),
      suspended_state_path_(kDefaultSuspendedStatePath),
      suspend_announced_path_(run_dir.Append(kSuspendAnnouncedFile)),
      session_state_(SESSION_STOPPED),
      created_suspended_state_file_(false),
      lock_vt_before_suspend_(false),
      log_suspend_with_mosys_eventlog_(false),
      suspend_to_idle_(false),
      set_wifi_transmit_power_for_tablet_mode_(false),
      weak_ptr_factory_(this) {}

Daemon::~Daemon() {
  for (auto controller : all_backlight_controllers_)
    controller->RemoveObserver(this);
  if (audio_client_)
    audio_client_->RemoveObserver(this);
  if (power_supply_)
    power_supply_->RemoveObserver(this);
}

void Daemon::Init() {
  prefs_ = delegate_->CreatePrefs();
  InitDBus();

  metrics_sender_ = delegate_->CreateMetricsSender();
  udev_ = delegate_->CreateUdev();
  input_watcher_ = delegate_->CreateInputWatcher(prefs_.get(), udev_.get());

  if (BoolPrefIsTrue(kHasAmbientLightSensorPref))
    light_sensor_ = delegate_->CreateAmbientLightSensor();

  display_watcher_ = delegate_->CreateDisplayWatcher(udev_.get());
  display_power_setter_ =
      delegate_->CreateDisplayPowerSetter(dbus_wrapper_.get());
  if (BoolPrefIsTrue(kExternalDisplayOnlyPref)) {
    display_backlight_controller_ =
        delegate_->CreateExternalBacklightController(
            display_watcher_.get(), display_power_setter_.get());
  } else {
    display_backlight_ = delegate_->CreateInternalBacklight(
        base::FilePath(kInternalBacklightPath), kInternalBacklightPattern);
    if (display_backlight_) {
      display_backlight_controller_ =
          delegate_->CreateInternalBacklightController(
              display_backlight_.get(), prefs_.get(), light_sensor_.get(),
              display_power_setter_.get());
    }
  }
  if (display_backlight_controller_)
    all_backlight_controllers_.push_back(display_backlight_controller_.get());

  if (BoolPrefIsTrue(kHasKeyboardBacklightPref)) {
    keyboard_backlight_ = delegate_->CreateInternalBacklight(
        base::FilePath(kKeyboardBacklightPath), kKeyboardBacklightPattern);
    if (keyboard_backlight_) {
      keyboard_backlight_controller_ =
          delegate_->CreateKeyboardBacklightController(
              keyboard_backlight_.get(), prefs_.get(), light_sensor_.get(),
              display_backlight_controller_.get(),
              input_watcher_->GetTabletMode());
      all_backlight_controllers_.push_back(
          keyboard_backlight_controller_.get());
    }
  }

  for (auto controller : all_backlight_controllers_)
    controller->AddObserver(this);

  prefs_->GetBool(kLockVTBeforeSuspendPref, &lock_vt_before_suspend_);
  prefs_->GetBool(kMosysEventlogPref, &log_suspend_with_mosys_eventlog_);
  prefs_->GetBool(kSuspendToIdlePref, &suspend_to_idle_);

  power_supply_ = delegate_->CreatePowerSupply(base::FilePath(kPowerStatusPath),
                                               prefs_.get(), udev_.get());
  power_supply_->AddObserver(this);
  if (!power_supply_->RefreshImmediately())
    LOG(ERROR) << "Initial power supply refresh failed; brace for weirdness";
  const system::PowerStatus power_status = power_supply_->GetPowerStatus();

  metrics_collector_->Init(prefs_.get(), display_backlight_controller_.get(),
                           keyboard_backlight_controller_.get(), power_status);

  dark_resume_ = delegate_->CreateDarkResume(power_supply_.get(), prefs_.get());
  suspender_->Init(this, dbus_wrapper_.get(), dark_resume_.get(), prefs_.get());

  input_controller_->Init(input_watcher_.get(), this, display_watcher_.get(),
                          dbus_wrapper_.get(), prefs_.get());

  acpi_wakeup_helper_ = delegate_->CreateAcpiWakeupHelper();
  ec_wakeup_helper_ = delegate_->CreateEcWakeupHelper();

  const LidState lid_state = input_watcher_->QueryLidState();
  wakeup_controller_->Init(display_backlight_controller_.get(), udev_.get(),
                           acpi_wakeup_helper_.get(), ec_wakeup_helper_.get(),
                           lid_state, DISPLAY_NORMAL, prefs_.get());

  const PowerSource power_source =
      power_status.line_power_on ? POWER_AC : POWER_BATTERY;
  state_controller_->Init(state_controller_delegate_.get(), prefs_.get(),
                          power_source, lid_state);

  if (BoolPrefIsTrue(kUseCrasPref)) {
    audio_client_ = delegate_->CreateAudioClient(dbus_wrapper_.get());
    audio_client_->AddObserver(this);
  }

  peripheral_battery_watcher_ =
      delegate_->CreatePeripheralBatteryWatcher(dbus_wrapper_.get());

  prefs_->GetBool(kSetWifiTransmitPowerForTabletModePref,
                  &set_wifi_transmit_power_for_tablet_mode_);
  if (set_wifi_transmit_power_for_tablet_mode_)
    PopulateIwlWifiTransmitPowerTable();

  // Call this last to ensure that all of our members are already initialized.
  OnPowerStatusUpdate();
}

bool Daemon::BoolPrefIsTrue(const std::string& name) const {
  bool value = false;
  return prefs_->GetBool(name, &value) && value;
}

bool Daemon::PidLockFileExists(const base::FilePath& path) {
  std::string pid;
  if (!base::ReadFileToString(path, &pid))
    return false;

  base::TrimWhitespaceASCII(pid, base::TRIM_TRAILING, &pid);
  if (!base::DirectoryExists(proc_path_.Append(pid))) {
    LOG(WARNING) << path.value() << " contains stale/invalid PID \""
                 << pid << "\"";
    return false;
  }

  return true;
}

bool Daemon::FirmwareIsBeingUpdated(std::string* details_out) {
  std::vector<std::string> paths;
  if (PidLockFileExists(flashrom_lock_path_))
    paths.push_back(flashrom_lock_path_.value());
  if (PidLockFileExists(battery_tool_lock_path_))
    paths.push_back(battery_tool_lock_path_.value());

  *details_out = base::JoinString(paths, ", ");
  return !paths.empty();
}

int Daemon::RunSetuidHelper(const std::string& action,
                            const std::string& additional_args,
                            bool wait_for_completion) {
  std::string command =
      kSetuidHelperPath + std::string(" --action=" + action);
  if (!additional_args.empty())
    command += " " + additional_args;
  if (wait_for_completion) {
    return delegate_->Run(command.c_str());
  } else {
    delegate_->Launch(command.c_str());
    return 0;
  }
}

void Daemon::AdjustKeyboardBrightness(int direction) {
  if (!keyboard_backlight_controller_)
    return;

  if (direction > 0)
    keyboard_backlight_controller_->IncreaseUserBrightness();
  else if (direction < 0)
    keyboard_backlight_controller_->DecreaseUserBrightness(
        true /* allow_off */);
}

void Daemon::SendBrightnessChangedSignal(
    double brightness_percent,
    policy::BacklightController::BrightnessChangeCause cause,
    const std::string& signal_name) {
  dbus::Signal signal(kPowerManagerInterface, signal_name);
  dbus::MessageWriter writer(&signal);
  writer.AppendInt32(round(brightness_percent));
  writer.AppendBool(cause ==
      policy::BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED);
  dbus_wrapper_->EmitSignal(&signal);
}

void Daemon::OnBrightnessChange(
    double brightness_percent,
    policy::BacklightController::BrightnessChangeCause cause,
    policy::BacklightController* source) {
  if (source == display_backlight_controller_.get() &&
      display_backlight_controller_) {
    SendBrightnessChangedSignal(brightness_percent, cause,
                                kBrightnessChangedSignal);
  } else if (source == keyboard_backlight_controller_.get() &&
             keyboard_backlight_controller_) {
    SendBrightnessChangedSignal(brightness_percent, cause,
                                kKeyboardBrightnessChangedSignal);
  } else {
    NOTREACHED() << "Received a brightness change callback from an unknown "
                 << "backlight controller";
  }
}

void Daemon::HandleLidClosed() {
  LOG(INFO) << "Lid closed";
  // It is important that we notify WakeupController first so that it can
  // inhibit input devices quickly. StateController will issue a blocking call
  // to Chrome which can take longer than a second.
  wakeup_controller_->SetLidState(LID_CLOSED);
  state_controller_->HandleLidStateChange(LID_CLOSED);
}

void Daemon::HandleLidOpened() {
  LOG(INFO) << "Lid opened";
  suspender_->HandleLidOpened();
  state_controller_->HandleLidStateChange(LID_OPEN);
  wakeup_controller_->SetLidState(LID_OPEN);
}

void Daemon::HandlePowerButtonEvent(ButtonState state) {
  // Don't log spammy repeat events if we see them.
  if (state != BUTTON_REPEAT)
    LOG(INFO) << "Power button " << ButtonStateToString(state);
  metrics_collector_->HandlePowerButtonEvent(state);
  if (state == BUTTON_DOWN)
    delegate_->Launch("sync");
  if (state == BUTTON_DOWN) {
    for (auto controller : all_backlight_controllers_)
      controller->HandlePowerButtonPress();
  }
}

void Daemon::HandleHoverStateChange(bool hovering) {
  VLOG(1) << "Hovering " << (hovering ? "on" : "off");
  for (auto controller : all_backlight_controllers_)
    controller->HandleHoverStateChange(hovering);
}

void Daemon::HandleTabletModeChange(TabletMode mode) {
  LOG(INFO) << (mode == TABLET_MODE_ON ? "Entered" : "Exited")
            << " tablet mode";
  for (auto controller : all_backlight_controllers_)
    controller->HandleTabletModeChange(mode);

  if (set_wifi_transmit_power_for_tablet_mode_) {
    std::string args = (mode == TABLET_MODE_ON) ?
        "--wifi_transmit_power_tablet" : "--nowifi_transmit_power_tablet";

    // Intel iwlwifi driver requires extra power table.
    if (!iwl_wifi_power_table_.empty()) {
      args += " --wifi_transmit_power_iwl_power_table=" + iwl_wifi_power_table_;
    }

    LOG(INFO) << ((mode == TABLET_MODE_ON) ? "Enabling": "Disabling")
              << " tablet mode wifi transmit power";
    RunSetuidHelper("set_wifi_transmit_power", args, false);
  }
}

void Daemon::DeferInactivityTimeoutForVT2() {
  LOG(INFO) << "Reporting synthetic user activity since VT2 is active";
  state_controller_->HandleUserActivity();
}

void Daemon::ShutDownForPowerButtonWithNoDisplay() {
  LOG(INFO) << "Shutting down due to power button press while no display is "
            << "connected";
  metrics_collector_->HandlePowerButtonEvent(BUTTON_DOWN);
  ShutDown(SHUTDOWN_MODE_POWER_OFF, SHUTDOWN_REASON_USER_REQUEST);
}

void Daemon::HandleMissingPowerButtonAcknowledgment() {
  LOG(INFO) << "Didn't receive power button acknowledgment from Chrome";
}

void Daemon::ReportPowerButtonAcknowledgmentDelay(base::TimeDelta delay) {
  metrics_collector_->SendPowerButtonAcknowledgmentDelayMetric(delay);
}

int Daemon::GetInitialSuspendId() {
  // Take powerd's PID modulo 2**15 (/proc/sys/kernel/pid_max is currently
  // 2**15, but just in case...) and multiply it by 2**16, leaving it able to
  // fit in a signed 32-bit int. This allows for 2**16 suspend attempts and
  // suspend delays per powerd run before wrapping or intruding on another
  // run's ID range (neither of which should be particularly problematic, but
  // doing this reduces the chances of a confused client that's using stale
  // IDs from a previous powerd run being able to conflict with the new run's
  // IDs).
  return (delegate_->GetPid() % 32768) * 65536 + 1;
}

int Daemon::GetInitialDarkSuspendId() {
  // We use the upper half of the suspend ID space for dark suspend attempts.
  // Assuming that we will go through dark suspend IDs faster than the regular
  // suspend IDs, we should never have a collision between the suspend ID and
  // the dark suspend ID until the dark suspend IDs wrap around.
  return GetInitialSuspendId() + 32768;
}

bool Daemon::IsLidClosedForSuspend() {
  return input_watcher_->QueryLidState() == LID_CLOSED;
}

bool Daemon::ReadSuspendWakeupCount(uint64_t* wakeup_count) {
  DCHECK(wakeup_count);
  std::string buf;
  LOG(INFO) << "Reading wakeup count from " << wakeup_count_path_.value();
  if (base::ReadFileToString(wakeup_count_path_, &buf)) {
    base::TrimWhitespaceASCII(buf, base::TRIM_TRAILING, &buf);
    if (base::StringToUint64(buf, wakeup_count)) {
      LOG(INFO) << "Read wakeup count " << *wakeup_count;
      return true;
    }
    LOG(ERROR) << "Could not parse wakeup count from \"" << buf << "\"";
  } else {
    PLOG(ERROR) << "Could not read " << wakeup_count_path_.value();
  }
  return false;
}

void Daemon::SetSuspendAnnounced(bool announced) {
  if (announced) {
    if (base::WriteFile(suspend_announced_path_, NULL, 0) < 0)
      PLOG(ERROR) << "Couldn't create " << suspend_announced_path_.value();
  } else {
    if (!base::DeleteFile(suspend_announced_path_, false))
      PLOG(ERROR) << "Couldn't delete " << suspend_announced_path_.value();
  }
}

bool Daemon::GetSuspendAnnounced() {
  return base::PathExists(suspend_announced_path_);
}

void Daemon::PrepareToSuspend() {
  // Before announcing the suspend request, notify the backlight controller so
  // it can turn the backlight off and tell the kernel to resume the current
  // level after resuming.  This must occur before Chrome is told that the
  // system is going to suspend (Chrome turns the display back on while leaving
  // the backlight off).
  SetBacklightsSuspended(true);

  // Do not let suspend change the console terminal.
  if (lock_vt_before_suspend_)
    RunSetuidHelper("lock_vt", "", true);

  power_supply_->SetSuspended(true);
  if (audio_client_)
    audio_client_->SetSuspended(true);
  metrics_collector_->PrepareForSuspend();
}

policy::Suspender::Delegate::SuspendResult Daemon::DoSuspend(
    uint64_t wakeup_count,
    bool wakeup_count_valid,
    base::TimeDelta duration) {
  // If a firmware update is ongoing, spin for a bit to wait for it to finish:
  // http://crosbug.com/p/38947
  const base::TimeDelta firmware_poll_interval =
      base::TimeDelta::FromMilliseconds(kFirmwareUpdatePollMs);
  const base::TimeDelta firmware_timeout =
      base::TimeDelta::FromMilliseconds(kFirmwareUpdateTimeoutMs);
  base::TimeDelta firmware_duration;
  std::string details;
  while (FirmwareIsBeingUpdated(&details)) {
    if (firmware_duration >= firmware_timeout) {
      LOG(INFO) << "Aborting suspend attempt for firmware update: " << details;
      return SUSPEND_FAILED;
    }
    firmware_duration += firmware_poll_interval;
    base::PlatformThread::Sleep(firmware_poll_interval);
  }

  // Touch a file that crash-reporter can inspect later to determine
  // whether the system was suspended while an unclean shutdown occurred.
  // If the file already exists, assume that crash-reporter hasn't seen it
  // yet and avoid unlinking it after resume.
  created_suspended_state_file_ = false;
  if (!base::PathExists(suspended_state_path_)) {
    if (base::WriteFile(suspended_state_path_, nullptr, 0) == 0)
      created_suspended_state_file_ = true;
    else
      PLOG(ERROR) << "Unable to create " << suspended_state_path_.value();
  }

  // This command is run synchronously to ensure that it finishes before the
  // system is suspended.
  if (log_suspend_with_mosys_eventlog_) {
    RunSetuidHelper("mosys_eventlog", "--mosys_eventlog_code=0xa7", true);
  }

  std::string args;
  if (wakeup_count_valid) {
    args += base::StringPrintf(" --suspend_wakeup_count_valid"
                               " --suspend_wakeup_count=%" PRIu64,
                               wakeup_count);
  }

  if (duration != base::TimeDelta()) {
    args += base::StringPrintf(" --suspend_duration=%" PRId64,
                               duration.InSeconds());
  }

  if (suspend_to_idle_)
    args += " --suspend_to_idle";

  const int exit_code = RunSetuidHelper("suspend", args, true);
  LOG(INFO) << "powerd_suspend returned " << exit_code;

  if (log_suspend_with_mosys_eventlog_)
    RunSetuidHelper("mosys_eventlog", "--mosys_eventlog_code=0xa8", false);

  if (created_suspended_state_file_) {
    if (!base::DeleteFile(base::FilePath(suspended_state_path_), false))
      PLOG(ERROR) << "Failed to delete " << suspended_state_path_.value();
  }

  // These exit codes are defined in powerd/powerd_suspend.
  switch (exit_code) {
    case 0:
      return SUSPEND_SUCCESSFUL;
    case 1:
      return SUSPEND_FAILED;
    case 2:  // Wakeup event received before write to wakeup_count.
    case 3:  // Wakeup event received after write to wakeup_count.
      return SUSPEND_CANCELED;
    default:
      LOG(ERROR) << "Treating unexpected exit code " << exit_code
                 << " as suspend failure";
      return SUSPEND_FAILED;
  }
}

void Daemon::UndoPrepareToSuspend(bool success,
                                  int num_suspend_attempts,
                                  bool canceled_while_in_dark_resume) {
  if (canceled_while_in_dark_resume && !dark_resume_->ExitDarkResume())
    ShutDown(SHUTDOWN_MODE_POWER_OFF, SHUTDOWN_REASON_EXIT_DARK_RESUME_FAILED);

  // Do this first so we have the correct settings (including for the
  // backlight).
  state_controller_->HandleResume();

  // Resume the backlight right after announcing resume. This might be where we
  // turn on the display, so we want to do this as early as possible. This
  // happens when we idle suspend (and the requested power state in Chrome is
  // off for the displays).
  SetBacklightsSuspended(false);

  // Allow virtual terminal switching again.
  if (lock_vt_before_suspend_)
    RunSetuidHelper("unlock_vt", "", true);

  if (audio_client_)
    audio_client_->SetSuspended(false);
  power_supply_->SetSuspended(false);

  if (success)
    metrics_collector_->HandleResume(num_suspend_attempts);
  else if (num_suspend_attempts > 0)
    metrics_collector_->HandleCanceledSuspendRequest(num_suspend_attempts);
}

void Daemon::GenerateDarkResumeMetrics(
    const std::vector<policy::Suspender::DarkResumeInfo>&
        dark_resume_wake_durations,
    base::TimeDelta suspend_duration) {
  metrics_collector_->GenerateDarkResumeMetrics(dark_resume_wake_durations,
                                                suspend_duration);
}

void Daemon::ShutDownForFailedSuspend() {
  ShutDown(SHUTDOWN_MODE_POWER_OFF, SHUTDOWN_REASON_SUSPEND_FAILED);
}

void Daemon::ShutDownForDarkResume() {
  ShutDown(SHUTDOWN_MODE_POWER_OFF, SHUTDOWN_REASON_DARK_RESUME);
}

void Daemon::OnAudioStateChange(bool active) {
  LOG(INFO) << "Audio is " << (active ? "active" : "inactive");
  state_controller_->HandleAudioStateChange(active);
  if (active) {
    log_audio_timer_.Start(FROM_HERE,
                           base::TimeDelta::FromSeconds(kLogAudioSec),
                           base::Bind(&LogAudioActivity));
  } else {
    log_audio_timer_.Stop();
  }
}

void Daemon::OnPowerStatusUpdate() {
  const system::PowerStatus status = power_supply_->GetPowerStatus();
  if (status.battery_is_present)
    LOG(INFO) << system::GetPowerStatusBatteryDebugString(status);

  metrics_collector_->HandlePowerStatusUpdate(status);

  const PowerSource power_source =
      status.line_power_on ? POWER_AC : POWER_BATTERY;
  for (auto controller : all_backlight_controllers_)
    controller->HandlePowerSourceChange(power_source);
  state_controller_->HandlePowerSourceChange(power_source);

  if (status.battery_is_present && status.battery_below_shutdown_threshold) {
    LOG(INFO) << "Shutting down due to low battery ("
              << base::StringPrintf("%0.2f", status.battery_percentage) << "%, "
              << util::TimeDeltaToString(status.battery_time_to_empty)
              << " until empty, "
              << base::StringPrintf(
                     "%0.3f", status.observed_battery_charge_rate)
              << "A observed charge rate)";
    ShutDown(SHUTDOWN_MODE_POWER_OFF, SHUTDOWN_REASON_LOW_BATTERY);
  }

  PowerSupplyProperties protobuf;
  system::CopyPowerStatusToProtocolBuffer(status, &protobuf);
  dbus_wrapper_->EmitSignalWithProtocolBuffer(kPowerSupplyPollSignal, protobuf);
}

void Daemon::InitDBus() {
  dbus_wrapper_ = delegate_->CreateDBusWrapper();

  dbus::ObjectProxy* chrome_proxy = dbus_wrapper_->GetObjectProxy(
      chromeos::kLibCrosServiceName, chromeos::kLibCrosServicePath);
  dbus_wrapper_->RegisterForServiceAvailability(
      chrome_proxy, base::Bind(&Daemon::HandleChromeAvailableOrRestarted,
                               weak_ptr_factory_.GetWeakPtr()));

  session_manager_dbus_proxy_ = dbus_wrapper_->GetObjectProxy(
      login_manager::kSessionManagerServiceName,
      login_manager::kSessionManagerServicePath);
  dbus_wrapper_->RegisterForServiceAvailability(
      session_manager_dbus_proxy_,
      base::Bind(&Daemon::HandleSessionManagerAvailableOrRestarted,
                 weak_ptr_factory_.GetWeakPtr()));
  dbus_wrapper_->RegisterForSignal(
      session_manager_dbus_proxy_, login_manager::kSessionManagerInterface,
      login_manager::kSessionStateChangedSignal,
      base::Bind(&Daemon::HandleSessionStateChangedSignal,
                 weak_ptr_factory_.GetWeakPtr()));

  if (BoolPrefIsTrue(kUseCrasPref)) {
    dbus::ObjectProxy* cras_proxy = dbus_wrapper_->GetObjectProxy(
        cras::kCrasServiceName, cras::kCrasServicePath);
    dbus_wrapper_->RegisterForServiceAvailability(
        cras_proxy, base::Bind(&Daemon::HandleCrasAvailableOrRestarted,
                               weak_ptr_factory_.GetWeakPtr()));
    dbus_wrapper_->RegisterForSignal(
        cras_proxy, cras::kCrasControlInterface, cras::kNodesChanged,
        base::Bind(&Daemon::HandleCrasNodesChangedSignal,
                   weak_ptr_factory_.GetWeakPtr()));
    dbus_wrapper_->RegisterForSignal(
        cras_proxy, cras::kCrasControlInterface, cras::kActiveOutputNodeChanged,
        base::Bind(&Daemon::HandleCrasActiveOutputNodeChangedSignal,
                   weak_ptr_factory_.GetWeakPtr()));
    dbus_wrapper_->RegisterForSignal(
        cras_proxy, cras::kCrasControlInterface,
        cras::kNumberOfActiveStreamsChanged,
        base::Bind(&Daemon::HandleCrasNumberOfActiveStreamsChanged,
                   weak_ptr_factory_.GetWeakPtr()));
  }

  update_engine_dbus_proxy_ = dbus_wrapper_->GetObjectProxy(
      update_engine::kUpdateEngineServiceName,
      update_engine::kUpdateEngineServicePath);
  dbus_wrapper_->RegisterForServiceAvailability(
      update_engine_dbus_proxy_,
      base::Bind(&Daemon::HandleUpdateEngineAvailable,
                 weak_ptr_factory_.GetWeakPtr()));
  dbus_wrapper_->RegisterForSignal(
      update_engine_dbus_proxy_, update_engine::kUpdateEngineInterface,
      update_engine::kStatusUpdate,
      base::Bind(&Daemon::HandleUpdateEngineStatusUpdateSignal,
                 weak_ptr_factory_.GetWeakPtr()));

  int64_t tpm_threshold = 0;
  prefs_->GetInt64(kTpmCounterSuspendThresholdPref, &tpm_threshold);
  if (tpm_threshold > 0) {
    cryptohomed_dbus_proxy_ = dbus_wrapper_->GetObjectProxy(
        cryptohome::kCryptohomeServiceName, cryptohome::kCryptohomeServicePath);
    dbus_wrapper_->RegisterForServiceAvailability(
        cryptohomed_dbus_proxy_, base::Bind(&Daemon::HandleCryptohomedAvailable,
                                            weak_ptr_factory_.GetWeakPtr()));

    int64_t tpm_status_sec = 0;
    prefs_->GetInt64(kTpmStatusIntervalSecPref, &tpm_status_sec);
    tpm_status_interval_ = base::TimeDelta::FromSeconds(tpm_status_sec);
  }

  // Export Daemon's D-Bus method calls.
  typedef std::unique_ptr<dbus::Response> (
      Daemon::*DaemonMethod)(dbus::MethodCall*);
  const std::map<const char*, DaemonMethod> kDaemonMethods = {
      {kRequestShutdownMethod, &Daemon::HandleRequestShutdownMethod},
      {kRequestRestartMethod, &Daemon::HandleRequestRestartMethod},
      {kRequestSuspendMethod, &Daemon::HandleRequestSuspendMethod},
      {kDecreaseScreenBrightnessMethod,
       &Daemon::HandleDecreaseScreenBrightnessMethod},
      {kIncreaseScreenBrightnessMethod,
       &Daemon::HandleIncreaseScreenBrightnessMethod},
      {kGetScreenBrightnessPercentMethod,
       &Daemon::HandleGetScreenBrightnessMethod},
      {kSetScreenBrightnessPercentMethod,
       &Daemon::HandleSetScreenBrightnessMethod},
      {kDecreaseKeyboardBrightnessMethod,
       &Daemon::HandleDecreaseKeyboardBrightnessMethod},
      {kIncreaseKeyboardBrightnessMethod,
       &Daemon::HandleIncreaseKeyboardBrightnessMethod},
      {kGetPowerSupplyPropertiesMethod,
       &Daemon::HandleGetPowerSupplyPropertiesMethod},
      {kHandleVideoActivityMethod, &Daemon::HandleVideoActivityMethod},
      {kHandleUserActivityMethod, &Daemon::HandleUserActivityMethod},
      {kSetIsProjectingMethod, &Daemon::HandleSetIsProjectingMethod},
      {kSetPolicyMethod, &Daemon::HandleSetPolicyMethod},
      {kSetPowerSourceMethod, &Daemon::HandleSetPowerSourceMethod},
      {kHandlePowerButtonAcknowledgmentMethod,
       &Daemon::HandlePowerButtonAcknowledgment},
  };
  for (const auto& it : kDaemonMethods) {
    dbus_wrapper_->ExportMethod(
        it.first, base::Bind(&HandleSynchronousDBusMethodCall,
                             base::Bind(it.second, base::Unretained(this))));
  }

  // Export |suspender_|'s D-Bus method calls.
  using policy::Suspender;
  typedef void (Suspender::*SuspenderMethod)(
      dbus::MethodCall*, dbus::ExportedObject::ResponseSender);
  const std::map<const char*, SuspenderMethod> kSuspenderMethods = {
      {kRegisterSuspendDelayMethod, &Suspender::RegisterSuspendDelay},
      {kUnregisterSuspendDelayMethod, &Suspender::UnregisterSuspendDelay},
      {kHandleSuspendReadinessMethod, &Suspender::HandleSuspendReadiness},
      {kRegisterDarkSuspendDelayMethod, &Suspender::RegisterDarkSuspendDelay},
      {kUnregisterDarkSuspendDelayMethod,
       &Suspender::UnregisterDarkSuspendDelay},
      {kHandleDarkSuspendReadinessMethod,
       &Suspender::HandleDarkSuspendReadiness},
      {kRecordDarkResumeWakeReasonMethod,
       &Suspender::RecordDarkResumeWakeReason},
  };
  for (const auto& it : kSuspenderMethods) {
    dbus_wrapper_->ExportMethod(
        it.first, base::Bind(it.second, base::Unretained(suspender_.get())));
  }

  // Note that this needs to happen *after* the above methods are exported
  // (http://crbug.com/331431).
  CHECK(dbus_wrapper_->PublishService()) << "Failed to publish D-Bus service";

  // Listen for NameOwnerChanged signals from the bus itself. Register for all
  // of these signals instead of calling individual proxies'
  // SetNameOwnerChangedCallback() methods so that Suspender can get notified
  // when clients with suspend delays for which Daemon doesn't have proxies
  // disconnect.
  const char kBusServiceName[] = "org.freedesktop.DBus";
  const char kBusServicePath[] = "/org/freedesktop/DBus";
  const char kBusInterface[] = "org.freedesktop.DBus";
  const char kNameOwnerChangedSignal[] = "NameOwnerChanged";
  dbus::ObjectProxy* proxy =
      dbus_wrapper_->GetObjectProxy(kBusServiceName, kBusServicePath);
  dbus_wrapper_->RegisterForSignal(
      proxy, kBusInterface, kNameOwnerChangedSignal,
      base::Bind(&Daemon::HandleDBusNameOwnerChanged,
                 weak_ptr_factory_.GetWeakPtr()));

#if USE_BUFFET
  // There's no underlying dbus::Bus object when we're being tested.
  dbus::Bus* bus = dbus_wrapper_->GetBus();
  if (bus) {
    buffet::InitCommandHandlers(bus,
                                base::Bind(&Daemon::ShutDown,
                                           weak_ptr_factory_.GetWeakPtr(),
                                           SHUTDOWN_MODE_REBOOT,
                                           SHUTDOWN_REASON_USER_REQUEST));
  }
#endif  // USE_BUFFET
}

void Daemon::HandleChromeAvailableOrRestarted(bool available) {
  if (!available) {
    LOG(ERROR) << "Failed waiting for Chrome to become available";
    return;
  }
  for (auto controller : all_backlight_controllers_)
    controller->HandleChromeStart();
}

void Daemon::HandleSessionManagerAvailableOrRestarted(bool available) {
  if (!available) {
    LOG(ERROR) << "Failed waiting for session manager to become available";
    return;
  }

  dbus::MethodCall method_call(
      login_manager::kSessionManagerInterface,
      login_manager::kSessionManagerRetrieveSessionState);
  std::unique_ptr<dbus::Response> response = dbus_wrapper_->CallMethodSync(
      session_manager_dbus_proxy_, &method_call,
      base::TimeDelta::FromMilliseconds(kSessionManagerDBusTimeoutMs));
  if (!response)
    return;

  std::string state;
  dbus::MessageReader reader(response.get());
  if (!reader.PopString(&state)) {
    LOG(ERROR) << "Unable to read "
               << login_manager::kSessionManagerRetrieveSessionState << " args";
    return;
  }
  OnSessionStateChange(state);
}

void Daemon::HandleCrasAvailableOrRestarted(bool available) {
  if (!available) {
    LOG(ERROR) << "Failed waiting for CRAS to become available";
    return;
  }
  if (audio_client_)
    audio_client_->LoadInitialState();
}

void Daemon::HandleUpdateEngineAvailable(bool available) {
  if (!available) {
    LOG(ERROR) << "Failed waiting for update engine to become available";
    return;
  }

  dbus::MethodCall method_call(update_engine::kUpdateEngineInterface,
                               update_engine::kGetStatus);
  std::unique_ptr<dbus::Response> response = dbus_wrapper_->CallMethodSync(
      update_engine_dbus_proxy_, &method_call,
      base::TimeDelta::FromMilliseconds(kUpdateEngineDBusTimeoutMs));
  if (!response)
    return;

  dbus::MessageReader reader(response.get());
  int64_t last_checked_time = 0;
  double progress = 0.0;
  std::string operation;
  if (!reader.PopInt64(&last_checked_time) ||
      !reader.PopDouble(&progress) ||
      !reader.PopString(&operation)) {
    LOG(ERROR) << "Unable to read " << update_engine::kGetStatus << " args";
    return;
  }
  OnUpdateOperation(operation);
}

void Daemon::HandleCryptohomedAvailable(bool available) {
  if (!available) {
    LOG(ERROR) << "Failed waiting for cryptohomed to become available";
    return;
  }
  if (!cryptohomed_dbus_proxy_)
    return;

  RequestTpmStatus();
  if (tpm_status_interval_ > base::TimeDelta::FromSeconds(0)) {
    tpm_status_timer_.Start(FROM_HERE, tpm_status_interval_,
                            this, &Daemon::RequestTpmStatus);
  }
}

void Daemon::HandleDBusNameOwnerChanged(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  std::string name, old_owner, new_owner;
  if (!reader.PopString(&name) ||
      !reader.PopString(&old_owner) ||
      !reader.PopString(&new_owner)) {
    LOG(ERROR) << "Unable to parse NameOwnerChanged signal";
    return;
  }

  if (name == login_manager::kSessionManagerServiceName && !new_owner.empty()) {
    LOG(INFO) << "D-Bus " << name << " ownership changed to " << new_owner;
    HandleSessionManagerAvailableOrRestarted(true);
  } else if (name == cras::kCrasServiceName && !new_owner.empty()) {
    LOG(INFO) << "D-Bus " << name << " ownership changed to " << new_owner;
    HandleCrasAvailableOrRestarted(true);
  } else if (name == chromeos::kLibCrosServiceName && !new_owner.empty()) {
    LOG(INFO) << "D-Bus " << name << " ownership changed to " << new_owner;
    HandleChromeAvailableOrRestarted(true);
  }
  suspender_->HandleDBusNameOwnerChanged(name, old_owner, new_owner);
}

void Daemon::HandleSessionStateChangedSignal(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  std::string state;
  if (reader.PopString(&state)) {
    OnSessionStateChange(state);
  } else {
    LOG(ERROR) << "Unable to read "
               << login_manager::kSessionStateChangedSignal << " args";
  }
}

void Daemon::HandleUpdateEngineStatusUpdateSignal(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);
  int64_t last_checked_time = 0;
  double progress = 0.0;
  std::string operation;
  if (!reader.PopInt64(&last_checked_time) ||
      !reader.PopDouble(&progress) ||
      !reader.PopString(&operation)) {
    LOG(ERROR) << "Unable to read " << update_engine::kStatusUpdate << " args";
    return;
  }
  OnUpdateOperation(operation);
}

void Daemon::HandleCrasNodesChangedSignal(dbus::Signal* signal) {
  DCHECK(audio_client_);
  audio_client_->UpdateDevices();
}

void Daemon::HandleCrasActiveOutputNodeChangedSignal(dbus::Signal* signal) {
  DCHECK(audio_client_);
  audio_client_->UpdateDevices();
}

void Daemon::HandleCrasNumberOfActiveStreamsChanged(dbus::Signal* signal) {
  DCHECK(audio_client_);
  audio_client_->UpdateNumActiveStreams();
}

void Daemon::HandleGetTpmStatusResponse(dbus::Response* response) {
  if (!response) {
    LOG(ERROR) << cryptohome::kCryptohomeGetTpmStatus << " call failed";
    return;
  }

  cryptohome::BaseReply base_reply;
  dbus::MessageReader reader(response);
  if (!reader.PopArrayOfBytesAsProto(&base_reply)) {
    LOG(ERROR) << "Unable to parse " << cryptohome::kCryptohomeGetTpmStatus
               << "response";
    return;
  }
  if (base_reply.has_error()) {
    LOG(ERROR) << cryptohome::kCryptohomeGetTpmStatus << " response contains "
               << "error code " << base_reply.error();
    return;
  }
  if (!base_reply.HasExtension(cryptohome::GetTpmStatusReply::reply)) {
    LOG(ERROR) << cryptohome::kCryptohomeGetTpmStatus << " response doesn't "
               << "contain nested reply";
    return;
  }

  cryptohome::GetTpmStatusReply tpm_reply =
      base_reply.GetExtension(cryptohome::GetTpmStatusReply::reply);
  LOG(INFO) << "Received " << cryptohome::kCryptohomeGetTpmStatus
            << " response with dictionary attack count "
            << tpm_reply.dictionary_attack_counter();
  state_controller_->HandleTpmStatus(tpm_reply.dictionary_attack_counter());
}

std::unique_ptr<dbus::Response> Daemon::HandleRequestShutdownMethod(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Got " << kRequestShutdownMethod << " message from "
            << method_call->GetSender();
  ShutDown(SHUTDOWN_MODE_POWER_OFF, SHUTDOWN_REASON_USER_REQUEST);
  return std::unique_ptr<dbus::Response>();
}

std::unique_ptr<dbus::Response> Daemon::HandleRequestRestartMethod(
    dbus::MethodCall* method_call) {
  LOG(INFO) << "Got " << kRequestRestartMethod << " message from "
            << method_call->GetSender();
  ShutdownReason shutdown_reason = SHUTDOWN_REASON_USER_REQUEST;

  dbus::MessageReader reader(method_call);
  int32_t arg = 0;
  if (reader.PopInt32(&arg)) {
    switch (static_cast<RequestRestartReason>(arg)) {
      case REQUEST_RESTART_FOR_USER:
        shutdown_reason = SHUTDOWN_REASON_USER_REQUEST;
        break;
      case REQUEST_RESTART_FOR_UPDATE:
        shutdown_reason = SHUTDOWN_REASON_SYSTEM_UPDATE;
        break;
      default:
        LOG(WARNING) << "Got unknown restart reason " << arg;
    }
  }
  ShutDown(SHUTDOWN_MODE_REBOOT, shutdown_reason);
  return std::unique_ptr<dbus::Response>();
}

std::unique_ptr<dbus::Response> Daemon::HandleRequestSuspendMethod(
    dbus::MethodCall* method_call) {
  // Read an optional uint64_t argument specifying the wakeup count that is
  // expected.
  dbus::MessageReader reader(method_call);
  uint64_t external_wakeup_count = 0;
  const bool got_external_wakeup_count = reader.PopUint64(
      &external_wakeup_count);
  LOG(INFO) << "Got " << kRequestSuspendMethod << " message"
            << (got_external_wakeup_count ?
                base::StringPrintf(" with external wakeup count %" PRIu64,
                                   external_wakeup_count).c_str() : "")
            << " from " << method_call->GetSender();
  Suspend(got_external_wakeup_count, external_wakeup_count);
  return std::unique_ptr<dbus::Response>();
}

std::unique_ptr<dbus::Response> Daemon::HandleDecreaseScreenBrightnessMethod(
    dbus::MethodCall* method_call) {
  if (!display_backlight_controller_)
    return CreateNotSupportedError(method_call, "Backlight uninitialized");

  bool allow_off = false;
  dbus::MessageReader reader(method_call);
  if (!reader.PopBool(&allow_off))
    LOG(ERROR) << "Missing " << kDecreaseScreenBrightnessMethod << " arg";
  bool changed =
      display_backlight_controller_->DecreaseUserBrightness(allow_off);
  double percent = 0.0;
  if (!changed &&
      display_backlight_controller_->GetBrightnessPercent(&percent)) {
    SendBrightnessChangedSignal(
        percent, policy::BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED,
        kBrightnessChangedSignal);
  }
  return std::unique_ptr<dbus::Response>();
}

std::unique_ptr<dbus::Response> Daemon::HandleIncreaseScreenBrightnessMethod(
    dbus::MethodCall* method_call) {
  if (!display_backlight_controller_)
    return CreateNotSupportedError(method_call, "Backlight uninitialized");

  bool changed = display_backlight_controller_->IncreaseUserBrightness();
  double percent = 0.0;
  if (!changed &&
      display_backlight_controller_->GetBrightnessPercent(&percent)) {
    SendBrightnessChangedSignal(
        percent, policy::BacklightController::BRIGHTNESS_CHANGE_USER_INITIATED,
        kBrightnessChangedSignal);
  }
  return std::unique_ptr<dbus::Response>();
}

std::unique_ptr<dbus::Response> Daemon::HandleSetScreenBrightnessMethod(
    dbus::MethodCall* method_call) {
  if (!display_backlight_controller_)
    return CreateNotSupportedError(method_call, "Backlight uninitialized");

  double percent = 0.0;
  int dbus_style = 0;
  dbus::MessageReader reader(method_call);
  if (!reader.PopDouble(&percent) || !reader.PopInt32(&dbus_style)) {
    LOG(ERROR) << "Missing " << kSetScreenBrightnessPercentMethod << " args";
    return CreateInvalidArgsError(method_call, "Expected percent and style");
  }

  policy::BacklightController::TransitionStyle style =
      policy::BacklightController::TRANSITION_FAST;
  switch (dbus_style) {
    case kBrightnessTransitionGradual:
      style = policy::BacklightController::TRANSITION_FAST;
      break;
    case kBrightnessTransitionInstant:
      style = policy::BacklightController::TRANSITION_INSTANT;
      break;
    default:
      LOG(ERROR) << "Invalid transition style (" << dbus_style << ")";
  }
  display_backlight_controller_->SetUserBrightnessPercent(percent, style);
  return std::unique_ptr<dbus::Response>();
}

std::unique_ptr<dbus::Response> Daemon::HandleGetScreenBrightnessMethod(
    dbus::MethodCall* method_call) {
  if (!display_backlight_controller_)
    return CreateNotSupportedError(method_call, "Backlight uninitialized");

  double percent = 0.0;
  if (!display_backlight_controller_->GetBrightnessPercent(&percent)) {
    return std::unique_ptr<dbus::Response>(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_FAILED, "Couldn't fetch brightness"));
  }
  std::unique_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(response.get());
  writer.AppendDouble(percent);
  return response;
}

std::unique_ptr<dbus::Response> Daemon::HandleDecreaseKeyboardBrightnessMethod(
    dbus::MethodCall* method_call) {
  AdjustKeyboardBrightness(-1);
  return std::unique_ptr<dbus::Response>();
}

std::unique_ptr<dbus::Response> Daemon::HandleIncreaseKeyboardBrightnessMethod(
    dbus::MethodCall* method_call) {
  AdjustKeyboardBrightness(1);
  return std::unique_ptr<dbus::Response>();
}

std::unique_ptr<dbus::Response> Daemon::HandleGetPowerSupplyPropertiesMethod(
    dbus::MethodCall* method_call) {
  PowerSupplyProperties protobuf;
  system::CopyPowerStatusToProtocolBuffer(power_supply_->GetPowerStatus(),
                                          &protobuf);
  std::unique_ptr<dbus::Response> response(
      dbus::Response::FromMethodCall(method_call));
  dbus::MessageWriter writer(response.get());
  writer.AppendProtoAsArrayOfBytes(protobuf);
  return response;
}

std::unique_ptr<dbus::Response> Daemon::HandleVideoActivityMethod(
    dbus::MethodCall* method_call) {
  bool fullscreen = false;
  dbus::MessageReader reader(method_call);
  if (!reader.PopBool(&fullscreen))
    LOG(ERROR) << "Unable to read " << kHandleVideoActivityMethod << " args";

  LOG(INFO) << "Saw " << (fullscreen ? "fullscreen" : "normal")
            << " video activity";
  for (auto controller : all_backlight_controllers_)
    controller->HandleVideoActivity(fullscreen);
  state_controller_->HandleVideoActivity();
  return std::unique_ptr<dbus::Response>();
}

std::unique_ptr<dbus::Response> Daemon::HandleUserActivityMethod(
    dbus::MethodCall* method_call) {
  int type_int = USER_ACTIVITY_OTHER;
  dbus::MessageReader reader(method_call);
  if (!reader.PopInt32(&type_int))
    LOG(ERROR) << "Unable to read " << kHandleUserActivityMethod << " args";
  UserActivityType type = static_cast<UserActivityType>(type_int);

  LOG(INFO) << "Saw user activity";
  suspender_->HandleUserActivity();
  state_controller_->HandleUserActivity();
  for (auto controller : all_backlight_controllers_)
    controller->HandleUserActivity(type);
  return std::unique_ptr<dbus::Response>();
}

std::unique_ptr<dbus::Response> Daemon::HandleSetIsProjectingMethod(
    dbus::MethodCall* method_call) {
  bool is_projecting = false;
  dbus::MessageReader reader(method_call);
  if (!reader.PopBool(&is_projecting)) {
    LOG(ERROR) << "Unable to read " << kSetIsProjectingMethod << " args";
    return CreateInvalidArgsError(method_call, "Expected boolean state");
  }

  DisplayMode mode = is_projecting ? DISPLAY_PRESENTATION : DISPLAY_NORMAL;
  LOG(INFO) << "Chrome is using " << DisplayModeToString(mode)
            << " display mode";
  state_controller_->HandleDisplayModeChange(mode);
  wakeup_controller_->SetDisplayMode(mode);
  for (auto controller : all_backlight_controllers_)
    controller->HandleDisplayModeChange(mode);
  return std::unique_ptr<dbus::Response>();
}

std::unique_ptr<dbus::Response> Daemon::HandleSetPolicyMethod(
    dbus::MethodCall* method_call) {
  PowerManagementPolicy policy;
  dbus::MessageReader reader(method_call);
  if (!reader.PopArrayOfBytesAsProto(&policy)) {
    LOG(ERROR) << "Unable to parse " << kSetPolicyMethod << " request";
    return CreateInvalidArgsError(method_call, "Expected protobuf");
  }

  LOG(INFO) << "Received updated external policy: "
            << policy::StateController::GetPolicyDebugString(policy);
  state_controller_->HandlePolicyChange(policy);
  for (auto controller : all_backlight_controllers_)
    controller->HandlePolicyChange(policy);
  return std::unique_ptr<dbus::Response>();
}

std::unique_ptr<dbus::Response> Daemon::HandleSetPowerSourceMethod(
    dbus::MethodCall* method_call) {
  std::string id;
  dbus::MessageReader reader(method_call);
  if (!reader.PopString(&id)) {
    LOG(ERROR) << "Unable to read " << kSetPowerSourceMethod << " args";
    return CreateInvalidArgsError(method_call, "Expected string");
  }

  LOG(INFO) << "Received request to switch to power source " << id;
  if (!power_supply_->SetPowerSource(id)) {
    return std::unique_ptr<dbus::Response>(dbus::ErrorResponse::FromMethodCall(
        method_call, DBUS_ERROR_FAILED, "Couldn't set power source"));
  }
  return std::unique_ptr<dbus::Response>();
}

std::unique_ptr<dbus::Response> Daemon::HandlePowerButtonAcknowledgment(
    dbus::MethodCall* method_call) {
  int64_t timestamp_internal = 0;
  dbus::MessageReader reader(method_call);
  if (!reader.PopInt64(&timestamp_internal)) {
    LOG(ERROR) << "Unable to parse " << kHandlePowerButtonAcknowledgmentMethod
               << " request";
    return CreateInvalidArgsError(method_call, "Expected int64_t timestamp");
  }
  input_controller_->HandlePowerButtonAcknowledgment(
      base::TimeTicks::FromInternalValue(timestamp_internal));
  return std::unique_ptr<dbus::Response>();
}

void Daemon::OnSessionStateChange(const std::string& state_str) {
  SessionState state = (state_str == kSessionStarted) ?
      SESSION_STARTED : SESSION_STOPPED;
  if (state == session_state_)
    return;

  LOG(INFO) << "Session state changed to " << SessionStateToString(state);
  session_state_ = state;
  metrics_collector_->HandleSessionStateChange(state);
  state_controller_->HandleSessionStateChange(state);
  for (auto controller : all_backlight_controllers_)
    controller->HandleSessionStateChange(state);
}

void Daemon::OnUpdateOperation(const std::string& operation) {
  LOG(INFO) << "Update operation is " << operation;
  UpdaterState state = UPDATER_IDLE;
  if (operation == update_engine::kUpdateStatusDownloading ||
      operation == update_engine::kUpdateStatusVerifying ||
      operation == update_engine::kUpdateStatusFinalizing) {
    state = UPDATER_UPDATING;
  } else if (operation == update_engine::kUpdateStatusUpdatedNeedReboot) {
    state = UPDATER_UPDATED;
  }
  state_controller_->HandleUpdaterStateChange(state);
}

void Daemon::RequestTpmStatus() {
  DCHECK(cryptohomed_dbus_proxy_);
  dbus::MethodCall method_call(cryptohome::kCryptohomeInterface,
                               cryptohome::kCryptohomeGetTpmStatus);
  dbus::MessageWriter writer(&method_call);
  writer.AppendProtoAsArrayOfBytes(cryptohome::GetTpmStatusRequest());
  dbus_wrapper_->CallMethodAsync(
      cryptohomed_dbus_proxy_, &method_call,
      base::TimeDelta::FromMilliseconds(kCryptohomedDBusTimeoutMs),
      base::Bind(&Daemon::HandleGetTpmStatusResponse,
                 weak_ptr_factory_.GetWeakPtr()));
}

void Daemon::ShutDown(ShutdownMode mode, ShutdownReason reason) {
  if (shutting_down_) {
    LOG(INFO) << "Shutdown already initiated; ignoring additional request";
    return;
  }

  std::string details;
  if (FirmwareIsBeingUpdated(&details)) {
    LOG(INFO) << "Postponing shutdown for firmware update: " << details;
    if (!retry_shutdown_for_firmware_update_timer_.IsRunning()) {
      retry_shutdown_for_firmware_update_timer_.Start(
          FROM_HERE,
          base::TimeDelta::FromSeconds(kRetryShutdownForFirmwareUpdateSec),
          base::Bind(&Daemon::ShutDown, weak_ptr_factory_.GetWeakPtr(), mode,
                     reason));
    }
    return;
  }

  shutting_down_ = true;
  retry_shutdown_for_firmware_update_timer_.Stop();
  suspender_->HandleShutdown();
  metrics_collector_->HandleShutdown(reason);

  // If we want to display a low-battery alert while shutting down, don't turn
  // the screen off immediately.
  if (reason != SHUTDOWN_REASON_LOW_BATTERY) {
    for (auto controller : all_backlight_controllers_)
      controller->SetShuttingDown(true);
  }

  const std::string reason_str = ShutdownReasonToString(reason);
  switch (mode) {
    case SHUTDOWN_MODE_POWER_OFF:
      LOG(INFO) << "Shutting down, reason: " << reason_str;
      RunSetuidHelper("shut_down", "--shutdown_reason=" + reason_str, false);
      break;
    case SHUTDOWN_MODE_REBOOT:
      LOG(INFO) << "Restarting, reason: " << reason_str;
      RunSetuidHelper("reboot", "", false);
      break;
  }
}

void Daemon::Suspend(bool use_external_wakeup_count,
                     uint64_t external_wakeup_count) {
  if (shutting_down_) {
    LOG(INFO) << "Ignoring request for suspend with outstanding shutdown";
    return;
  }

  if (use_external_wakeup_count)
    suspender_->RequestSuspendWithExternalWakeupCount(external_wakeup_count);
  else
    suspender_->RequestSuspend();
}

void Daemon::SetBacklightsDimmedForInactivity(bool dimmed) {
  for (auto controller : all_backlight_controllers_)
    controller->SetDimmedForInactivity(dimmed);
  metrics_collector_->HandleScreenDimmedChange(
      dimmed, state_controller_->last_user_activity_time());
}

void Daemon::SetBacklightsOffForInactivity(bool off) {
  for (auto controller : all_backlight_controllers_)
    controller->SetOffForInactivity(off);
  metrics_collector_->HandleScreenOffChange(
      off, state_controller_->last_user_activity_time());
}

void Daemon::SetBacklightsSuspended(bool suspended) {
  for (auto controller : all_backlight_controllers_)
    controller->SetSuspended(suspended);
}

void Daemon::SetBacklightsDocked(bool docked) {
  for (auto controller : all_backlight_controllers_)
    controller->SetDocked(docked);
}

void Daemon::PopulateIwlWifiTransmitPowerTable() {
  if (!prefs_->GetString(kIwlWifiTransmitPowerTablePref,
                         &iwl_wifi_power_table_))
    return;

  // Perform format checking to ensure no one can inject shell command.
  std::vector<std::string> str_values =
      base::SplitString(iwl_wifi_power_table_, ":", base::TRIM_WHITESPACE,
                        base::SPLIT_WANT_ALL);

  if (str_values.size() != 6) {
    LOG(ERROR) << "Wrong number of power table literal "
               << "(expected: 6; got: " << str_values.size() << ")";
    iwl_wifi_power_table_.clear();
    return;
  }

  // Parse string value to unsigned integers.
  for (const auto& str_value : str_values) {
    unsigned output;
    if (!base::StringToUint(str_value, &output)) {
      LOG(ERROR) << "Invalid power table literal \"" << str_value << "\"";
      iwl_wifi_power_table_.clear();
      return;
    }
  }
}

}  // namespace power_manager
