#!/usr/bin/env python3

import numpy as np
import matplotlib.pyplot as plt
import seaborn as sns
from pyulog import ULog
from pathlib import Path
from matplotlib.ticker import MultipleLocator

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

sns.set_theme(style="ticks", 
              font="serif", 
              rc={
                    "xtick.labelsize": 12.5,
                    "ytick.labelsize": 12.5,
                  "font.serif": ["Times New Roman", "DejaVu Serif", "serif"]
                  }
             )

THRESHOLD_COLOR = '#d62728'
FAILSAFE_COLOR = '#ffcccc'
MODE_COLORS = {
    'AUTO/MISSION': '#2ca02c',
    'RC FAILSAFE': '#d62728'
}


def load_ulog_data(ulog_path):
    print(f"Loading ULog file: {ulog_path}")
    ulog = ULog(ulog_path)
    
    manual_control_data = None
    for dataset in ulog.data_list:
        if dataset.name == 'manual_control_setpoint':
            manual_control_data = dataset
            break
    
    if manual_control_data is None:
        raise ValueError("manual_control_setpoint message not found")
    
    timestamps = manual_control_data.data['timestamp'] / 1e6
    
    intervals = np.diff(timestamps)
    interval_times = timestamps[1:]
    
    mode_data = None
    for dataset in ulog.data_list:
        if dataset.name == 'vehicle_status':
            mode_data = dataset
            break
    
    mode_timestamps = None
    nav_state = None
    arming_state = None
    failsafe = None
    
    if mode_data is not None:
        mode_timestamps = mode_data.data['timestamp'] / 1e6
        nav_state = mode_data.data.get('nav_state', None)
        arming_state = mode_data.data.get('arming_state', None)
        failsafe = mode_data.data.get('failsafe', None)
    
    param_changes = []
    for timestamp, name, value in ulog.changed_parameters:
        if 'COM_RC_LOSS_T' in name:
            param_changes.append((timestamp / 1e6, value))
            
    return {
        'timestamps': timestamps,
        'intervals': intervals,
        'interval_times': interval_times,
        'mode_timestamps': mode_timestamps,
        'nav_state': nav_state,
        'arming_state': arming_state,
        'failsafe': failsafe,
        'param_changes': param_changes
    }


def get_flight_mode_label(nav_state, failsafe=None):
    nav_state_map = {
        0: 'MANUAL',
        1: 'ALTCTL',
        2: 'POSCTL',
        3: 'AUTO/MISSION',
        4: 'AUTO/LOITER',
        5: 'AUTO/RTL',
        6: 'AUTO/RC_RECOVER',
        7: 'AUTO/RTGS',
        8: 'AUTO/LANDENGFAIL',
        9: 'AUTO/LANDGPSFAIL',
        10: 'ACRO',
        11: 'UNUSED',
        12: 'DESCEND',
        13: 'TERMINATION',
        14: 'OFFBOARD',
        15: 'STAB',
        16: 'RATTITUDE',
        17: 'AUTO/TAKEOFF',
        18: 'AUTO/LAND',
        19: 'AUTO/FOLLOW_TARGET',
        20: 'AUTO/PRECLAND',
        21: 'ORBIT',
        22: 'AUTO/VTOL_TAKEOFF',
    }
    
    if failsafe is not None and failsafe > 0:
        return 'RC FAILSAFE'
    
    return nav_state_map.get(nav_state, f'UNKNOWN({nav_state})')


