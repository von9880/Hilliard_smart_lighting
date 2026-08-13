#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <RBDdimmer.h>

#define zerocross  13  //D7
//PWM 1
#define outputPin1  3  //RX
//PWM 2
#define outputPin2  14 //D5
//Pwm 3
#define outputPin3  4  //D2

dimmerLamp dimmer1(outputPin1, zerocross);
dimmerLamp dimmer2(outputPin2, zerocross);
dimmerLamp dimmer3(outputPin3, zerocross);

int twinkleMap = 55;
int flashMap = 30;
int speed = 2;

// WiFi settings
const char *ssid = "HCL Interns";                     // WiFi name
const char *password = "Interns123!";                 // WiFi password

// MQTT Broker settings
const char *mqtt_broker = "192.168.60.6";             // MQRR broker IP
const char *mqtt_topic = "lights/AC/box1";            // Needs to be changed with box number
const char *will_topic = "lights/AC/box1/connection"; // Needs to be changed with box number

const char *mqtt_username = "AC1";                    // MQTT usernam
const char *mqtt_password = "HilliardAC#1";           // MQTT password
const int mqtt_port = 1883;                           // MQTT port

WiFiClient espClient;
PubSubClient mqtt_client(espClient);

void connectToWiFi();

void connectToMQTTBroker();

void mqttCallback(char *topic, byte *payload, unsigned int length);

void mode1();

void setup() {
    dimmer1.begin(NORMAL_MODE, ON);
    dimmer2.begin(NORMAL_MODE, ON);
    dimmer3.begin(NORMAL_MODE, ON);
    connectToWiFi();
    mqtt_client.setServer(mqtt_broker, mqtt_port);
    mqtt_client.setCallback(mqttCallback);
    connectToMQTTBroker();
}

void connectToWiFi() {
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
    }
    Serial.println("\nConnected to the WiFi network");
}

void connectToMQTTBroker() {
    while (!mqtt_client.connected()) {
        String client_id = "esp8266-client-" + String(WiFi.macAddress());
        if (mqtt_client.connect(client_id.c_str(), mqtt_username, mqtt_password, will_topic, 2, true, "Disconnected")) {
            mqtt_client.subscribe(mqtt_topic);
            mqtt_client.publish(will_topic, "Connected");
        } else {
            delay(2000);
        }
    }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
    String message;
    for (int i = 0; i < length; i++) {
       message += (char)payload[i];
    }
    int intMessage = message.toInt();
    if (intMessage == 1){
        mode1();
    }
    else if (intMessage == 2){
        mode2();
    }
}

void mode1 (){ //Twinkle
	for (int i = twinkleMap; i < 100; i += speed){
		dimmer1.setPower(round(i));
        dimmer2.setPower(round(i));
        dimmer3.setPower(round(i));
	}
	for (int i = 100; i > twinkleMap; i -= speed){
		dimmer1.setPower(round(i));
        dimmer2.setPower(round(i));
        dimmer3.setPower(round(i));
	}
}

void mode2 (){ //Flash
	for (int i = flashMap; i < 100; i += speed){
		dimmer1.setPower(round(i));
        dimmer2.setPower(round(i));
        dimmer3.setPower(round(i));
	}
	for (int i = 100; i > flashMap; i -= speed){
		dimmer1.setPower(round(i));
        dimmer2.setPower(round(i));
        dimmer3.setPower(round(i));
	}
}


void loop() {
    if (!mqtt_client.connected()) {
        connectToMQTTBroker();
    }
    mqtt_client.loop();
}