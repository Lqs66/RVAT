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
	"axes.labelsize": 22,
	"xtick.labelsize": 12,
	"ytick.labelsize": 12,
	"font.weight": "bold",
	"axes.labelweight": "bold",
	"mathtext.fontset": "cm",
	"axes.unicode_minus": False,
})

sns.set_theme(
	style="ticks",
	font="serif",
	rc={
		"xtick.labelsize": 15,
		"ytick.labelsize": 15,
		"font.serif": ["Times New Roman", "DejaVu Serif", "serif"],
	},
)


def parse_ardupilot_log(log_path):
	"""Parse ArduPilot .bin log and extract RCIN/RCOU/ESC/BARO/MODE/ARM/ERR."""
	print(f"Loading ArduPilot log: {log_path}")

	mlog = mavutil.mavlink_connection(log_path)

	rcin_data = {"timestamp": [], "C3": []}
	rcout_data = {"timestamp": [], "C1": [], "C2": [], "C3": [], "C4": []}
	esc_data = {"timestamp": [], "RPM1": [], "RPM2": [], "RPM3": [], "RPM4": []}
	alt_data = {"timestamp": [], "Alt": []}
	mode_data = {"timestamp": [], "Mode": [], "ModeNum": []}
	arm_data = {"timestamp": [], "Armed": []}
	err_data = {"timestamp": [], "Subsys": [], "ECode": []}
	msg_types_found = set()

	while True:
		msg = mlog.recv_match(blocking=False)
		if msg is None:
			break

		msg_type = msg.get_type()
		msg_types_found.add(msg_type)

		if msg_type == "RCIN":
			rcin_data["timestamp"].append(msg.TimeUS / 1e6)
			rcin_data["C3"].append(msg.C3)

		elif msg_type == "RCOU":
			rcout_data["timestamp"].append(msg.TimeUS / 1e6)
			rcout_data["C1"].append(msg.C1)
			rcout_data["C2"].append(msg.C2)
			rcout_data["C3"].append(msg.C3)
			rcout_data["C4"].append(msg.C4)

		elif msg_type == "ESC":
			esc_data["timestamp"].append(msg.TimeUS / 1e6)
			if hasattr(msg, "RPM"):
				esc_data["RPM1"].append(msg.RPM)
				esc_data["RPM2"].append(0)
				esc_data["RPM3"].append(0)
				esc_data["RPM4"].append(0)

		elif msg_type == "ESCX":
			ts = msg.TimeUS / 1e6
			if hasattr(msg, "Inst"):
				inst = msg.Inst
				rpm = msg.RPM if hasattr(msg, "RPM") else 0

				if inst == 0:
					if ts not in [t for t in esc_data["timestamp"][-10:]]:
						esc_data["timestamp"].append(ts)
						esc_data["RPM1"].append(rpm)
						esc_data["RPM2"].append(0)
						esc_data["RPM3"].append(0)
						esc_data["RPM4"].append(0)
					elif len(esc_data["RPM1"]) > 0:
						esc_data["RPM1"][-1] = rpm
				elif inst == 1 and len(esc_data["RPM2"]) > 0:
					esc_data["RPM2"][-1] = rpm
				elif inst == 2 and len(esc_data["RPM3"]) > 0:
					esc_data["RPM3"][-1] = rpm
				elif inst == 3 and len(esc_data["RPM4"]) > 0:
					esc_data["RPM4"][-1] = rpm

		elif msg_type == "BARO" or msg_type == "BAR2":
			alt_data["timestamp"].append(msg.TimeUS / 1e6)
			alt_data["Alt"].append(msg.Alt)

		elif msg_type == "MODE":
			mode_data["timestamp"].append(msg.TimeUS / 1e6)
			mode_data["Mode"].append(msg.Mode)
			mode_data["ModeNum"].append(msg.ModeNum)

		elif msg_type == "ARM":
			arm_data["timestamp"].append(msg.TimeUS / 1e6)
			arm_data["Armed"].append(msg.ArmState)

		elif msg_type == "ERR":
			err_data["timestamp"].append(msg.TimeUS / 1e6)
			err_data["Subsys"].append(msg.Subsys if hasattr(msg, "Subsys") else -1)
			err_data["ECode"].append(msg.ECode if hasattr(msg, "ECode") else -1)

	for dataset in [rcin_data, rcout_data, esc_data, alt_data, mode_data, arm_data, err_data]:
		for key in dataset:
			dataset[key] = np.array(dataset[key])

	print(f"  RCIN messages: {len(rcin_data['timestamp'])}")
	print(f"  RCOU messages: {len(rcout_data['timestamp'])}")
	print(f"  ESC messages: {len(esc_data['timestamp'])}")
	print(f"  Altitude messages: {len(alt_data['timestamp'])}")
	print(f"  Mode changes: {len(mode_data['timestamp'])}")
	print(f"  Arm/Disarm events: {len(arm_data['timestamp'])}")
	print(f"  ERR events: {len(err_data['timestamp'])}")
	print(f"  Message types found: {sorted(msg_types_found)[:20]}...")

	return {
		"rcin": rcin_data,
		"rcout": rcout_data,
		"esc": esc_data,
		"altitude": alt_data,
		"mode": mode_data,
		"arm": arm_data,
		"err": err_data,
	}


