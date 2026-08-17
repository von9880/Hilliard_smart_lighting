# pip install paho-mqtt numpy soundcard


import time
import paho.mqtt.client as mqtt
import tkinter as tk
import threading


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


def connect_mqtt():
    client = mqtt.Client()
    client.username_pw_set(MQTT_USERNAME, MQTT_PASSWORD)
    print(f"Connecting to MQTT {MQTT_BROKER}")
    client.connect(MQTT_BROKER, MQTT_PORT, 60)
    client.loop_start()
    print("MQTT Connected")
    return client


class AudioMqttApp:
    def __init__(self, root):
        self.root = root
        self.root.title("Music Processing")
        self.root.geometry("400x200")

        self.root.configure(bg="#cfe5f4")
        self.root.configure(background="#cfe5f4")
        
        # Threading control states
        self.is_running = False
        self.processing_thread = None

        # --- GUI Elements ---
        self.status_label = tk.Label(self.root, text="Status: Idle", font=("Arial", 14), fg="#1098d7", bg= "#cfe5f4")
        self.status_label.pack(pady=20)

        btn_frame = tk.Frame(self.root, bg="#cfe5f4")
        btn_frame.pack(pady=8)

        self.start_btn = tk.Button(
            btn_frame, 
            text="Start listening", 
            command=self.start_loop, 
            bg="#257ba3", fg="white", 
            activeforeground="white",  
            activebackground="#06a7f2",
            disabledforeground="white",                
            width=12,
            highlightthickness=10, 
            padx=20, 
            pady=10, 
            bd=0, 
            cursor="hand2",
            highlightbackground="red", 
            highlightcolor="blue"   
            )
        
        self.start_btn.pack(side="left", padx=10)
        self.stop_btn = tk.Button(
            btn_frame, 
            text="Stop listening", 
            command=self.stop_loop, 
            bg="#257ba3", 
            fg="white", 
            activeforeground="white",
            activebackground="#06a7f2",   
            disabledforeground="white",               
            width=12,  
            highlightthickness=10, 
            padx=20, 
            pady=10, 
            bd=0, 
            cursor="hand2",
            highlightbackground="red", 
            highlightcolor="blue"   
        )
        self.stop_btn.pack(side="right", padx=10)



        # Handle window exit securely
        self.root.protocol("WM_DELETE_WINDOW", self.on_closing)


    def audio_processing_worker(self):
        print("Background process started...")
        
        # 1. Setup everything inside the thread right as it boots up
        mqtt_client = connect_mqtt()
        audio = AudioStream(WINDOW_SIZE)
        analyzer = FFTAnalyzer(audio.sample_rate, WINDOW_SIZE)
        
        processors = {
            "bass": BandProcessor(smoothing=SMOOTHING),
            "low_mid": BandProcessor(smoothing=SMOOTHING),
            "mid": BandProcessor(smoothing=SMOOTHING),
            "treble": BandProcessor(smoothing=SMOOTHING)
        }

        beat_detector = BeatDetector(sensitivity=BEAT_SENSITIVITY)
        broadcaster = Broadcaster(mqtt_client)

        audio.start()
        print("Audio processing started...")

        # 2. Main processing execution loop
        try:
            # Added self.is_running check so hitting 'Stop' breaks this loop
            while self.is_running and audio.is_active():
                chunk = audio.get_chunk()
                if chunk is None:
                    continue

                # FFT
                data = analyzer.analyze(chunk)

                # Process brightness
                brightness = {}
                for band in processors:
                    brightness[band] = processors[band].process(data[band])

                # Beat detection
                beat_detector.process(data["spectrum"])
                beat_strength = beat_detector.get_strength()

                # Send to lights
                broadcaster.process(brightness, beat_strength)

        except Exception as e:
            print(f"Error in processing loop: {e}")

        # 3. Cleanup happens immediately when self.is_running is flipped to False
        finally:
            print("Cleaning up connections...")
            audio.stop()
            mqtt_client.loop_stop()
            mqtt_client.disconnect()
            print("Background process safely stopped.")

    def start_loop(self):
        if not self.is_running:
            self.is_running = True
            
            # Adjust Tkinter UI Buttons
            self.status_label.config(text="Status: Processing...", fg="#1098d7", bg="#cfe5f4")
            self.start_btn.config(state="disabled")
            self.stop_btn.config(state="normal")
            
            # Start background thread
            self.processing_thread = threading.Thread(target=self.audio_processing_worker, daemon=True)
            self.processing_thread.start()

    def stop_loop(self):
        if self.is_running:
            self.is_running = False
            
            # Adjust Tkinter UI Buttons
            self.status_label.config(text="Status: Idle", fg="#1098d7", bg="#cfe5f4")
            self.start_btn.config(state="normal")
            self.stop_btn.config(state="disabled")

    def on_closing(self):
        self.stop_loop()
        # Give the thread a tiny fraction of a second to complete its current loop iteration
        self.root.after(150, self.root.destroy)


# =========================
# Application Runner
# =========================
root = tk.Tk()
app = AudioMqttApp(root)
root.mainloop()