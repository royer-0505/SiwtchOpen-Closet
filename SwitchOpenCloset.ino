#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* WIFI_SSID  = "Tesalia";
const char* WIFI_PASS  = "Tesalia2025@";
const char* SERVER_URL = "http:192.168.31.243:5000";

//Pin del Rele
#define RELAY_PIN 13

const bool RELAY_ACTIVE_LOW = false;

bool ledEncendido = false;

unsigned long ultimaConsulta = 0;
const unsigned long INTERVALO_MS = 2000;

void aplicarEstado(bool encendido) {
  ledEncendido = encendido;

  bool nivelGPIO;

  if (RELAY_ACTIVE_LOW) {
    nivelGPIO = !encendido;
  } else {
    nivelGPIO = encendido;
  }

  digitalWrite(RELAY_PIN, nivelGPIO ? HIGH : LOW);

  Serial.println(
    encendido ? "RELEVADOR ACTIVADO" : "RELEVADOR DESACTIVADO"
  );
}

void reportarEstado() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado. No se reporta estado.");
    return;
  }

  HTTPClient http;
  String url = String(SERVER_URL) + "/api/esp32/estado";

  http.begin(url);
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  JsonDocument doc;
  doc["led"] = ledEncendido ? "on" : "off";

  String cuerpo;
  serializeJson(doc, cuerpo);

  int codigo = http.POST(cuerpo);

  Serial.printf("POST estado -> codigo %d\n", codigo);

  if (codigo > 0) {
    Serial.println("Respuesta: " + http.getString());
  } else {
    Serial.println(http.errorToString(codigo));
  }

  http.end();
}

void consultarServidor() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi desconectado");
    return;
  }

  HTTPClient http;
  String url = String(SERVER_URL) + "/api/esp32/config";

  http.begin(url);
  http.setTimeout(3000);

  int codigo = http.GET();
  bool cambioEstado = false;

  if (codigo == HTTP_CODE_OK) {
    String respuesta = http.getString();
    Serial.println("Respuesta: " + respuesta);

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, respuesta);

    if (!error) {
      const char* valor = doc["led"] | "off";

      if (strcmp(valor, "on") == 0) {
        cambioEstado = !ledEncendido;

        if (cambioEstado) {
          aplicarEstado(true);
        }
      } else if (strcmp(valor, "off") == 0) {
        cambioEstado = ledEncendido;

        if (cambioEstado) {
          aplicarEstado(false);
        }
      } else {
        Serial.println("Orden desconocida; se conserva el estado.");
      }
    } else {
      Serial.print("Error JSON: ");
      Serial.println(error.c_str());
    }
  } else {
    Serial.printf("GET config -> error %d\n", codigo);

    if (codigo < 0) {
      Serial.println(http.errorToString(codigo));
    }
  }

  http.end();

  if (cambioEstado) {
    reportarEstado();
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(RELAY_PIN, OUTPUT);

  // Estado inicial seguro: relevador desactivado.
  digitalWrite(
    RELAY_PIN,
    RELAY_ACTIVE_LOW ? HIGH : LOW
  );

  WiFi.begin(WIFI_SSID, WIFI_PASS);

  Serial.print("Conectando a WiFi");

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi conectado");
  Serial.print("IP del ESP32: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    delay(500);
    return;
  }

  if (millis() - ultimaConsulta >= INTERVALO_MS) {
    ultimaConsulta = millis();
    consultarServidor();
  }
}