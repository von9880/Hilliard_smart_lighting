#include <FastLED.h>
#include <WiFi.h> 
#include <PubSubClient.h>
#include "time.h"
#include <ESP32Time.h>
#include "millisDelay.h"


// ==========================================
// 1. SYSTEM CONFIGURATION & HARDWARE SETTINGS
// ==========================================
#define NUM_LEDS 200         // Total LED count
#define LEDS_PER_SEGMENT 25      // Number of LEDs per segment
#define DATA_PIN 4                // Output  to LED strip data line

// Non-blocking Animation Limits
#define MAX_PULSES 15            // Maximum simultaneous tracked active pulses

// Network Credentials
const char* ssid = "HCL Interns";
const char* password = "Interns123!";

// MQTT Broker Settings
const char *mqtt_broker   = "192.168.60.6"; 
const int   mqtt_port     = 1883;           
const char *mqtt_username = "RGB6"; //Change with box #        
const char *mqtt_password = "HilliardRGB#6"; //Change with box #   

// Topic Subscriptions
const char *mqtt_topic        = "lights/RGB/box6"; //Change with box #   
const char *speed_topic       = "lights/RGB/speed";
const char *brightness_topic  = "lights/RGB/brightness";
const char *will_topic        = "lights/RGB/box6/connection"; //Change with box #   

// TTime syncing
ESP32Time rtc(0);
const char* ntpServer = "pool.ntp.org";
unsigned long Epoch_Time; 
unsigned long New_Epoch_Time;

// Delay backolog
String delayBacklog[] = {"-1", "-1", "-1", "-1", "-1"};
String triggerBacklog[] = {"-1", "-1", "-1", "-1", "-1"};
int backlogCount = 0;
int executeCount = 0;

bool invertRedGreen = true; //switch red and green (seed lights)
//bool invertRedGreen = false; //(LED strip lights)

int customColor[3]; //array to store HSV codes for custom colors

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
  // Time syncing
  configTime(0, 0, ntpServer);
  Epoch_Time = Get_Epoch_Time() + 1; 
  New_Epoch_Time = Get_Epoch_Time();
  while (New_Epoch_Time != Epoch_Time){//breaks the loop when the second "ticks over"
    delay(1);
    New_Epoch_Time = Get_Epoch_Time();
  }
  Serial.println("Time set");
  rtc.setTime(New_Epoch_Time);
  Serial.println("Setup Complete");
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
  processDelays();
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

void drawPatterns(int pulseIndex, int distance) { //defines each color mode
  CRGB drawColor = pulseColors[pulseIndex];

  for (int c = 0; c < NUM_CENTERS; c++) {
    int centerPos = centers[c];
    /*New Color Template (also update processDelay() and Node-RED to match)
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
      CRGB rightColor = (distance % 2 == 0) ? CRGB::Red : CRGB::Lime;
      CRGB leftColor  = (distance % 2 == 0) ? CRGB::Lime : CRGB::Red;
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

unsigned long Get_Epoch_Time() { //gets the time (to the second) over WIFI
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return(0);
  }
  time(&now);
  return now;
}



void mqttCallback(char *topic, byte *payload, unsigned int length) { //recives MQTT messages and directs them to specific functions based on their topic
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


void handleModeMessage(String message) { // adds triggers and delays to backlog
  int delayMs = message.substring(message.length() - 5).toInt();  
  delayBacklog[backlogCount] = delayMs;
  triggerBacklog[backlogCount] = message;
  backlogCount ++;
  if(backlogCount == 5){
    backlogCount = 0;
  }
}

void processDelays(){ //excecutes trigger if within x ms of timestamp
  for (int i = 0; i < 5; i ++){
    if (triggerBacklog[i].equals("-1")){
      continue;
    }
    unsigned long currentMillis = rtc.getMillis();
    if (abs(delayBacklog[i].toInt() - currentMillis) % 1000 < 100){ //if the time is within x Ms of the timestamp
      if (triggerBacklog[i].substring(0, 1) == "1")       spawnPulse(CRGB::DarkOrange);
      else if (triggerBacklog[i].substring(0, 1) == "2")  spawnPulse(CRGB::DarkGreen);
      else if (triggerBacklog[i].substring(0, 1) == "3")  spawnPulse(CRGB::BlueViolet);
      else if (triggerBacklog[i].substring(0, 1) == "4")  spawnPulse(CRGB::Black); 
      else if (triggerBacklog[i].substring(0, 1) == "5")  spawnPulse(CRGB::White);
      else if (triggerBacklog[i].substring(0, 3) = "{\"h"){ //inputting custom colors from node-RED in this format: "{"h":205.20000000000002,"s":49,"v":100}"
        String shortenedMessage = triggerBacklog[i].substring(5);
        int firstCommaIndex = shortenedMessage.indexOf(",");
        customColor[0] = shortenedMessage.substring(0, firstCommaIndex).toInt() * 255 / 360;
        shortenedMessage = shortenedMessage.substring(firstCommaIndex + 5);
        int secondCommaIndex = shortenedMessage.indexOf(",");
        customColor[1] = shortenedMessage.substring(0, secondCommaIndex).toInt() * 255 / 100;
        customColor[2] = shortenedMessage.substring(secondCommaIndex + 5, shortenedMessage.length() - 1).toInt() * 255 / 100;
        spawnPulse(CRGB::Brown); //spawnPulse requires an argument
      }
      triggerBacklog[i] = "-1"; //Removes trigger from backlogs
      delayBacklog[i] = "-1";
    }
    delay(1);
  }
}

void handleSpeedMessage(String message) { // updates speed 
  int parsedSpeed = message.toInt(); 
  parsedSpeed = 50 - parsedSpeed / 2; // maps the Node-RED message to a usable range of speeds
  ledSpeed = parsedSpeed;
}


void handleBrightnessMessage(String message) { // updates brightness
  int parsedBrightness = message.toInt();
  if (parsedBrightness >= 0 && parsedBrightness <= 100) {
    currentBrightness = parsedBrightness;
    FastLED.setBrightness(currentBrightness); 
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



