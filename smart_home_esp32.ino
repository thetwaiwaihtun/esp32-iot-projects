#include <WiFi.h>
#include <WebServer.h>
#include <ESP32Servo.h>

// ================= WIFI =================
const char* ssid = "IOT";
const char* password = "mfuiot2023";

WebServer server(80);

// ================= PINS =================
#define SOUND_PIN 32
#define LED_PIN 2

#define SWITCH_LED 27
#define SWITCH_GATE 26
#define SWITCH_DOOR 33

// Ultrasonic
#define TRIG_GATE 5
#define ECHO_GATE 18

#define TRIG_DOOR 16
#define ECHO_DOOR 17

// Servo
#define SERVO_GATE 13
#define SERVO_DOOR 14

#define DIST_THRESHOLD 10

// ================= OBJECTS =================
Servo servoGate;
Servo servoDoor;

// ================= STATES =================
bool ledState = false;
bool gateState = false;
bool doorState = false;

bool manualGate = false;

// ================= TIMERS =================
unsigned long lastClap = 0;
unsigned long lastUltra = 0;
unsigned long manualTimer = 0;
unsigned long doorTimer = 0;

// ================= CLAP =================
int clapCount = 0;

// ================= FUNCTIONS =================

// LED
void setLED(bool state){
  if(state == ledState) return;
  ledState = state;
  digitalWrite(LED_PIN, state);
  Serial.println(state ? "LED ON" : "LED OFF");
}

// GATE
void setGate(bool open){
  if(open == gateState) return;
  gateState = open;
  servoGate.write(open ? 90 : 0);
  Serial.println(open ? "GATE OPEN" : "GATE CLOSE");
}

// DOOR
void setDoor(bool open){
  if(open == doorState) return;
  doorState = open;
  servoDoor.write(open ? 90 : 0);
  Serial.println(open ? "DOOR OPEN" : "DOOR CLOSE");
}

// ================= DISTANCE =================
long readDistance(int trig, int echo){
  digitalWrite(trig, LOW);
  delayMicroseconds(2);

  digitalWrite(trig, HIGH);
  delayMicroseconds(10);
  digitalWrite(trig, LOW);

  long duration = pulseIn(echo, HIGH, 15000);
  long distance = duration * 0.034 / 2;

  if(distance < 2 || distance > 400) return -1;
  return distance;
}

// ================= WEB =================
void root(){
  server.send(200, "text/plain", "ESP32 SMART HOME READY");
}

void lampOn(){
  setLED(true);
  server.send(200, "text/plain", "OK");
}

void lampOff(){
  setLED(false);
  server.send(200, "text/plain", "OK");
}

// GATE WEB CONTROL (FIXED)
void parkingOpen(){
  manualGate = true;
  manualTimer = millis();
  setGate(true);
  Serial.println("WEB: PARKING OPEN");
  server.send(200, "text/plain", "OK");
}

void parkingClose(){
  manualGate = true;
  manualTimer = millis();
  setGate(false);
  Serial.println("WEB: PARKING CLOSE");
  server.send(200, "text/plain", "OK");
}

// DOOR WEB CONTROL
void doorOpenCmd(){
  setDoor(true);
  doorTimer = millis();
  server.send(200, "text/plain", "OK");
}

void doorCloseCmd(){
  setDoor(false);
  server.send(200, "text/plain", "OK");
}

// ================= SWITCH =================
void checkSwitch(){
  static unsigned long debounce = 0;
  if(millis() - debounce < 300) return;

  if(digitalRead(SWITCH_LED) == LOW){
    setLED(!ledState);
    debounce = millis();
  }

  if(digitalRead(SWITCH_GATE) == LOW){
    manualGate = true;
    manualTimer = millis();
    setGate(!gateState);
    debounce = millis();
  }

  if(digitalRead(SWITCH_DOOR) == LOW){
    setDoor(!doorState);
    doorTimer = millis();
    debounce = millis();
  }
}

// ================= CLAP =================
void checkClap(){
  static int last = LOW;
  int sound = digitalRead(SOUND_PIN);

  if(sound == HIGH && last == LOW){
    if(millis() - lastClap > 200){
      clapCount++;
      lastClap = millis();
    }
  }
  last = sound;

  if(millis() - lastClap > 800 && clapCount > 0){
    if(clapCount == 1) setLED(true);
    else if(clapCount == 2) setLED(false);
    clapCount = 0;
  }
}

// ================= ULTRASONIC =================
void checkUltrasonic(){

  if(millis() - lastUltra < 100) return;
  lastUltra = millis();

  long dGate = readDistance(TRIG_GATE, ECHO_GATE);
  long dDoor = readDistance(TRIG_DOOR, ECHO_DOOR);

  // GATE AUTO (ONLY when not manual)
  if(!manualGate && dGate != -1){
    if(dGate < DIST_THRESHOLD) setGate(true);
    else setGate(false);
  }

  // DOOR AUTO
  if(dDoor != -1 && dDoor < DIST_THRESHOLD){
    setDoor(true);
    doorTimer = millis();
  }
}

// ================= AUTO =================
void autoCloseDoor(){
  if(doorState && millis() - doorTimer > 3000){
    setDoor(false);
  }
}

// RESET MANUAL MODE
void resetManual(){
  if(manualGate && millis() - manualTimer > 5000){
    manualGate = false;
    Serial.println("AUTO MODE ENABLED");
  }
}

// ================= SETUP =================
void setup(){
  Serial.begin(115200);

  pinMode(SOUND_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);

  pinMode(SWITCH_LED, INPUT_PULLUP);
  pinMode(SWITCH_GATE, INPUT_PULLUP);
  pinMode(SWITCH_DOOR, INPUT_PULLUP);

  pinMode(TRIG_GATE, OUTPUT);
  pinMode(ECHO_GATE, INPUT);

  pinMode(TRIG_DOOR, OUTPUT);
  pinMode(ECHO_DOOR, INPUT);

  servoGate.attach(SERVO_GATE);
  servoDoor.attach(SERVO_DOOR);

  setGate(false);
  setDoor(false);

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");

  while(WiFi.status() != WL_CONNECTED){
    delay(300);
    Serial.print(".");
  }

  Serial.println("\nConnected!");
  Serial.println(WiFi.localIP());

  // ROUTES
  server.on("/", root);
  server.on("/lampOn", lampOn);
  server.on("/lampOff", lampOff);
  server.on("/parkingOpen", parkingOpen);
  server.on("/parkingClose", parkingClose);
  server.on("/doorOpen", doorOpenCmd);
  server.on("/doorClose", doorCloseCmd);

  server.begin();
}

// ================= LOOP =================
void loop(){
  server.handleClient();

  checkSwitch();
  checkClap();
  checkUltrasonic();

  autoCloseDoor();
  resetManual();
}