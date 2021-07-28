#include <SPI.h>
#include <SD.h>
#include "Adafruit_Si7021.h"

// Pins
const int buttonPin = 5;
const int chipSelectPin = 4;

// TODO: Read chunk / cluster size from filesystem? Read SD card preferred write size?
const uint64_t write_chunk = 4096;
const uint64_t poll_ms = 5000;
const unsigned long debounce_ms = 50;

File dataFile;
Adafruit_Si7021 sensor;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  while (!Serial) {
    delay(100); // wait for serial port to connect. Needed for native USB port only
  }

  Serial.println("Starting up...");

  pinMode(buttonPin, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT);

  if (!SD.begin(chipSelectPin)) {
    Serial.println("MicroSD card failed, or not present.");
    while (true);
  }

  Serial.println("MicroSD card initialized.");

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

  Serial.println("Setup complete");
}

uint64_t previous_flush = 0;
void loop() {
  static unsigned long previous_run = 0;

  // TODO: This'll overflow after 50 days - is it necessary to handle it? Doesn't seem like it - failure mode is previous_run is huge and timestamp is small; it might poll sooner than without the problem but that doesn't seem to matter.
  unsigned long timestamp = millis();
  unsigned long duration = timestamp - previous_run;

  // Manual log flush
  if (button_on_press()) {
    flush_log();
  }

  if (duration < poll_ms) {
    return;
  }

  previous_run = timestamp;

  // <millseconds> <temperature> <humidity>\n
  dataFile.write(String(timestamp).c_str());
  dataFile.write(",");
  dataFile.write(String(sensor.readTemperature(), 2).c_str());
  dataFile.write(",");
  dataFile.write(String(sensor.readHumidity(), 2).c_str());
  dataFile.write("\n");

  uint64_t current_position = dataFile.position();
  if (current_position - previous_flush > write_chunk) {
    flush_log();
    previous_flush = current_position;
  } 
}

void flush_log() {
  Serial.println("Flushing file");
  digitalWrite(LED_BUILTIN, HIGH);
  dataFile.flush();
  digitalWrite(LED_BUILTIN, LOW);
}

// Return true when button, after debouncing, goes from not pressed the previous call to pressed. Return false otherwise.
bool button_on_press() {
  static int previous_reading = 0;
  static unsigned int previous_change_ms = 0;
  static int debounced_button = HIGH;

  bool on_press = false;
  
  int reading = digitalRead(buttonPin);
  if (previous_reading != reading) {
    previous_change_ms = millis();
  }

  // Consider a reading if it hasn't changed for the debounce period.
  if (millis() - previous_change_ms > debounce_ms) {
    if (debounced_button != reading) {
      debounced_button = reading;

      // Read is LOW when button is pressed. 
      if (reading == LOW) {
        on_press = true;
      }
    }
  }

  previous_reading = reading;
  
  return on_press;
}
