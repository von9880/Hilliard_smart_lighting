# Audio Reactive Lighting Trigger System

## Overview

This project analyzes an audio file in real time and detects musical events. When specific audio conditions are met, the program sends MQTT trigger messages to an external lighting controller.

The Python program is responsible for:

- Reading audio
- Analyzing frequency content
- Detecting musical events
- Sending MQTT triggers

The lighting controller is responsible for deciding what visual effect to run.

---

# System Flow

```
Audio File
    |
    v
Audio Stream
    |
    v
FFT Frequency Analysis
    |
    v
Audio Processing
    |
    v
Beat Detection
    |
    v
Trigger Detection
    |
    v
MQTT Message
    |
    v
Lighting Controller
```

---

# File Structure

```
music_processor/

├── main.py
├── config.py
│
├── audio_stream.py
├── analyzer.py
├── filters.py
├── beat_detector.py
├── broadcaster.py
│
└── README.md
```

---

# main.py

## Purpose

`main.py` controls the complete audio processing pipeline.

It initializes:

- MQTT connection
- Audio stream
- FFT analyzer
- Audio filters
- Beat detector
- MQTT broadcaster

The main loop continuously:

1. Receives an audio chunk
2. Analyzes frequency information
3. Processes audio levels
4. Detects beats
5. Sends triggers when events occur

---

# config.py

## Purpose

Stores all adjustable settings.

## Audio Settings

Example:

```python
WINDOW_SIZE = 2048
```

Controls the size of each audio analysis frame.

A larger window:

- More frequency accuracy
- More processing delay

A smaller window:

- Faster response
- Less frequency resolution


## MQTT Settings

Example:

```python
MQTT_BROKER = "192.168.60.6"
MQTT_PORT = 1883
```

Defines where trigger messages are sent.

---

# audio_stream.py

## Purpose

Handles loading and streaming the audio file.

Functions:

- Loads the audio file
- Converts stereo audio into a single channel
- Creates audio chunks
- Sends chunks to the analyzer
- Plays audio through the output device


The audio is split into small sections:

```
Full Song

|
|
v

Chunk 1
Chunk 2
Chunk 3
Chunk 4
```

Each chunk is analyzed individually.

---

# analyzer.py

## Purpose

Converts audio samples into frequency information using Fast Fourier Transform (FFT).

FFT converts the audio from:

```
Time Domain

Amplitude over time
```

into:

```
Frequency Domain

Energy at different frequencies
```

The analyzer separates audio into four frequency ranges.

---

# Frequency Bands

## Bass

```
40Hz - 180Hz
```

Detects:

- Kick drums
- Low bass frequencies


## Low Mid

```
180Hz - 600Hz
```

Detects:

- Lower instruments
- Body of sounds


## Mid

```
600Hz - 3000Hz
```

Detects:

- Vocals
- Main instruments


## Treble

```
3000Hz - 10000Hz
```

Detects:

- Cymbals
- High frequency sounds

---

# filters.py

## Purpose

Processes frequency levels so they can be used for event detection.

It contains:

## Adaptive Gain

Automatically adjusts sensitivity based on the current audio level.

This allows the system to respond to different songs without manually changing thresholds.

---

## Smoothing

Reduces rapid changes in values.

Example:

```
Raw:

20
90
30
100


Smoothed:

45
65
55
75
```

---

## Peak Hold

Keeps strong audio events active briefly.

This allows short sounds like drum hits to be detected more reliably.

---

## Brightness Mapper

Converts processed audio values into a 0-100 range.

Example:

```
Input:

0.75


Output:

75
```

---

# beat_detector.py

## Purpose

Detects sudden changes in audio energy.

It uses spectral flux.

Spectral flux measures how much the frequency spectrum changes between audio frames.

Process:

```
Current Spectrum

        +

Previous Spectrum

        |

        v

Difference Calculation

        |

        v

Beat Strength
```

When the change exceeds the detection threshold:

```
Beat Detected = True
```

---

# broadcaster.py

## Purpose

Sends detected audio events to the lighting controller.

The broadcaster does not control lighting effects.

It only sends MQTT trigger messages.

---

# Trigger Detection

The system checks:

## Bass Events

If bass energy crosses the trigger threshold:

```
bass > threshold
```

a bass trigger is sent.

---

## Beat Events

If the beat detector identifies a strong audio change:

```
beat_strength > threshold
```

a beat trigger is sent.

---

## Treble Events

If high frequency energy crosses the threshold:

```
treble > threshold
```

a treble trigger is sent.

---

# MQTT Output

The Python program sends messages through MQTT.

Topic:

```
lights
```

Example messages:

```
bass
```

```
beat
```

```
treble
```

The receiving controller interprets these messages and runs the appropriate lighting pattern.

---

# Trigger Protection

The system includes two methods to prevent excessive triggers.

## Rising Edge Detection

A trigger occurs only when the signal crosses above the threshold.

Example:

```
Audio Level:

40
60
85  <-- Trigger
90
80
70
```

Only the transition above the threshold creates an event.

---

## Cooldown Timer

After a trigger is sent, a short delay prevents repeated messages.

Example:

```
Beat

(wait)

Beat

(wait)

Beat
```

This keeps the output clean and prevents rapid repeated effects.

---

# Complete Data Flow

```
                Audio File

                    |

                    v

            audio_stream.py

                    |

                    v

             Audio Samples

                    |

                    v

              analyzer.py

                    |

                    v

        Frequency Band Energy

                    |

                    v

              filters.py

                    |

                    v

          Processed Audio Levels

                    |

          +---------+---------+

          |                   |

          v                   v

 beat_detector.py     Trigger Logic

          |                   |

          +---------+---------+

                    |

                    v

            broadcaster.py

                    |

                    v

                 MQTT

                    |

                    v

          Lighting Controller
```