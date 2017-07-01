// This is the firmware for the ATtiny85 in the WiFi Door and Window Sensor project.
// https://breadboards.co.nz/blogs/projects/project-esp8266-door-and-window-sensor
//
// The ATtiny85 is responsible for power management, allowing the ESP-01 to respond
// to pin changes on demand rather than polling.

#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>

#define ESP_RESET_PIN 2
#define ESP_RX_PIN 0
#define ESP_TX_PIN 1
#define SENSOR_A_PIN 3
#define SENSOR_B_PIN 4

// We create a blank interrupt service routine (ISR) here because we don't actually do
// much in the routine itself.
ISR (PCINT0_vect) {}

void setup() {
  pinMode(SENSOR_A_PIN, INPUT_PULLUP);
  pinMode(SENSOR_B_PIN, INPUT_PULLUP);
  pinMode(ESP_RX_PIN, INPUT);
  
  pinMode(ESP_RESET_PIN, OUTPUT);
  digitalWrite(ESP_RESET_PIN, HIGH);
  
  pinMode(ESP_TX_PIN, OUTPUT);
  digitalWrite(ESP_TX_PIN, HIGH);

  // This is a bit of manual jiggery-pokery to enable 
  PCMSK  |= bit (PCINT4);  // Specifying the first pin,
  PCMSK  |= bit (PCINT3);  // and the second pin.
  GIFR   |= bit (PCIF);    // Clear any outstanding interrupts, and then
  GIMSK  |= bit (PCIE);    // enable pin change interrupts.
}

int sensorA = 0;
int sensorB = 0;

void wakeFromSleep() {
}

void sleepNow() {
  // This is to put the ATtiny to sleep. Note we have to power down the ADC as well
  // to reach that 0.5uA sleep state.
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  ADCSRA = 0;            // turn off ADC
  power_all_disable ();  // power off ADC, Timer 0 and 1, serial interface
  sleep_enable();
  sleep_cpu();                       

  // Now, here's the cool bit - when an external interrupt is triggered, the ATtiny will
  // actually resume from this exact place - so we turn everything back on again.
  sleep_disable();   
  power_all_enable();
}

void loop() {
  // Get our current sensor values.
  int newSensorA = digitalRead(SENSOR_A_PIN);
  int newSensorB = digitalRead(SENSOR_B_PIN);

  if (newSensorA != sensorA || newSensorB != sensorB) {
    // Reset the ESP module.
    digitalWrite(ESP_TX_PIN, HIGH);
    digitalWrite(ESP_RESET_PIN, HIGH);
    delay(1);
    digitalWrite(ESP_RESET_PIN, LOW);
    delay(1);
    digitalWrite(ESP_RESET_PIN, HIGH);
    // Wait for the ESP to finish writing its debug output. Note that we could disable
    // this output on the ESP if we wanted to.
    delay(400);
    
    int data[] = { newSensorA, newSensorB };
    sensorA = newSensorA;
    sensorB = newSensorB;

    long now;
    for (int i = 0; i < 2; ++i) {
      now = millis();

      // Wait for a pulse from the ESP module. If no pulse is received, bail out.
      while ((millis() - now) < 5000 && digitalRead(ESP_RX_PIN)) ;
      if (digitalRead(ESP_RX_PIN)) {
        return;
      }

      // Measure the length of the low pulse from the ESP module. We expect this to
      // be less than 100uS if the ESP module wants the next value, or more than that
      // if the ESP wants the ATtiny to go to sleep. So, we do the same thing again,
      // but we set the timeout to 100uS.
      // We do this in case the ATtiny and the ESP got out of step for whatever reason.
      now = micros();
      while ((micros() - now) < 100 && !digitalRead(ESP_RX_PIN)) ;
      if (!digitalRead(ESP_RX_PIN)) {
        return;
      }

      // Provide the sensor value to the ESP module.
      digitalWrite(ESP_TX_PIN, data[i] ? HIGH : LOW);
    }

    // Now wait for the final go-to-sleep pulse from the ESP.
    now = millis();
    while ((millis() - now) < 5000 && digitalRead(ESP_RX_PIN)) ;
    now = micros();
    while ((micros() - now) < 500 && !digitalRead(ESP_RX_PIN)) ;
  }
  else 
  {
    sleepNow();  
  }
}
