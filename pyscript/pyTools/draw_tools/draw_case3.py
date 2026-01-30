#!/usr/bin/env python3

import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from pathlib import Path
from pymavlink import mavutil
from matplotlib.ticker import MultipleLocator

plt.rcParams.update({
    "font.family": "serif",
    "font.serif": ["Times New Roman", "DejaVu Serif", "Liberation Serif", "serif"],
    "font.size": 16,
    "axes.labelsize": 16,
    "xtick.labelsize": 14,
    "ytick.labelsize": 14,
    "font.weight": "bold", 
    "axes.labelweight": "bold",
    "mathtext.fontset": "cm", 
    "axes.unicode_minus": False
})

sns.set_theme(style="ticks", 
              font="serif", 
              rc={
                    "font.size": 15,
                    "xtick.labelsize": 12,
                    "ytick.labelsize": 12,
                  "font.serif": ["Times New Roman", "DejaVu Serif", "serif"]
                }
             )   


def parse_ardupilot_log(log_path):
    """Parse ArduPilot .bin log file and extract relevant data"""
    print(f"Loading ArduPilot log: {log_path}")
    
    mlog = mavutil.mavlink_connection(log_path)
    
    att_data = {'timestamp': [], 'Roll': [], 'Pitch': [], 'Yaw': []}
    rate_data = {'timestamp': [], 'RDes': [], 'PDes': [], 'YDes': [], 'R': [], 'P': [], 'Y': []}
    alt_data = {'timestamp': [], 'Alt': []}
    param_data = []
    
    msg_types_found = set()
    
    while True:
        msg = mlog.recv_match(blocking=False)
        if msg is None:
            break
        
        msg_type = msg.get_type()
        msg_types_found.add(msg_type)
        
        if msg_type == 'ATT':
            att_data['timestamp'].append(msg.TimeUS / 1e6)
            att_data['Roll'].append(msg.Roll)
            att_data['Pitch'].append(msg.Pitch)
            att_data['Yaw'].append(msg.Yaw)
        
        elif msg_type == 'RATE':
            rate_data['timestamp'].append(msg.TimeUS / 1e6)
            rate_data['RDes'].append(msg.RDes)
            rate_data['PDes'].append(msg.PDes)
            rate_data['YDes'].append(msg.YDes)
            rate_data['R'].append(msg.R)
            rate_data['P'].append(msg.P)
            rate_data['Y'].append(msg.Y)
        
        elif msg_type == 'BARO' or msg_type == 'BAR2':
            alt_data['timestamp'].append(msg.TimeUS / 1e6)
            alt_data['Alt'].append(msg.Alt)
        
        elif msg_type == 'PARM':
            param_data.append({
                'timestamp': msg.TimeUS / 1e6,
                'name': msg.Name,
                'value': msg.Value
            })
    
    for key in att_data:
        att_data[key] = np.array(att_data[key])
    for key in rate_data:
        rate_data[key] = np.array(rate_data[key])
    for key in alt_data:
        alt_data[key] = np.array(alt_data[key])
    
    print(f"  ATT messages: {len(att_data['timestamp'])}")
    print(f"  RATE messages: {len(rate_data['timestamp'])}")
    print(f"  Altitude messages: {len(alt_data['timestamp'])}")
    print(f"  Parameter changes: {len(param_data)}")
    print(f"  Message types found: {sorted(msg_types_found)[:20]}...")
    
    return {
        'attitude': att_data,
        'rate': rate_data,
        'altitude': alt_data,
        'params': param_data
    }


