import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

def analyze_update_times(file_path):
    try:
        # read CSV file
        df = pd.read_csv(file_path)
        
        # extract timestamp column
        timestamps = df['timestamp']
        
        # calculate differences between consecutive timestamps
        diff_us = timestamps.diff().dropna()
        
        # convert microseconds to seconds
        update_times = diff_us / 1_000_000
        
        # compute max, min, avg
        max_val = update_times.max()
        min_val = update_times.min()
        avg_val = update_times.mean()
        
        print("-" * 30)
        print("Results (unit: seconds s):")
        print(f"Max interval: {max_val:.6f} s")
        print(f"Min interval: {min_val:.6f} s")
        print(f"Average interval: {avg_val:.6f} s")
        print("-" * 30)

        return update_times, timestamps

    except Exception as e:
        print(f"Error: {e}")

def draw(data_series, timestamps=None, threshold=0.1, output_name='verifyDataBase/draw_datas/manual_control_setpoint_update_intervals'):
    if data_series is None or len(data_series) == 0:
        print("No data to plot.")
        return

    # set up plot style
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

    # Use Seaborn's ticks style
    sns.set_theme(style="ticks", 
              font="serif", 
              rc={
                    "xtick.labelsize": 12.5,
                    "ytick.labelsize": 12.5,
                  "font.serif": ["Times New Roman", "DejaVu Serif", "serif"]
                  }
             )

    # Create figure and axis
    fig, ax = plt.subplots(figsize=(9, 3.5))

    if timestamps is not None:
        x = (timestamps[1:].values - timestamps.iloc[0]) / 1_000_000
    else:
        x = data_series.index.values
    y = data_series.values
    
    mask = x <= 250
    x = x[mask]
    y = y[mask]


    # Remove KDE density estimation, directly plot scatter points with a uniform color
    # color='#9b59b6' is a typical light purple (Wisteria), #8981B8 (a more blueish purple)
    # You can also try '#cdb4db' (lighter pink-purple) or '#8e44ad' (dark purple)
    # alpha=0.7 holds some transparency for better visibility
    scatter = ax.scatter(x, y, color="#b465d3", s=20, alpha=0.7, edgecolor='none', label='Step Interval')

    # rolling_mean = data_series.rolling(window=100, center=True).mean()
    # ax.plot(data_series.index, rolling_mean, color='k', linewidth=1, alpha=0.6, label='Moving Avg (100)')

    # Draw threshold region
    y_min, y_max = ax.get_ylim()
    fill_max = max(y_max, data_series.max() * 1.05, threshold * 1.1)
    
    ax.axhspan(ymin=threshold, ymax=fill_max, color='#e74c3c', alpha=0.15, lw=0, zorder=0)
    ax.axhline(y=threshold, color='#c0392b', linestyle='--', linewidth=1.5, 
               label=f'COM_RC_LOSS_T', zorder=2)

    ax.set_xlabel('Time (s)', fontsize=15, fontweight='bold')
    ax.set_ylabel('Interval (s)', fontsize=15, fontweight='bold')

    ax.set_ylim(bottom=0, top=fill_max)

    # Legend settings
    ax.legend(
        loc='upper center',           
        bbox_to_anchor=(0.5, -0.2),
        ncol=3,                      
        frameon=False,                
        fontsize=15                 
    )

    ax.grid(True, linestyle=':', alpha=0.6)

    plt.tight_layout()

    formats = ['svg', 'pdf']
    for fmt in formats:
        save_path = f"{output_name}.{fmt}"
        plt.savefig(save_path, format=fmt, dpi=600, bbox_inches='tight')
        print(f"Chart saved as vector graphic: {save_path}")

    # plt.show()

if __name__ == "__main__":
    csv_file = 'verifyDataBase/draw_datas/case_1_manual_control_setpoint.csv' 
    result = analyze_update_times(csv_file)
    if result is not None:
        update_times, timestamps = result
        draw(update_times, timestamps=timestamps, threshold=0.1)