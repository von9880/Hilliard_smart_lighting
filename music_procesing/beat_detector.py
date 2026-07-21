# beat_detector.py

import numpy as np
from collections import deque


class BeatDetector:
    #Detects musical beats using spectral flux. 
    #Spectral flux measures how much the FFT spectrum changes between audio frames.

    def __init__(
        self,
        history_size=43,
        sensitivity=1.5,
        decay=0.95
    ):

        # Previous FFT frame
        self.previous_spectrum = None

        # Store recent flux values
        self.flux_history = deque(maxlen=history_size)

        self.sensitivity = sensitivity
        self.decay = decay

        self.energy = 0


    def process(self, spectrum):

        if self.previous_spectrum is None:
            self.previous_spectrum = spectrum
            return False


        # Calculate spectral difference
        difference = (spectrum - self.previous_spectrum)

        # Ignore decreases in energy
        difference = np.maximum(difference, 0)

        # Total increase in energy
        flux = np.sum(difference)


        self.previous_spectrum = spectrum


        # Store history
        self.flux_history.append(flux)


        if len(self.flux_history) < 10:
            return False


        # Calculate average energy
        average = np.mean(self.flux_history)

        # Calculate dynamic threshold
        threshold = (average * self.sensitivity)


        # Beat detected
        if flux > threshold:
            self.energy = 1.0
            return True


        # Slowly decay beat strength
        self.energy *= self.decay

        return False



    def get_strength(self):
        return np.clip(self.energy, 0, 1)