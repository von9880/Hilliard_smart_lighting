#include <FastLED.h>
#include <WiFi.h> 
#include <PubSubClient.h>

// ==========================================
// 1. SYSTEM CONFIGURATION & HARDWARE SETTINGS
// ==========================================
#define NUM_LEDS 144              // Total physical LED count
#define LEDS_PER_SEGMENT 48      // Desired number of LEDs per segment
#define DATA_PIN 4                // Pin output to LED strip data line

// Non-blocking Animation Limits
#define MAX_PULSES 15             // Maximum simultaneous tracked active pulses

// Network Credentials
const char *ssid          = "HCL Interns"; 
const char *password      = "Interns123!"; 

// MQTT Broker Settings
const char *mqtt_broker   = "192.168.60.6"; 
const int   mqtt_port     = 1883;           
const char *mqtt_username = "RGB1";         
const char *mqtt_password = "HilliardRGB#1"; 

// Topic Subscriptions
const char *mqtt_topic        = "lights/RGB/box1"; 
const char *speed_topic       = "lights/RGB/speed";
const char *brightness_topic  = "lights/RGB/brightness";
const char *will_topic        = "lights/RGB/box1/connection"; 

bool invertRedGreen = true; //switch red and green (seed lights)
//bool invertRedGreen = false; //(LED strip lights)

int customColor[3];

// ==========================================
// 2. COMPILE-TIME AUTOMATIC SAFETY MATH
// ==========================================
#define NUM_CENTERS       (NUM_LEDS / LEDS_PER_SEGMENT) 
#define LEFTOVER_LEDS     (NUM_LEDS % LEDS_PER_SEGMENT)






// ==========================================
// 3. GLOBAL VARIABLES & STATE TRACKING
// ==========================================
CRGB leds[NUM_LEDS];
int centers[NUM_CENTERS];         
int collisionDistance = 0;        
unsigned long lastFrameTime = 0;


int ledSpeed = 15;                // Tracks your animation timing frame window
int currentBrightness = 50;      // Tracks dynamic strip intensity limits

// Pulse Tracking Parallel Arrays
int pulsePositions[MAX_PULSES];
CRGB pulseColors[MAX_PULSES]; 

// Network Client Global Objects
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// Forward Declarations
void connectToWiFi();
void connectToMQTTBroker();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void handleModeMessage(String message);
void handleSpeedMessage(String message);
void handleBrightnessMessage(String message);
void spawnPulse(CRGB targetColor);
void setLedSafe(int index, CRGB color);
void drawPatterns(int pulseIndex, int distance);
void updateAnimations();






// ==========================================
// 4. MAIN SETUP AND INITIALIZATION
// ==========================================
void setup() {
  Serial.begin(9600);
  delay(1000); 
  
  // FastLED Setup
  if (invertRedGreen){ // inverted RGB setup
     FastLED.addLeds<WS2812B, DATA_PIN, RGB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  }
  else{ //origional RGB setup
    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip); 
  }
  FastLED.setBrightness(currentBrightness);
  FastLED.clear();
  FastLED.show();

  // Run Calculations and Spacing Setup
  collisionDistance = LEDS_PER_SEGMENT / 2 + 1;
  for (int i = 0; i < NUM_CENTERS; i++) {
    centers[i] = (LEDS_PER_SEGMENT * i) + (LEDS_PER_SEGMENT / 2);
  }

  // Initialize all tracking animation slots to inactive
  for (int i = 0; i < MAX_PULSES; i++) {
    pulsePositions[i] = -1;
    pulseColors[i] = CRGB::Black;
  }

  // Network Provisioning
  connectToWiFi();
  mqtt_client.setServer(mqtt_broker, mqtt_port);
  mqtt_client.setCallback(mqttCallback); // Pointing library to unified routing gate
  connectToMQTTBroker();
}






// ==========================================
// 5. MAIN CORE EXECUTION LOOP
// ==========================================
void loop() {
  if (!mqtt_client.connected()) {
    connectToWiFi(); 
    connectToMQTTBroker();
  }
  mqtt_client.loop(); 

  // Process Animation Frames
  unsigned long currentTime = millis();
  if (currentTime - lastFrameTime >= (unsigned long)ledSpeed) {
    lastFrameTime = currentTime;
    updateAnimations();
  }
}






// ==========================================
// 6. ANIMATION ENGINE & PATTERN RECTIFIERS
// ==========================================
void updateAnimations() {
  fadeToBlackBy(leds, NUM_LEDS, 50); 

  for (int i = 0; i < MAX_PULSES; i++) {
    int dist = pulsePositions[i];
    if (dist == -1) continue; 

    drawPatterns(i, dist);
    pulsePositions[i]++;

    if (pulsePositions[i] >= collisionDistance) {
      pulsePositions[i] = -1; 
    }
  }
  FastLED.show();
}

