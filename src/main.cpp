#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ESPTelnet.h>
#include <ArduinoOTA.h>
#include <esp_wifi.h>
#include <esp_system.h>

#define relay 2

// WiFi
static const char *ssid = "TP-Link_5235";
static const char *passwd = "MaDaPi16";
// MQTT
static const char *mqtt_server = "192.168.0.100";  // don't forget, that (in my case) it's running on MQTT
                                            // v. 3.1
static const int mqtt_port = 1883;
static const char *mqtt_user = "xiao0";
static const char *mqtt_passwd = "broker#1234";
static const char *mqtt_topic_sw = "xiao_light/relay";
static const char *mqtt_topic = "xiao_light/lightbulb";
// Telnet
static const int telnet_port = 23;

static bool force_refresh = false;
static bool lightbulb_state = false;
static bool isUpdating = false;
static int64_t last_temp_check = 0;
static int64_t last_recon_time = 0;
static int64_t last_temp_info = 0;
static float coreTemp = 0.0;

// Font colors
const static char* CLR_RST = "\033[0m";
const static char* CLR_RED = "\033[31m";
const static char* CLR_GRN = "\033[32m";
const static char* CLR_YLW = "\033[33m";

WiFiServer telnetServer(telnet_port);
WiFiClient telnet;

WiFiClient client;
PubSubClient mqttClient(client);

void reconnect();
void callback(char *topic, byte *payload, unsigned int length);
const char* getResetReason();


void setup() {
  Serial.begin(9600);
  WiFi.begin(ssid, passwd);

  while (WiFi.status() != WL_CONNECTED) {
    vTaskDelay(pdMS_TO_TICKS(200));
    Serial.print(".");
  }
  Serial.println(WiFi.localIP());
  
  telnetServer.begin();
  
  pinMode(relay, OUTPUT);
  
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);

  esp_wifi_set_ps(WIFI_PS_MAX_MODEM);        // light-sleep mode

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
    int64_t now = esp_timer_get_time();

    if (!mqttClient.connected() && now - last_recon_time > 5 * 1000000) { // 5 sec
      reconnect();
      last_recon_time = now;
    }

    mqttClient.loop();

    if (force_refresh) {
      
      String lightbulb_msg;
      String lightbulb_sw_msg;

      if (lightbulb_state) {
      
        lightbulb_msg = "on";
      
      } else {
      
        lightbulb_msg = "off";
      
      }

      mqttClient.publish(mqtt_topic, lightbulb_msg.c_str());      

      force_refresh = false;
    }

    now = esp_timer_get_time();

    if (now - last_temp_info > 60 * 1000000) {
      
      telnet.printf("MCU's temperature: %.4f\n\r", coreTemp);
      
      last_temp_info = now;

    }

    if (now - last_temp_check > 5 * 1000000) {

      coreTemp = temperatureRead();

      if (coreTemp > 90.0) {
        telnet.printf("%sWARNING%s: \t%sOVERHEAT!%s Turning into sleep mode for 5 mins...\n\r", 
        CLR_YLW, CLR_RST, CLR_RED, CLR_RST);

        esp_sleep_enable_timer_wakeup(300000000);
        esp_deep_sleep_start();
      }
    
      last_temp_check = esp_timer_get_time();

    }

    if (!telnet || !telnet.connected()) {
        
        telnet = telnetServer.accept();
        
        if (telnet) {
        
          telnet.print("INFO: \tTelnet client have just connected!\n\r");
          Serial.print("Telnet client have just connected!\n\r");
          telnet.print("...\n\r");

          telnet.printf("%sWARNING%s: \tLast reset reason: %s%s%s\n\r",
            CLR_YLW, CLR_RST, CLR_YLW, getResetReason(), CLR_RST);
          
        } 
      }

      while (telnet && telnet.available()) telnet.read();

    

    vTaskDelay(pdMS_TO_TICKS(100));
  }

  
}

void reconnect() {

  bool connected = false;

  Serial.printf("%sWARNING%s: Reconnecting with MQTT broker...\n\r", CLR_YLW, CLR_RST);
  telnet.printf("%sWARNING%s: Reconnecting with MQTT broker...\n\r", CLR_YLW, CLR_RST);


  if (mqttClient.connect("xiao", mqtt_user, mqtt_passwd)) {
    Serial.printf("%sConnected.%s", CLR_GRN, CLR_RST);
    telnet.printf("%sConnected.%s\n\r", CLR_GRN, CLR_RST);

    mqttClient.subscribe(mqtt_topic_sw);
    
  } else {
    
    Serial.printf("%sError%s: %i\n\r", CLR_RED, CLR_RST, mqttClient.state());
    telnet.printf("%sError%s: %i\n\r", CLR_RED, CLR_RST, mqttClient.state());

  }
  
  
  if (!connected) {
    Serial.printf("%sWARNING%s: Couldn't connect. Trying again later...\n\r", 
      CLR_YLW, CLR_RST);
    
    telnet.printf("%sWARNING%s: Couldn't connect. Trying again later...\n\r", 
      CLR_YLW, CLR_RST);
  }  
  
}

void callback(char *topic, byte *payload, unsigned int length) {

  String msg;

  for (uint i=0; i<length; i++) {
    msg += (char)payload[i];
  }

  if (String(topic) == mqtt_topic_sw) {

    if (msg == "on") {

      digitalWrite(relay, HIGH);
      lightbulb_state = true;
      force_refresh = true;

      telnet.printf("INFO: \tLight %sON%s\n\r", CLR_GRN, CLR_RST);
      Serial.printf("INFO: \tLight %sON%s\n", CLR_GRN, CLR_RST);

    } else if (msg == "off") {

      digitalWrite(relay, LOW);
      lightbulb_state = false;
      force_refresh = true;

      telnet.printf("INFO: \tLight %sOFF%s\n\r", CLR_RED, CLR_RST);
      Serial.printf("INFO: \tLight %sOFF%s\n", CLR_RED, CLR_RST);


    } else {

      telnet.printf("%sERROR%s: \tUnexpected msg - got: %s \n", CLR_RED, CLR_RST, msg.c_str());
      Serial.printf("%sERROR%s: \tUnexpected msg - got: %s \n", CLR_RED, CLR_RST, msg.c_str());

    }
  }

}

const char* getResetReason() {
    switch (esp_reset_reason()) {
        case ESP_RST_POWERON: return "Power on reset";
        case ESP_RST_SW:      return "Software reset";
        case ESP_RST_PANIC:   return "Exception / Panic reset";
        case ESP_RST_INT_WDT: return "Interrupt Watchdog";
        case ESP_RST_TASK_WDT:return "Task Watchdog";
        case ESP_RST_BROWNOUT:return "Voltage dip";
        default:              return "Other";
    }
}