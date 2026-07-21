# config.py


# =========================
# Audio Settings
# =========================

AUDIO_FILE = r"music_procesing\wave\think.wav"

AUDIO_DELAY_SECONDS = 0.0

# Bigger = better bass accuracy
# Smaller = lower latency
WINDOW_SIZE = 1024



# =========================
# FFT Frequency Bands
# =========================

BASS_RANGE = (40, 180)

LOW_MID_RANGE = (180, 600)

MID_RANGE = (600, 3000)

TREBLE_RANGE = (3000, 10000)



# =========================
# MQTT Settings
# =========================

MQTT_BROKER = "192.168.60.6"

MQTT_PORT = 1883

MQTT_USERNAME = "MusicBox"

MQTT_PASSWORD = "HilliardMusicBox"



# =========================
# Processing Settings
# =========================

# LED smoothing
SMOOTHING = 0.25


# Beat sensitivity
# Higher = fewer beats
# Lower = more sensitive
BEAT_SENSITIVITY = 1