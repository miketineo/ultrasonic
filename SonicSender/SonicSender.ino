#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include "arduino_secrets.h"

// connection  data 
/*const char* SONIC_WIFI_SSID       = "TIM-23263792";*/
/*const char* SONIC_WIFI_PASSWORD   = "yhJC2hErpEZsJ24MYXpkpy2h";*/
/*const char* SONIC_MQTT_HOST  = "10.0.0.43";*/
/*const int   SONIC_MQTT_PORT  = 1883;*/
/*const char* SONIC_MQTT_OUT_TOPIC = "pump-project-sensors";*/
/*const char* SONIC_MQTT_IN_TOPIC = "pump-states";*/
/*const char* SONIC_WIFI_SSID       = SONIC_WIFI_SONIC_WIFI_SSID*/
/*const char* SONIC_WIFI_PASSWORD   = SONIC_WIFI_SONIC_WIFI_PASSWORD*/
/*const char* SONIC_MQTT_HOST  = SONIC_SONIC_MQTT_HOST*/
/*const int   SONIC_MQTT_PORT  = SONIC_SONIC_MQTT_PORT*/
/*const char* SONIC_MQTT_OUT_TOPIC = SONIC_SONIC_MQTT_OUT_TOPIC*/
const char* PUMP_ON = "1";
const char* PUMP_OFF = "0";

// WIFI Stuff
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE	(50)
char msg[MSG_BUFFER_SIZE];
int value = 0;

// Sensors Variables
const int trigPin = 12;
const int echoPin = 13;
const int ledPin = 2, ledPin2 = 10;
const int buzzPin = 0;
const int relais = 16;
const int MIN_LEVEL = 100; // 20cm from the top
const int MAX_LEVEL = 20;  // 100cm from the top
const int TIMEOUT = 100000;
const int MAX_RETRIES = 3;
unsigned long fill_start = 0;
unsigned long fill_time = 0;
bool relay_state = false;
int retries = 0;
int lastSend;

// Ultrasound stuff
//define sound velocity in cm/uS
#define SOUND_VELOCITY 0.03
long duration;
float distanceCm;

//Initialization of I2C LCD display (address 0x27 in this case)
//If is not your i2c address please see at
//https://osoyoo.com/2014/12/07/16x2-i2c-liquidcrystal-displaylcd/
LiquidCrystal_I2C lcd(0x27, 20, 4);
String lcdMessage = "";

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SONIC_WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(SONIC_WIFI_SSID, SONIC_WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish(SONIC_MQTT_OUT_TOPIC, "hello world");
      // ... and resubscribe
      client.subscribe(SONIC_MQTT_IN_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200); // Starts the serial communication
  pinMode(trigPin, OUTPUT); // Sets the trigPin as an Output
  pinMode(echoPin, INPUT); // Sets the echoPin as an Input
  pinMode(ledPin, OUTPUT);
  pinMode(ledPin2, OUTPUT);
  pinMode(buzzPin, OUTPUT);
  pinMode(relais, OUTPUT);
  //WiFi.init(&Serial);
  lastSend = 0;
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Starting...");
  pinSet();

  // WiFi SetUp
  setup_wifi();
  client.setServer(SONIC_MQTT_HOST, SONIC_MQTT_PORT);
  client.setCallback(callback);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  Serial.println("Retries: " + (String) retries);

  if (retries >= MAX_RETRIES) {
    digitalWrite(relais, LOW); // Relay off
    Serial.println("ERROR: Max retries exceeded");
    lcd.clear();
    lcd.setCursor(0, 1); 
    lcd.print("Pump:" + (String) (relay_state ? "ON" : "OFF"));
    delay(1000);
    return;
  }

  //pinSet();
  trigSet();
  if (fill_start) {
    fill_time = millis() - fill_start;
  }

  
  // Reads the echoPin, returns the sound wave travel time in microseconds
  duration = pulseIn(echoPin, HIGH);
  printDistance(duration);

  if (!relay_state && distanceCm > MIN_LEVEL) {
    // Rel ON - Fill Time start
    onRelais();
    relay_state = true;
    fill_start = millis();
    printState();
  }

  if (relay_state && ( fill_time > TIMEOUT || distanceCm < MAX_LEVEL)) {
    if (fill_time > TIMEOUT) {
      retries += 1;
    } else {
      fill_start = 0;
      retries = 0;  
    }
    
    offRelais();
    fill_time = 0;
    relay_state = false;
    printState();
  }

  

//  while(distanceCm > MIN_LEVEL){
//    onRelais();
//    }
//  offRelais();
//  if(distanceCm < 20){
//    offRelais();
//    }
//  else if(distanceCm > 100){
//    onRelais();
//   } 

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    snprintf (msg, MSG_BUFFER_SIZE, "Distance #%f", distanceCm);
    Serial.print("Publishing distance: ");
    Serial.println(msg);
    client.publish(SONIC_MQTT_OUT_TOPIC, msg);
  }

  delay(250); // Sampling rate
}

void printState() {
  lcd.setCursor(0, 2); 
  lcd.print("Pump:" + (String) (relay_state ? "ON" : "OFF"));
  lcd.setCursor(0, 3);
  lcd.print("Filling Time: " + (String) (fill_time / 1000) + "s"); // /1000 to seconds
 }

void printDistance(long duration){
  // Calculate the distance
  distanceCm = duration * SOUND_VELOCITY/2;
    
  // Prints the distance on the Serial Monitor
  Serial.print("Distance (cm): ");
  Serial.println(distanceCm);
  //print to lcd display
  lcd.clear();
  lcd.setCursor(0, 0); 
  lcd.print("Distance: " + (String)distanceCm);
  printState();
  }

void trigSet(){
   // Clears the trigPin
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  // Sets the trigPin on HIGH state for 10 micro seconds
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  }

void pinSet(){
  digitalWrite(ledPin, LOW);
  digitalWrite(ledPin2, LOW);
  digitalWrite(buzzPin, LOW);
  // Normally Open configuration, send LOW signal to let current flow
  // (if you're usong Normally Closed configuration send HIGH signal)
  digitalWrite(relais, LOW);
  }

void onRelais(){
    //digitalWrite(ledPin, LOW);
    //digitalWrite(ledPin2, HIGH);
    //digitalWrite(buzzPin, LOW);
    digitalWrite(relais, HIGH);
    client.publish(SONIC_MQTT_IN_TOPIC, PUMP_ON);
    lcd.setCursor(0, 1); 
    lcd.print("Empty: " + (String)distanceCm);
    //delay(15000);     
  }

void offRelais(){
    //digitalWrite(ledPin, HIGH);
    //digitalWrite(ledPin2, LOW);
    //digitalWrite(buzzPin, HIGH);
    client.publish(SONIC_MQTT_IN_TOPIC, PUMP_OFF);
    digitalWrite(relais, LOW);
    lcd.setCursor(0, 1); 
    lcd.print("Full: " + (String)distanceCm);
    }
