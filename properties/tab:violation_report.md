# Reported Policy Violations

This document summarizes the policies that have been reported to the upstream projects.
Because several policies are closely related or concern the same subsystem, documentation page, or implementation behavior, they were reported together in the same GitHub issue or pull request. As a result, multiple policy IDs may share the same report link.

## Full Policy Tracking Table

| ID             | Link                                                    | State     |
| -------------- | ------------------------------------------------------- | --------- |
| AP_ALTHOLD_P1  | https://github.com/ArduPilot/ardupilot/issues/30177     | confirmed |
| AP_POSHOLD_P3  | https://github.com/ArduPilot/ardupilot/issues/30177     | confirmed |
| AP_LOITER_P1   | https://github.com/ArduPilot/ardupilot/issues/30177     | confirmed |
| A.SPORT1       | https://github.com/ArduPilot/ardupilot/issues/30177     | confirmed |
| AP_ACRO_P1     | https://github.com/ArduPilot/ardupilot/pull/31435       | fixed     |
| AP_ACRO_P2     | https://github.com/ArduPilot/ardupilot/pull/31435       | fixed     |
| AP_ZIGZAG_P1   | https://github.com/ArduPilot/ardupilot/issues/30177     | confirmed |
| AP_GCSFS_P1    | https://github.com/ArduPilot/ardupilot/issues/31798     | confirmed |
| PX.ORBIT3      | https://github.com/PX4/PX4-Autopilot/pull/26116         | fixed     |
| PX.ORBIT4      | https://github.com/PX4/PX4-Autopilot/pull/26116         | fixed     |
| PX_ORBIT_P1    | https://github.com/PX4/PX4-Autopilot/issues/26249       | pending   |
| PX_ORBIT_P2    | https://github.com/PX4/PX4-Autopilot/issues/26249       | pending   |
| PX_POSITION_P1 | https://github.com/PX4/PX4-Autopilot/issues/24930       | confirmed |
| PX_POSITION_P2 | https://github.com/PX4/PX4-Autopilot/issues/24930       | confirmed |
| PX_RCFS_P1     | https://github.com/PX4/PX4-Autopilot/issues/25045       | fixed     |
| PX_BRAKE_P1    | https://github.com/PX4/PX4-Autopilot/issues/24937       | pending   |


# Verification Note

As of July 7, 2026, we verified that `PX.ORBIT3` and `PX.ORBIT4` have been fixed by PX4 pull request `#26116`, which fixes issue `#26107` and has been merged into the PX4 main branch. Therefore, their states have been updated from `pending` to `fixed`.
