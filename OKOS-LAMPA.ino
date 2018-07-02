#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <FS.h>

#define FIVEMIN (1000UL * 60 * 5)

String ssid = "";
String pass = "";
bool apmode = false;
bool wifiConnected = false;
unsigned long min5 = 0;

WiFiEventHandler stationConnectedHandler;
WiFiEventHandler stationDisconnectedHandler;

DNSServer dnsServer;
ESP8266WebServer webServer(80);

void setup() {
  Serial.begin(9600);
  if (!SPIFFS.begin()) {ESP.restart();}
  
  if (getSsidAndPass(&ssid, &pass)) {
    if (ssid.length() != 0) {
      apmode = !connectWifiNetwork(ssid, pass);
    } else {
      apmode = true;
    }
  } else {
    apmode = true;
  }
  
  if (apmode) {
    scanWifiNetworks();
    Serial.println("wifi hálózatok");
    WiFi.mode(WIFI_AP);
    WiFi.softAP("OkosAP");
    dnsServer.setTTL(300);
    dnsServer.setErrorReplyCode(DNSReplyCode::ServerFailure);
    dnsServer.start(53, "*", WiFi.softAPIP());
    webServer.onNotFound([]() {
      File file = SPIFFS.open("/index.html", "r");
      webServer.streamFile(file, "text/html");
      file.close();
      if (webServer.method() == HTTP_POST) {
        ssid = webServer.arg("ssid");
        pass = webServer.arg("pass");
        if(setSsidAndPass(ssid, pass)) {
          ESP.restart();
        }
      }
    });
  } else {
    
  }

  stationConnectedHandler = WiFi.onSoftAPModeStationConnected(&onStationConnected);
  stationDisconnectedHandler = WiFi.onSoftAPModeStationDisconnected(&onStationDisconnected);
  webServer.begin();
}

void loop() {
  while (wifiConnected) {
    dnsServer.processNextRequest();
    webServer.handleClient();
    delay(100);
  }
  check5min();
  delay(1000);
}

void check5min() {
  if (millis()-min5 > FIVEMIN) {
    min5 = millis();
    if (apmode) {ESP.restart();}
    if (WiFi.status() != WL_CONNECTED) {ESP.restart();}
  }
}

bool getSsidAndPass(String *ssid, String *pass) {
  if (!SPIFFS.exists("/wifi")) {return false;}
  File file;
  file = SPIFFS.open("/wifi", "r");
  if (!file) {return false;}
  String content = file.readString();
  file.close();
  int8_t ssidStartPos = content.indexOf("SSID=\"");
  int8_t ssidStopPos = content.indexOf("\";", ssidStartPos + 6);
  int8_t passStartPos = content.indexOf("PASS=\"");
  int8_t passStopPos = content.indexOf("\";",passStartPos + 6);
  if (ssidStartPos == -1 || ssidStopPos == -1 || passStartPos == -1 || passStopPos == -1) {return false;}
  *ssid = content.substring(ssidStartPos + 6, ssidStopPos);
  *pass = content.substring(passStartPos + 6, passStopPos);
  return true;
}

bool setSsidAndPass(String ssid, String pass) {
  File file;
  file = SPIFFS.open("/wifi", "w");
  if (!file) {return false;}
  file.print("SSID=\"");
  file.print(ssid);
  file.println("\";");
  file.print("PASS=\"");
  file.println(pass);
  file.print("\";");
  file.close();
  return true;
}

bool connectWifiNetwork(String ssid, String pass) {
  WiFi.mode(WIFI_STA);
  unsigned long timeout = millis();
  WiFi.begin(ssid.c_str(), pass.c_str());
  while (WiFi.status() != WL_CONNECTED && millis()-timeout < 15000) {
    delay(500);
  }
  return (WiFi.status() == WL_CONNECTED);
}

void scanWifiNetworks() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  if (n > 0) {
    for (int i = 0; i < n; ++i) {
      WiFi.SSID(i);
      delay(10);
    }
  }
}

//wifi connect and disconnect events
void onStationConnected(const WiFiEventSoftAPModeStationConnected& evt) {wifiConnected = true;}
void onStationDisconnected(const WiFiEventSoftAPModeStationDisconnected& evt) {wifiConnected = false;}
