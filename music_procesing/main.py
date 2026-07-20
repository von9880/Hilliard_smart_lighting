from config import *
from filter import *


import queue
import sys
import time
import numpy as np
import paho.mqtt.client as mqtt
import sounddevice as sd
import soundfile as sf
from scipy.signal import butter, lfilter
import threading


class AudioStreamProcessor:
    def __init__(self, file_path, delay_seconds=0.120): 
        # Load and parse audio attributes
        data, self.fs = sf.read(file_path, dtype='float32')
        self.raw_data = data.mean(axis=1) if len(data.shape) > 1 else data
        
        # Prepare delayed audio track array for hardware sync
        delay_samples = int(delay_seconds * self.fs)
        delay_buffer = np.zeros(delay_samples, dtype='float32')
        self.delayed_data = np.concatenate((delay_buffer, self.raw_data))
        
        # Instantiate localized bandpass tracking bounds
        self.b_bass, self.a_bass = butter_bandpass(None, 250, self.fs)
        self.b_mid, self.a_mid = butter_bandpass(250, 4000, self.fs)
        self.b_treble, self.a_treble = butter_bandpass(4000, None, self.fs)
        
        # Thread pipeline queue and cursor pointer
        self.data_queue = queue.Queue()
        self.current_frame = 0

    def stream_callback(self, outdata, frames, time_info, status):
        """Asynchronous stream playback interrupt worker."""
        if status:
            print(status, file=sys.stderr)
        
        remainder = len(self.raw_data) - self.current_frame
        if remainder <= 0:
            raise sd.CallbackStop
        
        valid_frames = min(frames, remainder)
        chunk = self.raw_data[self.current_frame : self.current_frame + valid_frames]
        playback_chunk = self.delayed_data[self.current_frame : self.current_frame + valid_frames]
        
        outdata[:valid_frames, 0] = playback_chunk
        if valid_frames < frames:
            outdata[valid_frames:, 0] = 0
            raise sd.CallbackStop
            
        self.current_frame += valid_frames
        
        # Feed processed signals forward to structural output pipeline arrays
        self.data_queue.put({
            'bass': lfilter(self.b_bass, self.a_bass, chunk),
            'mid': lfilter(self.b_mid, self.a_mid, chunk),
            'treble': lfilter(self.b_treble, self.a_treble, chunk)
        })



class NetworkDataBroadcaster:
    def __init__(self, mqtt_client):
        self.mqtt_client = mqtt_client

        # Dynamic threshold tracking instances
        self.bass_tracker = DynamicPeakTracker(initial_max=0.40, decay_rate=0.001)
        self.mid_tracker = DynamicPeakTracker(initial_max=0.25, decay_rate=0.0008)
        self.treble_tracker = DynamicPeakTracker(initial_max=0.15, decay_rate=0.0005)

    def lights_trigger(self, freqBand):
        if freqBand >= 70:
            self.mqtt_client.publish("lights", payload="trigger", qos=0)

    def process_and_send(self, bands):

        # 1. Compute RMS intensities
        b_vol = np.sqrt(np.mean(bands['bass'] ** 2))
        m_vol = np.sqrt(np.mean(bands['mid'] ** 2))
        t_vol = np.sqrt(np.mean(bands['treble'] ** 2))

        # 2. Extract calibrated brightness positions
        bass_bright = map_volume_to_brightness(
            b_vol, 0.005, self.bass_tracker.update_and_get_max(b_vol)
        )
        mid_bright = map_volume_to_brightness(
            m_vol, 0.005, self.mid_tracker.update_and_get_max(m_vol)
        )
        treble_bright = map_volume_to_brightness(
            t_vol, 0.002, self.treble_tracker.update_and_get_max(t_vol)
        )

        # 3. Console update
        sys.stdout.write(
            f"\rLocal Status -> Bass: {bass_bright:<3}% | "
            f"Mid: {mid_bright:<3}% | "
            f"Treble: {treble_bright:<3}%"
        )
        sys.stdout.flush()

        # Trigger AC lights
        self.lights_trigger(bass_bright)