def detect_gcs_failsafe_event(mode_data, err_data, t0):
	"""Detect GCS failsafe using ERR(8,1) first, then fallback to mode transition 1->6 (Acro->RTL)."""
	# Preferred signal: ERR Subsys=8, ECode=1 (failsafe event in this dataset).
	if len(err_data["timestamp"]) > 0:
		for ts, subsys, ecode in zip(err_data["timestamp"], err_data["Subsys"], err_data["ECode"]):
			if int(subsys) == 8 and int(ecode) == 1:
				return ts - t0, "GCS_FAILSAFE(ERR 8,1)"

	# Fallback: mode reason or mode switch to RTL from Acro.
	if len(mode_data["timestamp"]) > 0:
		for ts, mode_num in zip(mode_data["timestamp"], mode_data["ModeNum"]):
			if int(mode_num) == 6:
				return ts - t0, "RTL (failsafe fallback)"

	return None, None


def get_throttle_at_time(rcin_times, throttle_pwm, query_time):
	"""Return nearest throttle value around query_time."""
	if query_time is None or len(rcin_times) == 0:
		return None, None
	idx = int(np.argmin(np.abs(rcin_times - query_time)))
	return float(throttle_pwm[idx]), idx


def plot_combined_figure(data, output_dir):
	"""Plot case2 failsafe-on figure and emphasize high-throttle failsafe trigger."""
	fig, (ax1, ax2, ax3) = plt.subplots(
		3,
		1,
		figsize=(12, 5.6),
		sharex=True,
		gridspec_kw={"hspace": 0.2, "left": 0.1},
	)

	rcin = data["rcin"]
	rcout = data["rcout"]
	esc = data["esc"]
	alt = data["altitude"]
	mode_data = data["mode"]
	err_data = data["err"]

	if len(rcin["timestamp"]) == 0 or len(rcout["timestamp"]) == 0:
		print("Warning: Missing RCIN/RCOU data")
		return

	t0 = rcin["timestamp"][0]
	rcin_times = rcin["timestamp"] - t0
	rcout_times = rcout["timestamp"] - t0

	failsafe_time, failsafe_mode = detect_gcs_failsafe_event(mode_data, err_data, t0)

	throttle_at_failsafe, throttle_idx = get_throttle_at_time(
		rcin_times, rcin["C3"], failsafe_time
	)
	high_throttle_triggered = (
		throttle_at_failsafe is not None and throttle_at_failsafe >= 1400.0
	)

	x_max = max(rcin_times[-1], rcout_times[-1])
	if len(alt["timestamp"]) > 0:
		x_max = max(x_max, (alt["timestamp"] - t0)[-1])
	failsafe_visible = failsafe_time is not None

	# Background regions: comparison only (before GCS failsafe vs after GCS failsafe).
	if failsafe_visible:
		ax1.axvspan(0, failsafe_time, alpha=0.15, color="green", zorder=0)
		ax2.axvspan(0, failsafe_time, alpha=0.15, color="green", zorder=0)
		ax3.axvspan(0, failsafe_time, alpha=0.15, color="green", zorder=0)

		fs_color = (0.25, 0.45, 0.85)
		ax1.axvspan(failsafe_time, x_max, alpha=0.14, color=fs_color, zorder=0)
		ax2.axvspan(failsafe_time, x_max, alpha=0.14, color=fs_color, zorder=0)
		ax3.axvspan(failsafe_time, x_max, alpha=0.14, color=fs_color, zorder=0)

	colors = sns.color_palette("husl", 2)
	ax1.plot(
		rcin_times,
		rcin["C3"],
		linewidth=2.5,
		color=colors[0],
		alpha=0.9,
		label="Throttle PWM",
	)
	ax1.axhline(
		y=1400,
		color="gray",
		linestyle="--",
		linewidth=2,
		label="High Throttle Reference (1400)",
		alpha=0.75,
	)
	if failsafe_visible:
		ax1.axvline(
			x=failsafe_time,
			color=(0.1, 0.25, 0.7),
			linestyle="-",
			linewidth=2.3,
			alpha=0.9,
			label="GCS Failsafe Trigger",
		)
	ax1.set_ylabel("PWM (μs)", fontsize=15.5, fontweight="bold")
	ax1.set_ylim(top=1800)
	ax1.legend(
		loc="upper left",
		bbox_to_anchor=(0, 1.06),
		frameon=False,
		fontsize=14,
		ncol=3,
	)
	ax1.grid(True, linestyle=":", alpha=0.6)
	ax1.set_xlim(left=0, right=x_max)
	ax1.set_xticks(np.arange(0, x_max + 1, 5))

	use_esc = len(esc["timestamp"]) > 0 and np.any(esc["RPM1"] > 0)
	if use_esc:
		timestamps = esc["timestamp"] - t0
		m1, m2, m3, m4 = esc["RPM1"], esc["RPM2"], esc["RPM3"], esc["RPM4"]
		ylabel = "Motor RPM"
	else:
		timestamps = rcout_times
		m1, m2, m3, m4 = rcout["C1"], rcout["C2"], rcout["C3"], rcout["C4"]
		ylabel = "PWM (μs)"

	colors_motor = sns.color_palette("dark", 4)
	ax2.plot(timestamps, m1, linewidth=8, color=colors_motor[0], alpha=1, label="Motor 1")
	ax2.plot(
		timestamps,
		m2,
		linewidth=5,
		color=sns.color_palette("bright")[8],
		alpha=1,
		label="Motor 2",
	)
	ax2.plot(
		timestamps,
		m3,
		linewidth=4.5,
		color=sns.color_palette()[2],
		alpha=1,
		label="Motor 3",
	)
	ax2.plot(
		timestamps,
		m4,
		linewidth=2,
		color=sns.color_palette("bright")[3],
		alpha=1,
		label="Motor 4",
	)
	ax2.set_ylabel(ylabel, fontsize=15.5, fontweight="bold")
	if not use_esc:
		ax2.set_ylim(top=2000)
	ax2.legend(
		loc="upper left",
		bbox_to_anchor=(0, 1.06),
		frameon=False,
		fontsize=14,
		ncol=4,
		columnspacing=1.0,
	)
	ax2.grid(True, linestyle=":", alpha=0.6)
	ax2.set_xlim(left=0, right=x_max)
	ax2.set_xticks(np.arange(0, x_max + 1, 5))

	if len(alt["timestamp"]) > 0:
		alt_times = alt["timestamp"] - t0
		altitude = alt["Alt"]
		ax3.plot(
			alt_times,
			altitude,
			linewidth=2.5,
			color=colors[1],
			marker="o",
			markersize=2,
			label="Altitude",
			alpha=0.8,
		)
		ax3.legend(loc="upper left", bbox_to_anchor=(0, 1.06), frameon=False, fontsize=14)
	ax3.set_ylabel("Altitude (m)", fontsize=15.5, fontweight="bold")
	ax3.grid(True, linestyle=":", alpha=0.6)
	ax3.set_xlim(left=0, right=x_max)
	ax3.set_xticks(np.arange(0, x_max + 1, 5))
	ax3.set_xlabel("Time (s)", fontsize=16.5, fontweight="bold")

	plt.tight_layout()

	# Region labels on throttle subplot.
	y_text = ax1.get_ylim()[0] + (ax1.get_ylim()[1] - ax1.get_ylim()[0]) * 0.08
	if failsafe_visible:
		ax1.text(
			failsafe_time / 2,
			y_text,
			"Normal Flight",
			ha="center",
			va="bottom",
			fontsize=14,
			fontweight="bold",
			color="darkgreen",
			alpha=1.0,
			zorder=5,
		)

		ax1.text(
			(failsafe_time + x_max) / 2,
			y_text,
			"After GCS Failsafe",
			ha="center",
			va="bottom",
			fontsize=14,
			fontweight="bold",
			color=(0.1, 0.25, 0.7),
			alpha=1.0,
			zorder=5,
		)

	# Key annotation: high throttle but still enters failsafe.
	if failsafe_visible and throttle_at_failsafe is not None:
		throttle_label = (
			"High throttle" if high_throttle_triggered else "Throttle"
		)
		fail_mode_label = failsafe_mode if failsafe_mode is not None else "FAILSAFE"
		point_x = rcin_times[throttle_idx]
		point_y = rcin["C3"][throttle_idx]
		text_x = min(x_max * 0.55, point_x + 9)
		text_y = ax1.get_ylim()[0] + (ax1.get_ylim()[1] - ax1.get_ylim()[0]) * 0.78
		ax1.annotate(
			f"{throttle_label}={throttle_at_failsafe:.0f} at {point_x:.1f}s\\n"
			f"Acro -> {fail_mode_label}",
			xy=(point_x, point_y),
			xytext=(text_x, text_y),
			ha="center",
			fontsize=13,
			fontweight="bold",
			color=(0.1, 0.25, 0.7),
			bbox=dict(
				boxstyle="round,pad=0.5",
				facecolor="white",
				edgecolor=(0.1, 0.25, 0.7),
				alpha=0.92,
			),
			arrowprops=dict(
				arrowstyle="->",
				connectionstyle="arc3,rad=-0.25",
				color=(0.1, 0.25, 0.7),
				lw=2,
			),
		)

	if failsafe_visible:
		motor_avg = (m1 + m2 + m3 + m4) / 4.0
		idx = np.argmin(np.abs(timestamps - failsafe_time))
		point_x = timestamps[idx]
		point_y = motor_avg[idx]
		text_x = min(x_max * 0.72, point_x + 10)
		text_y = ax2.get_ylim()[0] + (ax2.get_ylim()[1] - ax2.get_ylim()[0]) * 0.25
		ax2.annotate(
			"Controller enters GCS failsafe\\nwhile throttle input remains high",
			xy=(point_x, point_y),
			xytext=(text_x, text_y),
			ha="center",
			fontsize=13,
			fontweight="bold",
			color=(0.1, 0.45, 0.2),
			bbox=dict(
				boxstyle="round,pad=0.5",
				facecolor="white",
				edgecolor=(0.1, 0.45, 0.2),
				alpha=0.9,
			),
			arrowprops=dict(arrowstyle="->", color=(0.1, 0.45, 0.2), lw=2),
		)

	if failsafe_visible and len(alt["timestamp"]) > 0:
		alt_times = alt["timestamp"] - t0
		altitude = alt["Alt"]
		idx = np.argmin(np.abs(alt_times - failsafe_time))
		point_x = alt_times[idx]
		point_y = altitude[idx]
		text_x = min(x_max * 0.62, point_x + 8)
		text_y = ax3.get_ylim()[0] + (ax3.get_ylim()[1] - ax3.get_ylim()[0]) * 0.6
		ax3.annotate(
			"Altitude response after GCS failsafe",
			xy=(point_x, point_y),
			xytext=(text_x, text_y),
			ha="center",
			fontsize=13,
			fontweight="bold",
			color=(0.82, 0.32, 0.42),
			bbox=dict(
				boxstyle="round,pad=0.5",
				facecolor="white",
				edgecolor=(0.82, 0.32, 0.42),
				alpha=0.9,
			),
			arrowprops=dict(arrowstyle="->", color=(0.82, 0.32, 0.42), lw=2),
		)

	output_path_svg = Path(output_dir) / "case2_failsafe_on_combined.svg"
	output_path_pdf = Path(output_dir) / "case2_failsafe_on_combined.pdf"

	fig.savefig(output_path_svg, format="svg", dpi=600, bbox_inches="tight")
	fig.savefig(output_path_pdf, format="pdf", dpi=600, bbox_inches="tight")

	print("Combined figure saved:")
	print(f"  SVG: {output_path_svg}")
	print(f"  PDF: {output_path_pdf}")
	if failsafe_time is not None:
		print(
			f"  GCS failsafe event: t={failsafe_time:.2f}s, throttle={throttle_at_failsafe:.0f}"
		)

	plt.close(fig)


def main():
	log_path = "verifyDataBase/draw_datas/case2_failsafe_on.bin"
	output_dir = "verifyDataBase/draw_datas"

	data = parse_ardupilot_log(log_path)

	print("\nGenerating combined figure for failsafe-on case...")
	plot_combined_figure(data, output_dir)
	print("\nFigure generated successfully!")


if __name__ == "__main__":
	main()
