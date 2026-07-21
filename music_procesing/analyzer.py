# analyzer.py

import numpy as np

class FFTAnalyzer:
    # FFT-based audio analyzer. Takes a chunk of audio samples and returns the average energy in each configured frequency band.

    def __init__(self, sample_rate, chunk_size):
        self.sample_rate = sample_rate
        self.chunk_size = chunk_size

        # Precompute Hann window
        self.window = np.hanning(chunk_size)

        # Frequency values for each FFT bin
        self.freqs = np.fft.rfftfreq(chunk_size, d=1.0 / sample_rate)

        # Frequency bands (Hz)
        self.bands = {
            "bass": (40, 180),
            "low_mid": (180, 600),
            "mid": (600, 3000),
            "treble": (3000, 10000),
        }

        # Cache indices for speed
        self.band_indices = {}

        for name, (low, high) in self.bands.items():
            idx = np.where(
                (self.freqs >= low) &
                (self.freqs < high)
            )[0]

            self.band_indices[name] = idx

    def analyze(self, audio_chunk):

        # Safety check
        if len(audio_chunk) != self.chunk_size:
            audio_chunk = np.pad(
                audio_chunk,
                (0, self.chunk_size - len(audio_chunk)),
                mode="constant"
            )

        # Apply Hann window
        windowed = audio_chunk * self.window

        # FFT
        spectrum = np.abs(np.fft.rfft(windowed))

        results = {}

        for name, idx in self.band_indices.items():

            if len(idx) == 0:
                results[name] = 0.0
            else:
                # RMS energy of the frequency bins
                results[name] = float(
                    np.sqrt(np.mean(spectrum[idx] ** 2))
                )

        results["spectrum"] = spectrum

        return results