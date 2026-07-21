# audio_stream.py

import sounddevice as sd
import soundfile as sf
import numpy as np
import queue
import sys


class AudioStream:

    # Handles audio playback and provides audio chunks for processing.


    def __init__(
        self,
        file_path,
        chunk_size,
        delay_seconds=0
    ):

        # Load audio file
        data, self.sample_rate = sf.read(
            file_path,
            dtype="float32"
        )


        # Convert stereo to mono
        if len(data.shape) > 1:
            data = data.mean(axis=1)


        self.audio = data


        # Delay buffer for hardware sync
        delay_samples = int(
            delay_seconds *
            self.sample_rate
        )

        self.playback_audio = np.concatenate(
            (
                np.zeros(
                    delay_samples,
                    dtype="float32"
                ),
                self.audio
            )
        )


        self.chunk_size = chunk_size


        # Current playback location
        self.position = 0


        # Queue for analyzer
        self.audio_queue = queue.Queue(
            maxsize=10
        )

        self.stream = None

    def callback(
        self,
        outdata,
        frames,
        time,
        status
    ):

        if status:
            print(
                status,
                file=sys.stderr
            )

        start = self.position
        end = start + frames

        # End of song
        if start >= len(self.audio):

            outdata.fill(0)

            raise sd.CallbackStop


        # Get audio chunk
        chunk = self.audio[start:end]

        playback = self.playback_audio[
            start:end
        ]

        # Output audio
        outdata[:len(playback), 0] = playback


        # Fill remaining buffer
        if len(playback) < frames:
            outdata[len(playback):,0] = 0

        # Send chunk to analyzer
        if len(chunk) > 0:
            try:
                self.audio_queue.put_nowait(chunk.copy())

            except queue.Full:
                pass


        self.position += frames


    def start(self):

        self.stream = sd.OutputStream(
            samplerate=self.sample_rate,
            channels=1,
            blocksize=self.chunk_size,
            callback=self.callback
        )

        self.stream.start()



    def stop(self):
        if self.stream:
            self.stream.stop()
            self.stream.close()



    def is_active(self):
        if self.stream:
            return self.stream.active
        
        return False



    def get_chunk(self):
        # Gets the next audio chunk


        try:
            return self.audio_queue.get(timeout=1)
        except queue.Empty:
            return None