// ====================== Libraries ====================== //
#include "unihiker_k10.h"
#include <WiFi.h>
#include <SD.h>
#include <HTTPClient.h>
// ====================== Libraries ====================== //

// ====================== Wi-fi Config ====================== //
const char* ssid = "LA BENDICION DE DIOS ";
const char* password = "LOSCATAMOS05";
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
  
  mostrarTexto("Grabando audio...", 20, 100, 0xFFFF00);
  music.recordSaveToTFCard("S:/sound.wav", 4); 
  delay(300);

  Serial.println("📦 Creando paquete combinado en la SD...");
  mostrarTexto("Empaquetando...", 20, 100, 0x00FFFF);
  
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
  
  http.setTimeout(360000); // Mantenemos tus 6 minutos de tolerancia máxima
  
  http.addHeader("Content-Type", "application/octet-stream");
  http.addHeader("X-Foto-Size", String(fotoOriginalSize));

  int httpCode = http.sendRequest("POST", &paqueteEnvio, paqueteEnvio.size());
  paqueteEnvio.close();

  if (httpCode > 0) {
    Serial.printf("📥 Servidor n8n respondió: %d\n", httpCode);
    
    if (httpCode == HTTP_CODE_OK) {
      mostrarTexto("Procesando Voz...", 20, 100, 0x00FF00);
      Serial.println("📥 Descargando respuesta binaria de n8n...");
      
      // Cambiar a /response.wav si mantienes la salida OpenAI en MP3
      if (SD.exists("/response.wav")) {
        SD.remove("/response.wav");
      }
      
      File audioRespuesta = SD.open("/response.wav", FILE_WRITE);
      if (audioRespuesta) {
        WiFiClient* stream = http.getStreamPtr();
        uint8_t bufferDescarga[1024]; // Aumentamos el buffer a 1024 para mayor velocidad
        
        // 🔄 NUEVO BUCLE DE DESCARGA SEGURO A PRUEBA DE CORTES PREMATUROS
        while (http.connected() && stream->available() == 0) {
          delay(10); // Espera activa muy corta en caso de pequeños retrasos de red
        }
        
        int bytesLeidos = 0;
        while (http.connected() || stream->available()) {
          while (stream->available() > 0) {
            int len = stream->read(bufferDescarga, sizeof(bufferDescarga));
            if (len > 0) {
              audioRespuesta.write(bufferDescarga, len);
              bytesLeidos += len;
            }
          }
          delay(1);
        }
        audioRespuesta.close();
        Serial.printf("💾 Descarga completa. Total bytes guardados: %d\n", bytesLeidos);
        
        mostrarTexto("Hablando...", 40, 100, 0xFF00FF);
        Serial.println("🔊 Reproduciendo respuesta de la IA...");
        
        // Reproducir el archivo descargado
        music.playTFCardAudio("S:/response.wav"); 
        
      } else {
        Serial.println("❌ No se pudo crear el archivo en la SD.");
        mostrarTexto("Error local File", 20, 100, 0xFF0000);
      }
      
    } else {
      Serial.printf("❌ n8n retornó código de error: %d\n", httpCode);
      mostrarTexto("Error n8n", 50, 100, 0xFF0000);
    }
  } else {
    Serial.printf("❌ Error de envío. Código: %s\n", http.errorToString(httpCode).c_str());
    mostrarTexto("Error Envio", 40, 100, 0xFF0000);
  }

  http.end();
  delay(1000);
  mostrarTexto("Listo", 100, 100, 0x00FF00);
  k10.rgb->write(-1, 0, 0, 0); 
  ocupado = false;
}