def plot_time_series(data, output_dir, fault_time=12.5, time_range=(0, 20), zero_offset=0):
    fig, ax = plt.subplots(1, 1, figsize=(9, 3.5))
    
    t_start, t_end = time_range
    
    time_offset = data['timestamps'][0]
    rel_interval_times = data['interval_times'] - time_offset
    
    mask = (rel_interval_times >= t_start) & (rel_interval_times <= t_end)
    plot_times = rel_interval_times[mask] - zero_offset
    plot_intervals = data['intervals'][mask]
    
    if len(plot_intervals) == 0:
        print(f"Warning: No data points in time range {t_start:.2f}s - {t_end:.2f}s")
        t_start = max(0, t_start - 1)
        t_end = t_end + 1
        mask = (rel_interval_times >= t_start) & (rel_interval_times <= t_end)
        plot_times = rel_interval_times[mask] - zero_offset
        plot_intervals = data['intervals'][mask]
    
    plot_fault_time = fault_time - zero_offset
    
    if len(plot_times) > 0:
        x_offset = plot_times[0]
        plot_times = plot_times - x_offset
        plot_fault_time = plot_fault_time - x_offset
        x_min = 0
        x_max = plot_times[-1]
    else:
        x_offset = 0
        x_min = 0
        x_max = t_end - zero_offset
    
    if data['mode_timestamps'] is not None and data['nav_state'] is not None:
        rel_mode_times = data['mode_timestamps'] - time_offset
        mode_mask = (rel_mode_times >= t_start) & (rel_mode_times <= t_end)
        plot_mode_times = rel_mode_times[mode_mask] - zero_offset - x_offset
        
        nav_states = data['nav_state'][mode_mask]
        failsafes = data['failsafe'][mode_mask] if data['failsafe'] is not None else [0] * len(nav_states)
        
        first_failsafe_time = None
        for i, (t, fs) in enumerate(zip(plot_mode_times, failsafes)):
            if fs > 0:
                first_failsafe_time = t
                break
        
        if first_failsafe_time is not None:
            ax.axvspan(x_min, first_failsafe_time, alpha=0.15, color='green', zorder=1)
            ax.axvspan(first_failsafe_time, x_max, alpha=0.15, color=FAILSAFE_COLOR, zorder=1)
            
            spike_indices = np.where(plot_intervals > 0.1)[0]
            if len(spike_indices) > 0:
                spike_idx = spike_indices[0]
                spike_time = plot_times[spike_idx]
                spike_value = plot_intervals[spike_idx]
                
                ax.axvline(x=spike_time, color='red', linestyle='-', linewidth=2, alpha=0.7, zorder=4)
                
                first_failsafe_time = spike_time
    else:
        first_failsafe_time = plot_fault_time
        ax.axvspan(x_min, plot_fault_time, alpha=0.15, color='green', zorder=1)
        ax.axvspan(plot_fault_time, x_max, alpha=0.15, color=FAILSAFE_COLOR, zorder=1)
    
    ax.plot(plot_times, plot_intervals, linewidth=2.5, color='#4A90E2', 
            marker='o', markersize=4, label='manual_control_setpoint Interval', zorder=5)
    
    ax.axhline(y=0.1, color=THRESHOLD_COLOR, linestyle='--', linewidth=2.5, 
               label='COM_RC_LOSS_T', alpha=0.8, zorder=3)
    
    if len(plot_intervals) > 0:
        y_max = max(0.15, plot_intervals.max() * 1.1)
    else:
        y_max = 0.5
    ax.set_ylim(0.01, y_max)
    
    if first_failsafe_time is not None:
        y_pos = y_max * 0.97
        
        ax.text(x_min + (first_failsafe_time - x_min) * 0.025, y_pos, 
               'Loiter', fontsize=14, color='green', 
               fontweight='bold', ha='left', va='top')
        
        ax.text(first_failsafe_time + (x_max - first_failsafe_time) * 0.05, y_pos, 
               'RC Failsafe', fontsize=14, color='red', 
               fontweight='bold', ha='left', va='top')
    
    spike_indices = np.where(plot_intervals > 0.1)[0]
    if len(spike_indices) > 0:
        for idx in spike_indices[:1]:
            spike_time = plot_times[idx]
            spike_value = plot_intervals[idx]
            
            text_x = x_min + (first_failsafe_time - x_min) * 0.5
            text_y = y_max * 0.79
            ax.annotate(f'At {spike_time:.2f}s, manual_control_setpoint interval\nexceeds 0.1s, triggering erroneous RC Failsafe',
                       xy=(spike_time, spike_value), 
                       xytext=(text_x, text_y),
                       fontsize=12, color='#4A90E2', fontweight='bold',
                       arrowprops=dict(arrowstyle='->', color='#4A90E2', lw=2, connectionstyle='arc3,rad=0.1'),
                       bbox=dict(boxstyle='round,pad=0.3', facecolor='#E6F2FF', alpha=0.95, edgecolor='#4A90E2', linewidth=1.5),
                       ha='center', va='center')
    
    ax.set_xlabel('Time (s)', fontsize=15, fontweight='bold')
    ax.set_ylabel('Interval (s)', fontsize=15, fontweight='bold')
    ax.set_xlim(x_min, x_max)
    
    ax.legend(loc='upper center', bbox_to_anchor=(0.5, -0.2), frameon=False, fontsize=15, ncol=2)
    
    ax.grid(True, linestyle=':', alpha=0.6)
    
    plt.tight_layout()
    
    Path(output_dir).mkdir(parents=True, exist_ok=True)
    
    output_path_svg = Path(output_dir) / 'rc_failsafe_time_series.svg'
    output_path_pdf = Path(output_dir) / 'rc_failsafe_time_series.pdf'
    
    fig.savefig(output_path_svg, format='svg', dpi=600, bbox_inches='tight')
    fig.savefig(output_path_pdf, format='pdf', dpi=600, bbox_inches='tight')
    
    print(f"Figures saved:")
    print(f"  SVG: {output_path_svg}")
    print(f"  PDF: {output_path_pdf}")



