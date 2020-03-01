#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <ESP8266WebServer.h>
#include <PubSubClient.h>
#include <PZEM004T.h>

#ifndef STASSID
#define STASSID "TP-LINK_2.4GHz_38F3BB"
#define STAPSK  "qqnamuniu7878"
#define MQTTSRV "192.168.1.50"
#endif

const char* ssid = STASSID;
const char* password = STAPSK;
const char* mqtt_server = MQTTSRV;

#define power_topic "sensor/power"
#define energy_topic "sensor/energy"
#define voltage_topic "sensor/voltage"
#define current_topic "sensor/current"

WiFiClient espClient;
PubSubClient client(espClient);
long lastMsg = 0;
char msg[50];
int value = 0;

HardwareSerial hwserial(UART0);     // Use hwserial UART0 at pins GPIO1 (TX) and GPIO3 (RX)
PZEM004T pzem(&hwserial);           // Attach PZEM to hwserial
IPAddress ip(192,168,1,1);

bool pzemrdy = false;

ESP8266WebServer server(80);

//HardwareSerial hwserial(UART0);
//IPAddress ip(192,168,1,227);

void handleRoot() {
  server.send(200, "text/plain", "Running...");
  Serial1.print("Request from www... ");
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial1.print("Message arrived [");
  Serial1.print(topic);
  Serial1.print("] ");
  for (int i = 0; i < length; i++) {
    Serial1.print((char)payload[i]);
  }
  Serial1.println();

  // Switch on the LED if an 1 was received as first character
  if ((char)payload[0] == '1') {
    digitalWrite(BUILTIN_LED, LOW);   // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
  } else {
    digitalWrite(BUILTIN_LED, HIGH);  // Turn the LED off by making the voltage HIGH
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial1.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial1.println("connected");
      // Once connected, publish an announcement...
      client.publish("outTopic", "hello world");

      // ... and resubscribe
      client.subscribe("inTopic");
    } else {
      Serial1.print("failed, rc=");
      Serial1.print(client.state());
      Serial1.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  Serial1.begin(115200);
  Serial1.println("Booting");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial1.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  randomSeed(micros());
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
  
  ArduinoOTA.setHostname("ESP_OTA_1");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }

    // NOTE: if updating FS this would be the place to unmount FS using FS.end()
    Serial1.println("Start updating " + type);
  });
  ArduinoOTA.onEnd([]() {
    Serial1.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial1.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial1.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial1.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial1.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial1.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial1.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial1.println("End Failed");
    }
  });
  ArduinoOTA.begin();
  Serial1.println("Ready");
  Serial1.print("IP address: ");
  Serial1.println(WiFi.localIP());

  if (MDNS.begin("esp8266")) {
    Serial1.println("MDNS responder started");
  }

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial1.println("HTTP server started");

   while (!pzemrdy) {
      Serial1.println("Connecting to PZEM...");
      pzemrdy = pzem.setAddress(ip);
      delay(1000);
   }

  
}

void loop() {
  ///////////////////////////// MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    ++value;
    snprintf (msg, 50, "hello world #%ld", value);
    Serial1.print("Publish message: ");
    Serial1.println(msg);
    client.publish("outTopic", msg);
  }
  /////////////////////// MQTT///////////////////
  /////////////// PZEM004T////////////////
    float v = pzem.voltage(ip);
  if (v < 0.0) v = 0.0;
   Serial1.print(v);Serial1.print("V; ");

  float i = pzem.current(ip);
   if(i >= 0.0){ Serial1.print(i);Serial1.print("A; "); }

  float p = pzem.power(ip);
   if(p >= 0.0){ Serial1.print(p);Serial1.print("W; "); }

  float e = pzem.energy(ip);
   if(e >= 0.0){ Serial1.print(e);Serial1.print("Wh; "); }
  ///////////// PZEM004T //////////////
  
  ArduinoOTA.handle();
  server.handleClient();
  MDNS.update();
  delay(3000);
}
