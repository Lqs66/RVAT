# ArduPilot Properties Discovered by RVAT

This document lists all specifications and properties discovered by RVAT for ArduPilot.

## Naming Convention

- **AP_\***       : properties discovered by RVAT
- **A.\***        : properties discovered by PGFuzz

---

<table class="tg"><thead>
  <tr>
    <th class="tg-amwm">Mode</th>
    <th class="tg-1wig">ID</th>
    <th class="tg-1wig">Specification Description</th>
    <th class="tg-0lax">UPPAAL Formula</th>
    <th class="tg-0lax">RVAT</th>
    <th class="tg-0lax">PGFuzz</th>
    <th class="tg-0lax">RouthSearch</th>
  </tr></thead>
<tbody>
  <tr>
    <td class="tg-baqh" rowspan="6">RTL</td>
    <td class="tg-0lax">AP_RTL_P1</td>
    <td class="tg-0lax">If the vehicle is in RTL mode and no RTL cone or altitude fence limit applies, then the target return altitude shall be at least the greater of the current altitude plus RTL_CLIMB_MIN, RTL_ALT_M, and the minimum RTL altitude.</td>
    <td class="tg-0lax">(mode == RTL and RTL_CONE_SLOPE == 0 and no_altitude_fence) --> target_alt >= max(curr_alt + RTL_CLIMB_MIN, RTL_ALT_M, 30cm)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_RTL_P2</td>
    <td class="tg-0lax">If the vehicle is in RTL mode, then the target return altitude shall be maintained at or above a minimum of 30cm.</td>
    <td class="tg-0lax">mode == RTL --&gt; target_alt &gt;= 30cm</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.RTL2</td>
    <td class="tg-0lax">In RTL mode, if the current altitude exceeds RTL_ALT, the drone must return to the home position while maintaining its current altitude.</td>
    <td class="tg-0lax">(mode == RTL and curr_alt &gt;= RTL_ALT and at_home == false) --&gt; target_alt == curr_alt</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_RTL_P3</td>
    <td class="tg-0lax">During RTL, if terrain-based RTL altitude is enabled, WP_RFND_USE is set to 1, and the rangefinder is healthy and within range, then the rangefinder is used instead of the terrain database as the altitude reference.</td>
    <td class="tg-0lax">(mode == RTL and RTL_ALT_TYPE == Terrain and WP_RFND_USE == 1 and rangefinder_healthy == true and target_alt_within_rangefinder_range == true) --> alt_ref == rangefinder</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.RTL3</td>
    <td class="tg-0lax">The flight mode must switch to LAND if the drone is at the home position and its altitude is greater than or equal to the RTL_ALT.</td>
    <td class="tg-0lax">(curr_alt &gt;= RTL_ALT and at_home == true) --&gt; mode == LAND</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.RTL1<br>(outdated-doc policy)</td>
    <td class="tg-0lax">If the current altitude is less than RTL_ALT, then altitude must be increased until the altitude is greater or equal to the RTL_ALT.</td>
    <td class="tg-0lax">curr_alt &lt; RTL_ALT --&gt; target_alt &gt;= RTL_ALT</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="4">LAND</td>
    <td class="tg-0lax">A.RTL4</td>
    <td class="tg-0lax">In LAND mode, if the drone has touched the ground and the motor state has transitioned to idle, then its motors must be disarmed.</td>
    <td class="tg-0lax">(mode == LAND and on_ground == true and motors_idle == true) --&gt; armed == false</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.LAND1</td>
    <td class="tg-0lax">If the current altitude is greater than 10 meters, then the drone must descend at the velocity specified by the LAND_SPEED_HIGH parameter.</td>
    <td class="tg-0lax">curr_alt &gt; 10 --&gt; descend_rate == LAND_SPEED_HIGH</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.LAND2</td>
    <td class="tg-0lax">If the drone's altitude is below 10 meters, then the system must execute the descent at the rate defined by the LAND_SPEED parameter.</td>
    <td class="tg-0lax">curr_alt &lt; 10 --&gt; descend_rate == LAND_SPEED</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_LAND_P1</td>
    <td class="tg-0lax">If the vehicle has landed in Land mode and the pilot throttle is at minimum, then the motors shall shut down and the vehicle shall disarm.</td>
    <td class="tg-0lax">(mode == LAND and landed_detected and throttle_min) --> disarmed</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="4">FLIP</td>
    <td class="tg-0lax">A.FLIP1</td>
    <td class="tg-0lax">If the current flight mode is FLIP, then the roll angle must be less than 45 degrees, the throttle must be greater than or equal to 1,500, the altitude must exceed 10 meters, and the previous mode must have been either ACRO or ALT_HOLD.</td>
    <td class="tg-0lax">curr_mode == FLIP --&gt; (roll_angle &lt; 45 and throttle &gt;= 1500 and curr_alt &gt; 10 and (previous_mode == ACRO or previous_mode == ALT_HOLD))</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.FLIP2</td>
    <td class="tg-0lax">If the current flight mode is FLIP and the roll angle is between -90 and 45 degrees, then the vehicle must maintain a rightward angular velocity of 400 deg/s.</td>
    <td class="tg-0lax">(roll_start and roll_dir_is_right and roll_angle &gt;= -90 and roll_angle &lt;= 45) --&gt; roll_rate == 400</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.FLIPGeneral</td>
    <td class="tg-0lax">If the rolling flip is completed, then the time cost must be less than or equal to 2.5 seconds and the vehicle must revert to its original flight mode.</td>
    <td class="tg-0lax">roll_finished --&gt; (time_to_complete_roll &lt;= 2.5s and mode == previous_mode)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>

  </tr>
  <tr>
    <td class="tg-0lax">A.FLIP3</td>
    <td class="tg-0lax">Vehicle should complete the roll within 2.5sec and will then return to the original flight mode it was in before flip was triggered.</td>
    <td class="tg-0lax">roll_finished --&gt; (recover_time &lt;= 2.5s)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="3">ALT_HOLD</td>
    <td class="tg-0lax">A.ALT_HOLD1</td>
    <td class="tg-0lax">If the barometer is selected as the altitude source, then the vehicle must prioritize the altitude computed by this sensor over the GPS data.</td>
    <td class="tg-0lax">(alt_sensor == barometer and baro_has_data) --&gt; alt_measured == baro_val</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_ALTHOLD_P1</td>
    <td class="tg-0lax">If the throttle stick is pushed upward in AltHold mode, then the vehicle's climb rate must be capped at the PILOT_SPEED_UP value.</td>
    <td class="tg-0lax">(mode == ALT_HOLD and throttle_stick &gt; 0) --&gt; (climb_rate &gt; 0 and climb_rate &lt;= PILOT_SPEED_UP)</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.ALT_HOLD2</td>
    <td class="tg-0lax">If the throttle PWM input is 1,500, then the vehicle must maintain its current altitude.</td>
    <td class="tg-0lax">(mode == ALT_HOLD and throttle_stick == 1500) --&gt; velocity_z == 0</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="3">POS_HOLD</td>
    <td class="tg-0lax">AP_POSHOLD_P1</td>
    <td class="tg-0lax">If mode is POS_HOLD, then roll lean angle is less than or equal to&nbsp;&nbsp;ANGLE_MAX parameter.</td>
    <td class="tg-0lax">mode == POS_HOLD --&gt; roll_angle &lt;= ANGLE_MAX</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_POSHOLD_P2</td>
    <td class="tg-0lax">If mode is POS_HOLD, then pitch lean angle is less than or equal to&nbsp;&nbsp;ANGLE_MAX parameter.</td>
    <td class="tg-0lax">mode == POS_HOLD --&gt; pitch_angle &lt;= ANGLE_MAX</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_POSHOLD_P3</td>
    <td class="tg-0lax">If the throttle stick is advanced upward in PosHold mode, then the vehicle's vertical ascent rate must be capped by the PILOT_SPEED_UP parameter.</td>
    <td class="tg-0lax">(mode == PosHold and throttle_stick &gt; 0) --&gt; (climb_rate &gt; 0 and climb_rate &lt;= PILOT_SPEED_UP)</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="7">CIRCLE</td>
    <td class="tg-0lax">A.CIRCLE1</td>
    <td class="tg-0lax">If the pitch stick is moved upward, then the vehicle must decrease the radius.</td>
    <td class="tg-0lax">(RC_pitch &lt; 0 and circle_radius &gt; 0) --&gt; radius_change &lt; 0</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.CIRCLE2</td>
    <td class="tg-0lax">If the pitch stick is moved downward, then the vehicle must increase the radius.</td>
    <td class="tg-0lax">RC_pitch &gt; 0 --&gt; radius_change &gt; 0</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.CIRCLE3</td>
    <td class="tg-0lax">If the roll stick is moved to the right (clockwise command), then the vehicle must increase its speed while moving in a clockwise direction.</td>
    <td class="tg-0lax">(roll_stick &gt; 0 and clockwise == true) --&gt; next_speed &gt; current_speed</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.CIRCLE4</td>
    <td class="tg-0lax">If the roll stick is moved to the right (clockwise command), then the vehicle must decrease its speed while moving in a counterclockwise direction.</td>
    <td class="tg-0lax">(roll_stick &gt; 0 and counterclockwise == true) --&gt; next_speed &lt; current_speed</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.CIRCLE5</td>
    <td class="tg-0lax">If the roll stick is moved to the left (counterclockwise command), then the vehicle must increase its speed while moving in a counterclockwise direction.</td>
    <td class="tg-0lax">(roll_stick &lt; 0 and counterclockwise == true) --&gt; next_speed &gt; current_speed</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.CIRCLE6</td>
    <td class="tg-0lax">If the roll stick is moved to the left (counterclockwise command), then the vehicle must decrease its speed while moving in a clockwise direction.</td>
    <td class="tg-0lax">(roll_stick &lt; 0 and clockwise == true) --&gt; next_speed &lt; current_speed</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.CIRCLE7</td>
    <td class="tg-0lax">If the throttle stick changes, then the altitude changes (vertical speed &gt; 0).</td>
    <td class="tg-0lax">(throttle_stick &gt; 0) --&gt; (radius_change == 0 and rate_change == 0 and climb_rate &gt; 0)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh">AUTO</td>
    <td class="tg-0lax">A.AUTO1</td>
    <td class="tg-0lax">If the vehicle is in AUTO mode, then the pilot's roll, pitch, and throttle inputs must be ignored, whereas the yaw angle can be overridden via the yaw stick.</td>
    <td class="tg-0lax">mode == AUTO --&gt; (roll_input_disabled and pitch_input_disabled and throttle_input_disabled and yaw_input_enabled)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh">BRAKE</td>
    <td class="tg-0lax">A.BRAKE1<br>(non-doc-derived)</td>
    <td class="tg-0lax">When the vehicle is in BRAKE mode, it must stop within k seconds. (in PGFuzz, k=12.7s)</td>
    <td class="tg-0lax">mode_is_BRAKE --&gt; stop_time &lt;= 12.7s</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="3">DRIFT</td>
    <td class="tg-0lax">AP_DRIFT_P1</td>
    <td class="tg-0lax">In Drift mode, if the pilot puts the throttle completely down, then the motors shall go to their minimum rate MOT_SPIN_MIN.</td>
    <td class="tg-0lax">(mode == DRIFT and throttle_input == 0) --> motors_spin_rate == MOT_SPIN_MIN</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.DRIFT1</td>
    <td class="tg-0lax">If the GPS signal is lost in DRIFT mode, then the vehicle must switch to either LAND or ALT_HOLD mode, depending on the FS_EKF_ACTION parameter.</td>
    <td class="tg-0lax">(mode == DRIFT and GPS_loss and (FS_EKF_ACTION == 1 or FS_EKF_ACTION == 2)) --&gt; (mode == LAND or mode == ALT_HOLD)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_DRIFT_P2</td>
    <td class="tg-0lax">If the vehicle is in Drift, then the target roll angle is strictly constrained between -45.0° and 45.0°.</td>
    <td class="tg-0lax">mode == DRIFT --&gt; |roll_angle| &lt;= 45</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="4">LOITER</td>
    <td class="tg-0lax">A.LOITER1</td>
    <td class="tg-0lax">The vehicle must maintain a constant location, heading, and altitude.</td>
    <td class="tg-0lax">A[] (mode == LOITER imply (target_x == curr_x and target_y == curr_y and target_z == curr_z and target_yaw == curr_yaw))</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_LOITER_P1</td>
    <td class="tg-0lax">If the throttle stick is advanced upward in Loiter mode, then the vehicle's ascent rate must be restricted to a maximum of the PILOT_SPEED_UP parameter.</td>
    <td class="tg-0lax">(mode == LOITER and throttle_stick &gt; 0) --&gt; (climb_rate &gt; 0 and climb_rate &lt;= PILOT_SPEED_UP)</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_LOITER_P2</td>
    <td class="tg-0lax">If mode is LOITER, then the maximum horizontal speed is LOIT_SPEED.</td>
    <td class="tg-0lax">mode == LOITER --&gt; horizontal_speed &lt;= LOIT_SPEED</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_LOITER_P3</td>
    <td class="tg-0lax">If mode is LOITER and LOIT_ACC_MAX is not 0, then the maximum acceleration is LOIT_ACC_MAX.</td>
    <td class="tg-0lax">(mode == LOITER and LOIT_ACC_MAX != 0) --&gt; acceleration &lt;= LOIT_ACC_MAX</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh">GUIDED</td>
    <td class="tg-0lax">A.GUIDED1</td>
    <td class="tg-0lax">If there are no remaining waypoints, then the vehicle must maintain its current location, heading, and altitude.</td>
    <td class="tg-0lax">all_wp_reached --&gt; (target_pos_matches_current and target_yaw_matches_current)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh">SPORT</td>
    <td class="tg-0lax">A.SPORT1</td>
    <td class="tg-0lax">In SPORT mode, if the throttle stick is moved upward, then the vehicle must climb at a rate that does not exceed the PILOT_SPEED_UP limit.</td>
    <td class="tg-0lax">(mode == SPORT and throttle_stick &gt; 0) --&gt; (climb_rate &gt; 0 and climb_rate &lt;= PILOT_SPEED_UP)</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="2">ACRO</td>
    <td class="tg-0lax">AP_ACRO_P1</td>
    <td class="tg-0lax">If during Acro, ACRO_TRAINER is 1 or 2, then the vehicle generates roll correction values opposite to the roll angle.</td>
    <td class="tg-0lax">(ACRO_TRAINER &gt;= 1 and roll_angle != 0) --&gt; ((roll_angle &gt; 0 and roll_correction &lt; 0) or (roll_angle &lt; 0 and roll_correction &gt; 0))</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_ACRO_P2</td>
    <td class="tg-0lax">If during Acro, ACRO_TRAINER is 1 or 2, then the vehicle generates pitch correction values opposite to the pitch angle.</td>
    <td class="tg-0lax">(ACRO_TRAINER &gt;= 1 and pitch_angle != 0) --&gt; ((pitch_angle &gt; 0 and pitch_correction &lt; 0) or (pitch_angle &lt; 0 and pitch_correction &gt; 0))</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="2">STABILIZE</td>
    <td class="tg-0lax">AP_STABILIZE_P1</td>
    <td class="tg-0lax">If the vehicle is in STABILIZE mode, then the pilot's requested roll lean angle is constrained to be less than or equal to ANGLE_MAX.</td>
    <td class="tg-0lax">mode == STABILIZED --&gt; roll_angle &lt;= ANGLE_MAX</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_STABILIZE_P2</td>
    <td class="tg-0lax">If the vehicle is in STABILIZE mode, then the pilot's requested pitch lean angle is constrained to be less than or equal to ANGLE_MAX.</td>
    <td class="tg-0lax">mode == STABILIZED --&gt; pitch_angle &lt;= ANGLE_MAX</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh">ZIGZAG</td>
    <td class="tg-0lax">AP_ZIGZAG_P1</td>
    <td class="tg-0lax">If the throttle stick is pushed upward in ZigZag mode, then the aircraft's climb speed must be limited to the PILOT_SPEED_UP maximum.</td>
    <td class="tg-0lax">(mode == ZigZag and throttle_stick &gt; 0) --&gt; (climb_rate &gt; 0 and climb_rate &lt;= PILOT_SPEED_UP)</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="9">Failsafe</td>
    <td class="tg-0lax">A.RC.FS1</td>
    <td class="tg-0lax">If the vehicle is armed in ACRO mode and the throttle input falls below the FS_THR_VALUE threshold, then the system must immediately disarm the motors.</td>
    <td class="tg-0lax">(mode == ACRO and throttle_input &lt; FS_THR_VALUE and armed) --&gt; disarmed</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.RC.FS2</td>
    <td class="tg-0lax">If the throttle input is less than the FS_THR_VALUE parameter, then the drone must switch to the RC Failsafe mode.</td>
    <td class="tg-0lax">throttle_input &lt; FS_THR_VALUE --&gt; RC_failsafe_triggered</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_RCFS_P1</td>
    <td class="tg-0lax">If a RC Failsafe is triggered while the drone is armed and landed, then the vehicle must disarm.</td>
    <td class="tg-0lax">(RC_failsafe_triggered and landed and armed) --&gt; disarmed</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_GCSFS_P1</td>
    <td class="tg-0lax">Triggering a GCS failsafe and disarming while in ACRO mode implies that the throttle is at minimum and the vehicle has landed.</td>
    <td class="tg-0lax">A[] ((mode == ACRO and GCS_failsafe_triggered and disarmed) imply (landed and throttle_is_zero))</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_GCSFS_P2</td>
    <td class="tg-0lax">If a GCS Failsafe is triggered while the drone is armed and landed, then the vehicle must disarm.</td>
    <td class="tg-0lax">(GCS_failsafe_triggered and landed and armed) --&gt; disarmed</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_BATTFS_P1</td>
    <td class="tg-0lax">In battery failsafe, the vehicle shall disarm if it is armed and either it is in Stabilize/Acro mode with throttle at zero, or it has landed.</td>
    <td class="tg-0lax">(battery_failsafe_triggered and armed and (((mode == STABILIZE or mode == ACRO) and throttle_input == 0) or landed)) --> disarmed</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.GPS.FS1</td>
    <td class="tg-0lax">If the number of detected GPS satellites is less than four, then the vehicle triggers the GPS Failsafe mode.</td>
    <td class="tg-0lax">GPS_satellite_count &lt; 4 --&gt; GPS_failsafe_triggered</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.GPS.FS2</td>
    <td class="tg-0lax">If the GPS fail-safe mode is triggered and a secondary altitude sensor is available, then the vehicle changes the primary altitude source to the secondary sensor.</td>
    <td class="tg-0lax">(GPS_failsafe and has_secondary_alt_sensor) --&gt; (alt_sensor != None and alt_sensor != GPS)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">A.CHUTE1</td>
    <td class="tg-0lax">Deploying a parachute requires following conditions: (1) the motors must be armed, (2) the vehicle must not be in the FLIP or ACRO flight modes, (3) the barometer must show that the vehicle is not climbing, and (4) the vehicle’s current altitude must be above the CHUTE_ALT_MIN parameter value.</td>
    <td class="tg-0lax">parachute_release --&gt; (armed and mode != FLIP and mode != ACRO and not_climbing and curr_alt &gt;= CHUTE_ALT_MIN)</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
</tbody></table>
