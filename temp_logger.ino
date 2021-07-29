#include <SPI.h>
#include <SD.h>
#include "Adafruit_Si7021.h"
#include "uTimerLib.h"
#include "ArduinoLowPower.h"

// Pins
const int buttonPin = 5;
const int chipSelectPin = 4;
const int greenLed = 8;

// TODO: Read chunk / cluster size from filesystem? Read SD card preferred write size?
const uint64_t write_chunk = 4096;
const uint64_t sensor_seconds = 5;
const unsigned long debounce_ms = 50;
const unsigned long button_ms = 200;

volatile bool button_pressed = false;
volatile unsigned long press_timestamp;

File dataFile;
Adafruit_Si7021 sensor;

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(greenLed, OUTPUT);

  // Turn on both LEDs during setup.
  // TODO: Blink codes for errors?
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(greenLed, HIGH);

  // Open serial communications and wait for port to open
  Serial.begin(9600);
  while (!Serial) {
    delay(100); // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("Setting up...");

  if (!SD.begin(chipSelectPin)) {
    Serial.println("MicroSD card failed, or not present.");
    while (true);
  }

  pinMode(buttonPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(buttonPin), button_down, FALLING);
  attachInterrupt(digitalPinToInterrupt(buttonPin), button_up, RISING);

  sensor = Adafruit_Si7021();
  if (!sensor.begin()) {
    Serial.println("Si7021 sensor not found.");
    while (true);
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

  dataFile = SD.open("datalog.txt", FILE_WRITE);
  if (!dataFile) {
    Serial.println("Failed to open datalog.txt");
    while (true);
  }

  TimerLib.setInterval_s(log_sensor, sensor_seconds);

  // Turn off LEDs to indicate setup finished.
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(greenLed, LOW);

  Serial.println("Setup complete");
}

void loop() {
  Serial.println("Checking button");
  // Manual log file flush if the button down survives debouncing.
  if (button_pressed == true && (millis() - press_timestamp) > debounce_ms) {
    flush_log();
    button_pressed = false;
  }

  LowPower.idle(button_ms);
}

void log_sensor() {
  static uint64_t previous_flush = 0;

  // <seconds> <temperature> <humidity>\n
  // Blink LED while reading sensor / composing output
  digitalWrite(LED_BUILTIN, HIGH);
  dataFile.write(String(millis() / 1000).c_str());
  dataFile.write(",");
  dataFile.write(String(sensor.readTemperature()).c_str());
  dataFile.write(",");
  dataFile.write(String(sensor.readHumidity()).c_str());
  dataFile.write("\n");
  digitalWrite(LED_BUILTIN, LOW);

  uint64_t current_position = dataFile.position();
  if (current_position - previous_flush > write_chunk) {
    flush_log();
    previous_flush = current_position;
  }
}

void flush_log() {
  Serial.println("Flushing file");
  digitalWrite(greenLed, HIGH);
  dataFile.flush();
  digitalWrite(greenLed, LOW);
}

void button_down() {
  button_pressed = true;
  press_timestamp = millis();
}

void button_up() {
  button_pressed = false;
}
