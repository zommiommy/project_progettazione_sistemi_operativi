import numpy as np
import json
from tqdm.auto import tqdm
import matplotlib.pyplot as plt
from scipy import signal as sig
from matplotlib.ticker import FuncFormatter, ScalarFormatter

def load_data(channel):

    with open(f"preamble_{channel}.json", "r") as f:
        preabmle = json.load(f)

    print("Loaded preamble ", preabmle)

    data = np.fromfile(f"dump_{channel}.bin", dtype=np.dtype('<u2')).astype(np.float64)
    data = data[len(data)//2:]
    print("RawMean: ", np.mean(data))
    print("RawMax : ", np.max(data))
    print("RawMin : ", np.min(data))
    print("Loaded")

    parsed = ((data + preabmle["yorigin"]) * preabmle["yincrement"])

    print("Parsed")

    print("Mean: ", np.mean(parsed))
    print("Max : ", np.max(parsed))
    print("Min : ", np.min(parsed))
    print("Trace length: ",  len(data) * preabmle["xincrement"])

    times = np.linspace(0, len(data) * preabmle["xincrement"], len(data), dtype=np.float64)
    
    return times, parsed, preabmle

x1, y1, p1 = load_data(1)
x2, y2, p2 = load_data(2)
#x3, y3, p3 = load_data(3)

def segment_signal_robust(signal, square_wave, high_thresh=0.6, low_thresh=0.4, min_segment_length=10):
    """
    Segments a signal based on a noisy square wave and aligns the segments.
    Handles variable-length segments properly.
    """
    if len(signal) != len(square_wave):
        raise ValueError("Signal and square wave must have the same length")
    
    # Apply median filter to reduce noise in square wave
    square_filtered = sig.medfilt(square_wave, kernel_size=5)
    
    high_segments = []
    low_segments = []
    
    # Initialize state
    current_state = None
    segment_start = 0
    
    for i in range(len(square_filtered)):
        # Determine state using hysteresis
        if current_state is None or current_state == False:
            if square_filtered[i] > high_thresh:
                if current_state is not None and i - segment_start >= min_segment_length:
                    low_segments.append(signal[segment_start:i])
                current_state = True
                segment_start = i
        elif current_state == True:
            if square_filtered[i] < low_thresh:
                if i - segment_start >= min_segment_length:
                    high_segments.append(signal[segment_start:i])
                current_state = False
                segment_start = i
    
    # Add final segment
    if current_state is not None and len(signal) - segment_start >= min_segment_length:
        if current_state:
            high_segments.append(signal[segment_start:])
        else:
            low_segments.append(signal[segment_start:])
    
    # Align segments using cross-correlation
    aligned_high, high_shifts = align_segments(high_segments)
    aligned_low, low_shifts = align_segments(low_segments)
    
    # Find longest segments to create properly sized arrays for statistics
    max_len_high = max(len(seg) for seg in aligned_high) if aligned_high else 0
    max_len_low = max(len(seg) for seg in aligned_low) if aligned_low else 0
    
    # Calculate statistics using NaN padding
    if aligned_high:
        high_array = np.full((len(aligned_high), max_len_high), np.nan)
        for i, seg in enumerate(aligned_high):
            high_array[i, :len(seg)] = seg
        high_avg = np.nanmean(high_array, axis=0)
        high_std = np.nanstd(high_array, axis=0)
    else:
        high_avg = np.array([])
        high_std = np.array([])
        
    if aligned_low:
        low_array = np.full((len(aligned_low), max_len_low), np.nan)
        for i, seg in enumerate(aligned_low):
            low_array[i, :len(seg)] = seg
        low_avg = np.nanmean(low_array, axis=0)
        low_std = np.nanstd(low_array, axis=0)
    else:
        low_avg = np.array([])
        low_std = np.array([])
    
    return {
        'high_segments': high_segments,
        'low_segments': low_segments,
        'aligned_high': aligned_high,
        'aligned_low': aligned_low,
        'high_shifts': high_shifts,
        'low_shifts': low_shifts,
        'high_avg': high_avg,
        'high_std': high_std,
        'low_avg': low_avg,
        'low_std': low_std
    }


def align_segments(segments, reference=None, max_shift=None):
    """
    Aligns segments using cross-correlation while preserving their natural lengths.
    
    Parameters:
    segments (list): List of signal segments to align
    reference (array): Reference signal for alignment. If None, uses median length segment
    max_shift (int): Maximum allowed shift for alignment. If None, uses 20% of segment length
    
    Returns:
    tuple: (aligned_segments, shifts_applied)
    """
    if not segments:
        return np.array([]), []
    
    # Convert segments to numpy array if they aren't already
    segments = [np.array(seg) for seg in segments]
    
    # Find segment lengths
    lengths = [len(seg) for seg in segments]
    median_length = int(np.median(lengths))
    
    # Use median length segment as reference if none provided
    if reference is None:
        # Find segment closest to median length
        idx = np.argmin(np.abs(np.array(lengths) - median_length))
        reference = segments[idx]
    
    # Set maximum shift if not specified
    if max_shift is None:
        max_shift = int(0.2 * len(reference))  # 20% of reference length
    
    aligned_segments = []
    shifts = []
    
    for seg in segments:
        # Use the shorter signal length for correlation
        min_len = min(len(reference), len(seg))
        ref_temp = reference[:min_len]
        seg_temp = seg[:min_len]
        
        # Compute cross-correlation
        corr = sig.correlate(ref_temp, seg_temp, mode='full')
        max_corr_idx = np.argmax(corr)
        shift = max_corr_idx - (len(seg_temp) - 1)
        
        # Limit shift to max_shift
        shift = np.clip(shift, -max_shift, max_shift)
        shifts.append(shift)
        
        # Apply shift while preserving original length
        if shift > 0:
            aligned_seg = np.pad(seg, (0, shift), mode='edge')[shift:]
        elif shift < 0:
            aligned_seg = np.pad(seg, (-shift, 0), mode='edge')[:shift]
        else:
            aligned_seg = seg
            
        aligned_segments.append(aligned_seg)
    
    return aligned_segments, shifts

def plot_aligned_waveforms(results, sample_period=None):
    """
    Plots all aligned waveforms and their average with standard deviation,
    using the same scales for both plots.
    """
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(12, 8))
    
    if sample_period is None:
        sample_period = 1.0
    
    # Find global min and max for y-axis
    y_values = []
    if len(results['aligned_high']) > 0:
        y_values.extend([seg.min() for seg in results['aligned_high']])
        y_values.extend([seg.max() for seg in results['aligned_high']])
    if len(results['aligned_low']) > 0:
        y_values.extend([seg.min() for seg in results['aligned_low']])
        y_values.extend([seg.max() for seg in results['aligned_low']])
    
    if y_values:
        y_min = min(y_values)
        y_max = max(y_values)
        # Add 5% padding to y-axis limits
        y_range = y_max - y_min
        y_min -= 0.05 * y_range
        y_max += 0.05 * y_range
    
    # Find global max length for x-axis
    max_len = 0
    if len(results['aligned_high']) > 0:
        max_len = max(max_len, max(len(seg) for seg in results['aligned_high']))
    if len(results['aligned_low']) > 0:
        max_len = max(max_len, max(len(seg) for seg in results['aligned_low']))
    
    time_base_max = np.arange(max_len) * sample_period
    
    # Plot high state segments
    if len(results['aligned_high']) > 0:
        all_high = np.full((len(results['aligned_high']), max_len), np.nan)
        
        # Plot individual waveforms
        for i, seg in enumerate(results['aligned_high']):
            t = np.arange(len(seg)) * sample_period
            ax1.plot(t, seg, 'b-', alpha=0.1)
            all_high[i, :len(seg)] = seg
        
        # Calculate and plot mean/std
        high_mean = np.nanmean(all_high, axis=0)
        high_std = np.nanstd(all_high, axis=0)
        ax1.plot(time_base_max[:len(high_mean)], high_mean, 'b-', 
                label='Average', linewidth=2)
        ax1.fill_between(time_base_max[:len(high_mean)],
                        high_mean - high_std,
                        high_mean + high_std,
                        alpha=0.3, color='b')
        
        ax1.set_title(f'High State Segments (n={len(results["aligned_high"])}) = addition')
        ax1.grid(True)
        ax1.legend()
    
    # Plot low state segments
    if len(results['aligned_low']) > 0:
        all_low = np.full((len(results['aligned_low']), max_len), np.nan)
        
        # Plot individual waveforms
        for i, seg in enumerate(results['aligned_low']):
            t = np.arange(len(seg)) * sample_period
            ax2.plot(t, seg, 'r-', alpha=0.1)
            all_low[i, :len(seg)] = seg
        
        # Calculate and plot mean/std
        low_mean = np.nanmean(all_low, axis=0)
        low_std = np.nanstd(all_low, axis=0)
        ax2.plot(time_base_max[:len(low_mean)], low_mean, 'r-', 
                label='Average', linewidth=2)
        ax2.fill_between(time_base_max[:len(low_mean)],
                        low_mean - low_std,
                        low_mean + low_std,
                        alpha=0.3, color='r')
        
        ax2.set_title(f'Low State Segments (n={len(results["aligned_low"])}) = multiplication')
        ax2.grid(True)
        ax2.legend()
    
    # Set same scales for both plots
    if y_values:
        ax1.set_ylim(y_min, y_max)
        ax2.set_ylim(y_min, y_max)
    
    ax1.set_xlim(0, max_len * sample_period)
    ax2.set_xlim(0, max_len * sample_period)
    
    # Add labels
    ax1.set_xlabel('Time (ns)')
    ax2.set_xlabel('Time (ns)')
    ax1.set_ylabel('Voltage (V)')
    ax2.set_ylabel('Voltage (V)')
    
    plt.tight_layout()
    return fig

