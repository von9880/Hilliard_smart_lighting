import time
import paho.mqtt.client as mqtt

from config import *
from audio_stream import AudioStream
from analyzer import FFTAnalyzer
from filters import BandProcessor
from beat_detector import BeatDetector
from broadcaster import Broadcaster



# =========================
# MQTT Setup
# =========================

def connect_mqtt():
    client = mqtt.Client()
    client.username_pw_set(MQTT_USERNAME,MQTT_PASSWORD)

    print(f"Connecting to MQTT {MQTT_BROKER}")

    client.connect(MQTT_BROKER,MQTT_PORT,60)

    client.loop_start()
    print("MQTT Connected")

    return client



# =========================
# Main
# =========================


def main():
    # MQTT
    mqtt_client = connect_mqtt()

    # Audio engine
    audio = AudioStream(AUDIO_FILE, WINDOW_SIZE, AUDIO_DELAY_SECONDS)

    # FFT analyzer
    analyzer = FFTAnalyzer(audio.sample_rate, WINDOW_SIZE)

    # Band processors
    processors = {
        "bass":
        BandProcessor(
            smoothing=SMOOTHING
        ),

        "low_mid":
        BandProcessor(
            smoothing=SMOOTHING
        ),

        "mid":
        BandProcessor(
            smoothing=SMOOTHING
        ),

        "treble":
        BandProcessor(
            smoothing=SMOOTHING
        )
    }

    # Beat detection
    beat_detector = BeatDetector(sensitivity=BEAT_SENSITIVITY)

    # Output
    broadcaster = Broadcaster(mqtt_client)

    # Start audio
    audio.start()

    print("Audio processing started...")

    try:
        while audio.is_active():

            chunk = audio.get_chunk()

            if chunk is None:
                continue

            # FFT
            data = analyzer.analyze(
                chunk
            )

            # Process brightness
            brightness = {}

            for band in processors:
                brightness[band] = processors[band].process(data[band])

            # Beat detection
            beat_detector.process(data["spectrum"])

            beat_strength = beat_detector.get_strength()

            # Send to lights
            broadcaster.process(brightness, beat_strength)



    except KeyboardInterrupt:
        print("\nStopping...")


    finally:
        audio.stop()

        mqtt_client.loop_stop()

        mqtt_client.disconnect()



if __name__ == "__main__":
    main()