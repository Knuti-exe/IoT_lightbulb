#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPTelnet.h>
#include <ArduinoOTA.h>

#define relay 2

// WiFi
const char *ssid = "*********";
const char *passwd = "*******";
// MQTT
const char *mqtt_server = "192.168.0.100";  // don't forget, that (in my case) it's running on MQTT
                                            // v. 3.1
const int mqtt_port = 1883;
const char *mqtt_user = "xiao0";
const char *mqtt_passwd = "broker#1234";
const char *mqtt_topic_sw = "xiao_light/relay";
const char *mqtt_topic = "xiao_light/lightbulb";
const char *temp_topic = "xiao_light/temp";
// Telnet
const int telnet_port = 23;

long long int last_time = 0;
bool force_refresh = false;
bool lightbulb_state = false;
bool telnet_connected = false;
long long int last_recon_time = 0;
bool isUpdating = false;

WiFiServer telnetServer(telnet_port);
WiFiClient telnetClient;

WiFiClient client;
PubSubClient mqttClient(client);

void reconnect();
void callback(char *topic, byte *payload, unsigned int length);


void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, passwd);
  // TODO : wdrozyc strone WWW logowania do sieci WIFI 

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(200));
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());
  
  telnetServer.begin();
  
  pinMode(relay, OUTPUT);
  
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);

  ArduinoOTA.begin();

  ArduinoOTA.onStart([]() {
    isUpdating = true;
  });

  ArduinoOTA.onEnd([]() {
    isUpdating = false;
  });
}

void loop() {
  ArduinoOTA.handle();

  if (!isUpdating) {
    if (!mqttClient.connected() && millis() - last_recon_time > 5000) {
      reconnect();
      last_recon_time = millis();
    }

    mqttClient.loop();

    if (millis() - last_time > 10000 || force_refresh) {
      
      String lightbulb_msg;
      String lightbulb_sw_msg;

      if (lightbulb_state) {
      
        lightbulb_msg = "on";
      
      } else {
      
        lightbulb_msg = "off";
      
      }

      mqttClient.publish(mqtt_topic, lightbulb_msg.c_str());

      // just to check if temps are fine in closed box
      float coreTemp = temperatureRead();
      mqttClient.publish(temp_topic, String(coreTemp).c_str());

      if (coreTemp > 90.0) {
        Serial.println("WARNING : OVERHEAT! Turning into sleep mode for 5 mins...");
        esp_sleep_enable_timer_wakeup(300 * 1000000);
        esp_deep_sleep_start();
      }

      force_refresh = false;
      last_time = millis();
    }

    if ((!telnetClient || !telnetClient.connected()) && !telnet_connected) {
        
        telnetClient = telnetServer.accept();
        
        if (telnetClient) {
        
          telnetClient.println("Telnet client have just connected!");
          Serial.println("Telnet client have just connected!");
          telnetClient.println("...\r\n");
          telnet_connected = true;
        
        }
      }
    

    vTaskDelay(pdMS_TO_TICKS(10));
  }

  
}

void reconnect() {

  bool connected = false;

  Serial.println("Reconnecting with MQTT broker...");
  telnetClient.println("\rReconnecting with MQTT broker...");


  if (mqttClient.connect("xiao", mqtt_user, mqtt_passwd)) {
    Serial.println("Connected.");
    telnetClient.println("Connected.");

    mqttClient.subscribe(mqtt_topic_sw);
    
  } else {
    
    Serial.printf("Error : %i", mqttClient.state());
    telnetClient.printf("Error : %i", mqttClient.state());

    
  }
  
  
  if (!connected) {
    Serial.println("Trying again later...");
    telnetClient.println("Trying again later...");
  }  
  
}

void callback(char *topic, byte *payload, unsigned int length) {

  String msg;

  for (int i=0; i<length; i++) {
    msg += (char)payload[i];
  }

  if (String(topic) == mqtt_topic_sw) {

    if (msg == "on") {

      digitalWrite(relay, HIGH);
      lightbulb_state = true;
      force_refresh = true;

      Serial.printf("Light ON\n");
      telnetClient.printf("\rLight ON\n");

    } else if (msg == "off") {

      digitalWrite(relay, LOW);
      lightbulb_state = false;
      force_refresh = true;

      Serial.printf("Light OFF\n");
      telnetClient.printf("\rLight OFF\n");

    } else {

      telnetClient.printf("\r\nUnexpected msg : %s \n", msg.c_str());
      Serial.printf("\nUnexpected msg : %s \n", msg.c_str());
    }
  }

}
