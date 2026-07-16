//G4 Thomas Ducau, Thiago Ducau, Julian Gendler
#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <DHT.h>
#include <U8g2lib.h>
#include "time.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <FirebaseClient.h>
#include <Wire.h>

#define DHTPIN 4        
#define DHTTYPE DHT11
#define SW1 34
#define SW2 35

#define NTP_SERVER = "pool.ntp.org";

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);
DHT dht(DHTPIN, DHTTYPE);

#define WIFI_SSID "Iphone 16 Pro Max THOMAS"
#define WIFI_PASSWORD "2008714T"
#define Web_API_KEY "AIzaSyCP5k_My1tU7bSnTh3cUhSW1K5z5P_Tinw"
#define DATABASE_URL "https://st-g4-tp5-default-rtdb.firebaseio.com/"
#define USER_EMAIL "ducauthomas@gmail.com"
#define USER_PASS "thomas123"

typedef enum {
  RST,
  Pantalla1,
  Pantalla2,
  Pantalla1APantalla2,
  Pantalla2APantalla1
} estados_t;

estados_t maquinaDeEstado = RST;

void processData(AsyncResult &aResult);
UserAuth user_auth(Web_API_KEY, USER_EMAIL, USER_PASS);

FirebaseApp app;
WiFiClientSecure ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
RealtimeDatabase Database;

unsigned long lastSendTime = 0;
unsigned long sendInterval = 30000;
long int ultimoCheck = 0;

String uid;
String databasePath;
String parentPath;
String tempPath = "/temperature";
String timePath = "/timestamp";

int timestamp;
int getOffset = -3;

float temperature = 0.0;

object_t jsonData, obj1, obj2;
JsonWriter writer;

bool btn1presionado = false;
bool btn2presionado = false;

void initWiFi() {
  int intentos = 0;

  while (WiFi.status() != WL_CONNECTED && intentos < 20) {
    delay(500);
    Serial.print(".");
    intentos++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi OK");
  } else {
    Serial.println("\nSin WiFi");
  }
}
unsigned long getTime() {
  time_t now;
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return(0);
  }
  time(&now);
  return now;
}

void setup() {
  Serial.begin(115200);
  
  pinMode(SW1, INPUT);
  pinMode(SW2, INPUT);

  dht.begin();
  u8g2.begin();

  initWiFi();
  configTime(getOffset * 3600, 0, NTP_SERVER);

  ssl_client.setInsecure();
  ssl_client.setHandshakeTimeout(5);

  initializeApp(aClient, app, getAuth(user_auth), processData, "authTask");
  app.getApp<RealtimeDatabase>(Database);
  Database.url(DATABASE_URL);
}

void loop() {
  app.loop();

  bool btn1Actual = !digitalRead(SW1);
  bool btn2Actual = !digitalRead(SW2);

  if (millis() - ultimoCheck >= 5000) {
    float t = dht.readTemperature();
    if (!isnan(t)) {
      temperature = t;
    }
    ultimoCheck = millis();
  }

  switch (maquinaDeEstado) {
    
    case RST:
      maquinaDeEstado = Pantalla1;
      break;

    case Pantalla1:
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB10_tr);
      u8g2.setCursor(0, 20);
      u8g2.print("VA : ");
      u8g2.print(temperature);
      u8g2.sendBuffer();

      if (btn1actual) btn1presionado = true;
      if (btn2Actual) btn2presionado = true;

      if (!btn1actual && btn1presionado && !btn2Actual) {
        btn1presionado = false;
      }
      if (!btn2Actual && btn2presionado && !btn1actual) {
        btn2presionado = false;
      }

      if (btn1presionado && btn2presionado) {
        btn1presionado = false;
        btn2presionado = false;
        maquinaDeEstado = Pantalla1APantalla2;
      }
      break;

    case Pantalla1APantalla2:
      if (!btn1actual && !btn2Actual) {
        maquinaDeEstado = Pantalla2;
      }
      break;

    case Pantalla2:
      u8g2.clearBuffer();
      u8g2.setFont(u8g2_font_ncenB10_tr);
      u8g2.setCursor(0, 20);
      u8g2.print("ciclo: ");
      u8g2.print(sendInterval / 1000);
      u8g2.sendBuffer();

      if (btn1actual) btn1presionado = true;
      if (btn2Actual) btn2presionado = true;

      if (!btn1actual && btn1presionado && !btn2Actual) {
        sendInterval += 30000;
        btn1presionado = false;
      }
      
      if (!btn2Actual && btn2presionado && !btn1actual) {
        if (sendInterval > 30000) {
          sendInterval -= 30000;
        }
        btn2presionado = false;
      }

      if (btn1presionado && btn2presionado) {
        btn1presionado = false;
        btn2presionado = false;
        maquinaDeEstado = Pantalla2APantalla1;
      }
      break;

    case Pantalla2APantalla1:
      if (!btn1actual && !btn2Actual) {
        maquinaDeEstado = Pantalla1;
      }
      break;
  }

  if (app.ready()) {
    unsigned long tiempoActual = millis();
    if (tiempoActual - lastSendTime >= sendInterval) {
      lastSendTime = tiempoActual;

      uid = app.getUid().c_str();
      databasePath = "/UsersData/" + uid + "/readings";

      timestamp = getTime();
      Serial.print("Enviando datos - time: ");
      Serial.println(timestamp);

      parentPath = databasePath + "/" + String(timestamp);

      writer.create(obj1, tempPath, temperature);
      writer.create(obj2, timePath, timestamp);
      writer.join(jsonData, 2, obj1, obj2);

      Database.set<object_t>(aClient, parentPath, jsonData, processData, "RTDB_Send_Data");
    }
  }
}

void processData(AsyncResult &aResult) {
  if (!aResult.isResult()) return;
  if (aResult.isEvent()) Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
  if (aResult.isDebug()) Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
  if (aResult.isError()) Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
  if (aResult.available()) Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
}