void drawPatterns(int pulseIndex, int distance) {
  CRGB drawColor = pulseColors[pulseIndex];

  for (int c = 0; c < NUM_CENTERS; c++) {
    int centerPos = centers[c];
    /*New Color Template (also update handleModeMessage and node-RED)
    else if (drawColor == CRGB::[color]) {
      CRGB [color]Color = CHSV(hue, saturation , random8(200, 255));
      setLedSafe(centerPos + distance, [color]Color);
      setLedSafe(centerPos - distance, [color]Color);
    }*/ 
    if (drawColor == CRGB(0,0,0)) {
      uint8_t hue = distance * 8; 
      CRGB rainbowColor = CHSV(hue, 255, 255);
      setLedSafe(centerPos + distance, rainbowColor);
      setLedSafe(centerPos - distance, rainbowColor);
    } 
    else if (drawColor == CRGB(255,255,255)) {
      CRGB rightColor = (distance % 2 == 0) ? CRGB::Red : CRGB::Green;
      CRGB leftColor  = (distance % 2 == 0) ? CRGB::Green : CRGB::Red;
      setLedSafe(centerPos + distance, rightColor);
      setLedSafe(centerPos - distance, leftColor);
    } 
    else if (drawColor == CRGB::BlueViolet) {
      uint8_t oceanHue = 130 + (distance % 4) * 10; 
      CRGB oceanColor = CHSV(oceanHue, 240, 255);
      setLedSafe(centerPos + distance, oceanColor);
      setLedSafe(centerPos - distance, oceanColor);
    } 
    else if (drawColor == CRGB::DarkOrange) {
      CRGB fireColor = CHSV(random8(0, 32), 255, random8(180, 255));
      setLedSafe(centerPos + distance, fireColor);
      setLedSafe(centerPos - distance, fireColor);
    } 
    else if (drawColor == CRGB::DarkGreen) {
      CRGB grassColor = CHSV(64 + (distance % 4) * 8, 255, random8(200, 255));
      setLedSafe(centerPos + distance, grassColor);
      setLedSafe(centerPos - distance, grassColor);
    } 
    else if (drawColor == CRGB::HotPink) {
      CRGB pinkColor = CHSV(230, 255 , random8(200, 255));
      setLedSafe(centerPos + distance, pinkColor);
      setLedSafe(centerPos - distance, pinkColor);
    } 
    else if (drawColor == CRGB::LightBlue) {
      CRGB lBlueColor = CHSV(127, 255 , random8(200, 255));
      setLedSafe(centerPos + distance, lBlueColor);
      setLedSafe(centerPos - distance, lBlueColor);
    } 
    
    else if (drawColor == CRGB::Brown) { //custom color (brown is placeholder)
      CRGB customColorRGB = CHSV(customColor[0], customColor[1], customColor[2]);
      setLedSafe(centerPos + distance, customColorRGB);
      setLedSafe(centerPos - distance, customColorRGB);
    }
    else {
      setLedSafe(centerPos + distance, drawColor);
      setLedSafe(centerPos - distance, drawColor);
    }
  }
}

void spawnPulse(CRGB targetColor) {
  for (int i = 0; i < MAX_PULSES; i++) {
    if (pulsePositions[i] == -1) {
      pulsePositions[i] = 0; 
      pulseColors[i] = targetColor; 
      return;
    }
  }
}

void setLedSafe(int index, CRGB color) {
  if (index >= 0 && index < NUM_LEDS) {
    leds[index] = color;
  }
}






// ==========================================
// 7. NETWORK & COMMUNICATIONS PROTOCOLS
// ==========================================

// UNIFIED HANDLER: Receives ALL topic payloads and filters them by string matching
void mqttCallback(char *topic, byte *payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char) payload[i]; 
  }

  String topicStr = String(topic);

  if (topicStr == mqtt_topic) {
    handleModeMessage(message);
  } 
  else if (topicStr == speed_topic) {
    handleSpeedMessage(message);
  } 
  else if (topicStr == brightness_topic) {
    handleBrightnessMessage(message);
  }
}


void handleModeMessage(String message) {
  if (message == "1")       spawnPulse(CRGB::DarkOrange);
  else if (message == "2")  spawnPulse(CRGB::DarkGreen);
  else if (message == "3")  spawnPulse(CRGB::BlueViolet);
  else if (message == "4")  spawnPulse(CRGB::Black); 
  else if (message == "5")  spawnPulse(CRGB::White);
  else if (message.substring(0, 3) = "{\"h"){ //inputting custom colors from node-RED in this format: hsv(hue, saturation%, value%)
    message = message.substring(5);
    Serial.println(message);
    int firstCommaIndex = message.indexOf(",");
    customColor[0] = message.substring(0, firstCommaIndex).toInt() * 255 / 360;
    Serial.println(customColor[0]);
    message = message.substring(firstCommaIndex + 5);
    Serial.println(message);
    int secondCommaIndex = message.indexOf(",");
    customColor[1] = message.substring(0, secondCommaIndex).toInt() * 255 / 100;
    Serial.println(customColor[1]);
    customColor[2] = message.substring(secondCommaIndex + 5, message.length() - 1).toInt() * 255 / 100;
    Serial.println(customColor[2]);
    spawnPulse(CRGB::Brown); //temporary color that we WILL NOT USE for anything else (it crashes the ESP if we dont do this)
  }
}
//{"h":205.20000000000002,"s":49,"v":100}

void handleSpeedMessage(String message) {
  int parsedSpeed = message.toInt(); // Fixed: safely parses numeric text value strings
  ledSpeed = parsedSpeed;
}

void handleBrightnessMessage(String message) {
  int parsedBrightness = message.toInt();
  if (parsedBrightness >= 0 && parsedBrightness <= 255) {
    currentBrightness = parsedBrightness;
    FastLED.setBrightness(currentBrightness); // Refreshes global hardware matrix registers
  }
}


void connectToWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
  }
  Serial.println("WiFi Infrastructure Link Established.");
}


void connectToMQTTBroker() {
  while (!mqtt_client.connected()) {
    String client_id = "esp32-client-" + String(WiFi.macAddress());
    
    if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password, will_topic, 2, true, "Disconnected" )) {
      Serial.println("MQTT Connection Handshake Complete.");
      mqtt_client.subscribe(mqtt_topic); 
      mqtt_client.subscribe(speed_topic); 
      mqtt_client.subscribe(brightness_topic); 
      mqtt_client.publish(will_topic, "Connected");
    } else {
      delay(5000);
    }
  }
}