def main():
    ulog_path = 'verifyDataBase/draw_datas/case1.ulg'
    output_dir = 'verifyDataBase/draw_datas'
    
    data = load_ulog_data(ulog_path)
    
    print(f"\nData Statistics:")
    print(f"  Total messages: {len(data['timestamps'])}")
    print(f"  Time range: {data['timestamps'][0]:.2f}s - {data['timestamps'][-1]:.2f}s")
    print(f"  Average interval: {np.mean(data['intervals']):.4f}s")
    print(f"  Maximum interval: {np.max(data['intervals']):.4f}s")
    
    fault_time = None
    if data['failsafe'] is not None:
        failsafe_indices = np.where(data['failsafe'] > 0)[0]
        if len(failsafe_indices) > 0:
            first_idx = failsafe_indices[0]
            abs_fault_time = data['mode_timestamps'][first_idx]
            fault_time = abs_fault_time - data['timestamps'][0]
            print(f"  First Failsafe at: {fault_time:.2f}s")
        else:
            print("  Warning: No Failsafe event detected")
    
    if fault_time is None:
        print("  Using maximum interval time as fault point")
        max_idx = np.argmax(data['intervals'])
        fault_time = data['interval_times'][max_idx] - data['timestamps'][0]
        print(f"  Maximum interval at: {fault_time:.2f}s")
    
    param_change_time = None
    for t, v in data['param_changes']:
        if abs(v - 0.1) < 0.01:
            param_change_time = t - data['timestamps'][0]
            print(f"  Parameter changed to 0.1s at: {param_change_time:.2f}s")
            break
            
    zero_offset = param_change_time if param_change_time is not None else 0
    
    rel_interval_times = data['interval_times'] - data['timestamps'][0]
    
    large_intervals_mask = data['intervals'] > 0.1
    large_intervals_indices = np.where(large_intervals_mask)[0]
    
    if len(large_intervals_indices) == 0:
        print("  Warning: No interval exceeds 0.1s, using default time range")
        t_start = fault_time - 5
        t_end = fault_time + 5
    else:
        fault_idx = None
        for i, t in enumerate(rel_interval_times):
            if abs(t - fault_time) < 0.01:
                fault_idx = i
                break
        
        if fault_idx is None:
            fault_idx = np.argmin(np.abs(rel_interval_times - fault_time))
            print(f"  Using closest failsafe interval: index={fault_idx}, time={rel_interval_times[fault_idx]:.2f}s")
        
        prev_large_idx = None
        next_large_idx = None
        
        for idx in large_intervals_indices:
            if idx < fault_idx:
                prev_large_idx = idx
            elif idx > fault_idx and next_large_idx is None:
                next_large_idx = idx
                break
        
        if prev_large_idx is not None and next_large_idx is not None:
            if prev_large_idx + 1 < len(rel_interval_times) and next_large_idx > 0:
                t_start = rel_interval_times[prev_large_idx + 1]
                t_end = rel_interval_times[next_large_idx - 1]
                print(f"  Display range (excluding endpoints): {t_start:.2f}s - {t_end:.2f}s")
                print(f"  Previous large interval (excluded): index={prev_large_idx}, time={rel_interval_times[prev_large_idx]:.2f}s, interval={data['intervals'][prev_large_idx]:.4f}s")
                print(f"  Next large interval (excluded): index={next_large_idx}, time={rel_interval_times[next_large_idx]:.2f}s, interval={data['intervals'][next_large_idx]:.4f}s")
            else:
                t_start = rel_interval_times[prev_large_idx]
                t_end = rel_interval_times[next_large_idx]
                print(f"  Warning: Index out of bounds, display range: {t_start:.2f}s - {t_end:.2f}s")
        else:
            print("  Warning: Cannot find surrounding large intervals, using default time range")
            t_start = fault_time - 5
            t_end = fault_time + 5
    
    plot_time_series(data, output_dir, fault_time=fault_time, time_range=(t_start, t_end), zero_offset=zero_offset)


if __name__ == '__main__':
    main()
