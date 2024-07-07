#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ArduinoJson.h>

WiFiClient espClient;
PubSubClient client(espClient);

#define DHTPIN D4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "desktop";
const char* password = "12345789";

const int relayLight = D6;
const int relayFan = D7;
const int relayPump = D8;

const int buttonD1 = D1;
const int buttonD2 = D2;
const int buttonD3 = D3;

const char* mqtt_server = "54.255.244.186";
const int mqtt_port = 1883;

const char* deviceId = "esp"; // Thêm biến deviceId

bool fanState = false;
bool lightState = false;
bool pumpState = false;

void setupMQTT();
void setup_wifi();
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);
void publishControlMessage(const char* topic, const char* message, bool isFromAutomation);

void setup() {
  Serial.begin(115200);
  setup_wifi();
  setupMQTT();
  dht.begin();

  pinMode(relayFan, OUTPUT);
  pinMode(relayLight, OUTPUT);
  pinMode(relayPump, OUTPUT);

  pinMode(buttonD1, INPUT_PULLUP);
  pinMode(buttonD2, INPUT_PULLUP);
  pinMode(buttonD3, INPUT_PULLUP);

  digitalWrite(relayFan, LOW);
  digitalWrite(relayLight, LOW);
  digitalWrite(relayPump, LOW);
  publishControlMessage((String(deviceId) + "/pump").c_str(), "off", false);
  publishControlMessage((String(deviceId) + "/light").c_str(), "off", false);
  publishControlMessage((String(deviceId) + "/fan").c_str(), "off", false);
}

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      reconnect();
    }
    client.loop();
  }

  static bool lastButtonStateD3 = HIGH;
  static bool lastButtonStateD2 = HIGH;
  static bool lastButtonStateD1 = HIGH;

  bool currentButtonStateD3 = digitalRead(buttonD3);
  bool currentButtonStateD2 = digitalRead(buttonD2);
  bool currentButtonStateD1 = digitalRead(buttonD1);

  if (lastButtonStateD3 == HIGH && currentButtonStateD3 == LOW) {
    pumpState = !pumpState;
    digitalWrite(relayPump, pumpState ? HIGH : LOW);
    if (WiFi.status() == WL_CONNECTED) {
      publishControlMessage((String(deviceId) + "/pump").c_str(), pumpState ? "on" : "off", false);
    }
    delay(50); 
  }
  lastButtonStateD3 = currentButtonStateD3;

  if (lastButtonStateD2 == HIGH && currentButtonStateD2 == LOW) {
    fanState = !fanState;
    digitalWrite(relayFan, fanState ? HIGH : LOW);
    if (WiFi.status() == WL_CONNECTED) {
      publishControlMessage((String(deviceId) + "/fan").c_str(), fanState ? "on" : "off", false);
    }
    delay(50); 
  }
  lastButtonStateD2 = currentButtonStateD2;

  if (lastButtonStateD1 == HIGH && currentButtonStateD1 == LOW) {
    lightState = !lightState;
    digitalWrite(relayLight, lightState ? HIGH : LOW);
    if (WiFi.status() == WL_CONNECTED) {
      publishControlMessage((String(deviceId) + "/light").c_str(), lightState ? "on" : "off", false);
    }
    delay(50); 
  }
  lastButtonStateD1 = currentButtonStateD1;

  static long previous_time = 0;
  long now = millis();

  if (now - previous_time > 10000) { // Thay đổi từ 1000 thành 10000 để gửi dữ liệu mỗi 10 giây
    float temperature = dht.readTemperature();
    char tempString[8];
    dtostrf(temperature, 1, 2, tempString);
    if (WiFi.status() == WL_CONNECTED) {
      client.publish((String(deviceId) + "/temp").c_str(), tempString);
    }

    float humidity = dht.readHumidity();
    char humString[8];
    dtostrf(humidity, 1, 2, humString);
    if (WiFi.status() == WL_CONNECTED) {
      client.publish((String(deviceId) + "/hum").c_str(), humString);
    }

    previous_time = now; 
  }

  delay(100);
}

void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Đang kết nối tới ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempts++;
    if (attempts > 20) {
      Serial.println("Kết nối thất bại. Kiểm tra Tên và Mật khẩu mạng.");
      return;
    }
  }

  Serial.println("");
  Serial.println("Kết nối Wi-Fi thành công");
  Serial.println("Địa chỉ IP: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    String clientId = "ESPClient-";
    clientId += String(random(0xffff), HEX);

    if (client.connect(clientId.c_str())) {
      Serial.println("Connected");
      client.subscribe((String(deviceId) + "/pump").c_str());
      client.subscribe((String(deviceId) + "/fan").c_str());
      client.subscribe((String(deviceId) + "/light").c_str());
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  payload[length] = '\0';
  Serial.println((char*)payload);

  StaticJsonDocument<200> doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("deserializeJson() failed: ");
    Serial.println(error.f_str());
    return;
  }

  const char* message = doc["message"];
  bool isFromAutomation = doc["isFromAutomation"];

  if (strcmp(topic, (String(deviceId) + "/pump").c_str()) == 0) {
    pumpState = strcmp(message, "on") == 0;
    digitalWrite(relayPump, pumpState ? HIGH : LOW);
  } else if (strcmp(topic, (String(deviceId) + "/fan").c_str()) == 0) {
    fanState = strcmp(message, "on") == 0;
    digitalWrite(relayFan, fanState ? HIGH : LOW);
  } else if (strcmp(topic, (String(deviceId) + "/light").c_str()) == 0) {
    lightState = strcmp(message, "on") == 0;
    digitalWrite(relayLight, lightState ? HIGH : LOW);
  }
}

void setupMQTT() {
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void publishControlMessage(const char* topic, const char* message, bool isFromAutomation) {
  StaticJsonDocument<200> doc;
  doc["message"] = message;
  doc["isFromAutomation"] = isFromAutomation;

  char payload[256];
  serializeJson(doc, payload);

  client.publish(topic, payload, true);
}
