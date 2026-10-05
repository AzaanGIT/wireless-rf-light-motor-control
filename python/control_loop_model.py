#!/usr/bin/env python3
"""
Closed-Loop Proportional Control System Modeling & Step Response Analysis
Author: K Azaan Ahamed (23BEC0362)
"""

import os
os.environ['MPLCONFIGDIR'] = '/tmp/mpl'
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np

def main():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    assets_dir = os.path.join(script_dir, '..', 'assets')
    os.makedirs(assets_dir, exist_ok=True)

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 5.5), dpi=150)
    fig.patch.set_facecolor('#ffffff')

    # 1. Transfer characteristic: Light Intensity vs Motor PWM & RPM
    ldr_raw = np.linspace(0, 1023, 200)
    threshold = 50
    pwm_out = np.zeros_like(ldr_raw)
    mask = ldr_raw > threshold
    pwm_out[mask] = 60 + (ldr_raw[mask] - threshold) / (1023 - threshold) * (255 - 60)

    # Motor angular speed (RPM) assuming linear motor model with stall offset
    motor_rpm = np.maximum(0, (pwm_out - 45) * 12.5)

    ax1.plot(ldr_raw, pwm_out, color='#0066cc', linewidth=2.5, label='Motor PWM Duty (0-255)')
    ax1.axvline(threshold, color='#e63946', linestyle='--', label=f'Deadband Threshold ({threshold})')
    ax1.set_title('Sensor-to-Actuator Transfer Curve', fontsize=12, fontweight='bold')
    ax1.set_xlabel('LDR Sensor Raw ADC Input (0 - 1023)', fontsize=10)
    ax1.set_ylabel('Arduino PWM Output (0 - 255)', fontsize=10, color='#0066cc')
    ax1.tick_params(axis='y', labelcolor='#0066cc')
    ax1.grid(True, linestyle=':', alpha=0.6)

    ax1_rpm = ax1.twinx()
    ax1_rpm.plot(ldr_raw, motor_rpm, color='#2a9d8f', linewidth=2, linestyle='-.', label='Motor Speed (RPM)')
    ax1_rpm.set_ylabel('DC Motor Speed (RPM)', fontsize=10, color='#2a9d8f')
    ax1_rpm.tick_params(axis='y', labelcolor='#2a9d8f')

    lines1, labels1 = ax1.get_legend_handles_labels()
    lines2, labels2 = ax1_rpm.get_legend_handles_labels()
    ax1.legend(lines1 + lines2, labels1 + labels2, loc='upper left')

    # 2. Dynamic Closed-Loop Step Response (Simulating Sudden Flashlight Pulse)
    t = np.linspace(0, 3.0, 600) # seconds
    # Step input at t = 0.5s from ambient (100) to flashlight (800)
    ldr_stimulus = np.where(t < 0.5, 100, np.where(t < 2.0, 800, 150))

    # Control loop simulation with RF packet latency (100ms) + 1st order motor lag (tau = 250ms)
    dt = t[1] - t[0]
    tau_motor = 0.25 # seconds mechanical time constant
    motor_speed_sim = np.zeros_like(t)
    rf_delay_steps = int(0.10 / dt)

    target_pwm = np.zeros_like(t)
    for i in range(len(t)):
        if ldr_stimulus[i] > threshold:
            target_pwm[i] = 60 + (ldr_stimulus[i] - threshold) / (1023 - threshold) * (255 - 60)
        else:
            target_pwm[i] = 0

    # Apply delay and first-order lag
    for i in range(1, len(t)):
        delayed_idx = max(0, i - rf_delay_steps)
        cmd_pwm = target_pwm[delayed_idx]
        target_rpm = max(0, (cmd_pwm - 45) * 12.5)
        motor_speed_sim[i] = motor_speed_sim[i-1] + (dt / tau_motor) * (target_rpm - motor_speed_sim[i-1])

    ax2.plot(t, ldr_stimulus, color='#f4a261', linewidth=2, label='Ambient Light Input (ADC)')
    ax2.set_ylabel('LDR Sensor Reading', color='#f4a261', fontsize=10)
    ax2.set_xlabel('Time (seconds)', fontsize=10)
    ax2.tick_params(axis='y', labelcolor='#f4a261')
    ax2.grid(True, linestyle=':', alpha=0.6)

    ax2_motor = ax2.twinx()
    ax2_motor.plot(t, motor_speed_sim, color='#264653', linewidth=2.5, label='Motor Dynamic RPM Response')
    ax2_motor.set_ylabel('Actuator Speed (RPM)', color='#264653', fontsize=10)
    ax2_motor.tick_params(axis='y', labelcolor='#264653')

    lines2a, labels2a = ax2.get_legend_handles_labels()
    lines2b, labels2b = ax2_motor.get_legend_handles_labels()
    ax2.legend(lines2a + lines2b, labels2a + labels2b, loc='upper right')
    ax2.set_title('Transient Closed-Loop Response (RF Latency + Inertia)', fontsize=12, fontweight='bold')

    plt.tight_layout()
    out_file = os.path.join(assets_dir, 'closed_loop_response.png')
    plt.savefig(out_file, dpi=150)
    plt.close()
    print(f"Generated control response plot at: {out_file}")

if __name__ == '__main__':
    main()
