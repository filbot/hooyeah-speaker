/*
  Hooyeah Speaker: ESP8266 Webhook Trigger

  - Connects to Wi‑Fi using credentials in config/secrets.h
  - Hosts a minimal HTTP server on port 80
  - Endpoint /trigger pulses an output LOW to ground an Adafruit Audio FX trigger pin

  Hardware notes:
  - Use a safe GPIO (e.g., D1 = GPIO5) on ESP8266.
  - Idle state is high‑impedance (INPUT) to emulate open‑drain.
  - On trigger, drive LOW for ~150 ms, then return to INPUT.
  - Prefer an NPN transistor or MOSFET to sink the Audio FX trigger current.
*/

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#include "secrets.h"  // Provide WIFI_SSID, WIFI_PASS, optional WEBHOOK_TOKEN

// Choose a safe pin (avoid boot pins like GPIO0, GPIO2, GPIO15).
// D1 (GPIO5) is a good default on NodeMCU/WeMos.
#ifndef TRIGGER_PIN
#define TRIGGER_PIN D1
#endif

// Default pulse length in milliseconds
#ifndef TRIGGER_PULSE_MS
#define TRIGGER_PULSE_MS 150
#endif

ESP8266WebServer server(80);

static void setIdleOpenDrain() {
  // High-impedance when idle to avoid driving the line HIGH.
  pinMode(TRIGGER_PIN, INPUT);
}

static void pulseTrigger(uint16_t ms) {
  // Drive LOW to emulate grounding the Audio FX trigger.
  pinMode(TRIGGER_PIN, OUTPUT);
  digitalWrite(TRIGGER_PIN, LOW);
  delay(ms);
  setIdleOpenDrain();
}

static bool tokenRequired() {
#ifdef WEBHOOK_TOKEN
  return (WEBHOOK_TOKEN[0] != '\0');
#else
  return false;
#endif
}

static bool checkAuth() {
  if (!tokenRequired()) return true;
  if (!server.hasArg("token")) return false;
  String t = server.arg("token");
  return t.equals(WEBHOOK_TOKEN);
}

void handleRoot() {
  server.send(200, "application/json",
              String("{\n  \"name\": \"hooyeah-speaker\",\n  \"endpoints\": [\"/trigger\", \"/healthz\"],\n  \"trigger_pin\": ") + TRIGGER_PIN + "\n}\n");
}

void handleHealth() { server.send(200, "text/plain", "ok"); }

void handleTrigger() {
  if (!checkAuth()) {
    server.send(401, "text/plain", "unauthorized");
    return;
  }

  uint16_t ms = TRIGGER_PULSE_MS;
  if (server.hasArg("ms")) {
    int v = server.arg("ms").toInt();
    if (v > 0) ms = (uint16_t)constrain(v, 20, 2000);
  }

  pulseTrigger(ms);
  server.send(200, "application/json", String("{\n  \"status\": \"triggered\", \"pulse_ms\": ") + ms + "}\n");
}

void setup() {
  setIdleOpenDrain();
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // Off on many ESP8266 boards

  Serial.begin(115200);
  Serial.println();
  Serial.println(F("[boot] Hooyeah Speaker webhook trigger"));

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("[wifi] Connecting to %s", WIFI_SSID);
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print('.');
    if ((++dots % 4) == 0) {
      // brief blink while connecting
      digitalWrite(LED_BUILTIN, LOW);
      delay(60);
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }
  Serial.printf("\n[wifi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());

  if (MDNS.begin("hooyeah")) {
    Serial.println(F("[mdns] Started at http://hooyeah.local"));
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/healthz", HTTP_GET, handleHealth);
  server.on("/trigger", HTTP_ANY, handleTrigger);  // allow GET or POST
  server.onNotFound([] { server.send(404, "text/plain", "not found"); });
  server.begin();
  Serial.println(F("[http] Server listening on :80"));
}

void loop() {
  server.handleClient();
  MDNS.update();
}

