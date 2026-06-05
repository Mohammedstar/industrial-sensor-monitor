/*
 * =========================================================
 * INDUSTRIAL SENSOR MONITOR - ESP32 Firmware
 * =========================================================
 * Author:   Mohammed Satar
 * Version:  1.0
 * Hardware: ESP32 DevKit v1
 * 
 * Description:
 *   Reads temperature, pressure, flow, and level sensors.
 *   Displays values on OLED. Sends JSON data over Serial/MQTT.
 *   Implements basic alarm logic (HH/H/L/LL limits).
 * 
 * Career context: Instrumentation & Control Engineering
 * Relevant to: Oil & Gas field instrumentation, process control
 * =========================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <ArduinoJson.h>
#include "config.h"

// ─── OLED Display ───────────────────────────────────────
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ─── DS18B20 Temperature Sensor ─────────────────────────
OneWire oneWire(PIN_TEMP);
DallasTemperature tempSensor(&oneWire);

// ─── Alarm States (ISA-18.2) ────────────────────────────
enum AlarmState { NORMAL, ALERT, WARNING, ALARM_ACTIVE };

struct SensorReading {
  float temperature;   // °C
  float pressure;      // bar
  float flowRate;      // L/min
  float level;         // cm
  AlarmState tempAlarm;
  AlarmState pressAlarm;
  AlarmState flowAlarm;
  AlarmState levelAlarm;
};

SensorReading currentReading;
unsigned long lastSampleTime = 0;
unsigned long lastSendTime   = 0;
int displayPage = 0;

// ─── Alarm Limits ────────────────────────────────────────
// Temperature limits (°C)
const float TEMP_HH = 95.0,  TEMP_H = 85.0;
const float TEMP_L  = 20.0,  TEMP_LL = 10.0;
// Pressure limits (bar)
const float PRESS_HH = 7.0,  PRESS_H = 6.0;
const float PRESS_L  = 1.0,  PRESS_LL = 0.5;
// Flow limits (L/min)
const float FLOW_HH = 25.0,  FLOW_H = 20.0;
const float FLOW_L  = 2.0,   FLOW_LL = 1.0;
// Level limits (cm)
const float LEVEL_HH = 90.0, LEVEL_H = 80.0;
const float LEVEL_L  = 15.0, LEVEL_LL = 5.0;

// ─────────────────────────────────────────────────────────
// FUNCTION: Read pressure from 4-20mA via ADS1115 ADC
// 4-20mA -> 0-5V via 250Ω shunt -> ADC 0-1023
// Scaled to 0-10 bar range
// ─────────────────────────────────────────────────────────
float readPressure() {
  int rawADC = analogRead(PIN_PRESSURE);
  // 4mA = 164 counts (at 12-bit: 4/20 * 4095 = 819 for 4mA)
  // Linear interpolation: 4mA->0 bar, 20mA->10 bar
  float milliAmps = (rawADC / 4095.0) * 20.0;
  milliAmps = constrain(milliAmps, 4.0, 20.0);
  float pressure = (milliAmps - 4.0) / 16.0 * PRESSURE_MAX_BAR;
  return pressure;
}

// ─────────────────────────────────────────────────────────
// FUNCTION: Read flow rate from pulse sensor (YF-S201)
// Pulse frequency (Hz) to flow rate (L/min)
// Calibration factor: 7.5 pulses/sec per L/min
// ─────────────────────────────────────────────────────────
volatile int flowPulseCount = 0;
unsigned long flowLastTime = 0;

void IRAM_ATTR flowPulseISR() {
  flowPulseCount++;
}

float readFlowRate() {
  unsigned long elapsed = millis() - flowLastTime;
  if (elapsed >= 1000) {
    float frequency = flowPulseCount / (elapsed / 1000.0);
    float flowRate = frequency / FLOW_CALIBRATION_FACTOR;
    flowPulseCount = 0;
    flowLastTime = millis();
    return flowRate;
  }
  return currentReading.flowRate; // Return last value
}

// ─────────────────────────────────────────────────────────
// FUNCTION: Read level from HC-SR04 ultrasonic sensor
// Distance in cm (tank height - distance = level)
// ─────────────────────────────────────────────────────────
float readLevel() {
  digitalWrite(PIN_TRIG, LOW);
  delayMicroseconds(2);
  digitalWrite(PIN_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_TRIG, LOW);
  
  long duration = pulseIn(PIN_ECHO, HIGH, 30000); // 30ms timeout
  if (duration == 0) return -1; // Sensor error
  
  float distance = duration * 0.0343 / 2.0; // cm
  float level = TANK_HEIGHT_CM - distance;
  return constrain(level, 0, TANK_HEIGHT_CM);
}

// ─────────────────────────────────────────────────────────
// FUNCTION: Evaluate alarm state per ISA-18.2
// Returns: NORMAL, ALERT, WARNING, or ALARM_ACTIVE
// ─────────────────────────────────────────────────────────
AlarmState evaluateAlarm(float value, float ll, float l, float h, float hh) {
  if (value >= hh || value <= ll) return ALARM_ACTIVE;
  if (value >= h  || value <= l)  return WARNING;
  if (value >= h * 0.95 || value <= l * 1.05) return ALERT;
  return NORMAL;
}

// ─────────────────────────────────────────────────────────
// FUNCTION: Send JSON data over Serial (to Python dashboard)
// ─────────────────────────────────────────────────────────
void sendSerialData() {
  StaticJsonDocument<256> doc;
  doc["ts"]    = millis();
  doc["temp"]  = currentReading.temperature;
  doc["press"] = currentReading.pressure;
  doc["flow"]  = currentReading.flowRate;
  doc["level"] = currentReading.level;
  doc["alarms"]["temp"]  = (int)currentReading.tempAlarm;
  doc["alarms"]["press"] = (int)currentReading.pressAlarm;
  doc["alarms"]["flow"]  = (int)currentReading.flowAlarm;
  doc["alarms"]["level"] = (int)currentReading.levelAlarm;
  serializeJson(doc, Serial);
  Serial.println();
}

// ─────────────────────────────────────────────────────────
// FUNCTION: Update OLED display (cycles through pages)
// ─────────────────────────────────────────────────────────
const char* alarmStr(AlarmState s) {
  switch(s) {
    case NORMAL:       return "OK";
    case ALERT:        return "ALT";
    case WARNING:      return "WRN";
    case ALARM_ACTIVE: return "ALM!";
    default:           return "---";
  }
}

void updateDisplay() {
  display.clearDisplay();
  display.setTextColor(WHITE);
  
  // Header
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("SENSOR MONITOR v1.0");
  display.drawLine(0, 9, 127, 9, WHITE);
  
  if (displayPage == 0) {
    // Page 1: Temperature & Pressure
    display.setCursor(0, 12);
    display.print("TEMP: ");
    display.setTextSize(2);
    display.print(currentReading.temperature, 1);
    display.setTextSize(1);
    display.print("C [");
    display.print(alarmStr(currentReading.tempAlarm));
    display.print("]");
    
    display.setCursor(0, 36);
    display.print("PRESS: ");
    display.setTextSize(2);
    display.print(currentReading.pressure, 2);
    display.setTextSize(1);
    display.print("bar");
    
  } else {
    // Page 2: Flow & Level
    display.setCursor(0, 12);
    display.print("FLOW: ");
    display.setTextSize(2);
    display.print(currentReading.flowRate, 1);
    display.setTextSize(1);
    display.print("L/m [");
    display.print(alarmStr(currentReading.flowAlarm));
    display.print("]");
    
    display.setCursor(0, 36);
    display.print("LEVEL: ");
    display.setTextSize(2);
    display.print(currentReading.level, 1);
    display.setTextSize(1);
    display.print("cm");
  }
  
  // Bottom status bar
  display.drawLine(0, 54, 127, 54, WHITE);
  display.setCursor(0, 56);
  display.print("Page ");
  display.print(displayPage + 1);
  display.print("/2  t=");
  display.print(millis() / 1000);
  display.print("s");
  
  display.display();
}

// ─────────────────────────────────────────────────────────
// SETUP
// ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  // Initialize OLED
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("ERROR: OLED init failed");
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(20, 20);
  display.setTextSize(2);
  display.print("INIT...");
  display.display();
  
  // Initialize temperature sensor
  tempSensor.begin();
  
  // Initialize pins
  pinMode(PIN_TRIG, OUTPUT);
  pinMode(PIN_ECHO, INPUT);
  pinMode(PIN_FLOW, INPUT_PULLUP);
  analogReadResolution(12);
  
  // Attach flow pulse interrupt
  attachInterrupt(digitalPinToInterrupt(PIN_FLOW), flowPulseISR, RISING);
  
  delay(2000);
  Serial.println("{\"status\":\"INIT_OK\",\"device\":\"IndustrialSensorMonitor\",\"version\":\"1.0\"}");
}

// ─────────────────────────────────────────────────────────
// MAIN LOOP
// ─────────────────────────────────────────────────────────
void loop() {
  unsigned long now = millis();
  
  // ── Sample sensors every SAMPLE_INTERVAL ms ──
  if (now - lastSampleTime >= SAMPLE_INTERVAL_MS) {
    lastSampleTime = now;
    
    // Read all sensors
    tempSensor.requestTemperatures();
    currentReading.temperature = tempSensor.getTempCByIndex(0);
    currentReading.pressure    = readPressure();
    currentReading.flowRate    = readFlowRate();
    currentReading.level       = readLevel();
    
    // Evaluate alarms
    currentReading.tempAlarm  = evaluateAlarm(currentReading.temperature, TEMP_LL,  TEMP_L,  TEMP_H,  TEMP_HH);
    currentReading.pressAlarm = evaluateAlarm(currentReading.pressure,    PRESS_LL, PRESS_L, PRESS_H, PRESS_HH);
    currentReading.flowAlarm  = evaluateAlarm(currentReading.flowRate,    FLOW_LL,  FLOW_L,  FLOW_H,  FLOW_HH);
    currentReading.levelAlarm = evaluateAlarm(currentReading.level,       LEVEL_LL, LEVEL_L, LEVEL_H, LEVEL_HH);
    
    // Update display
    updateDisplay();
  }
  
  // ── Send data every SEND_INTERVAL ms ──
  if (now - lastSendTime >= SEND_INTERVAL_MS) {
    lastSendTime = now;
    sendSerialData();
  }
  
  // ── Cycle display page every 5 seconds ──
  if ((now / 5000) % 2 != displayPage) {
    displayPage = (now / 5000) % 2;
  }
}
