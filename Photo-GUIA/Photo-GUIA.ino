/*!
 * @file  Photo.ino
 * @brief Press the UNIHIKER K10's on-board A button to take a photo and store it to the TF card, and press the B button to display the photo on the screen. 
 * A microSD card must be installed in the UNIHIKER K10 to run this demo.
 * @copyright   Copyright (c) 2021 DFRobot Co.Ltd (http://www.dfrobot.com)
 * @license     The MIT License (MIT)
 * @author      
 * @version     V1.0
 * @date        2024-12-30
 * @url         
 */
#include "unihiker_k10.h"

void onButtonAPressed();
void onButtonBPressed();

UNIHIKER_K10 k10;
uint8_t screen_dir = 2;


void setup() {
  k10.begin();
  k10.initScreen(screen_dir);
  k10.initBgCamerImage();
  k10.setBgCamerImage(false);
  k10.creatCanvas();
  k10.buttonA->setPressedCallback(onButtonAPressed);
  k10.initSDFile();
  k10.buttonB->setPressedCallback(onButtonBPressed);
  k10.setBgCamerImage(true);
}
void loop() {
}


void onButtonAPressed() {
  k10.photoSaveToTFCard("S:/photo.bmp"); // Guarda una foto en la SD
}
void onButtonBPressed() {
  k10.canvas->canvasDrawImage(0, 0, "S:/photo.bmp");
  delay(3000);
  k10.canvas->canvasClear();
  k10.setBgCamerImage(true);  // Reactivar cámara
  k10.canvas->updateCanvas();
}