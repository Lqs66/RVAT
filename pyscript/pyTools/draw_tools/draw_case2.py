#!/usr/bin/env python3

import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from pymavlink import mavutil

plt.rcParams.update({
    "font.family": "serif",
    "font.serif": ["Times New Roman", "DejaVu Serif", "Liberation Serif", "serif"],
    "font.size": 14,
    "axes.labelsize": 14,
    "xtick.labelsize": 11,
    "ytick.labelsize": 11,
    "font.weight": "bold", 
    "axes.labelweight": "bold",
    "mathtext.fontset": "cm", 
    "axes.unicode_minus": False
})

sns.set_theme(style="ticks", font="serif", rc={"font.serif": ["Times New Roman", "DejaVu Serif", "serif"]})


def parse_ardupilot_log(log_path):
    """Parse ArduPilot .bin log file and extract relevant data"""
    print(f"Loading ArduPilot log: {log_path}")
    
    mlog = mavutil.mavlink_connection(log_path)
    
    rcin_data = {'timestamp': [], 'C3': []}
    rcout_data = {'timestamp': [], 'C1': [], 'C2': [], 'C3': [], 'C4': []}
    esc_data = {'timestamp': [], 'RPM1': [], 'RPM2': [], 'RPM3': [], 'RPM4': []}
    alt_data = {'timestamp': [], 'Alt': []}
    param_data = []
    mode_data = {'timestamp': [], 'Mode': [], 'ModeNum': []}
    arm_data = {'timestamp': [], 'Armed': []}
    
    msg_types_found = set()
    
    while True:
        msg = mlog.recv_match(blocking=False)
        if msg is None:
            break
        
        msg_type = msg.get_type()
        msg_types_found.add(msg_type)
        
        if msg_type == 'RCIN':
            rcin_data['timestamp'].append(msg.TimeUS / 1e6)
            rcin_data['C3'].append(msg.C3)
        
        elif msg_type == 'RCOU':
            rcout_data['timestamp'].append(msg.TimeUS / 1e6)
            rcout_data['C1'].append(msg.C1)
            rcout_data['C2'].append(msg.C2)
            rcout_data['C3'].append(msg.C3)
            rcout_data['C4'].append(msg.C4)
        
        elif msg_type == 'ESC':
            esc_data['timestamp'].append(msg.TimeUS / 1e6)
            if hasattr(msg, 'RPM'):
                esc_data['RPM1'].append(msg.RPM)
                esc_data['RPM2'].append(0)
                esc_data['RPM3'].append(0)
                esc_data['RPM4'].append(0)
        
        elif msg_type == 'ESCX':
            ts = msg.TimeUS / 1e6
            if hasattr(msg, 'Inst'):
                inst = msg.Inst
                rpm = msg.RPM if hasattr(msg, 'RPM') else 0
                
                if inst == 0:
                    if ts not in [t for t in esc_data['timestamp'][-10:]]:
                        esc_data['timestamp'].append(ts)
                        esc_data['RPM1'].append(rpm)
                        esc_data['RPM2'].append(0)
                        esc_data['RPM3'].append(0)
                        esc_data['RPM4'].append(0)
                    else:
                        if len(esc_data['RPM1']) > 0:
                            esc_data['RPM1'][-1] = rpm
                elif inst == 1 and len(esc_data['RPM2']) > 0:
                    esc_data['RPM2'][-1] = rpm
                elif inst == 2 and len(esc_data['RPM3']) > 0:
                    esc_data['RPM3'][-1] = rpm
                elif inst == 3 and len(esc_data['RPM4']) > 0:
                    esc_data['RPM4'][-1] = rpm
        
        elif msg_type == 'BARO' or msg_type == 'BAR2':
            alt_data['timestamp'].append(msg.TimeUS / 1e6)
            alt_data['Alt'].append(msg.Alt)
        
        elif msg_type == 'PARM':
            if 'FS_THR_VALUE' in msg.Name:
                param_data.append({
                    'timestamp': msg.TimeUS / 1e6,
                    'name': msg.Name,
                    'value': msg.Value
                })
        
        elif msg_type == 'MODE':
            mode_data['timestamp'].append(msg.TimeUS / 1e6)
            mode_data['Mode'].append(msg.Mode)
            mode_data['ModeNum'].append(msg.ModeNum)
        
        elif msg_type == 'ARM':
            arm_data['timestamp'].append(msg.TimeUS / 1e6)
            arm_data['Armed'].append(msg.ArmState)
    
    for key in rcin_data:
        rcin_data[key] = np.array(rcin_data[key])
    for key in rcout_data:
        rcout_data[key] = np.array(rcout_data[key])
    for key in esc_data:
        esc_data[key] = np.array(esc_data[key])
    for key in alt_data:
        alt_data[key] = np.array(alt_data[key])
    for key in mode_data:
        mode_data[key] = np.array(mode_data[key])
    for key in arm_data:
        arm_data[key] = np.array(arm_data[key])
    
    print(f"  RCIN messages: {len(rcin_data['timestamp'])}")
    print(f"  RCOU messages: {len(rcout_data['timestamp'])}")
    print(f"  ESC messages: {len(esc_data['timestamp'])}")
    if len(esc_data['timestamp']) > 0:
        print(f"  ESC RPM1 max: {np.max(esc_data['RPM1'])}, RPM2 max: {np.max(esc_data['RPM2'])}")
        print(f"  ESC non-zero samples: RPM1={np.sum(esc_data['RPM1'] > 0)}, RPM2={np.sum(esc_data['RPM2'] > 0)}")
    print(f"  Altitude messages: {len(alt_data['timestamp'])}")
    print(f"  Parameter changes: {len(param_data)}")
    print(f"  Mode changes: {len(mode_data['timestamp'])}")
    print(f"  Arm/Disarm events: {len(arm_data['timestamp'])}")
    print(f"  Message types found: {sorted(msg_types_found)[:20]}...")
    
    return {
        'rcin': rcin_data,
        'rcout': rcout_data,
        'esc': esc_data,
        'altitude': alt_data,
        'params': param_data,
        'mode': mode_data,
        'arm': arm_data
    }


