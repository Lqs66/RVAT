# PX4 Properties Discovered by RVAT

This document lists all specifications and properties discovered by RVAT for PX4.

## Naming Convention

- **PX_\***       : properties discovered by RVAT
- **PX.\***        : properties discovered by PGFuzz

---


<table class="tg"><thead>
  <tr>
    <th class="tg-amwm">Mode</th>
    <th class="tg-1wig">ID</th>
    <th class="tg-1wig">Specification Description</th>
    <th class="tg-0lax">UPPAAL Formula</th>
    <th class="tg-0lax">RVAT</th>
    <th class="tg-0lax">PGFUZZ</th>
  </tr></thead>
<tbody>
  <tr>
    <td class="tg-baqh" rowspan="7">RTL</td>
    <td class="tg-0lax">PX.RTL1</td>
    <td class="tg-0lax">If the current altitude is below RTL_RETURN_ALT, then the drone increases the altitude to at least the RTL_RETURN_ALT value.</td>
    <td class="tg-0lax">curr_alt &lt; RTL_RETURN_ALT --&gt; target_alt &gt;= RTL_RETURN_ALT</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.RTL2</td>
    <td class="tg-0lax">If the vehicle is in RTL mode at or above the RTL_RETURN_ALT but is not yet at the home position, then it must fly toward the home position while holding its current altitude.</td>
    <td class="tg-0lax">(curr_alt &gt;= RTL_RETURN_ALT and&nbsp;&nbsp;mode == RTL and at_home == false) --&gt; target_alt == curr_alt</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.RTL3</td>
    <td class="tg-0lax">If the vehicle reaches the home position and its altitude is at least RTL_RETURN_ALT, then the flight mode switchs to LAND.</td>
    <td class="tg-0lax">curr_alt &gt;= RTL_RETURN_ALT and at_home == true --&gt; mode == LAND</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.RTL4</td>
    <td class="tg-0lax">If the RTL_LAND_DELAY parameter is set to -1, then the vehicle shall hover at RTL_DESCEND_ALT.</td>
    <td class="tg-0lax">RTL_LAND_DELAY == -1 --&gt; target_alt == RTL_DESCEND_ALT</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_RTL_P1</td>
    <td class="tg-0lax">If the vehicle is in RTL mode, then the target altitude shall never be lower than the current altitude during the return phase.</td>
    <td class="tg-0lax">mode == RTL --&gt; return_alt &gt;= curr_alt</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_RTL_P2</td>
    <td class="tg-0lax">If RTL_CONE_ANG is 0, then return target alt must be greater than or euqal to RTL_RETURN_ALT. </td>
    <td class="tg-0lax">RTL_CONE_ANG == 0 --&gt; return_alt &gt;= RTL_RETURN_ALT</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_RTL_P3</td>
    <td class="tg-0lax">If RTL_CONE_ANG is 90,then the target return altitude must be greater than or equal to either the current altitude or RTL_DESCEND_ALT.</td>
    <td class="tg-0lax">RTL_CONE_ANG == 90 --&gt; (return_alt &gt;= RTL_DESCEND_ALT or return_alt &gt;= curr_alt)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="2">LAND</td>
    <td class="tg-0lax">PX.RTL5</td>
    <td class="tg-0lax">If the current flight mode is LAND and ground contact is detected, then the vehicle must disarm its motors.</td>
    <td class="tg-0lax">(mode == LAND and on_ground == true) --&gt; armed == false</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.LAND1</td>
    <td class="tg-0lax">If the vehicle starts landing, then the descent speed must be equal to the MPC_LAND_SPEED parameter.</td>
    <td class="tg-0lax">start_landing --&gt; descend_rate == MPC_LAND_SPEED</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="8">ORBIT</td>
    <td class="tg-0lax">PX.ORBIT1</td>
    <td class="tg-0lax">If the pitch stick is moved up, then the radius must decrease.</td>
    <td class="tg-0lax">pitch_stick &gt;= 0 --&gt; next_radius &lt;= current_radius</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.ORBIT2</td>
    <td class="tg-0lax">If the pitch stick is moved down, then the radius must increase.</td>
    <td class="tg-0lax">pitch_stick &lt;= 0 --&gt; next_radius &gt;= current_radius</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.ORBIT3</td>
    <td class="tg-0lax">If the vehicle is orbiting clockwise and the roll stick is moved to the right, then the velocity must increase.</td>
    <td class="tg-0lax">(roll_stick &gt;= 0 and clockwise == true) --&gt; clockwise_acceleration &gt;= 0</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.ORBIT4</td>
    <td class="tg-0lax">If the vehicle is orbiting counter-clockwise and the roll stick is moved to the right, then the velocity must decrease.</td>
    <td class="tg-0lax">(roll_stick &gt;= 0 and counterclockwise == true) --&gt; clockwise_acceleration &gt;= 0</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.ORBIT5</td>
    <td class="tg-0lax">The maximum radius output must be 100 meters.</td>
    <td class="tg-0lax">A[] (is_outputting imply circle_radius &lt;= 100)</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.ORBIT6</td>
    <td class="tg-0lax">The acceleration output must be at most 2 m/s².</td>
    <td class="tg-0lax">A[] (is_outputting imply circle_acceleration &lt;= 2)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_ORBIT_P1</td>
    <td class="tg-0lax">The minimum radius output is 1m.</td>
    <td class="tg-0lax">A[] (is_outputting imply circle_radius &gt;= 1)</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_ORBIT_P2</td>
    <td class="tg-0lax">The maximum radius output must be MC_ORBIT_RAD_MAX meters.</td>
    <td class="tg-0lax">A[] (is_outputting imply circle_radius &lt;= MC_ORBIT_RAD_MAX)</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="3">ALTITUDE</td>
    <td class="tg-0lax">PX.ALTITUDE1</td>
    <td class="tg-0lax">If the throttle stick is centered at 1,500, then the vehicle must maintain its current altitude.</td>
    <td class="tg-0lax">(throttle_stick_centered and has_stopped) --&gt; (target_alt == current_alt)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_ALTITUDE_P1</td>
    <td class="tg-0lax">If the throttle stick is moved above the center position, then the vehicle will move upward, and the vertical ascent speed must be less than or equal to the MPC_Z_VEL_MAX_UP (Note: Defined in the North-East-Down coordinate system where upward movement corresponds to a negative velocity value).</td>
    <td class="tg-0lax">throttle &lt;= 0 --&gt; (vertical_vel &lt;= 0 and |vertical_vel| &lt;= MPC_Z_VEL_MAX_UP)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_ALTITUDE_P2</td>
    <td class="tg-0lax">If the throttle stick is moved below the center position,then the vehicle will move downward, and the vertical descent speed must be less than or equal to the MPC_Z_VEL_MAX_DN (Note: Defined in the North-East-Down coordinate system where upward movement corresponds to a negative velocity value).</td>
    <td class="tg-0lax">throttle &gt;= 0 --&gt; (vertical_vel &gt;= 0 and |vertical_vel| &lt;= MPC_Z_VEL_MAX_DN)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="3">POSITION</td>
    <td class="tg-0lax">PX.POSITION1</td>
    <td class="tg-0lax">If the drone has come to a stop and all RC sticks are centered, then the vehicle must maintain a constant position.</td>
    <td class="tg-0lax">(sticks_centered and has_stopped) --&gt; (target_x == curr_x and target_y == curr_y and target_z == curr_z)</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_POSITION_P1</td>
    <td class="tg-0lax">If the pitch stick is moved (forward or backward), then the drone generates a forward-back velocity setpoint where the magnitude is less than or equal to MPC_VEL_MANUAL.</td>
    <td class="tg-0lax">pitch_stick &gt;= -1 and pitch_stick &lt;= 1 --&gt; |x_velocity_setpoint| &lt;= MPC_VEL_MANUAL</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX_POSITION_P2</td>
    <td class="tg-0lax">If the roll stick is moved (left or right), then the drone generates a eft-right velocity setpoint where the magnitude is less than or equal to MPC_VEL_MANUAL.</td>
    <td class="tg-0lax">roll_stick &gt;= -1 and roll_stick &lt;= 1 --&gt; |y_velocity_setpoint| &lt;= MPC_VEL_MANUAL </td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="2">HOLD</td>
    <td class="tg-0lax">PX.HOLD1</td>
    <td class="tg-0lax">The drone must maintain it's position and heading.</td>
    <td class="tg-0lax">A[] (mode == HOLD imply (target_x == curr_x and target_y == curr_y and target_z == curr_z and target_yaw == curr_yaw))</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.HOLD2</td>
    <td class="tg-0lax">If NAV_MIN_LTR_ALT is not -1 and current altitude is less than the parameter value, then the vehicle must ascend to this altitude.</td>
    <td class="tg-0lax">(NAV_MIN_LTR_ALT != -1 and curr_alt &lt; NAV_MIN_LTR_ALT) --&gt; target_alt == NAV_MIN_LTR_ALT</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="2">TAKEOFF</td>
    <td class="tg-0lax">PX.TAKEOFF1</td>
    <td class="tg-0lax">When the vehicle conducts a default taking off command, the target altitude must be increased by the MIS_TAKEOFF_ALT parameter value relative to the current altitude.</td>
    <td class="tg-0lax">default_alt_takeoff --&gt; target_alt == curr_alt + MIS_TAKEOFF_ALT</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.TAKEOFF2</td>
    <td class="tg-0lax">When the vehicle conducts a taking off command, the speed of ascent must be the MPC_TKO_SPEED parameter value.</td>
    <td class="tg-0lax">takingoff --&gt; ascend_rate == MPC_TKO_SPEED</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh" rowspan="4">Failsafe</td>
    <td class="tg-0lax">PX_RCFS_P1</td>
    <td class="tg-0lax">The RC failsafe mode is triggered only when the physical link is lost and the RC message update time exceeds COM_RC_LOSS_T.</td>
    <td class="tg-0lax">A[] (RC_failsafe == true imply RC_physical_link_loss and rc_update_time &gt; COM_RC_LOSS_T)</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.GPS.FS1</td>
    <td class="tg-0lax">If GPS signal loss is detected and a time delay duration has elapsed, then the GPS failsafe must be activated.</td>
    <td class="tg-0lax">gps_failsafe_triggered --&gt; (time_since_gps_loss &gt;= COM_POS_FS_DELAY and COM_POS_FS_DELAY &gt; 0)</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.GPS.FS2</td>
    <td class="tg-0lax">If the GPS failsafe is triggered and a remote controller is available, then the flight mode must be changed to ALTITUDE mode.</td>
    <td class="tg-0lax">(curr_mode == POSITION and battery_ok and failsafe_start and gps_fail and rc_available) --&gt; mode == ALTITUDE</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-0lax">PX.GPS.FS3</td>
    <td class="tg-0lax">If a GPS failsafe occurs while the RC link is lost, then the system shall automatically transition to LAND mode.</td>
    <td class="tg-0lax">(curr_mode == POSITION and battery_ok and failsafe_start and gps_fail and rc_lost) --&gt; mode == LAND</td>
    <td class="tg-0lax">satisfied</td>
    <td class="tg-0lax">unsatisfied</td>
  </tr>
  <tr>
    <td class="tg-baqh">BRAKE</td>
    <td class="tg-0lax">PX_BRAKE_P1</td>
    <td class="tg-0lax">If emergency braking is triggered by an excessive vertical rate, then the current vertical speed must be greater than the maximum allowed vertical speed(&gt;0).</td>
    <td class="tg-0lax">(Vertical_Speed_Exceeded == true and Emergency_Braking_activated == true) --&gt; (vertical_speed &gt; max_vertical_speed and max_vertical_speed &gt; 0)</td>
    <td class="tg-0lax">unsatisfied</td>
    <td class="tg-0lax">satisfied</td>
  </tr>
</tbody></table>