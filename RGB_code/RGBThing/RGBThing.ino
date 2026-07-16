#include <FastLED.h>
#include <WiFi.h> 
#include <PubSubClient.h>

#define NUM_LEDS 50
#define DATA_PIN 4
#define BRIGHTNESS 100

// Non-blocking Animation Configuration
#define MAX_PULSES 15 
#define FRAME_DELAY 20 

CRGB leds[NUM_LEDS];
String ledState = "O"; 


// Tracking arrays for active pulses
int pulsePositions[MAX_PULSES];
CRGB pulseColors[MAX_PULSES]; 
unsigned long lastFrameTime = 0;
int center = NUM_LEDS / 2;

const char *ssid = "HCL Interns"; 
const char *password = "Interns123!"; 

// MQTT Broker settings
const char *mqtt_broker = "192.168.60.6"; 
const char *mqtt_topic = "lights/RGB/box1"; //Needs to be changed with box number 
const char *will_topic = "lights/RGB/box1/connection"; //Needs to be changed with box number
const char *mqtt_username = "emqx"; 
const char *mqtt_password = "public"; 
const int mqtt_port = 1883; 

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

void connectToWiFi();
void connectToMQTTBroker();
void mqttCallback(char *topic, byte *payload, unsigned int length);

void setup() {
  Serial.begin(9600);
  
  FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  FastLED.clear();
  FastLED.show();

  // Initialize all pulse slots to -1 (inactive)
  for (int i = 0; i < MAX_PULSES; i++) {
    pulsePositions[i] = -1;
    pulseColors[i] = CRGB::Black;
  }

  connectToWiFi();
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback);
  connectToMQTTBroker();
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected to the WiFi network");
}

void connectToMQTTBroker() {
  while (!mqtt_client.connected()) {
    String client_id = "esp32-client-" + String(WiFi.macAddress());
    Serial.printf("Connecting to MQTT Broker as %s.....\n", client_id.c_str());
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password,  will_topic, 2, true, "Disconnected" )) {
      Serial.println("Connected to MQTT broker");
      mqtt_client.subscribe(mqtt_topic); 
      mqtt_client.publish(will_topic, "Connected");
    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqtt_client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

// Spawns a pulse with a specified target color
void spawnPulse(CRGB targetColor) {
  bool spawned = false;
  for (int i = 0; i < MAX_PULSES; i++) {
    if (pulsePositions[i] == -1) {
      pulsePositions[i] = 0; // Set starting distance from center to 0
      pulseColors[i] = targetColor; // Store the color chosen by the MQTT message
      Serial.printf("Pulse spawned in slot %d\n", i);
      spawned = true;
      break; 
    }
  }
  if (!spawned) {
    Serial.println("Warning: Max simultaneous pulses reached!");
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);
  
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char) payload[i]; 
  }
  Serial.print("Message: ");
  Serial.println(message);

  // Trigger different pulse colors based on the payload
  if (message == "1") {
    // Flag DarkOrange for Fire/Ember pattern
    spawnPulse(CRGB::DarkOrange);
  } 
  
  else if (message == "2") {
    // Flag DarkGreen for Grass/Nature pattern
    spawnPulse(CRGB::DarkGreen);spawnPulse(CRGB::Green);
  } 
  
  else if (message == "3") {
    // Flag BlueViolet for Ocean pattern (Shades of blue, cyan, aqua)
    spawnPulse(CRGB::BlueViolet);
  } 
  
  else if (message == "4") {
    // Flag black for rainbow pattern
    spawnPulse(CRGB::Black); 
  } 
  
  else if (message == "5") {
    // Flag white for Christmas pattern (Alternating Red/Green)
    spawnPulse(CRGB::White);
  }
  
  
  Serial.println("-----------------------");
}

void loop() {
  // 1. Maintain MQTT Connection
  if (!mqtt_client.connected()) {
    connectToWiFi(); 
    connectToMQTTBroker();
  }
  mqtt_client.loop(); 

  // 2. Animate LEDs incrementally based on time intervals
  unsigned long currentTime = millis();
  if (currentTime - lastFrameTime >= FRAME_DELAY) {
    lastFrameTime = currentTime;
    
    // Smoothly fade existing trail points downward into darkness
    fadeToBlackBy(leds, NUM_LEDS, 50); 

    // Move and draw each active pulse inside the tracker list
    for (int i = 0; i < MAX_PULSES; i++) {
      int dist = pulsePositions[i];
      
      if (dist != -1) { 
        CRGB drawColor = pulseColors[i];
        
        // Dynamic pattern checking
        if (drawColor == CRGB(0,0,0)) {
          // RAINBOW PATTERN (Flag: Black)
          uint8_t hue = dist * 8; 
          drawColor = CHSV(hue, 255, 255);
          
          if (center + dist < NUM_LEDS) leds[center + dist] = drawColor; 
          if (center - dist >= 0)        leds[center - dist] = drawColor;
          
        } else if (drawColor == CRGB(255,255,255)) {
          // CHRISTMAS PATTERN (Flag: White)
          CRGB rightColor = (dist % 2 == 0) ? CRGB::Red : CRGB::Green;
          CRGB leftColor  = (dist % 2 == 0) ? CRGB::Green : CRGB::Red;
          
          if (center + dist < NUM_LEDS) leds[center + dist] = rightColor; 
          if (center - dist >= 0)        leds[center - dist] = leftColor;
          
        } else if (drawColor == CRGB::BlueViolet) {
          // OCEAN PATTERN (Flag: BlueViolet)
          uint8_t oceanHue = 130 + (dist % 4) * 10; 
          drawColor = CHSV(oceanHue, 240, 255);
          
          if (center + dist < NUM_LEDS) leds[center + dist] = drawColor; 
          if (center - dist >= 0)        leds[center - dist] = drawColor;
          
        } else if (drawColor == CRGB::DarkOrange) {
          // FIRE PATTERN (Flag: DarkOrange)
          // Randomly mixes red (Hue 0) through orange/yellow (Hue 32)
          uint8_t fireHue = random8(0, 32); 
          // Add random brightness variation to simulate a real flicker
          uint8_t fireVal = random8(180, 255); 
          drawColor = CHSV(fireHue, 255, fireVal);
          
          if (center + dist < NUM_LEDS) leds[center + dist] = drawColor; 
          if (center - dist >= 0)        leds[center - dist] = drawColor;
          
        }else if (drawColor == CRGB::DarkGreen) {
          // GRASS PATTERN (Flag: DarkGreen)
          // Shuffles colors in the green range (Hue 64 is mid-green, Hue 96 is yellowish lime green)
          uint8_t grassHue = 64 + (dist % 4) * 8; 
          // Subtle variance in grass brightness to look like swaying blades
          uint8_t grassVal = random8(200, 255); 
          drawColor = CHSV(grassHue, 255, grassVal);
          
          if (center + dist < NUM_LEDS) leds[center + dist] = drawColor; 
          if (center - dist >= 0)        leds[center - dist] = drawColor;
        } 
        else {
          // STATIC SOLID COLOR PATTERNS (Red, Green, Blue)
          if (center + dist < NUM_LEDS) leds[center + dist] = drawColor; 
          if (center - dist >= 0)        leds[center - dist] = drawColor;
        }

        // Advance distance for the next calculation tick
        pulsePositions[i]++;

        // If both fronts clear the ends of the strip, terminate this tracking slot
        if ((center + pulsePositions[i] >= NUM_LEDS) && (center - pulsePositions[i] < 0)) {
          pulsePositions[i] = -1; 
        }
      }
    }

    // Refresh the physical LED lights
    FastLED.show();
  }
}
