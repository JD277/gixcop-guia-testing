// ====================== Libraries ====================== //
#include "unihiker_k10.h"
#include <WiFi.h>
#include <SD.h>
#include <HTTPClient.h>
#include <WebServer.h>
// ====================== Libraries ====================== //

// ====================== Wi-fi Config ====================== //
const char* ssid = "TP-Link MIGUEL";
const char* password = "jdam1825";
const char* webhookUrl = "https://newserver-n8n.5bxr29.easypanel.host/webhook/bc98f2bd-4775-4da7-adad-76e8d006d4b8";
WebServer server(80);
// ====================== Wi-fi Config ====================== //

UNIHIKER_K10 k10;
Music music;
bool wifiConnected = false;

void mostrarTexto(const char* texto, int x, int y, uint32_t color) {
  k10.canvas->canvasClear();
  k10.canvas->canvasText(texto, x, y, color, k10.canvas->eCNAndENFont24, 25, true);
  k10.canvas->updateCanvas();
}

void setup() {
  Serial.begin(115200);
  k10.begin();
  k10.initScreen(2);
  k10.creatCanvas();
  k10.initBgCamerImage();
  k10.setBgCamerImage(false);
  k10.initSDFile();
  k10.setScreenBackground(0x000000);
  
  // WiFi con timeout
  WiFi.begin(ssid, password);
  mostrarTexto("Conectando...", 40, 100, 0xFFFFFF);
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n✅ WiFi: " + WiFi.localIP().toString());
  } else {
    Serial.println("\n❌ WiFi FAIL");
    mostrarTexto("Sin WiFi", 80, 100, 0xFF0000);
    return;
  }
  
  // SD
  if (!SD.begin(21)) {
    Serial.println("❌ SD Error");
    mostrarTexto("Error SD", 80, 100, 0xFF0000);
    return;
  }
  
  k10.buttonA->setPressedCallback(onButtonAPressed);
  mostrarTexto("Listo", 100, 100, 0x00FF00);
}

void loop() {
}

void onButtonAPressed() {
  if (!wifiConnected) {
    mostrarTexto("Sin WiFi", 80, 100, 0xFF0000);
    return;
  }
  
  k10.rgb->write(-1, 0x00FF00);
  
  // 🔹 PASO 1: Tomar foto (cámara activa)
  k10.setBgCamerImage(true);
  k10.canvas->updateCanvas();
  delay(500);
  k10.photoSaveToTFCard("S:/photo.bmp");
  k10.setBgCamerImage(false);  // 🔹 DESACTIVAR CÁMARA INMEDIATAMENTE
  
  // 🔹 PASO 2: Grabar audio
  mostrarTexto("Grabando audio...", 20, 100, 0xFFFF00);
  k10.canvas->updateCanvas();
  
  music.recordSaveToTFCard("S:/sound.wav", 2);
  
  // 🔹 PASO 3: Enviar a n8n (sin cámara activa)
  mostrarTexto("Enviando...", 70, 100, 0x00FFFF);
  k10.canvas->updateCanvas();
  
  enviarWAVaN8N("/sound.wav");  // 🔹 RUTA CON "/"
  
  // 🔹 PASO 4: Confirmación
  mostrarTexto("Listo", 100, 100, 0x00FF00);
  k10.rgb->write(-1, 0x000000);
}



