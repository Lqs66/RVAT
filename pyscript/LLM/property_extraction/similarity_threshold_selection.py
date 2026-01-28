#!/usr/bin/env python3
"""
Similarity Threshold Selection Visualization (Final Fix)
"""

import pandas as pd
import seaborn as sns
import numpy as np
import os
import matplotlib.pyplot as plt
from matplotlib.figure import Figure

def plot_threshold_selection():
    """
    Plot silhouette scores vs thresholds.
    Highlights the optimal threshold with consistent BOLD formatting.
    """
    # File paths
    base_dir = "/home/lqs66/Desktop/modelCheckingFlightControl"
    ardupilot_file = os.path.join(base_dir, "verifyDataBase/LLM/ardupilot_threshold.csv")
    px4_file = os.path.join(base_dir, "verifyDataBase/LLM/px4_threshold.csv")
    
    # Read CSV files
    try:
        ardupilot_data = pd.read_csv(ardupilot_file)
        px4_data = pd.read_csv(px4_file)
    except FileNotFoundError as e:
        print(f"Error: Could not find file {e.filename}")
        return
    except Exception as e:
        print(f"Error reading CSV files: {e}")
        return
    
    # Find optimal thresholds
    ardupilot_max_idx = ardupilot_data['silhouette'].idxmax()
    px4_max_idx = px4_data['silhouette'].idxmax()
    
    ardupilot_optimal_threshold = ardupilot_data.loc[ardupilot_max_idx, 'threshold']
    ardupilot_optimal_silhouette = ardupilot_data.loc[ardupilot_max_idx, 'silhouette']
    
    px4_optimal_threshold = px4_data.loc[px4_max_idx, 'threshold']
    px4_optimal_silhouette = px4_data.loc[px4_max_idx, 'silhouette']
    
    # Prepare data
    ardupilot_data['Dataset'] = 'ArduPilot'
    px4_data['Dataset'] = 'PX4'
    
    # --- Style Configuration ---
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
    
    fig, ax = plt.subplots(figsize=(12, 3.5))
    
    # Get seaborn default colors
    seaborn_colors = sns.color_palette()
    color_ardu = seaborn_colors[0]  # Seaborn blue
    color_px4 = seaborn_colors[3]   # Seaborn red

    # Lines
    sns.lineplot(data=ardupilot_data, x='threshold', y='silhouette', 
             marker='o', linewidth=2.5, markersize=8, ax=ax,
             color=color_ardu, label='ArduPilot')

    sns.lineplot(data=px4_data, x='threshold', y='silhouette', 
                marker='s', linewidth=2.5, markersize=8, ax=ax,
                color=color_px4, label='PX4')
    
    # Points
    ax.scatter(ardupilot_optimal_threshold, ardupilot_optimal_silhouette, 
               s=80, color=color_ardu, edgecolor='darkblue', linewidth=1.5, 
               zorder=5, label='_nolegend_') 
    ax.scatter(px4_optimal_threshold, px4_optimal_silhouette, 
               s=70, color=color_px4, edgecolor='darkred', linewidth=1.5, marker='s',
               zorder=5, label='_nolegend_')
    
    # --- Annotations (FINAL FIX) ---
    
    # ArduPilot Annotation
    ax.annotate(f'ArduPilot Optimal\n$\\mathbf{{\\theta}}$ = {ardupilot_optimal_threshold}\nScore = {ardupilot_optimal_silhouette:.4f}',
                xy=(ardupilot_optimal_threshold, ardupilot_optimal_silhouette),
                xytext=(ardupilot_optimal_threshold - 0.09, ardupilot_optimal_silhouette - 0.15),
                fontsize=14, fontweight='bold', ha='center', va='bottom',
                bbox=dict(boxstyle='round,pad=0.3', facecolor=color_ardu, alpha=0.2, edgecolor=color_ardu),
                arrowprops=dict(arrowstyle='->', connectionstyle='arc3,rad=-0.1', 
                               color=color_ardu, lw=1.5))
    
    # PX4 Annotation
    ax.annotate(f'PX4 Optimal\n$\\mathbf{{\\theta}}$ = {px4_optimal_threshold}\nScore = {px4_optimal_silhouette:.4f}',
                xy=(px4_optimal_threshold, px4_optimal_silhouette),
                xytext=(px4_optimal_threshold - 0.01, px4_optimal_silhouette - 0.4),
                fontsize=14, fontweight='bold', ha='center', va='bottom',
                bbox=dict(boxstyle='round,pad=0.3', facecolor=color_px4, alpha=0.2, edgecolor=color_px4),
                arrowprops=dict(arrowstyle='->', connectionstyle='arc3,rad=-0.1', 
                               color=color_px4, lw=1.5))
    
    # --- Axis Styling ---
    ax.set_xlabel(r'Similarity Threshold ($\mathbf{\theta}$)', fontsize=14, fontweight='bold')
    ax.set_ylabel('Silhouette Score', fontsize=14, fontweight='bold')
    
    # Set background color using seaborn palette
    ax.set_facecolor(sns.color_palette("light:#e8ecf1", as_cmap=False)[0])
    fig.patch.set_facecolor('white')
    
    for spine in ax.spines.values():
        spine.set_visible(True)
        spine.set_linewidth(1.0) 
        spine.set_color('black')
    sns.despine(ax=ax, top=False, right=False)

    ax.grid(True, linestyle=':', alpha=0.6)

    # Ticks
    x_min = min(ardupilot_data['threshold'].min(), px4_data['threshold'].min())
    x_max = max(ardupilot_data['threshold'].max(), px4_data['threshold'].max())
    x_ticks = np.arange(x_min, x_max + 0.02, 0.02)
    ax.set_xticks(x_ticks)
    ax.set_xticklabels([f'{x:.2f}' for x in x_ticks], rotation=45)
    ax.tick_params(axis='both', which='major', labelsize=11) 

    # Legend
    ax.legend(
        loc='upper left',
        bbox_to_anchor=(0.02, 0.98),
        frameon=False,
        fontsize=14,
        ncol=1,
        prop={'weight': 'bold', 'size': 14, 'family': 'serif'}
    )
    
    # Limits
    ax.set_xlim(x_min - 0.005, x_max + 0.005)
    y_min = min(ardupilot_data['silhouette'].min(), px4_data['silhouette'].min()) - 0.05
    y_max = max(ardupilot_data['silhouette'].max(), px4_data['silhouette'].max()) + 0.15
    ax.set_ylim(y_min, y_max)
    
    plt.tight_layout()
    
    # Save
    output_dir = os.path.join(base_dir, "pyscript/LLM/property_extraction")
    plots_dir = os.path.join(output_dir, "plots")
    os.makedirs(plots_dir, exist_ok=True)
    
    svg_file = os.path.join(plots_dir, "threshold_selection_comparison.svg")
    pdf_file = os.path.join(plots_dir, "threshold_selection_comparison.pdf")
    
    fig.savefig(svg_file, format='svg', dpi=600, bbox_inches='tight', facecolor='white')
    fig.savefig(pdf_file, format='pdf', dpi=600, bbox_inches='tight', facecolor='white')
    
    print(f"Saved plots to {plots_dir}")

if __name__ == "__main__":
    plot_threshold_selection()