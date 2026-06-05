/*
 * config.h - Pin definitions and system configuration
 * Industrial Sensor Monitor - Mohammed Satar
 */
#pragma once

// ─── SENSOR PIN ASSIGNMENTS ─────────────────────────────
#define PIN_TEMP     4    // DS18B20 OneWire data pin
#define PIN_PRESSURE 34   // 4-20mA pressure input (ADC)
#define PIN_FLOW     18   // YF-S201 flow sensor pulse pin
#define PIN_TRIG     5    // HC-SR04 ultrasonic trigger
#define PIN_ECHO     19   // HC-SR04 ultrasonic echo

// ─── CALIBRATION VALUES ──────────────────────────────────
#define PRESSURE_MAX_BAR         10.0   // Full-scale pressure (bar)
#define FLOW_CALIBRATION_FACTOR  7.5    // Pulses/sec per L/min
#define TANK_HEIGHT_CM           100.0  // Total tank height in cm

// ─── TIMING CONFIGURATION ───────────────────────────────
#define SAMPLE_INTERVAL_MS   500   // Sensor read every 500ms
#define SEND_INTERVAL_MS     1000  // Serial TX every 1000ms

// ─── SYSTEM INFO ────────────────────────────────────────
#define FIRMWARE_VERSION  "1.0.0"
#define DEVICE_ID         "ISM-001"
