#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h> 
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include <EEPROM.h>

// eeprom
#define MQTT_IP_OFFSET         0
#define MQTT_IP_LENGTH        16
#define MQTT_USER_OFFSET      16
#define MQTT_USER_LENGTH      32
#define MQTT_PASSWORD_OFFSET  48
#define MQTT_PASSWORD_LENGTH  32
#define DIM_OFFSET            80
#define DIM_LENGTH             1

// pins
#define SWITCH_GPIO 13

// dimming
#define DIM_STEP 1

// access point
#define AP_NAME "NX-4653"
#define AP_TIMEOUT 300
#define MQTT_PORT 1883

// topics
char topic_dim[30] = "/";
char topic_dim_fb[30] = "/";
char topic_onoff[30] = "/";

// dimming
bool    dim_start = false;
bool    dim_dir = false;
uint8_t dim_onoff = 1;
uint8_t dim_cmd[6] = {0xFF, 0x55, 0x00, 0x05, 0xDC, 0x0A};
uint8_t dim     = 0x00;
uint8_t dim_old = 0x00;
char    dim_str[4];

// AC voltage phase counter
volatile uint32 phase = 0ul;
uint32 phase_old = 0ul;

// mqtt
IPAddress mqtt_server;
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
bool mqtt_dimming = true;
char mqtt_ip_pre[MQTT_IP_LENGTH] = "";
char mqtt_user_pre[MQTT_USER_LENGTH] = "";
char mqtt_password_pre[MQTT_PASSWORD_LENGTH] = "";
char mqtt_ip[MQTT_IP_LENGTH] = "";
char mqtt_user[MQTT_USER_LENGTH] = "";
char mqtt_password[MQTT_PASSWORD_LENGTH] = "";

// wifi
WiFiManager wifiManager;
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
Ticker wifiReconnectTimer;
char mac_str[13];

void ICACHE_RAM_ATTR switch_interrupt()
{
  phase ++;
}

String readEEPROM(int offset, int len)
{
    String res = "";
    for (int i = 0; i < len; ++i)
    {
      res += char(EEPROM.read(i + offset));
    }
    return res;
}
  
void writeEEPROM(int offset, int len, String value)
{
    for (int i = 0; i < len; ++i)
    {
      if (i < value.length()) {
        EEPROM.write(i + offset, value[i]);
      } else {
        EEPROM.write(i + offset, 0x00);
      }
    }
}
  
void connectToWifi()
{
  //Serial.println("Re-Connecting to Wi-Fi...");
  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  WiFi.mode(WIFI_STA);
  WiFi.begin();
}

void onWifiConnect(const WiFiEventStationModeGotIP& event)
{
  //Serial.println("Connected to Wi-Fi.");
  connectToMqtt();
}

void onWifiDisconnect(const WiFiEventStationModeDisconnected& event)
{
  //Serial.println("Disconnected from Wi-Fi.");
  mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  wifiReconnectTimer.once(2, connectToWifi);
}

void connectToMqtt()
{
  //Serial.println("Connecting to MQTT...");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent)
{
  //Serial.println("Connected to MQTT.");
  //Serial.print("Session present: ");
  //Serial.println(sessionPresent);
  
  mqttClient.subscribe(topic_dim, 2);
  mqttClient.subscribe(topic_onoff, 2);

  mqttClient.publish(topic_dim, 0, true, dim_str);
  mqttClient.publish(topic_dim_fb, 0, false, dim_str);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  //Serial.println("Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    mqttReconnectTimer.once(2, connectToMqtt);
  }
}

void onMqttMessage(char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total)
{
  char pl[7];

  strncpy(pl, payload, 6);
  pl[len] = 0;
  //Serial.printf("Publish received. Topic: %s Payload: %s\n", topic, pl);

  if ((0 == strcmp(topic, topic_onoff)) && (mqtt_dimming == true))
  {
    char val[4];

    if (len <= 3)
    {
      strncpy(val, (char*)payload, 3);
      val[len] = 0;
      dim_onoff = (uint8_t)strtol(val, NULL, 10);
      if (dim_onoff == 0)
      {
        dim_cmd[2] = 0x00;
      }
      else
      {
        dim_cmd[2] = dim;    
      }
      Serial.write(dim_cmd, 6);
    }
  }
  
  if (0 == strcmp(topic, topic_dim))
  {
    char val[4];

    if (len <= 3)
    {
      mqtt_dimming = true;
      strncpy(val, (char*)payload, 3);
      val[len] = 0;
      dim = (uint8_t)strtol(val, NULL, 10);
      if (dim != dim_old)
      {
        if (dim_onoff == 0)
        {
          dim_cmd[2] = 0x00;
        }
        else
        {
          dim_cmd[2] = dim;
        }
        Serial.write(dim_cmd, 6);
        EEPROM.write(DIM_OFFSET, dim);
        EEPROM.commit();
      }
      
      dim_old = dim;
    }
  }
}

