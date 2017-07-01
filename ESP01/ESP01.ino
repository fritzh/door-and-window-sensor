#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#define INPUT_PIN 2
#define OUTPUT_PIN 0

// === BEGIN CONFIGURATION

// Set this to the device ID you want this module to have in MQTT messages.
const char* device_id = "your-device-id";

// Set these up according to your network settings. Note that the firmware
// at this point requires a static IP address, so that long hang-ups on getting
// a DHCP lease won't cause it to drain the batteries unnecessarily.
IPAddress device_ip(192, 168, 1, 200);
IPAddress gateway_ip(192, 168, 1, 1);
IPAddress dns_ip(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// Enter your WiFi network details here.
const char* ssid = "MyNetworkId";
const char* password = "MySuperSecretNetworkPassword";

// Enter your MQTT server IP address or hostname here. You can use a couple of
// open public test servers. You can find some here: 
// https://github.com/mqtt/mqtt.github.io/wiki/public_brokers
const char* mqtt_server = "192.168.1.252";

// This is the VCC correction factor. When you manually measure your battery
// voltage and it doesn't line up with the voltage reported by the module,
// you can adjust it using this. 
// Note that whether or not the VCC reporting code actually _works_ is a bit dubious.
const double vcc_factor = 1.17479;

// === END CONFIGURATION

// Empty MQTT callback.
void callback(char* topic, byte* payload, unsigned int length) {
}

WiFiClient espClient;
PubSubClient client(mqtt_server, 1883, callback, espClient);

ADC_MODE(ADC_VCC)

void setup() {
  pinMode(INPUT_PIN, INPUT);
   
  pinMode(OUTPUT_PIN, OUTPUT);
  digitalWrite(OUTPUT_PIN, HIGH);
  
  setup_wifi();
}

void setup_wifi() {
  WiFi.hostname(device_id);
  WiFi.config(device_ip, gateway_ip, subnet, dns_ip);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(1);
}

void loop() {
  char topic[35];
  char payload[10];
  
  if (client.connect(device_id)) {
    for (int i = 0; i < 2; ++i) {
      // Tell the ATtiny85 to give us the next value to send.
      digitalWrite(OUTPUT_PIN, LOW);
      delayMicroseconds(50);
      digitalWrite(OUTPUT_PIN, HIGH);
      delayMicroseconds(50);

      // Get the value from the ATtiny.
      int value = digitalRead(INPUT_PIN);

      // Publish the value to the MQTT broker.
      (String(device_id) + "/input-" + i).toCharArray(topic, sizeof(topic));
      String(value).toCharArray(payload, sizeof(payload));
      client.publish(topic, payload, true);
    }

    // Publish our known VCC to the MQTT broker.
    uint32_t vcc = vcc_factor * (double)ESP.getVcc();
    (String(device_id) + "/vcc").toCharArray(topic, sizeof(topic));
    String(vcc).toCharArray(payload, sizeof(payload));
    client.publish(topic, payload);
  }

  // Tell the ATtiny85 to go to sleep.
  digitalWrite(OUTPUT_PIN, LOW); // If the ATTiny sees a low pulse more than 200uS, it too will go to sleep.
  delayMicroseconds(200);

  // Go to sleep ourselves.
  ESP.deepSleep(0);
}


