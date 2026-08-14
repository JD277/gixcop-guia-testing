// ====================== Libraries ====================== //
#include "unihiker_k10.h"
#include <WiFi.h>
#include <SD.h>
#include <HTTPClient.h>
// ====================== Libraries ====================== //

// ====================== Wi-fi Config ====================== //
const char* ssid = "Perc";
const char* password = "Alelu2410";
const char* webhookUrl = "https://newserver-n8n.5bxr29.easypanel.host/webhook/099aafb3-27de-473e-a1a7-934d77943d3f";

// Configuración de Polling para esperar el audio generado por IA
const int MAX_POLL_RETRIES = 20;      // Máximo de intentos (~60-80s total)
const int INITIAL_POLL_DELAY = 20000;  // Primer reintento a los 2s
const int MAX_POLL_DELAY = 5000;      // Tope máximo entre reintentos
const int DOWNLOAD_TIMEOUT = 30000;   // Timeout para descarga de audio desde CDN
// ====================== Wi-fi Config ====================== //

UNIHIKER_K10 k10;
Music music; 
bool wifiConnected = false;
bool ocupado = false;

const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// ====================== Funciones Auxiliares ====================== //

void mostrarTexto(const char* texto, int x, int y, uint32_t color) {
  k10.canvas->canvasClear();
  k10.canvas->canvasText(texto, x, y, color, k10.canvas->eCNAndENFont24, 25, true);
  k10.canvas->updateCanvas();
}

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

/**
 * Extrae un valor string de un JSON simple sin librerías pesadas.
 * Busca "clave":"valor" y retorna valor.
 */
String extractJsonString(const String& json, const String& key) {
  String searchKey = "\"" + key + "\":\"";
  int startIdx = json.indexOf(searchKey);
  if (startIdx == -1) return "";
  
  startIdx += searchKey.length();
  int endIdx = json.indexOf("\"", startIdx);
  if (endIdx == -1) return "";
  
  return json.substring(startIdx, endIdx);
}

/**
 * Descarga robusta con validación de Content-Length y timeout de inactividad.
 * Retorna true solo si se recibieron TODOS los bytes esperados.
 */
bool descargarAudioRobusto(HTTPClient &http, const char* filePath) {
  if (SD.exists(filePath)) SD.remove(filePath);
  
  File outFile = SD.open(filePath, FILE_WRITE);
  if (!outFile) {
    Serial.println("❌ No se pudo crear archivo de descarga en SD");
    return false;
  }

  int expectedSize = http.getSize();
  Serial.printf("📏 Tamaño esperado: %d bytes\n", expectedSize);
  
  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[1024];
  int totalRead = 0;
  unsigned long lastDataTime = millis();
  
  while (totalRead < expectedSize || expectedSize <= 0) {
    // Timeout de inactividad (no datos en DOWNLOAD_TIMEOUT ms)
    if (millis() - lastDataTime > DOWNLOAD_TIMEOUT) {
      Serial.println("⏰ Timeout de inactividad en descarga");
      break;
    }
    
    if (stream->available()) {
      lastDataTime = millis();
      int len = stream->read(buffer, sizeof(buffer));
      if (len > 0) {
        outFile.write(buffer, len);
        totalRead += len;
      }
    } else {
      delay(5); // Ceder CPU al stack WiFi/TLS
    }
  }
  
  outFile.close();
  
  // Validación estricta
  bool success = (expectedSize > 0 && totalRead == expectedSize);
  
  if (success) {
    Serial.printf("✅ Descarga verificada: %d/%d bytes\n", totalRead, expectedSize);
  } else {
    Serial.printf("❌ Descarga CORRUPTA: recibido %d de %d bytes\n", totalRead, expectedSize);
    SD.remove(filePath); // Eliminar archivo parcial/corrupto
  }
  
  return success;
}

// ====================== Setup & Loop ====================== //

