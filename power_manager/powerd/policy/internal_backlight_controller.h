// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef POWER_MANAGER_POWERD_POLICY_INTERNAL_BACKLIGHT_CONTROLLER_H_
#define POWER_MANAGER_POWERD_POLICY_INTERNAL_BACKLIGHT_CONTROLLER_H_

#include <stdint.h>

#include <memory>

#include <base/compiler_specific.h>
#include <base/macros.h>
#include <base/observer_list.h>
#include <base/time/time.h>
#include <chromeos/dbus/service_constants.h>

#include "power_manager/powerd/policy/ambient_light_handler.h"
#include "power_manager/powerd/policy/backlight_controller.h"

namespace power_manager {

class Clock;
class PrefsInterface;

namespace system {
class AmbientLightSensorInterface;
class BacklightInterface;
class DisplayPowerSetterInterface;
}  // namespace system

namespace policy {

// Controls the internal backlight on devices with built-in displays.
//
// In the context of this class, "percent" refers to a double-precision
// brightness percentage in the range [0.0, 100.0] (where 0 indicates a
// fully-off backlight), while "level" refers to a 64-bit hardware-specific
// brightness in the range [0, max-brightness-per-sysfs].
class InternalBacklightController : public BacklightController,
                                    public AmbientLightHandler::Delegate {
 public:
  // Maximum number of brightness adjustment steps.
  static const int64_t kMaxBrightnessSteps;

  // Percent corresponding to |min_visible_level_|, which takes the role of the
  // lowest brightness step before the screen is turned off.
  static const double kMinVisiblePercent;

  // Minimum brightness, as a fraction of the maximum level in the range [0.0,
  // 1.0], that is used as the bottom step before turning the backlight off
  // entirely.  This is arbitrarily chosen but seems to be a reasonable
  // marginally-visible brightness for a darkened room on current devices:
  // http://crosbug.com/24569. A custom level can be set via the
  // kMinVisibleBacklightLevelPref setting. This is a fraction of the
  // driver-supplied maximum level rather than a percent so it won't change if
  // kDefaultLevelToPercentExponent is modified.
  static const double kDefaultMinVisibleBrightnessFraction;

  // If an ambient light reading hasn't been seen after this many seconds,
  // give up on waiting for the sensor to be initialized and just set
  // |use_ambient_light_| to false.
  static const int kAmbientLightSensorTimeoutSec;

  InternalBacklightController();
  virtual ~InternalBacklightController();

  Clock* clock() { return clock_.get(); }

  // Initializes the object. Ownership of the passed-in pointers remains with
  // the caller.
  void Init(system::BacklightInterface* backlight,
            PrefsInterface* prefs,
            system::AmbientLightSensorInterface* sensor,
            system::DisplayPowerSetterInterface* display_power_setter);

  // Converts between [0, 100] and [0, |max_level_|] brightness scales.
  double LevelToPercent(int64_t level);
  int64_t PercentToLevel(double percent);

  // BacklightController implementation:
  void AddObserver(BacklightControllerObserver* observer) override;
  void RemoveObserver(BacklightControllerObserver* observer) override;
  void HandlePowerSourceChange(PowerSource source) override;
  void HandleDisplayModeChange(DisplayMode mode) override;
  void HandleSessionStateChange(SessionState state) override;
  void HandlePowerButtonPress() override;
  void HandleUserActivity(UserActivityType type) override;
  void HandleVideoActivity(bool is_fullscreen) override;
  void HandleHoverStateChange(bool hovering) override;
  void HandleTabletModeChange(TabletMode mode) override;
  void HandlePolicyChange(const PowerManagementPolicy& policy) override;
  void HandleChromeStart() override;
  void SetDimmedForInactivity(bool dimmed) override;
  void SetOffForInactivity(bool off) override;
  void SetSuspended(bool suspended) override;
  void SetShuttingDown(bool shutting_down) override;
  void SetDocked(bool docked) override;
  bool GetBrightnessPercent(double* percent) override;
  bool SetUserBrightnessPercent(double percent, TransitionStyle style) override;
  bool IncreaseUserBrightness() override;
  bool DecreaseUserBrightness(bool allow_off) override;
  int GetNumAmbientLightSensorAdjustments() const override;
  int GetNumUserAdjustments() const override;

  // AmbientLightHandler::Delegate implementation:
  void SetBrightnessPercentForAmbientLight(
      double brightness_percent,
      AmbientLightHandler::BrightnessChangeCause cause) override;

 private:
  // Snaps |percent| to the nearest step, as defined by |step_percent_|.
  double SnapBrightnessPercentToNearestStep(double percent) const;

  // Returns either |ac_explicit_brightness_percent_| or
  // |battery_explicit_brightness_percent_| depending on |power_source_|.
  double GetExplicitBrightnessPercent() const;

  // Returns the brightness percent that should be used when the system is
  // in an undimmed state (|ambient_light_brightness_percent_| if
  // |use_ambient_light_| is true or a user- or policy-set level otherwise).
  double GetUndimmedBrightnessPercent() const;

  // Increases the explicitly-set brightness to the minimum visible level if
  // it's currently set to zero. Note that the brightness is left unchanged if
  // an external display is connected to avoid resizing the desktop, or if the
  // brightness was set to zero via a policy.
  void EnsureUserBrightnessIsNonzero();

  // Method that disables ambient light adjustments, updates the
  // |*_explicit_brightness_percent_| members, and updates the backlight's
  // brightness if needed. Returns true if the backlight's brightness was
  // changed.
  bool SetExplicitBrightnessPercent(
      double ac_percent,
      double battery_percent,
      TransitionStyle style,
      BrightnessChangeCause cause);

  // Updates the current brightness after assessing the current state
  // (based on |power_source_|, |dimmed_for_inactivity_|, etc.).  Should be
  // called whenever the state changes.
  void UpdateState();

  // If the display is currently in the undimmed state, calls
  // ApplyBrightnessPercent() to update the backlight brightness.  Returns
  // true if the brightness was changed.
  bool UpdateUndimmedBrightness(TransitionStyle style,
                                BrightnessChangeCause cause);

  // Sets |backlight_|'s brightness to |percent| over |transition|.  If the
  // brightness changed, notifies |observers_| that the change was due to
  // |cause| and returns true.
  bool ApplyBrightnessPercent(double percent,
                              TransitionStyle transition,
                              BrightnessChangeCause cause);

  // Configures |backlight_| to resume from suspend at |resume_percent|.
  bool ApplyResumeBrightnessPercent(double resume_percent);

  // Updates displays to |state| after |delay| if |state| doesn't match
  // |display_power_state_|.  If another change has already been scheduled,
  // it will be aborted.
  void SetDisplayPower(chromeos::DisplayPowerState state,
                       base::TimeDelta delay);

  // Backlight used for dimming. Weak pointer.
  system::BacklightInterface* backlight_;

  // Interface for saving preferences. Weak pointer.
  PrefsInterface* prefs_;

  // Used to turn displays on and off.
  system::DisplayPowerSetterInterface* display_power_setter_;

  std::unique_ptr<AmbientLightHandler> ambient_light_handler_;

  std::unique_ptr<Clock> clock_;

  // Observers for changes to the brightness level.
  base::ObserverList<BacklightControllerObserver> observers_;

  // Information describing the current state of the system.
  PowerSource power_source_;
  DisplayMode display_mode_;
  bool dimmed_for_inactivity_;
  bool off_for_inactivity_;
  bool suspended_;
  bool shutting_down_;
  bool docked_;

  // Time at which Init() was called.
  base::TimeTicks init_time_;

  // Indicates whether SetBrightnessPercentForAmbientLight() and
  // HandlePowerSourceChange() have been called yet.
  bool got_ambient_light_brightness_percent_;
  bool got_power_source_;

  // Has UpdateState() already set the initial state?
  bool already_set_initial_state_;

  // Number of ambient-light- and user-triggered brightness adjustments in the
  // current session.
  int als_adjustment_count_;
  int user_adjustment_count_;

  // Ambient-light-sensor-derived brightness percent supplied by
  // |ambient_light_handler_|.
  double ambient_light_brightness_percent_;

  // User- or policy-set brightness percent when on AC or battery power.
  double ac_explicit_brightness_percent_;
  double battery_explicit_brightness_percent_;

  // True if the most-recently-received policy message requested a specific
  // brightness and no user adjustments have been made since then.
  bool using_policy_brightness_;

  // True if the brightness should be forced to be nonzero in response to user
  // activity.
  bool force_nonzero_brightness_for_user_activity_;

  // Maximum raw brightness level for |backlight_| (0 is assumed to be the
  // minimum, with the backlight turned off).
  int64_t max_level_;

  // Minimum raw brightness level that we'll stop at before turning the
  // backlight off entirely when adjusting the brightness down.  Note that we
  // can still quickly animate through lower (still technically visible) levels
  // while transitioning to the off state; this is the minimum level that we'll
  // use in the steady state while the backlight is on.
  int64_t min_visible_level_;

  // Indicates whether transitions between 0 and |min_visible_level_| must be
  // instant, i.e. the brightness may not smoothly transition between those
  // levels.
  bool instant_transitions_below_min_level_;

  // If true, then suggestions from |ambient_light_handler_| are used. False if
  // |ambient_light_handler_| is NULL or the user has manually set the
  // brightness.
  bool use_ambient_light_;

  // Percentage by which we offset the brightness in response to increase and
  // decrease requests.
  double step_percent_;

  // Percentage, in the range [0.0, 100.0], to which we dim the backlight on
  // idle.
  double dimmed_brightness_percent_;

  // Brightness level fractions (e.g. 140/200) are raised to this power when
  // converting them to percents.  A value below 1.0 gives us more granularity
  // at the lower end of the range and less at the upper end.
  double level_to_percent_exponent_;

  // |backlight_|'s current brightness level (or the level to which it's
  // transitioning).
  int64_t current_level_;

  // Most-recently-requested display power state.
  chromeos::DisplayPowerState display_power_state_;

  // Screen off delay when user sets brightness to 0.
  base::TimeDelta turn_off_screen_timeout_;

  DISALLOW_COPY_AND_ASSIGN(InternalBacklightController);
};

}  // namespace policy
}  // namespace power_manager

#endif  // POWER_MANAGER_POWERD_POLICY_INTERNAL_BACKLIGHT_CONTROLLER_H_
