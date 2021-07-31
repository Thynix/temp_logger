#include <SPI.h>
#include <SD.h>
#include "Adafruit_Si7021.h"
#include "uTimerLib.h"
#include "ArduinoLowPower.h"

// Pins
const int serial_enable_pin = 6;
const int button_pin = 5;
const int chip_select_pin = 4;
const int green_led = 8;
const int red_led = LED_BUILTIN;

// TODO: Read chunk / cluster size from filesystem? Read SD card preferred write size?
const uint64_t write_chunk = 4096;
const uint64_t sensor_seconds = 5;
const unsigned long debounce_microseconds = 20000;

volatile unsigned long button_last_down = 0;
volatile bool button_pressed = false;
volatile bool sensor_read_pending = false;

File dataFile;
Adafruit_Si7021 sensor;

void setup() {
  pinMode(red_led, OUTPUT);
  pinMode(green_led, OUTPUT);

  // Turn on both LEDs during setup.
  digitalWrite(red_led, HIGH);
  digitalWrite(green_led, HIGH);

  pinMode(serial_enable_pin, INPUT);

  // Open serial communications and wait for port to open
  // if the serial enable pin is high. While waiting,
  // blink red then green.
  Serial.begin(9600);
  if (digitalRead(serial_enable_pin) == HIGH) {
    while (!Serial) {
      digitalWrite(green_led, LOW);
      digitalWrite(red_led, HIGH);
      delayMicroseconds(500000);
      digitalWrite(green_led, HIGH);
      digitalWrite(red_led, LOW);
      delayMicroseconds(500000);
    }
  }

  Serial.println("Starting setup");

  // Failed to find MicroSD card: blink green.
  if (!SD.begin(chip_select_pin)) {
    Serial.println("MicroSD card failed, or not present.");

    digitalWrite(red_led, LOW);
    while (true) {
      digitalWrite(green_led, HIGH);
      delayMicroseconds(500000);
      digitalWrite(green_led, LOW);
      delayMicroseconds(500000);
    }
  }

  pinMode(button_pin, INPUT_PULLUP);
  LowPower.attachInterruptWakeup(digitalPinToInterrupt(button_pin), button_down, FALLING);

  // Failed to find sensor: blink red.
  sensor = Adafruit_Si7021();
  if (!sensor.begin()) {
    Serial.println("Si7021 sensor not found.");

    digitalWrite(green_led, LOW);
    while (true) {
      digitalWrite(red_led, HIGH);
      delayMicroseconds(500000);
      digitalWrite(red_led, LOW);
      delayMicroseconds(500000);
    }
  }

  Serial.print("Found sensor model ");
  switch(sensor.getModel()) {
    case SI_Engineering_Samples:
      Serial.print("SI engineering samples"); break;
    case SI_7013:
      Serial.print("Si7013"); break;
    case SI_7020:
      Serial.print("Si7020"); break;
    case SI_7021:
      Serial.print("Si7021"); break;
    case SI_UNKNOWN:
    default:
      Serial.print("Unknown");
  }
  Serial.print(" Rev(");
  Serial.print(sensor.getRevision());
  Serial.print(")");
  Serial.print(" Serial #"); Serial.print(sensor.sernum_a, HEX); Serial.println(sensor.sernum_b, HEX);

  // Failed to open data file: blink red and green at once.
  dataFile = SD.open("datalog.txt", FILE_WRITE);
  if (!dataFile) {
    Serial.println("Failed to open datalog.txt");

    while (true) {
      digitalWrite(green_led, HIGH);
      digitalWrite(red_led, HIGH);
      delayMicroseconds(500000);
      digitalWrite(green_led, LOW);
      digitalWrite(red_led, LOW);
      delayMicroseconds(500000);
    }
  }

  TimerLib.setInterval_s(flag_sensor, sensor_seconds);

  Serial.println("Setup complete");
  Serial.flush();

  // Turn off LEDs to indicate setup finished.
  digitalWrite(red_led, LOW);
  digitalWrite(green_led, LOW);
}

void loop() {
  if (digitalRead(button_pin) == LOW && button_pressed && (micros() - button_last_down) > debounce_microseconds) {
    flush_log();
    button_pressed = false;
  }

  if (sensor_read_pending) {
    log_sensor();
    sensor_read_pending = false;
  }

  LowPower.idle();
}

void log_sensor() {
  static uint64_t previous_flush = 0;

  // <seconds> <temperature> <humidity>\n
  // Blink LED while reading sensor / composing output
  // TODO: More than one sample from sensor.
  digitalWrite(red_led, HIGH);
  dataFile.write(String((float)millis() / 1000).c_str());
  dataFile.write(",");
  dataFile.write(String(sensor.readTemperature()).c_str());
  dataFile.write(",");
  dataFile.write(String(sensor.readHumidity()).c_str());
  dataFile.write("\n");
  digitalWrite(red_led, LOW);

  uint64_t current_position = dataFile.position();
  if (current_position - previous_flush > write_chunk) {
    flush_log();
    previous_flush = current_position;
  }
}

void flush_log() {
  digitalWrite(green_led, HIGH);
  dataFile.flush();
  digitalWrite(green_led, LOW);
}

void button_down() {
  if ((micros() - button_last_down) > debounce_microseconds) {
    button_last_down = micros();
    button_pressed = true;
  }
}

void flag_sensor() {
  sensor_read_pending = true;
}