void setup() {
  Serial.begin(115200);
  k10.begin();
  k10.initScreen(2);
  k10.creatCanvas();
  k10.initBgCamerImage();
  k10.setBgCamerImage(false);
  k10.initSDFile();
  k10.setScreenBackground(0x000000);
  
  // Desactivar ahorro de energía WiFi para evitar micro-cortes TLS
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  
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

// ====================== Callback Botón A ====================== //

void onButtonAPressed() {
  if (!wifiConnected || ocupado) return;
  
  ocupado = true;
  k10.rgb->write(-1, 0x00FF00); 
  
  // ===== FASE 1: Captura y Empaquetado =====
  k10.canvas->canvasClear(); 
  k10.setBgCamerImage(true);  
  k10.canvas->updateCanvas(); 
  //delay(300); 
  
  k10.photoSaveToTFCard("S:/photo.bmp"); 
  k10.setBgCamerImage(false);
  k10.canvas->canvasClear();
  k10.canvas->updateCanvas();
  delay(300);
  
  mostrarTexto("Grabando audio...", 20, 100, 0xFFFF00);
  music.recordSaveToTFCard("S:/sound.wav", 4); 
  delay(300);

  Serial.println("📦 Creando paquete combinado en la SD...");
  mostrarTexto("Empaquetando...", 20, 100, 0x00FFFF);
  
  if (SD.exists("/paquete.bin")) SD.remove("/paquete.bin");

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

  long fotoOriginalSize = foto.size();
  uint8_t copyBuf[512];
  while (foto.available()) {
    int len = foto.read(copyBuf, sizeof(copyBuf));
    paquete.write(copyBuf, len);
  }
  foto.close();

  paquete.print("|||");
  writeAudioAsBase64ToPaquete(audio, paquete);
  audio.close();
  paquete.close();

  // ===== FASE 2: Envío al Webhook y Recepción de URL =====
  File paqueteEnvio = SD.open("/paquete.bin", FILE_READ);
  if (!paqueteEnvio) {
    Serial.println("❌ Error al reabrir el paquete unificado.");
    ocupado = false;
    return;
  }

  mostrarTexto("Enviando...", 20, 100, 0x00FFFF);
  Serial.println("🚀 Transmitiendo paquete a n8n...");

  String audioUrl = "";
  {
    HTTPClient http;
    http.begin(webhookUrl);
    http.setTimeout(60000); // 30s es suficiente para recibir JSON inmediato
    http.addHeader("Content-Type", "application/octet-stream");
    http.addHeader("X-Foto-Size", String(fotoOriginalSize));

    int httpCode = http.sendRequest("POST", &paqueteEnvio, paqueteEnvio.size());
    paqueteEnvio.close();

    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();
      Serial.printf("📨 Respuesta webhook: %s\n", payload.c_str());
      audioUrl = extractJsonString(payload, "audioUrl");
      
      if (audioUrl.length() < 10) {
        Serial.println("❌ No se encontró audioUrl en la respuesta");
        mostrarTexto("Error Respuesta", 20, 100, 0xFF0000);
        http.end();
        ocupado = false;
        return;
      }
    } else {
      Serial.printf("❌ Error webhook: %d - %s\n", httpCode, http.errorToString(httpCode).c_str());
      mostrarTexto("Error Envio", 40, 100, 0xFF0000);
      http.end();
      ocupado = false;
      return;
    }
    http.end(); // ⚡ CERRAR conexión webhook ANTES de polling
  }

  // ===== FASE 3: Polling con Backoff hasta que el audio esté listo =====
  Serial.printf("🔗 Audio URL: %s\n", audioUrl.c_str());
  mostrarTexto("Pensando...", 40, 100, 0xFFFF00);
  
  bool audioListo = false;
  int currentDelay = INITIAL_POLL_DELAY;
  
  for (int attempt = 0; attempt < MAX_POLL_RETRIES; attempt++) {
    Serial.printf("🔄 Polling intento %d/%d (espera %ds)...\n", 
                  attempt + 1, MAX_POLL_RETRIES, currentDelay / 1000);
    
    delay(currentDelay);
    
    HTTPClient httpPoll;
    httpPoll.begin(audioUrl);
    httpPoll.setTimeout(DOWNLOAD_TIMEOUT);
    
    int pollCode = httpPoll.GET();
    
    if (pollCode == HTTP_CODE_OK) {
      Serial.println("✅ Audio disponible en Supabase!");
      audioListo = descargarAudioRobusto(httpPoll, "/response.wav");
      httpPoll.end();
      break;
      
    } else if (pollCode == HTTP_CODE_NOT_FOUND) {
      Serial.println("⏳ Audio aún generándose...");
      httpPoll.end();
      // Backoff progresivo: 2s → 3s → 4s → 5s (tope)
      currentDelay = min(currentDelay + 1000, MAX_POLL_DELAY);
      
    } else {
      Serial.printf("❌ Error inesperado en polling: %d\n", pollCode);
      httpPoll.end();
      break;
    }
  }
  
  // ===== FASE 4: Reproducción =====
  if (audioListo) {
    mostrarTexto("Hablando...", 40, 100, 0xFF00FF);
    music.playTFCardAudio("S:/response.wav");
  } else {
    mostrarTexto("Ocurrio un error Audio", 20, 100, 0xFF0000);
  }

  // ===== Limpieza Final =====
  delay(1000);
  mostrarTexto("Listo", 100, 100, 0x00FF00);
  k10.rgb->write(-1, 0, 0, 0); 
  ocupado = false;
}
