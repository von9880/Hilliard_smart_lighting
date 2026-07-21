# broadcaster.py

import time


class ThresholdTrigger:
    #Only triggers when audio crosses above threshold.

    def __init__(self, threshold):
        self.threshold = threshold
        self.was_high = False


    def check(self, value):

        triggered = (value >= self.threshold and not self.was_high)

        self.was_high = (value >= self.threshold)

        return triggered



class Broadcaster:

    def __init__(self, mqtt_client, topic="lights"):

        self.client = mqtt_client
        self.topic = topic

        self.last_trigger = 0

        # Minimum time between effects
        self.cooldown = 0.35


        self.bass_trigger = ThresholdTrigger(75)

        self.treble_trigger = ThresholdTrigger(85)


    def trigger(self, pattern):

        now = time.time()

        if now - self.last_trigger < self.cooldown:
            return

        self.client.publish(self.topic, payload=pattern, qos=0)


        self.last_trigger = now



    def process(self, brightness, beat_strength):


        bass = brightness["bass"]
        treble = brightness["treble"]

        # Strong beat event
        if beat_strength > 0.8:

            self.trigger("beat")

        # Bass hit
        elif self.bass_trigger.check(bass):

            self.trigger("bass")

        # Treble hit
        elif self.treble_trigger.check(treble):

            self.trigger("treble")