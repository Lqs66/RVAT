# Results Comparison: RVAT vs. PGFuzz

This document highlights the comparative performance of **RVAT** and **PGFuzz** on the **newly introduced flight control specifications by RVAT**.

<table class="tg"><thead>
  <tr>
    <th class="tg-0lax">ID</th>
    <th class="tg-0lax">Specification Description</th>
    <th class="tg-baqh">RVAT</th>
    <th class="tg-baqh">PGFUZZ</th>
  </tr></thead>
<tbody>
  <tr>
    <td class="tg-0lax">AP_RTL_P1</td>
    <td class="tg-0lax">If the mode is set to RTL, then the return target altitude must be greater than or equal to its current altitude.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_RTL_P2</td>
    <td class="tg-0lax">If the vehicle is in RTL mode, then the target return altitude shall be maintained at or above a minimum of 30cm.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_RTL_P4</td>
    <td class="tg-0lax">If during RTL, WPNAV_RFND_USE is 1, and the rangefinder is available, then the rangefinder is used instead of the terrain database as alt type.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_LAND_P4</td>
    <td class="tg-0lax">If the drone has landed and the motors are in the idle state, then the motors are disarmed.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_ALTHOLD_P2</td>
    <td class="tg-0lax">If the throttle stick is pushed upward in AltHold mode, then the vehicle's climb rate must be capped at the PILOT_SPEED_UP value.</td>
    <td class="tg-baqh">unsatisfied</td>
    <td class="tg-baqh">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_POSHOLD_P1</td>
    <td class="tg-0lax">If mode is POS_HOLD, then roll lean angle is less than or equal to&nbsp;&nbsp;ANGLE_MAX parameter.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_POSHOLD_P2</td>
    <td class="tg-0lax">If mode is POS_HOLD, then pitch lean angle is less than or equal to&nbsp;&nbsp;ANGLE_MAX parameter.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_POSHOLD_P3</td>
    <td class="tg-0lax">If the throttle stick is advanced upward in PosHold mode, then the vehicle's vertical ascent rate must be capped by the PILOT_SPEED_UP parameter.</td>
    <td class="tg-baqh">unsatisfied</td>
    <td class="tg-baqh">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_DRIFT_P1</td>
    <td class="tg-0lax">In DRIFT mode, if the throttle stick is at the lowest position, then the motors shall spin at MOT_SPIN_ARMED.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_DRIFT_P3</td>
    <td class="tg-0lax">If the vehicle is in Drift, then the target roll angle is strictly constrained between -45.0° and 45.0°.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_LOITER_P2</td>
    <td class="tg-0lax">If the throttle stick is advanced upward in Loiter mode, then the vehicle's ascent rate must be restricted to a maximum of the PILOT_SPEED_UP parameter.</td>
    <td class="tg-baqh">unsatisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_LOITER_P3</td>
    <td class="tg-0lax">If mode is LOITER, then the maximum horizontal speed is LOIT_SPEED.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_LOITER_P4</td>
    <td class="tg-0lax">If mode is LOITER and LOIT_ACC_MAX is not 0, then the maximum acceleration is LOIT_ACC_MAX.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_ACRO_P1</td>
    <td class="tg-0lax">If during Acro, ACRO_TRAINER is 1 or 2, then the vehicle generates roll correction values opposite to the roll angle.</td>
    <td class="tg-baqh">unsatisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_ACRO_P2</td>
    <td class="tg-0lax">If during Acro, ACRO_TRAINER is 1 or 2, then the vehicle generates pitch correction values opposite to the pitch angle.</td>
    <td class="tg-baqh">unsatisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_STABILIZE_P1</td>
    <td class="tg-0lax">If the vehicle is in STABILIZE mode, then the pilot's requested roll lean angle is constrained to be less than or equal to ANGLE_MAX.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_STABILIZE_P2</td>
    <td class="tg-0lax">If the vehicle is in STABILIZE mode, then the pilot's requested pitch lean angle is constrained to be less than or equal to ANGLE_MAX.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_ZIGZAG_P1</td>
    <td class="tg-0lax">If the throttle stick is pushed upward in ZigZag mode, then the aircraft's climb speed must be limited to the PILOT_SPEED_UP maximum.</td>
    <td class="tg-baqh">unsatisfied</td>
    <td class="tg-baqh">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_RCFS_P3</td>
    <td class="tg-0lax">If a RC Failsafe is triggered while the drone is armed and landed, then the vehicle must disarm.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_GCSFS_P1</td>
    <td class="tg-0lax">Triggering a GCS failsafe and disarming while in ACRO mode implies that the throttle is at minimum and the vehicle has landed.</td>
    <td class="tg-baqh">unsatisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_GCSFS_P2</td>
    <td class="tg-0lax">If a GCS Failsafe is triggered while the drone is armed and landed, then the vehicle must disarm.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">AP_BATTFS_P1</td>
    <td class="tg-0lax">In battery failsafe, if the vehicle is in Stabilize or Acro mode and either the throttle is at zero or the vehicle has landed, then the drone shall disarm.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_RTL_P4</td>
    <td class="tg-0lax">If the vehicle is in RTL mode, then the target altitude shall never be lower than the current altitude during the return phase.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_RTL_P5</td>
    <td class="tg-0lax">If RTL_CONE_ANG is 0, then return target alt must be greater than or euqal to RTL_RETURN_ALT. </td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_RTL_P6</td>
    <td class="tg-0lax">If RTL_CONE_ANG is 90,then the target return altitude must be greater than or equal to either the current altitude or RTL_DESCEND_ALT.</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_ORBIT_P6</td>
    <td class="tg-0lax">The minimum radius output is 1m.</td>
    <td class="tg-baqh">unsatisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_ORBIT_P7</td>
    <td class="tg-0lax">The maximum radius output must be MC_ORBIT_RAD_MAX meters.</td>
    <td class="tg-baqh">unsatisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_ALTITUDE_P2</td>
    <td class="tg-0lax">If the throttle stick is moved above the center position, then the vehicle will move upward, and the vertical ascent speed must be less than or equal to the MPC_Z_VEL_MAX_UP (Note: Defined in the North-East-Down coordinate system where upward movement corresponds to a negative velocity value).</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_ALTITUDE_P3</td>
    <td class="tg-0lax">If the throttle stick is moved below the center position,then the vehicle will move downward, and the vertical descent speed must be less than or equal to the MPC_Z_VEL_MAX_DN (Note: Defined in the North-East-Down coordinate system where upward movement corresponds to a negative velocity value).</td>
    <td class="tg-baqh">satisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_POSITION_P2</td>
    <td class="tg-0lax">If the pitch stick is moved (forward or backward), then the drone generates a forward-back velocity setpoint where the magnitude is less than or equal to MPC_VEL_MANUAL.</td>
    <td class="tg-baqh">unsatisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_POSITION_P3</td>
    <td class="tg-0lax">If the roll stick is moved (left or right), then the drone generates a eft-right velocity setpoint where the magnitude is less than or equal to MPC_VEL_MANUAL.</td>
    <td class="tg-baqh">unsatisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_RCFS_P1</td>
    <td class="tg-0lax">A valid RC signal entails that manual control commands be updated within the allowable timeout (COM_RC_LOSS_T) and that the RC loss failsafe remains inactive.</td>
    <td class="tg-baqh">unsatisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_BRAKE_P1</td>
    <td class="tg-0lax">If emergency braking is triggered by an excessive vertical rate, then the current vertical speed must be greater than the maximum allowed vertical speed(&gt;0).</td>
    <td class="tg-baqh">unsatisfied</td>
    <td class="tg-baqh">satisfied</td>
  </tr>
</tbody></table>