void setup(void)
{
  uint8_t mac[6];
  
  // init UART
  Serial.begin(9600);

  // init EEPROM
  EEPROM.begin(128);

  pinMode(SWITCH_GPIO, INPUT);
  attachInterrupt(digitalPinToInterrupt(SWITCH_GPIO), switch_interrupt, FALLING);
  delay(1000);
  // check if button is pressed during startup
  if (phase > 15ul)
  {
    //Serial.println("reset wifi settings and restart.");
    wifiManager.resetSettings();
    delay(1000);
    ESP.restart();    
  }

  // init WIFI
  readEEPROM(MQTT_IP_OFFSET, MQTT_IP_LENGTH).toCharArray(mqtt_ip_pre, MQTT_IP_LENGTH);
  readEEPROM(MQTT_USER_OFFSET, MQTT_USER_LENGTH).toCharArray(mqtt_user_pre, MQTT_USER_LENGTH);
  readEEPROM(MQTT_PASSWORD_OFFSET, MQTT_PASSWORD_LENGTH).toCharArray(mqtt_password_pre, MQTT_PASSWORD_LENGTH);
  
  WiFiManagerParameter custom_mqtt_ip("ip", "MQTT ip", mqtt_ip_pre, MQTT_IP_LENGTH);
  WiFiManagerParameter custom_mqtt_user("user", "MQTT user", mqtt_user_pre, MQTT_USER_LENGTH);
  WiFiManagerParameter custom_mqtt_password("passord", "MQTT password", mqtt_password_pre, MQTT_PASSWORD_LENGTH, "type=\"password\"");
  
  wifiManager.addParameter(&custom_mqtt_ip);
  wifiManager.addParameter(&custom_mqtt_user);
  wifiManager.addParameter(&custom_mqtt_password);

  WiFi.setSleepMode(WIFI_NONE_SLEEP);
  wifiManager.setConfigPortalTimeout(AP_TIMEOUT);
  wifiManager.setAPStaticIPConfig(IPAddress(192,168,1,1), IPAddress(192,168,1,1), IPAddress(255,255,255,0));
  if (!wifiManager.autoConnect(AP_NAME))
  {
    //Serial.println("failed to connect and restart.");
    delay(1000);
    // restart and try again
    ESP.restart();
  }

  strcpy(mqtt_ip, custom_mqtt_ip.getValue());
  strcpy(mqtt_user, custom_mqtt_user.getValue());
  strcpy(mqtt_password, custom_mqtt_password.getValue());

  if ((0 != strcmp(mqtt_ip, mqtt_ip_pre)) || 
      (0 != strcmp(mqtt_user, mqtt_user_pre)) || 
      (0 != strcmp(mqtt_password, mqtt_password_pre)))
  {
    //Serial.println("Parameters changed, need to update EEPROM.");
    writeEEPROM(MQTT_IP_OFFSET, MQTT_IP_LENGTH, mqtt_ip);
    writeEEPROM(MQTT_USER_OFFSET, MQTT_USER_LENGTH, mqtt_user);
    writeEEPROM(MQTT_PASSWORD_OFFSET, MQTT_PASSWORD_LENGTH, mqtt_password);
    
    EEPROM.commit();
  }

  wifiConnectHandler = WiFi.onStationModeGotIP(onWifiConnect);
  wifiDisconnectHandler = WiFi.onStationModeDisconnected(onWifiDisconnect);

  // construct MQTT topics with MAC
  WiFi.macAddress(mac);
  sprintf(mac_str, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  strcat(topic_dim, mac_str);
  strcat(topic_dim, "/dim"); 
  strcat(topic_dim_fb, mac_str);
  strcat(topic_dim_fb, "/dim_fb"); 
  strcat(topic_onoff, mac_str);
  strcat(topic_onoff, "/onoff");  

  // read dim value
  dim   = EEPROM.read(DIM_OFFSET);
  dim_old = dim;

  // set dimming
  sprintf(dim_str, "%d", dim);
  dim_cmd[2] = dim;
  Serial.write(dim_cmd, 6);

  if (mqtt_server.fromString(mqtt_ip))
  {
    char mqtt_id[30] = AP_NAME;

    strcat(mqtt_id, "-");
    strcat(mqtt_id, mac_str);
    mqttClient.onConnect(onMqttConnect);
    mqttClient.onDisconnect(onMqttDisconnect);
    mqttClient.onMessage(onMqttMessage);
    mqttClient.setServer(mqtt_server, MQTT_PORT);
    mqttClient.setCredentials(mqtt_user, mqtt_password);
    mqttClient.setClientId(mqtt_id);
  
    connectToMqtt();
  }
  else
  {
    //Serial.println("invalid MQTT Broker IP.");
  }
}

void loop(void)
{
  delay(25); // wait at least 20ms (for 50Hz AC voltage)

  if (phase_old != phase)
  {
    mqtt_dimming = false;
    dim_start = true;
    if (dim_dir == true)
    {
      if (dim >= DIM_STEP)
      {
        dim -= DIM_STEP;
      }
      else
      {
        dim = 0;
      }
    }
    else
    {
      if (dim <= (255 - DIM_STEP))
      {
        dim += DIM_STEP;
      }
      else
      {
        dim = 255;
      }
    }

    // update dimmer
    dim_cmd[2] = dim;
    Serial.write(dim_cmd, 6);
  }
  else
  {
    if (dim_start == true)
    {
      if ((dim == 0) || (dim == 255))
      {
        dim_dir = !dim_dir;
      }
      // update mqtt dim feedback
      sprintf(dim_str, "%d", dim);
      mqttClient.publish(topic_dim_fb, 0, false, dim_str);
      EEPROM.write(DIM_OFFSET, dim);
      EEPROM.commit();
    }
    dim_start = false;
  }
  phase_old = phase;
}