# ==============================================================================
# 5. RUNTIME EXECUTION ENTRY POINT
# ==============================================================================
def main():
    # Variables to track the window size initialization
    window_size_received = threading.Event()
    
    # Fallback default value in case the network request times out
    dynamic_window_size = 2048 

    # MQTT V2 callback to catch the incoming window size
    def on_message(client, userdata, message):
        nonlocal dynamic_window_size
        try:
            payload_str = message.payload.decode("utf-8")
            print(f"\n[MQTT Sync] Received window size string: '{payload_str}'")
            
            # Convert the incoming data to an integer for the audio stream
            dynamic_window_size = int(payload_str)
            print(f"[MQTT Sync] Applied window size: {dynamic_window_size} frames")
            
            # Stop listening to the window configuration topic
            client.unsubscribe("audio/config/window_size")
            
            # Unlock the main execution thread
            window_size_received.set()
        except ValueError:
            print(f"\n[Error] Received invalid window size: '{payload_str}'. Must be an integer.")
            # Let it fallback gracefully or keep waiting depending on preference
            window_size_received.set() 
        except Exception as e:
            print(f"Error handling message: {e}")
            window_size_received.set()

    # Phase A: Initialize MQTT Client
    client = mqtt.Client(callback_api_version=mqtt.CallbackAPIVersion.VERSION2)
    client.username_pw_set("MusicBox", "HilliardMusicBox") 
    client.on_message = on_message

    # Persistent Connection Retry Loop
    connected = False
    while not connected:
        try:
            print(f"Connecting to MQTT Broker ({MQTT_BROKER})...")
            client.connect(MQTT_BROKER, MQTT_PORT, 60)
            client.loop_start()
            print("MQTT Connected successfully.")
            connected = True  # Break the retry loop
            
            # 1. Subscribe to your window configuration topic
            print("Subscribing to window size topic...")
            client.subscribe("testing/frames/broker")
            
            # 2. Ask the broker or controller to publish the size right now
            print("Requesting window frame configurations...")
            client.publish("testing/frames/client", payload="get_window_size", qos=1)
            
        except (OSError, Exception) as e:
            print(f"Failed to connect to MQTT: {e}. Retrying in 3 seconds...")
            client.loop_stop()  # Safe cleanup step if it partially failed
            time.sleep(3)

    # 3. Halt until the data arrives (up to a 1-second safety limit)
    print("Waiting for window frame size from MQTT server...")
    data_acquired = window_size_received.wait(timeout=1.0)

    if not data_acquired:
        print(f"\n[Warning] MQTT fetch timed out! Proceeding with fallback window size: {dynamic_window_size}")
    else:
        print("\nInitialization complete. Preparing audio pipeline...")

    # Phase B: Spin up internal engine state properties
    processor = AudioStreamProcessor(AUDIO_FILE, AUDIO_DELAY_SECONDS)
    broadcaster = NetworkDataBroadcaster(client)

    # Phase C: Initialize background playback loops using the dynamically fetched size
    try:
        stream = sd.OutputStream(
            samplerate=processor.fs, 
            channels=1, 
            callback=processor.stream_callback, 
            blocksize=dynamic_window_size  # <-- Applied dynamically here!
        )
        with stream:
            print(f"\nStreaming started with block size {dynamic_window_size}. Broadcasting Bass brightness...")
            while stream.active:
                try:
                    while processor.data_queue.qsize() > 1:
                        processor.data_queue.get_nowait()
                    
                    bands = processor.data_queue.get(timeout=1.0)
                    broadcaster.process_and_send(bands)
                    
                except queue.Empty:
                    continue
    except Exception as e:
        print(f"\nError during playback engine run: {e}")
    finally:
        print("\nShutting down MQTT network pipes...")
        client.loop_stop()
        client.disconnect()
        print("Streaming and data transmission complete.")


if __name__ == "__main__":
    main()