def print_segment_stats(results):
    print("\nSegment Statistics:")
    
    if len(results['aligned_high']) > 0:
        high_lengths = [len(seg) for seg in results['aligned_high']]
        print("\nHigh segments:")
        print(f"Number of segments: {len(high_lengths)}")
        print(f"Mean length: {np.mean(high_lengths):.1f} samples")
        print(f"Median length: {np.median(high_lengths):.1f} samples")
        print(f"Min length: {np.min(high_lengths)} samples")
        print(f"Max length: {np.max(high_lengths)} samples")
        print(f"Length std: {np.std(high_lengths):.1f} samples")
        
    if len(results['aligned_low']) > 0:
        low_lengths = [len(seg) for seg in results['aligned_low']]
        print("\nLow segments:")
        print(f"Number of segments: {len(low_lengths)}")
        print(f"Mean length: {np.mean(low_lengths):.1f} samples")
        print(f"Median length: {np.median(low_lengths):.1f} samples")
        print(f"Min length: {np.min(low_lengths)} samples")
        print(f"Max length: {np.max(low_lengths)} samples")
        print(f"Length std: {np.std(low_lengths):.1f} samples")


mean = np.mean(y2)
high_thresh = mean - 0.05
low_thresh = mean + 0.05

results = segment_signal_robust(y1, y2, high_thresh=high_thresh, low_thresh=low_thresh)
print_segment_stats(results)

fig = plot_aligned_waveforms(results, sample_period=p1['xincrement']*1e9)
plt.show()