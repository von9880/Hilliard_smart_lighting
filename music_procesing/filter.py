from scipy.signal import butter, lfilter
from numpy import clip

def butter_bandpass(lowcut, highcut, fs, order=4):
    """Generates coefficients for lowpass, highpass, or bandpass filters."""
    nyq = 0.5 * fs
    if lowcut is None:
        normal_high = highcut / nyq
        return butter(order, normal_high, btype='low')
    elif highcut is None:
        normal_low = lowcut / nyq
        return butter(order, normal_low, btype='high')
    else:
        normal_low = lowcut / nyq
        normal_high = highcut / nyq
        return butter(order, [normal_low, normal_high], btype='band')


class DynamicPeakTracker:
    """Tracks peak volume inputs with an automated decay to adjust sensitivity."""
    def __init__(self, initial_max, decay_rate=0.005):
        self.current_max = initial_max
        self.decay_rate = decay_rate

    def update_and_get_max(self, current_volume):
        if current_volume > self.current_max:
            self.current_max = current_volume
        else:
            self.current_max = max(0.01, self.current_max - self.decay_rate)
        return self.current_max


def map_volume_to_brightness(volume, min_vol, max_vol, target_max=100):
    """Maps raw float intensities to a perceived logarithmic 0-100% curve."""
    if max_vol <= min_vol:
        max_vol = min_vol + 0.01
    clipped_vol = clip(volume, min_vol, max_vol)
    normalized = (clipped_vol - min_vol) / (max_vol - min_vol)
    return int((normalized ** 2) * target_max)