#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include "unihiker_k10.h"

WebServer server(80);
const char* ssid = "TP-Link MIGUEL";
const char* password = "jdam1825";
UNIHIKER_K10 k10;

void setup() {
  Serial.begin(115200);
  k10.begin();
  k10.initScreen(2);
  k10.creatCanvas();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) delay(500);
  
  if (!SD.begin(21)) {
    Serial.println("❌ SD Error");
    return;
  }
  
  // Verificar archivo
  if (!SD.exists("/sound.wav")) {
    Serial.println("❌ sound.wav no existe");
    return;
  }
  
  // Ruta corregida: /download/sound.wav
  server.on("/download/sound.wav", HTTP_GET, []() {
    File file = SD.open("/sound.wav", FILE_READ);
    if (!file) {
      server.send(404, "text/plain", "Archivo no encontrado");
      return;
    }
    
    Serial.print("📤 Enviando: ");
    Serial.println(file.size());
    
    // streamFile envía headers correctos automáticamente
    server.streamFile(file, "audio/wav");
    file.close();
  });
  
  // Página raíz informativa
  server.on("/", HTTP_GET, []() {
    String html = "<h1>K10 Server</h1>";
    html += "<a href='/download/sound.wav'>Descargar sound.wav</a>";
    server.send(200, "text/html", html);
  });
  
  server.begin();
  Serial.println("🌐 http://" + WiFi.localIP().toString());
  
  k10.setScreenBackground(0xFFFFFF);  
  k10.canvas->canvasText(("IP: " + WiFi.localIP().toString()).c_str(), 0, 0x000000);
  k10.canvas->canvasText("Servidor activo", 1, 0x000000);
  k10.canvas->updateCanvas();
}

void loop() {
  server.handleClient();
}