def plot_combined_figure(data, output_dir):
    """Plot combined figure with throttle input, motor output, and altitude"""
    
    fig, (ax1, ax2, ax3) = plt.subplots(3, 1, figsize=(12, 5.6), sharex=True,
                                         gridspec_kw={'hspace': 0.1, 'left': 0.1})
    
    rcin = data['rcin']
    rcout = data['rcout']
    esc = data['esc']
    alt = data['altitude']
    params = data['params']
    mode_data = data['mode']
    arm_data = data['arm']
    
    if len(rcin['timestamp']) == 0 or len(rcout['timestamp']) == 0:
        print("Warning: Missing required data")
        return
    
    t0 = min(rcin['timestamp'][0], rcout['timestamp'][0])
    rcin_times = rcin['timestamp'] - rcin['timestamp'][0]
    rcout_times = rcout['timestamp'] - rcout['timestamp'][0]
    
    fs_thr_time = None
    for param in params:
        if 'FS_THR_VALUE' in param['name'] and param['value'] == -1:
            fs_thr_time = param['timestamp'] - rcin['timestamp'][0]
            break
    
    failsafe_time = None
    for i, (ts, mode) in enumerate(zip(mode_data['timestamp'], mode_data['Mode'])):
        if 'FAIL' in str(mode).upper() or 'RTL' in str(mode).upper():
            failsafe_time = ts - rcin['timestamp'][0]
            break
    
    disarm_time = None
    for ts, armed in zip(arm_data['timestamp'], arm_data['Armed']):
        if armed == 0:
            disarm_time = ts - rcin['timestamp'][0]
            break
    
    zero_throttle_time = None
    for i, pwm in enumerate(rcin['C3']):
        if pwm < 1000:
            zero_throttle_time = rcin_times[i]
            break
    
    # Detect the time period starting from 30s when throttle is 1000
    zero_throttle_after_30s = None
    for i, (t, pwm) in enumerate(zip(rcin_times, rcin['C3'])):
        if t >= 30.0 and pwm == 1000:
            zero_throttle_after_30s = t
            break
    
    trigger_time = disarm_time if disarm_time is not None else failsafe_time
    
    # Calculate gray_start time for altitude detection (needs to be done before background drawing)
    gray_start = None
    if len(alt['timestamp']) > 0:
        alt_times_temp = alt['timestamp'] - rcin['timestamp'][0]
        altitude_temp = alt['Alt']
        initial_alt = altitude_temp[0]
        for i, (t, alt_val) in enumerate(zip(alt_times_temp, altitude_temp)):
            if t >= 60.0 and alt_val <= initial_alt:
                gray_start = t
                break
    
    # Define colors
    colors = sns.color_palette("husl", 1)
    base_pink = colors[0]  # Use the same color as throttle input line
    # Make the pink color slightly deeper - light red, darker than background
    pink_color = tuple(np.array(base_pink) * 0.85)  # Darken by 15% to be slightly darker than background
    
    # Draw background regions
    if zero_throttle_after_30s is not None and trigger_time is not None:
        # Green background: from 0 to disarm (merged green and orange-yellow)
        ax1.axvspan(0, trigger_time, alpha=0.15, color='green', zorder=0)
        ax2.axvspan(0, trigger_time, alpha=0.15, color='green', zorder=0)
        ax3.axvspan(0, trigger_time, alpha=0.15, color='green', zorder=0)
        
        # Pink background: after disarm (but before gray if gray exists)
        x_max = max(rcin_times[-1], rcout_times[-1])
        pink_end = gray_start if gray_start is not None else x_max
        ax1.axvspan(trigger_time, pink_end, alpha=0.15, color=pink_color, zorder=0)
        ax2.axvspan(trigger_time, pink_end, alpha=0.15, color=pink_color, zorder=0)
        ax3.axvspan(trigger_time, pink_end, alpha=0.15, color=pink_color, zorder=0)
        
        # Gray background: when altitude drops to initial level after 60s
        if gray_start is not None:
            ax1.axvspan(gray_start, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
            ax2.axvspan(gray_start, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
            ax3.axvspan(gray_start, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
    elif trigger_time is not None:
        # If no zero throttle detected after 30s, only show pink background
        x_max = max(rcin_times[-1], rcout_times[-1])
        pink_end = gray_start if gray_start is not None else x_max
        ax1.axvspan(trigger_time, pink_end, alpha=0.15, color=pink_color, zorder=0)
        ax2.axvspan(trigger_time, pink_end, alpha=0.15, color=pink_color, zorder=0)
        ax3.axvspan(trigger_time, pink_end, alpha=0.15, color=pink_color, zorder=0)
        
        # Gray background: when altitude drops to initial level after 60s
        if gray_start is not None:
            ax1.axvspan(gray_start, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
            ax2.axvspan(gray_start, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
            ax3.axvspan(gray_start, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
    ax1.plot(rcin_times, rcin['C3'], linewidth=2.5, color=colors[0], 
             alpha=0.9, label='Throttle PWM')
    
    ax1.axhline(y=1000, color='gray', linestyle='--', linewidth=2, 
                label='Zero Throttle Threshold', alpha=0.7)
    
    ax1.set_ylabel('PWM (μs)', fontsize=14, fontweight='bold')
    ax1.set_ylim(top=1700)
    ax1.legend(loc='upper left', bbox_to_anchor=(0, 1.02), frameon=False, fontsize=12, ncol=2)
    ax1.grid(True, linestyle=':', alpha=0.6)
    ax1.set_xlim(left=0, right=rcin_times[-1])
    ax1.set_xticks(np.arange(0, rcin_times[-1] + 1, 5))
    
    use_esc = len(esc['timestamp']) > 0 and np.any(esc['RPM1'] > 0)
    motor_data = esc if use_esc else rcout
    
    if use_esc:
        timestamps = motor_data['timestamp'] - rcin['timestamp'][0]
        m1, m2, m3, m4 = motor_data['RPM1'], motor_data['RPM2'], motor_data['RPM3'], motor_data['RPM4']
        ylabel = 'Motor RPM'
    else:
        timestamps = rcout_times
        m1, m2, m3, m4 = rcout['C1'], rcout['C2'], rcout['C3'], rcout['C4']
        ylabel = 'PWM (μs)'
    
    colors_motor = sns.color_palette("dark", 4)
    colors_motor2 = sns.color_palette("husl", 4)
    ax2.plot(timestamps, m1, linewidth=8, color=colors_motor[0], alpha=1, label='Motor 1')
    ax2.plot(timestamps, m2, linewidth=5, color=sns.color_palette("bright")[8], alpha=1, label='Motor 2')
    ax2.plot(timestamps, m3, linewidth=4.5, color=sns.color_palette()[2], alpha=1, label='Motor 3')
    ax2.plot(timestamps, m4, linewidth=2, color=sns.color_palette("bright")[3], alpha=1, label='Motor 4')
    
    ax2.set_ylabel(ylabel, fontsize=14, fontweight='bold')
    if not use_esc:  # Only set PWM limit for RCOUT data, not RPM
        ax2.set_ylim(top=2000)
    ax2.legend(loc='upper left', bbox_to_anchor=(0, 1.02), frameon=False, fontsize=12, ncol=4, columnspacing=1.0)
    ax2.grid(True, linestyle=':', alpha=0.6)
    ax2.set_xlim(left=0, right=rcout_times[-1])
    ax2.set_xticks(np.arange(0, rcout_times[-1] + 1, 5))
    
    if len(alt['timestamp']) > 0:
        alt_times = alt['timestamp'] - rcin['timestamp'][0]
        altitude = alt['Alt']
        
        # Gray background already drawn above, no need to draw again here
        
        colors_alt = sns.color_palette("husl", 2)
        ax3.plot(alt_times, altitude, linewidth=2.5, color=colors_alt[1], 
                 marker='o', markersize=2, label='Altitude', alpha=0.8)
        
        ax3.set_ylabel('Altitude (m)', fontsize=14, fontweight='bold')
        ax3.legend(loc='upper left', bbox_to_anchor=(0, 1.02), frameon=False, fontsize=12)
        ax3.grid(True, linestyle=':', alpha=0.6)
        ax3.set_xlim(left=0, right=alt_times[-1])
        ax3.set_xticks(np.arange(0, alt_times[-1] + 1, 5))
    
    ax3.set_xlabel('Time (s)', fontsize=14, fontweight='bold')
    
    plt.tight_layout()
    
    # Add background text labels to ax1 after tight_layout
    if zero_throttle_after_30s is not None and trigger_time is not None:
        y_text = ax1.get_ylim()[0] + (ax1.get_ylim()[1] - ax1.get_ylim()[0]) * 0.08
        x_max = max(rcin_times[-1], rcout_times[-1])
        
        # Normal Flight text (green region)
        ax1.text(trigger_time / 2, y_text, 'Normal Flight', 
                ha='center', va='bottom', fontsize=12, fontweight='bold', 
                color='darkgreen', alpha=1.0, zorder=5)
        
        # Disarmed text (pink region)
        if gray_start is not None:
            disarmed_center = (trigger_time + gray_start) / 2
        else:
            disarmed_center = (trigger_time + x_max) / 2
        ax1.text(disarmed_center, y_text, 'Disarm', 
                ha='center', va='bottom', fontsize=12, fontweight='bold', 
                color='darkred', alpha=1.0, zorder=5)
        
        # Crashed text (gray region)
        if gray_start is not None:
            crashed_center = (gray_start + x_max) / 2
            ax1.text(crashed_center, y_text, 'Crash', 
                    ha='center', va='bottom', fontsize=12, fontweight='bold', 
                    color='black', alpha=1.0, zorder=5)
    
    # Add annotations after tight_layout
    # Define annotation colors - lighter versions of background colors
    blue_color = (0.3, 0.5, 0.9)  # Light blue, slightly darker than background
    yellow_color = (0.75, 0.6, 0.05)  # Darker yellow, more visible
    
    # Annotation 1: Throttle stick to zero in ax1 (blue region)
    if zero_throttle_after_30s is not None:
        # Find the boundary point
        idx_boundary = np.where(rcin_times >= zero_throttle_after_30s)[0][0]
        
        # Find the starting point: search backward to find the previous turning point
        idx_start = idx_boundary - 1
        for i in range(idx_boundary - 2, max(0, idx_boundary - 50), -1):
            if i > 0:
                # Check if this is a turning point (change in slope direction or significant change)
                diff_before = rcin['C3'][i] - rcin['C3'][i-1]
                diff_after = rcin['C3'][i+1] - rcin['C3'][i]
                # Look for where the descent starts (slope changes from ~0 or positive to negative)
                if diff_after < -10 and abs(diff_before) < 10:
                    idx_start = i
                    break
        
        # Point arrow to the left endpoint (start point) of the segment
        point_x = rcin_times[idx_start]
        point_y = rcin['C3'][idx_start]
        
        text_x = zero_throttle_after_30s * 0.8  # Place text in the middle of blue region
        text_y = ax1.get_ylim()[1] * 0.7
        ax1.annotate(f'Throttle stick to zero\nAt {point_x:.1f} s', 
                     xy=(point_x, point_y), xytext=(text_x, text_y),
                     ha='center', fontsize=12, fontweight='bold', color=blue_color,
                     bbox=dict(boxstyle='round,pad=0.5', facecolor='white', edgecolor=blue_color, alpha=0.9),
                     arrowprops=dict(arrowstyle='->', connectionstyle='arc3,rad=0.3', color=blue_color, lw=2))
    
    # Annotation 2: Motor maintains minimum PWM in ax2 (blue-yellow boundary, bottom of blue region)
    if zero_throttle_after_30s is not None:
        # Find the first turning point at the blue-yellow boundary
        idx = np.where(timestamps >= zero_throttle_after_30s)[0][0]
        point_x = timestamps[idx]
        point_y = m1[idx]  # Use motor 1 as reference
        text_x = zero_throttle_after_30s * 0.5
        text_x += 7
        text_y = ax2.get_ylim()[0] + (ax2.get_ylim()[1] - ax2.get_ylim()[0]) * 0.2
        ax2.annotate('Motors maintain low-level PWM\nto prevent shutdown when throttle is zero', 
                     xy=(point_x, point_y), xytext=(text_x, text_y),
                     ha='center', fontsize=12, fontweight='bold', color=yellow_color,
                     bbox=dict(boxstyle='round,pad=0.5', facecolor='white', edgecolor=yellow_color, alpha=0.9),
                     arrowprops=dict(arrowstyle='->', color=yellow_color, lw=2))
    
    # Annotation 3: Disarm in ax2 (pink region, pointing to ~35s before trigger)
    if trigger_time is not None and trigger_time > 2:
        # Find the point about 1-2 seconds before trigger_time where motor PWM starts dropping
        search_start = max(0, trigger_time - 3)
        idx_range = np.where((timestamps >= search_start) & (timestamps <= trigger_time))[0]
        if len(idx_range) > 0:
            # Find where motor output starts dropping significantly
            motor_avg = (m1 + m2 + m3 + m4) / 4.0
            idx = idx_range[0] + np.argmin(motor_avg[idx_range])
            # Point to the turning point TWO steps after current idx
            idx = idx + 8
            point_x = timestamps[idx]
            point_y = motor_avg[idx]
            text_x = trigger_time + 6
            text_y = trigger_time + 0.3 * (ax2.get_ylim()[1] - ax2.get_ylim()[0]) + ax2.get_ylim()[0]
            ax2.annotate(f'Disarm after\nGCS Failsafe\nAt {point_x:.1f} s', 
                         xy=(point_x, point_y), xytext=(text_x, text_y),
                         ha='center', fontsize=12, fontweight='bold', color=pink_color,
                         bbox=dict(boxstyle='round,pad=0.5', facecolor='white', edgecolor=pink_color, alpha=0.9),
                         arrowprops=dict(arrowstyle='->', color=pink_color, lw=2))
    
    # Annotation 4: Loss of thrust, falling in ax3 (pink region, point to boundary)
    if trigger_time is not None and len(alt['timestamp']) > 0:
        # Find the point where altitude curve intersects with pink region boundary
        idx = np.where(alt_times >= trigger_time)[0]
        if len(idx) > 0:
            point_x = alt_times[idx[0]]
            point_y = altitude[idx[0]]
            x_max = max(rcin_times[-1], rcout_times[-1])
            text_x = trigger_time + (x_max - trigger_time) * 0.55
            text_y = ax3.get_ylim()[1] * 0.75
            ax3.annotate('Thrust Lost', 
                         xy=(point_x, point_y), xytext=(text_x, text_y),
                         ha='center', fontsize=12, fontweight='bold', color=pink_color,
                         bbox=dict(boxstyle='round,pad=0.5', facecolor='white', edgecolor=pink_color, alpha=0.9),
                         arrowprops=dict(arrowstyle='->', color=pink_color, lw=2))
    
    output_path_svg = Path(output_dir) / 'case2_combined.svg'
    output_path_pdf = Path(output_dir) / 'case2_combined.pdf'
    
    fig.savefig(output_path_svg, format='svg', dpi=600, bbox_inches='tight')
    fig.savefig(output_path_pdf, format='pdf', dpi=600, bbox_inches='tight')
    
    print(f"Combined figure saved:")
    print(f"  SVG: {output_path_svg}")
    print(f"  PDF: {output_path_pdf}")
    
    plt.close(fig)


def plot_throttle_input(data, output_dir):
    """Plot throttle input PWM changes"""
    
    fig, ax = plt.subplots(1, 1, figsize=(12, 3))
    
    rcin = data['rcin']
    params = data['params']
    mode_data = data['mode']
    arm_data = data['arm']
    
    if len(rcin['timestamp']) == 0:
        print("Warning: No throttle input data found")
        return
    
    rcin_times = rcin['timestamp'] - rcin['timestamp'][0]
    throttle_input = rcin['C3']
    t0 = rcin['timestamp'][0]
    
    fs_thr_time = None
    for param in params:
        if 'FS_THR_VALUE' in param['name'] and param['value'] == -1:
            fs_thr_time = param['timestamp'] - t0
            break
    
    failsafe_time = None
    for i, (ts, mode) in enumerate(zip(mode_data['timestamp'], mode_data['Mode'])):
        if 'FAIL' in str(mode).upper() or 'RTL' in str(mode).upper():
            failsafe_time = ts - t0
            break
    
    disarm_time = None
    for ts, armed in zip(arm_data['timestamp'], arm_data['Armed']):
        if armed == 0:
            disarm_time = ts - t0
            break
    
    zero_throttle_time = None
    for i, pwm in enumerate(throttle_input):
        if pwm < 1000:
            zero_throttle_time = rcin_times[i]
            break
    
    colors = sns.color_palette("husl", 1)
    ax.plot(rcin_times, throttle_input, linewidth=2.5, color=colors[0], 
            alpha=0.9, label='Throttle Input (RC Channel 3)')
    
    ax.axhline(y=1000, color='red', linestyle='--', linewidth=2, 
               label='Zero Throttle Threshold', alpha=0.7)
    
    if zero_throttle_time is not None:
        ax.axvline(x=zero_throttle_time, color='blue', linestyle=':', linewidth=2.5, 
                   label='Zero Throttle', alpha=0.8)
    
    if failsafe_time is not None:
        ax.axvline(x=failsafe_time, color='orange', linestyle='-', linewidth=2.5, 
                   label='Enter Failsafe', alpha=0.8)
    
    if disarm_time is not None:
        ax.axvline(x=disarm_time, color='red', linestyle='-', linewidth=2.5, 
                   label='Disarm', alpha=0.8)
    
    ax.set_xlabel('Time (s)', fontsize=14, fontweight='bold')
    ax.set_ylabel('Throttle PWM', fontsize=14, fontweight='bold')
    ax.set_title('Throttle Input Changes', fontsize=15, fontweight='bold', pad=15)
    
    ax.legend(loc='upper center', bbox_to_anchor=(0.5, -0.1), frameon=False, fontsize=14, ncol=3)
    ax.grid(True, linestyle=':', alpha=0.6)
    
    plt.tight_layout()
    
    output_path_svg = Path(output_dir) / 'case2_throttle_input.svg'
    output_path_pdf = Path(output_dir) / 'case2_throttle_input.pdf'
    
    fig.savefig(output_path_svg, format='svg', dpi=600, bbox_inches='tight')
    fig.savefig(output_path_pdf, format='pdf', dpi=600, bbox_inches='tight')
    
    print(f"Throttle input figure saved:")
    print(f"  SVG: {output_path_svg}")
    print(f"  PDF: {output_path_pdf}")
    
    plt.close(fig)


def plot_pwm_changes(data, output_dir):
    """Plot motor output changes from zero throttle to disarm"""
    
    fig, ax = plt.subplots(1, 1, figsize=(12, 2))
    
    rcin = data['rcin']
    rcout = data['rcout']
    esc = data['esc']
    params = data['params']
    mode_data = data['mode']
    arm_data = data['arm']
    
    use_esc = len(esc['timestamp']) > 0 and np.any(esc['RPM1'] > 0)
    motor_data = esc if use_esc else rcout
    data_label = 'ESC RPM' if use_esc else 'RCOU PWM'
    
    if len(motor_data['timestamp']) == 0:
        print("Warning: No motor output data found")
        return
    
    print(f"  Using {data_label} data for motor output")
    
    if use_esc:
        timestamps = motor_data['timestamp']
        m1, m2, m3, m4 = motor_data['RPM1'], motor_data['RPM2'], motor_data['RPM3'], motor_data['RPM4']
        ylabel = 'Motor RPM'
    else:
        timestamps = motor_data['timestamp']
        m1, m2, m3, m4 = motor_data['C1'], motor_data['C2'], motor_data['C3'], motor_data['C4']
        ylabel = 'Motor PWM Output'
    
    motor_avg = (m1 + m2 + m3 + m4) / 4.0
    
    timestamps = timestamps - timestamps[0]
    t0 = motor_data['timestamp'][0]
    
    fs_thr_time = None
    for param in params:
        if 'FS_THR_VALUE' in param['name'] and param['value'] == -1:
            fs_thr_time = param['timestamp'] - t0
            print(f"  FS_THR_VALUE set to -1 at: {fs_thr_time:.2f}s")
            break
    
    failsafe_time = None
    for i, (ts, mode) in enumerate(zip(mode_data['timestamp'], mode_data['Mode'])):
        if 'FAIL' in str(mode).upper() or 'RTL' in str(mode).upper():
            failsafe_time = ts - t0
            print(f"  Failsafe mode '{mode}' at: {failsafe_time:.2f}s")
            break
    
    disarm_time = None
    for ts, armed in zip(arm_data['timestamp'], arm_data['Armed']):
        if armed == 0:
            disarm_time = ts - t0
            print(f"  Disarm at: {disarm_time:.2f}s")
            break
    
    zero_throttle_time = None
    if len(rcin['timestamp']) > 0:
        rcin_times = rcin['timestamp'] - rcin['timestamp'][0]
        for i, pwm in enumerate(rcin['C3']):
            if pwm < 1000:
                zero_throttle_time = rcin_times[i]
                print(f"  Zero throttle input at: {zero_throttle_time:.2f}s, PWM={pwm}")
                break
    
    colors = sns.color_palette("husl", 4)
    ax.plot(timestamps, m1, linewidth=2, color=colors[0], alpha=0.7, label='Motor 1')
    ax.plot(timestamps, m2, linewidth=2, color=colors[1], alpha=0.7, label='Motor 2')
    ax.plot(timestamps, m3, linewidth=2, color=colors[2], alpha=0.7, label='Motor 3')
    ax.plot(timestamps, m4, linewidth=2, color=colors[3], alpha=0.7, label='Motor 4')
    ax.plot(timestamps, motor_avg, linewidth=3, color='black', linestyle='--', 
            alpha=0.9, label='Average')
    
    if zero_throttle_time is not None:
        ax.axvline(x=zero_throttle_time, color='blue', linestyle=':', linewidth=2.5, 
                   label='Zero Throttle', alpha=0.8)
    
    if failsafe_time is not None:
        ax.axvline(x=failsafe_time, color='orange', linestyle='-', linewidth=2.5, 
                   label='Enter Failsafe', alpha=0.8)
    
    if disarm_time is not None:
        ax.axvline(x=disarm_time, color='red', linestyle='-', linewidth=2.5, 
                   label='Disarm', alpha=0.8)
    
    ax.set_xlabel('Time (s)', fontsize=14, fontweight='bold')
    ax.set_ylabel(ylabel, fontsize=14, fontweight='bold')
    ax.set_title('Motor Output Changes: Zero Throttle to Disarm', 
                 fontsize=15, fontweight='bold', pad=15)
    
    ax.legend(loc='upper center', bbox_to_anchor=(0.5, -0.1), frameon=False, fontsize=12, ncol=4)
    ax.grid(True, linestyle=':', alpha=0.6)
    
    plt.tight_layout()
    
    output_path_svg = Path(output_dir) / 'case2_motor_output.svg'
    output_path_pdf = Path(output_dir) / 'case2_motor_output.pdf'
    
    fig.savefig(output_path_svg, format='svg', dpi=600, bbox_inches='tight')
    fig.savefig(output_path_pdf, format='pdf', dpi=600, bbox_inches='tight')
    
    print(f"Motor output figure saved:")
    print(f"  SVG: {output_path_svg}")
    print(f"  PDF: {output_path_pdf}")
    
    plt.close(fig)


def plot_altitude_changes(data, output_dir):
    """Plot altitude changes aligned with time"""
    
    fig, ax = plt.subplots(1, 1, figsize=(12, 3))
    
    alt = data['altitude']
    
    if len(alt['timestamp']) == 0:
        print("Warning: No altitude data found")
        return
    
    timestamps = alt['timestamp']
    altitude = alt['Alt']
    
    timestamps = timestamps - timestamps[0]
    
    colors = sns.color_palette("husl", 2)
    ax.plot(timestamps, altitude, linewidth=2.5, color=colors[1], 
            marker='o', markersize=2, label='Barometric Altitude', alpha=0.8)
    
    params = data['params']
    fs_thr_time = None
    for param in params:
        if 'FS_THR_VALUE' in param['name'] and param['value'] == -1:
            fs_thr_time = param['timestamp'] - alt['timestamp'][0]
            break
    
    if fs_thr_time is not None and 0 <= fs_thr_time <= timestamps[-1]:
        ax.axvline(x=fs_thr_time, color='orange', linestyle='-', linewidth=2.5, 
                   label='FS_THR_VALUE = -1', alpha=0.8)
    
    ax.set_xlabel('Time (s)', fontsize=14, fontweight='bold')
    ax.set_ylabel('Altitude (m)', fontsize=14, fontweight='bold')
    ax.set_title('Altitude Changes Over Time', fontsize=15, fontweight='bold', pad=15)
    
    ax.legend(loc='upper center', bbox_to_anchor=(0.5, -0.1), frameon=False, fontsize=14, ncol=2)
    ax.grid(True, linestyle=':', alpha=0.6)
    
    plt.tight_layout()
    
    output_path_svg = Path(output_dir) / 'case2_altitude_changes.svg'
    output_path_pdf = Path(output_dir) / 'case2_altitude_changes.pdf'
    
    fig.savefig(output_path_svg, format='svg', dpi=600, bbox_inches='tight')
    fig.savefig(output_path_pdf, format='pdf', dpi=600, bbox_inches='tight')
    
    print(f"Altitude figmotor output saved:")
    print(f"  SVG: {output_path_svg}")
    print(f"  PDF: {output_path_pdf}")
    
    plt.close(fig)


def main():
    log_path = '/home/lqs66/Desktop/modelCheckingFlightControl/verifyDataBase/draw_datas/case2.bin'
    output_dir = '/home/lqs66/Desktop/modelCheckingFlightControl/verifyDataBase/draw_datas'
    
    data = parse_ardupilot_log(log_path)
    
    print("\nGenerating combined figure...")
    plot_combined_figure(data, output_dir)
    
    print("\nAll figures generated successfully!")


if __name__ == '__main__':
    main()