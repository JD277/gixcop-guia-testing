// ====================== Libraries ====================== //
#include "unihiker_k10.h"
#include <WiFi.h>
#include <SD.h>
#include <HTTPClient.h>
// ====================== Libraries ====================== //

// ====================== Wi-fi Config ====================== //
const char* ssid = "TP-Link MIGUEL";
const char* password = "jdam1825";
const char* webhookUrl = "https://newserver-n8n.5bxr29.easypanel.host/webhook/099aafb3-27de-473e-a1a7-934d77943d3f";
// ====================== Wi-fi Config ====================== //

UNIHIKER_K10 k10;
Music music;
bool wifiConnected = false;
bool ocupado = false;

const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

void mostrarTexto(const char* texto, int x, int y, uint32_t color) {
  k10.canvas->canvasClear();
  k10.canvas->canvasText(texto, x, y, color, k10.canvas->eCNAndENFont24, 25, true);
  k10.canvas->updateCanvas();
}

// Escribe un archivo directamente en Base64 dentro de otro archivo abierto (Streaming a SD)
void writeAudioAsBase64ToPaquete(File &audioFile, File &paqueteFile) {
  uint8_t inputBuffer[3];
  uint8_t outputBuffer[4];
  int count = 0;

  while (audioFile.available()) {
    inputBuffer[count++] = audioFile.read();
    if (count == 3) {
      outputBuffer[0] = (inputBuffer[0] & 0xFC) >> 2;
      outputBuffer[1] = ((inputBuffer[0] & 0x03) << 4) | ((inputBuffer[1] & 0xF0) >> 4);
      outputBuffer[2] = ((inputBuffer[1] & 0x0F) << 2) | ((inputBuffer[2] & 0xC0) >> 6);
      outputBuffer[3] = inputBuffer[2] & 0x3F;

      for (int i = 0; i < 4; i++) {
        paqueteFile.print((char)b64_table[outputBuffer[i]]);
      }
      count = 0;
    }
  }

  if (count > 0) {
    outputBuffer[0] = (inputBuffer[0] & 0xFC) >> 2;
    outputBuffer[1] = ((inputBuffer[0] & 0x03) << 4);
    if (count == 2) {
      outputBuffer[1] |= ((inputBuffer[1] & 0xF0) >> 4);
      outputBuffer[2] = (inputBuffer[1] & 0x0F) << 2;
    }
    
    paqueteFile.print((char)b64_table[outputBuffer[0]]);
    paqueteFile.print((char)b64_table[outputBuffer[1]]);
    if (count == 2) {
      paqueteFile.print((char)b64_table[outputBuffer[2]]);
    } else {
      paqueteFile.print('=');
    }
    paqueteFile.print('=');
  }
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
  
  WiFi.begin(ssid, password);
  mostrarTexto("Conectando...", 40, 100, 0xFFFFFF);
  
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("\n✅ WiFi Conectado");
  } else {
    mostrarTexto("Sin WiFi", 80, 100, 0xFF0000);
    return;
  }
  
  if (!SD.begin(21)) {
    mostrarTexto("Error SD", 80, 100, 0xFF0000);
    return;
  }
  
  k10.buttonA->setPressedCallback(onButtonAPressed);
  mostrarTexto("Listo", 100, 100, 0x00FF00);
}

void loop() {
  delay(10); 
}

void onButtonAPressed() {
  if (!wifiConnected || ocupado) return;
  
  ocupado = true;
  k10.rgb->write(-1, 0x00FF00); 
  
  // 🔹 PASO 1: Captura de imagen limpia
  Serial.println("📸 Capturando foto...");
  k10.canvas->canvasClear(); 
  k10.setBgCamerImage(true);  
  k10.canvas->updateCanvas(); 
  delay(1200); 
  
  k10.photoSaveToTFCard("S:/photo.bmp");
  k10.setBgCamerImage(false);
  k10.canvas->canvasClear();
  k10.canvas->updateCanvas();
  delay(300);
  
  // 🔹 PASO 2: Grabar audio (2 segundos)
  mostrarTexto("Grabando audio...", 20, 100, 0xFFFF00);
  music.recordSaveToTFCard("S:/sound.wav", 4);
  delay(300);

  // 🔹 PASO 3: Ensamble del paquete combinado en la SD (RAM Cero)
  Serial.println("📦 Creando paquete combinado en la SD...");
  mostrarTexto("Empaquetando...", 20, 100, 0x00FFFF);
  
  // Si existía un paquete anterior, lo borramos para asegurar espacio limpio
  if (SD.exists("/paquete.bin")) {
    SD.remove("/paquete.bin");
  }

  File foto = SD.open("/photo.bmp", FILE_READ);
  File audio = SD.open("/sound.wav", FILE_READ);
  File paquete = SD.open("/paquete.bin", FILE_WRITE);

  if (!foto || !audio || !paquete) {
    Serial.println("❌ Error abriendo archivos en SD");
    mostrarTexto("Error SD Pack", 40, 100, 0xFF0000);
    if(foto) foto.close();
    if(audio) audio.close();
    if(paquete) paquete.close();
    ocupado = false;
    return;
  }

  // Guardamos el tamaño exacto de la foto original para pasárselo a n8n
  long fotoOriginalSize = foto.size();

  // 1. Copiar los bytes binarios de la foto directamente
  uint8_t copyBuf[512];
  while (foto.available()) {
    int len = foto.read(copyBuf, sizeof(copyBuf));
    paquete.write(copyBuf, len);
  }
  foto.close();

  // 2. Inyectar el divisor textual exacto
  paquete.print("|||");

  // 3. Convertir y volcar el audio en Base64 en tiempo real
  writeAudioAsBase64ToPaquete(audio, paquete);
  audio.close();
  
  // Guardamos el tamaño total del paquete y cerramos el archivo para salvar cambios
  long paqueteTotalSize = paquete.size();
  paquete.close();

  // 🔹 PASO 4: Enviar el paquete unificado usando el método nativo robusto
  File paqueteEnvio = SD.open("/paquete.bin", FILE_READ);
  if (!paqueteEnvio) {
    Serial.println("❌ Error al reabrir el paquete unificado.");
    ocupado = false;
    return;
  }

  mostrarTexto("Enviando Pack...", 20, 100, 0x00FFFF);
  Serial.println("🚀 Transmitiendo paquete unificado por HTTPS...");

  HTTPClient http;
  http.begin(webhookUrl);
  http.setTimeout(30000); // 30 segundos de paciencia para n8n
  
  // Enviamos metadatos ligeros e inofensivos en los headers (no causan desborde)
  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("X-Foto-Size", String(fotoOriginalSize));

  // El método nativo de streaming por fragmentos que el core de Unihiker maneja a la perfección
  int httpCode = http.sendRequest("POST", &paqueteEnvio, paqueteEnvio.size());
  paqueteEnvio.close();

  // 🔹 PASO 5: Evaluar respuesta
  if (httpCode > 0) {
    Serial.printf("📥 Servidor n8n respondió: %d\n", httpCode);
    if (httpCode >= 200 && httpCode < 300) {
      mostrarTexto("Enviado OK", 60, 100, 0x00FF00);
    } else {
      mostrarTexto("Error n8n", 50, 100, 0xFF0000);
    }
  } else {
    Serial.printf("❌ Error de envío. Código: %s\n", http.errorToString(httpCode).c_str());
    mostrarTexto("Error HTTPS", 50, 100, 0xFF0000);
  }

  http.end();
  delay(2000);
  mostrarTexto("Listo", 100, 100, 0x00FF00);
  k10.rgb->write(-1, 0x000000); 
  ocupado = false;
}