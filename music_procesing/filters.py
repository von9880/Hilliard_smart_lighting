# filters.py

import numpy as np


class AdaptiveGain:

    #Automatically adjusts sensitivity based on recent audio levels. Makes quiet songs and loud songs behave similarly.

    def __init__(self, attack=0.05, release=0.001):
        self.average = 0.001
        self.attack = attack
        self.release = release

    def process(self, value):

        # Fast response when music gets louder
        if value > self.average:
            self.average += (value - self.average) * self.attack

        # Slow decay when music gets quieter
        else:
            self.average += (value - self.average) * self.release

        if self.average <= 0:
            return 0

        return value / self.average



class ExponentialSmoother:

    # Smooths values to prevent LED flickering.

    def __init__(self, smoothing=0.25):
        self.value = 0
        self.smoothing = smoothing

    def process(self, new_value):
        self.value = (self.value * (1 - self.smoothing) + new_value * self.smoothing)

        return self.value



class PeakHold:
    #Holds peaks briefly before releasing Useful for bass hits and beat effects.

    def __init__(self, decay=0.92):
        self.peak = 0
        self.decay = decay

    def process(self, value):

        if value > self.peak:
            self.peak = value

        else:
            self.peak *= self.decay

        return self.peak



class BrightnessMapper:

    #Converts normalized audio intensity into LED brightness.


    def __init__(self, curve=2.0):
        self.curve = curve

    def process(self, value):

        # Limit extreme values
        value = np.clip(value, 0, 1)

        # Perceived brightness curve
        brightness = value ** self.curve

        return int(brightness * 100)



class BandProcessor:

    #Combines gain control, smoothing, peak holding, and brightness mapping for a single frequency band.
    
    def __init__(self, gain_attack=0.05, gain_release=0.001, smoothing=0.25, peak_decay=0.92):

        self.gain = AdaptiveGain(attack=gain_attack, release=gain_release)

        self.smoother = ExponentialSmoother(smoothing)

        self.peak = PeakHold(peak_decay)

        self.mapper = BrightnessMapper()


    def process(self, value):

        # Automatic volume adjustment
        value = self.gain.process(value)

        # Keep values reasonable
        value = np.clip(value / 5, 0, 1)

        # Smooth response
        value = self.smoother.process(value)

        # Add peak response
        peak = self.peak.process(value)

        # Combine normal + peak
        value = max(value, peak * 0.8)

        return self.mapper.process(value)