def plot_case3_roll_figure(data, output_dir):
    """Plot figure for ACRO_BAL_ROLL case"""
    
    param_name = 'ACRO_BAL_ROLL'
    figure_name = 'case3_roll'
    
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 3.5), sharex=True,
                                    gridspec_kw={'hspace': 0.1})
    
    att = data['attitude']
    alt = data['altitude']
    params = data['params']
    
    if len(att['timestamp']) == 0:
        print("Warning: Missing required data")
        return
    
    t0 = att['timestamp'][0]
    att_times = att['timestamp'] - t0
    
    # Find parameter change time (looking for value = -10 or -20)
    param_change_time = None
    param_changes = []
    for param in params:
        if param_name in param['name']:
            time = param['timestamp'] - t0
            value = param['value']
            param_changes.append((time, value))
            print(f"  {param_name} changed at: {time:.2f}s, value: {value}")
            if value == -10 or abs(value + 10) < 0.001:
                param_change_time = time
            elif value == -20 or abs(value + 20) < 0.001:
                param_change_time = time
                print(f"  *** Found -20 at: {time:.2f}s ***")
    
    if param_change_time is None and len(param_changes) > 0:
        param_change_time = param_changes[0][0]
        print(f"  Using first parameter change at: {param_change_time:.2f}s")
    
    # Find the first Roll attitude turning point after 50s
    roll_turning_point = None
    threshold_time = 50.0
    idx_after_50s = np.where(att_times >= threshold_time)[0]
    if len(idx_after_50s) > 2:
        roll_segment = att['Roll'][idx_after_50s]
        time_segment = att_times[idx_after_50s]
        print(f"  Searching for Roll turning point after {threshold_time}s...")
        # Calculate differences between consecutive points
        roll_diffs = np.abs(np.diff(roll_segment))
        # Find the first point with a change > 10 degrees (significant attitude change)
        for i in range(len(roll_diffs)):
            if roll_diffs[i] > 10.0:
                roll_turning_point = time_segment[i]
                print(f"  Roll turning point found at: {roll_turning_point:.2f}s, Roll change: {roll_diffs[i]:.2f} deg")
                break
    
    # Draw background regions with three colors
    if param_change_time is not None:
        # Green: before parameter change
        ax1.axvspan(0, param_change_time, alpha=0.15, color='green', zorder=0)
        ax2.axvspan(0, param_change_time, alpha=0.15, color='green', zorder=0)
        
        if roll_turning_point is not None:
            # Red: from parameter change to turning point
            ax1.axvspan(param_change_time, roll_turning_point, alpha=0.15, color='red', zorder=0)
            ax2.axvspan(param_change_time, roll_turning_point, alpha=0.15, color='red', zorder=0)
            # Gray: after turning point
            x_max = att_times[-1]
            ax1.axvspan(roll_turning_point, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
            ax2.axvspan(roll_turning_point, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
        else:
            # If no turning point found, use gray for everything after parameter change
            x_max = att_times[-1]
            ax1.axvspan(param_change_time, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
            ax2.axvspan(param_change_time, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
    
    # Plot 1: Attitude
    colors_att = sns.color_palette("husl", 3)
    ax1.plot(att_times, att['Roll'], linewidth=2.5, color=colors_att[0], alpha=0.9, label='Roll')
    ax1.plot(att_times, att['Pitch'], linewidth=2.5, color=colors_att[1], alpha=0.9, label='Pitch')
    ax1.set_ylabel('Attitude (deg)', fontsize=13, fontweight='bold')
    ax1.grid(True, linestyle=':', alpha=0.6)
    x_limit = 65
    ax1.set_xlim(left=0, right=x_limit)
    
    # Plot 2: Altitude
    if len(alt['timestamp']) > 0:
        alt_times = alt['timestamp'] - t0
        altitude = alt['Alt']
        colors_alt = sns.color_palette("husl", 2)
        ax2.plot(alt_times, altitude, linewidth=2.5, color=colors_alt[1], 
                 marker='o', markersize=2, label='Altitude', alpha=0.8)
        ax2.set_ylabel('Altitude (m)', fontsize=13, fontweight='bold')
        ax2.grid(True, linestyle=':', alpha=0.6)
        ax2.set_xlim(left=0, right=x_limit)
        
        handles1, labels1 = ax1.get_legend_handles_labels()
        handles2, labels2 = ax2.get_legend_handles_labels()
        ax2.legend(handles1 + handles2, labels1 + labels2, 
                  loc='upper center', bbox_to_anchor=(0.5, -0.35), 
                  frameon=False, fontsize=13, ncol=3)
    
    ax2.set_xlabel('Time (s)', fontsize=13, fontweight='bold')
    plt.tight_layout()
    
    # Annotations
    param_change_color = (0.85, 0.55, 0.0)  # Darker orange-yellow for parameter change
    dark_orange = (0.75, 0.4, 0.05)
    
    # Annotation 1: Parameter change
    if param_change_time is not None:
        idx = np.where(att_times >= param_change_time)[0]
        if len(idx) > 0:
            point_x = att_times[idx[0]]
            point_y = att['Roll'][idx[0]]
            text_x = param_change_time * 1.7
            text_y = ax1.get_ylim()[1] * -1.06
            ax1.annotate(f'{param_name}\nchanged At {param_change_time:.1f} s', 
                         xy=(point_x, point_y), xytext=(text_x, text_y),
                         ha='center', fontsize=12, fontweight='bold', color=param_change_color,
                         bbox=dict(boxstyle='round,pad=0.3', facecolor='white', 
                                 edgecolor=param_change_color, alpha=0.9),
                         arrowprops=dict(arrowstyle='->', connectionstyle='arc3,rad=0.3', 
                                       color=param_change_color, lw=2))
    
    # Annotation 2: Altitude descent
    if param_change_time is not None and len(alt['timestamp']) > 0:
        idx_after = np.where(alt_times >= param_change_time)[0]
        if len(idx_after) > 3:
            alt_segment = altitude[idx_after]
            max_idx_in_segment = np.argmax(alt_segment[:min(30, len(alt_segment))])
            descent_start_idx = None
            if max_idx_in_segment < len(alt_segment) - 2:
                for i in range(max_idx_in_segment, min(len(alt_segment) - 2, max_idx_in_segment + 20)):
                    if alt_segment[i+1] < alt_segment[i]:
                        descent_start_idx = idx_after[i]
                        break
            if descent_start_idx is None:
                descent_start_idx = idx_after[max_idx_in_segment]
            
            point_x = alt_times[descent_start_idx]
            point_y = altitude[descent_start_idx]
            alt_y_min, alt_y_max = ax2.get_ylim()
            text_x = point_x + (x_limit - point_x) * 0.13
            text_y = alt_y_min + (alt_y_max - alt_y_min) * 0.35
            ax2.annotate(f'Falling \nAt {point_x:.1f} s', 
                         xy=(point_x, point_y), xytext=(text_x, text_y),
                         ha='center', fontsize=12, fontweight='bold', color=dark_orange,
                         bbox=dict(boxstyle='round,pad=0.3', facecolor='white', 
                                 edgecolor=dark_orange, alpha=0.9),
                         arrowprops=dict(arrowstyle='->', color=dark_orange, lw=2))
            print(f"  Altitude descent starts at: {point_x:.2f}s, altitude: {point_y:.2f}m")
    
    # Add "Crash" text in gray background region on ax2
    if roll_turning_point is not None:
        alt_y_min, alt_y_max = ax2.get_ylim()
        text_x = roll_turning_point + (x_limit - roll_turning_point) * 0.5
        text_y = alt_y_min + (alt_y_max - alt_y_min) * 0.7
        ax2.text(text_x, text_y, 'Crash', 
                ha='center', va='center', fontsize=13, fontweight='bold',
                color='black', alpha=1.0)
    
    output_path_svg = Path(output_dir) / f'{figure_name}_combined.svg'
    output_path_pdf = Path(output_dir) / f'{figure_name}_combined.pdf'
    fig.savefig(output_path_svg, format='svg', dpi=600, bbox_inches='tight')
    fig.savefig(output_path_pdf, format='pdf', dpi=600, bbox_inches='tight')
    print(f"Combined figure saved:")
    print(f"  SVG: {output_path_svg}")
    print(f"  PDF: {output_path_pdf}")
    plt.close(fig)


def plot_case3_pitch_figure(data, output_dir):
    """Plot figure for ACRO_BAL_PITCH case"""
    
    param_name = 'ACRO_BAL_PITCH'
    figure_name = 'case3_pitch'
    
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(8, 3.5), sharex=True,
                                    gridspec_kw={'hspace': 0.1})
    
    att = data['attitude']
    alt = data['altitude']
    params = data['params']
    
    if len(att['timestamp']) == 0:
        print("Warning: Missing required data")
        return
    
    t0 = att['timestamp'][0]
    att_times = att['timestamp'] - t0
    
    # Find parameter change time (looking for value = -10 or -20)
    param_change_time = None
    param_changes = []
    for param in params:
        if param_name in param['name']:
            time = param['timestamp'] - t0
            value = param['value']
            param_changes.append((time, value))
            print(f"  {param_name} changed at: {time:.2f}s, value: {value}")
            if value == -10 or abs(value + 10) < 0.001:
                param_change_time = time
            elif value == -20 or abs(value + 20) < 0.001:
                param_change_time = time
                print(f"  *** Found -20 at: {time:.2f}s ***")
    
    if param_change_time is None and len(param_changes) > 0:
        param_change_time = param_changes[0][0]
        print(f"  Using first parameter change at: {param_change_time:.2f}s")
    
    # Find the first Pitch attitude turning point after 40s
    pitch_turning_point = None
    threshold_time = 40.0
    idx_after_40s = np.where(att_times >= threshold_time)[0]
    if len(idx_after_40s) > 2:
        pitch_segment = att['Pitch'][idx_after_40s]
        time_segment = att_times[idx_after_40s]
        print(f"  Searching for Pitch turning point after {threshold_time}s...")
        # Calculate differences between consecutive points
        pitch_diffs = np.abs(np.diff(pitch_segment))
        # Find the first point with a change > 5 degrees (significant attitude change)
        for i in range(len(pitch_diffs)):
            if pitch_diffs[i] > 5.0:
                pitch_turning_point = time_segment[i]
                print(f"  Pitch turning point found at: {pitch_turning_point:.2f}s, Pitch change: {pitch_diffs[i]:.2f} deg")
                break
    
    # Draw background regions with three colors
    if param_change_time is not None:
        # Green: before parameter change
        ax1.axvspan(0, param_change_time, alpha=0.15, color='green', zorder=0)
        ax2.axvspan(0, param_change_time, alpha=0.15, color='green', zorder=0)
        
        if pitch_turning_point is not None:
            # Red: from parameter change to turning point
            ax1.axvspan(param_change_time, pitch_turning_point, alpha=0.15, color='red', zorder=0)
            ax2.axvspan(param_change_time, pitch_turning_point, alpha=0.15, color='red', zorder=0)
            # Gray: after turning point
            x_max = att_times[-1]
            ax1.axvspan(pitch_turning_point, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
            ax2.axvspan(pitch_turning_point, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
        else:
            # If no turning point found, use gray for everything after parameter change
            x_max = att_times[-1]
            ax1.axvspan(param_change_time, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
            ax2.axvspan(param_change_time, x_max, alpha=0.15, color=(0.5, 0.5, 0.5), zorder=0)
    
    # Plot 1: Attitude
    colors_att = sns.color_palette("husl", 3)
    ax1.plot(att_times, att['Roll'], linewidth=2.5, color=colors_att[0], alpha=0.9, label='Roll')
    ax1.plot(att_times, att['Pitch'], linewidth=2.5, color=colors_att[1], alpha=0.9, label='Pitch')
    ax1.set_ylabel('Attitude (deg)', fontsize=13, fontweight='bold')
    ax1.grid(True, linestyle=':', alpha=0.6)
    x_limit = att_times[-1]
    ax1.set_xlim(left=0, right=x_limit)
    
    # Plot 2: Altitude
    if len(alt['timestamp']) > 0:
        alt_times = alt['timestamp'] - t0
        altitude = alt['Alt']
        colors_alt = sns.color_palette("husl", 2)
        ax2.plot(alt_times, altitude, linewidth=2.5, color=colors_alt[1], 
                 marker='o', markersize=2, label='Altitude', alpha=0.8)
        ax2.set_ylabel('Altitude (m)', fontsize=13, fontweight='bold')
        ax2.grid(True, linestyle=':', alpha=0.6)
        ax2.set_xlim(left=0, right=x_limit)
        
        handles1, labels1 = ax1.get_legend_handles_labels()
        handles2, labels2 = ax2.get_legend_handles_labels()
        ax2.legend(handles1 + handles2, labels1 + labels2, 
                  loc='upper center', bbox_to_anchor=(0.5, -0.35), 
                  frameon=False, fontsize=13, ncol=3)
    
    ax2.set_xlabel('Time (s)', fontsize=13, fontweight='bold')
    plt.tight_layout()
    
    # Annotations
    param_change_color = (0.85, 0.55, 0.0)  # Darker orange-yellow for parameter change
    dark_orange = (0.75, 0.4, 0.05)
    
    # Annotation 1: Parameter change
    if param_change_time is not None:
        idx = np.where(att_times >= param_change_time)[0]
        if len(idx) > 0:
            point_x = att_times[idx[0]]
            point_y = att['Pitch'][idx[0]]
            text_x = param_change_time * 1.8
            text_y = ax1.get_ylim()[1] * -4.5
            ax1.annotate(f'{param_name}\nchanged At {param_change_time:.1f} s', 
                         xy=(point_x, point_y), xytext=(text_x, text_y),
                         ha='center', fontsize=12, fontweight='bold', color=param_change_color,
                         bbox=dict(boxstyle='round,pad=0.3', facecolor='white', 
                                 edgecolor=param_change_color, alpha=0.9),
                         arrowprops=dict(arrowstyle='->', connectionstyle='arc3,rad=0.3', 
                                       color=param_change_color, lw=2))
    
    # Annotation 2: Altitude descent
    if param_change_time is not None and len(alt['timestamp']) > 0:
        idx_after = np.where(alt_times >= param_change_time)[0]
        if len(idx_after) > 3:
            alt_segment = altitude[idx_after]
            max_idx_in_segment = np.argmax(alt_segment[:min(30, len(alt_segment))])
            descent_start_idx = None
            if max_idx_in_segment < len(alt_segment) - 2:
                for i in range(max_idx_in_segment, min(len(alt_segment) - 2, max_idx_in_segment + 20)):
                    if alt_segment[i+1] < alt_segment[i]:
                        descent_start_idx = idx_after[i]
                        break
            if descent_start_idx is None:
                descent_start_idx = idx_after[max_idx_in_segment]
            
            point_x = alt_times[descent_start_idx]
            point_y = altitude[descent_start_idx]
            alt_y_min, alt_y_max = ax2.get_ylim()
            text_x = point_x + (x_limit - point_x) * 0.11
            text_y = alt_y_min + (alt_y_max - alt_y_min) * 0.36
            ax2.annotate(f'Falling\nAt {point_x:.1f} s', 
                         xy=(point_x, point_y), xytext=(text_x, text_y),
                         ha='center', fontsize=12, fontweight='bold', color=dark_orange,
                         bbox=dict(boxstyle='round,pad=0.3', facecolor='white', 
                                 edgecolor=dark_orange, alpha=0.9),
                         arrowprops=dict(arrowstyle='->', color=dark_orange, lw=2))
            print(f"  Altitude descent starts at: {point_x:.2f}s, altitude: {point_y:.2f}m")
    
    # Add "Crash" text in gray background region on ax2
    if pitch_turning_point is not None:
        alt_y_min, alt_y_max = ax2.get_ylim()
        text_x = pitch_turning_point + (x_limit - pitch_turning_point) * 0.5
        text_y = alt_y_min + (alt_y_max - alt_y_min) * 0.7
        ax2.text(text_x, text_y, 'Crash', 
                ha='center', va='center', fontsize=13, fontweight='bold',
                color='black', alpha=1.0)
    
    output_path_svg = Path(output_dir) / f'{figure_name}_combined.svg'
    output_path_pdf = Path(output_dir) / f'{figure_name}_combined.pdf'
    fig.savefig(output_path_svg, format='svg', dpi=600, bbox_inches='tight')
    fig.savefig(output_path_pdf, format='pdf', dpi=600, bbox_inches='tight')
    print(f"Combined figure saved:")
    print(f"  SVG: {output_path_svg}")
    print(f"  PDF: {output_path_pdf}")
    plt.close(fig)


def main():
    output_dir = 'verifyDataBase/draw_datas'
    
    # Figure 1: ACRO_BAL_ROLL
    print("\n" + "="*60)
    print("Processing Figure 1: ACRO_BAL_ROLL")
    print("="*60)
    log_path_roll = 'verifyDataBase/draw_datas/case3_roll.bin'
    data_roll = parse_ardupilot_log(log_path_roll)
    
    print("\nGenerating combined figure for ACRO_BAL_ROLL...")
    plot_case3_roll_figure(data_roll, output_dir)
    
    # Figure 2: ACRO_BAL_PITCH
    print("\n" + "="*60)
    print("Processing Figure 2: ACRO_BAL_PITCH")
    print("="*60)
    log_path_pitch = 'verifyDataBase/draw_datas/case3_pitch.bin'
    data_pitch = parse_ardupilot_log(log_path_pitch)
    
    print("\nGenerating combined figure for ACRO_BAL_PITCH...")
    plot_case3_pitch_figure(data_pitch, output_dir)
    
    print("\n" + "="*60)
    print("All figures generated successfully!")
    print("="*60)


if __name__ == '__main__':
    main()
