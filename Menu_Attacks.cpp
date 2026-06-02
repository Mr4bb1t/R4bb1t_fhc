#include "Menu_Attacks.h"
#include "Globals.h"
#include "UI.h"
#include "Radio.h"
#include "Attacks.h"
#include "Captive.h"
#include "Menu_Networks.h"
#include <SPIFFS.h>

void displayMenuAtaques() {
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextSize(1);
  tft.setCursor(2, 2);
  tft.setTextColor(TFT_CYAN);
  
  String ssidTruncado = ssidSelecionado;
  if (ssidTruncado.length() > 20) {
    ssidTruncado = ssidTruncado.substring(0, 20) + "..";
  }
  tft.println(ssidTruncado);
  
  tft.setCursor(30, 15);
  tft.setTextColor(TFT_YELLOW);
  tft.println("ATAQUES");
  
  int yPos = 32;
  int spacing = 24;
  
  // Back
  if (opcaoAtaqueSelecionada == 0) {
    tft.fillRect(0, yPos, 128, 18, TFT_DARKGREY);
    drawText(8, yPos + 5, "< VOLTAR", TFT_YELLOW);
  } else {
    drawText(8, yPos + 5, "< Voltar", TFT_DARKGREY);
  }
  
  yPos += spacing;
  
  // Captive Portal
  if (opcaoAtaqueSelecionada == 1) {
    tft.fillRect(0, yPos, 5, 18, TFT_GREEN);
    tft.fillRect(5, yPos, 123, 18, 0x1082);
    drawText(10, yPos + 5, "Captive Portal", TFT_WHITE);
  } else {
    drawText(10, yPos + 5, "Captive Portal", TFT_GREEN);
  }
  
  yPos += spacing;
  
  // Deauther
  if (opcaoAtaqueSelecionada == 2) {
    tft.fillRect(0, yPos, 5, 18, TFT_RED);
    tft.fillRect(5, yPos, 123, 18, 0x1082);
    drawText(10, yPos + 5, "Deauther", TFT_WHITE);
  } else {
    drawText(10, yPos + 5, "Deauther", TFT_RED);
  }
  
  yPos += spacing;
  
  // Handshake
  if (opcaoAtaqueSelecionada == 3) {
    tft.fillRect(0, yPos, 5, 18, TFT_MAGENTA);
    tft.fillRect(5, yPos, 123, 18, 0x1082);
    drawText(10, yPos + 5, "Cap Handshake", TFT_WHITE);
  } else {
    drawText(10, yPos + 5, "Cap Handshake", TFT_MAGENTA);
  }
  
  yPos += spacing;
  
  // Beacon
  if (opcaoAtaqueSelecionada == 4) {
    tft.fillRect(0, yPos, 5, 18, TFT_ORANGE);
    tft.fillRect(5, yPos, 123, 18, 0x1082);
    drawText(10, yPos + 5, "Beacon Spam", TFT_WHITE);
  } else {
    drawText(10, yPos + 5, "Beacon Spam", TFT_ORANGE);
  }
}

void handleMenuAtaques() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      if (opcaoAtaqueSelecionada > 0) {
        opcaoAtaqueSelecionada--;
      } else {
        opcaoAtaqueSelecionada = 4;
      }
      lastDebounceTime = millis();
      displayMenuAtaques();
    }

    if (digitalRead(BUTTON_RIGHT) == LOW) {
      if (opcaoAtaqueSelecionada < 4) {
        opcaoAtaqueSelecionada++;
      } else {
        opcaoAtaqueSelecionada = 0;
      }
      lastDebounceTime = millis();
      displayMenuAtaques();
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      switch(opcaoAtaqueSelecionada) {
        case 0:
          estadoAtual = SELECAO_REDES;
          displayNetworks();
          break;
          
        case 1:
          estadoAtual = ATAQUE_CAPTIVE_PORTAL;
          createCaptivePortal(ssidSelecionado);
          displayAtaqueCaptivePortal();
          break;
          
        case 2:
          estadoAtual = ATAQUE_DEAUTHER;
          displayAtaqueDeauther();
          break;
          
        case 3:
          estadoAtual = ATAQUE_HANDSHAKE;
          displayAtaqueHandshake();
          break;
          
        case 4:
          estadoAtual = ATAQUE_BEACON;
          displayAtaqueBeacon();
          break;
      }
      lastDebounceTime = millis();
    }
  }
}

// ==================== CAPTIVE PORTAL HANDLING ====================

void displayAtaqueCaptivePortal() {
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextSize(1);
  tft.setCursor(15, 10);
  tft.setTextColor(TFT_GREEN);
  tft.println("CAPTIVE PORTAL");
  
  tft.setCursor(2, 30);
  tft.setTextColor(TFT_CYAN);
  String ssidTruncado = ssidSelecionado;
  if (ssidTruncado.length() > 20) {
    ssidTruncado = ssidTruncado.substring(0, 20) + "..";
  }
  tft.println(ssidTruncado);
  
  tft.setCursor(10, 45);
  tft.setTextColor(TFT_RED);
  tft.println("ATAQUE ATIVO");
  
  tft.setCursor(5, 65);
  tft.setTextColor(TFT_WHITE);
  tft.println("Aguardando...");
  
  int yPos = 95;
  
  if (opcaoSubMenuAtaque == 0) {
    tft.fillRect(0, yPos, 128, 14, TFT_DARKGREY);
    drawText(5, yPos + 3, "< VOLTAR", TFT_YELLOW);
  } else {
    drawText(5, yPos + 3, "< Voltar", TFT_DARKGREY);
  }
  
  yPos += 20;
  
  drawText(5, yPos, "[Erase]", opcaoSubMenuAtaque == 1 ? TFT_RED : TFT_WHITE);
  if (opcaoSubMenuAtaque == 1) {
    tft.drawRect(0, yPos - 2, 128, 14, TFT_RED);
  }
  
  yPos += 20;
  
  drawText(5, yPos, "[Credenciais]", opcaoSubMenuAtaque == 2 ? TFT_RED : TFT_WHITE);
  if (opcaoSubMenuAtaque == 2) {
    tft.drawRect(0, yPos - 2, 128, 14, TFT_RED);
  }
}

void handleAtaqueCaptivePortal() {
  dnsServer.processNextRequest();
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      if (opcaoSubMenuAtaque > 0) {
        opcaoSubMenuAtaque--;
      } else {
        opcaoSubMenuAtaque = 2;
      }
      lastDebounceTime = millis();
      displayAtaqueCaptivePortal();
    }

    if (digitalRead(BUTTON_RIGHT) == LOW) {
      if (opcaoSubMenuAtaque < 2) {
        opcaoSubMenuAtaque++;
      } else {
        opcaoSubMenuAtaque = 0;
      }
      lastDebounceTime = millis();
      displayAtaqueCaptivePortal();
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      switch(opcaoSubMenuAtaque) {
        case 0:
          stopCaptivePortal();
          estadoAtual = MENU_ATAQUES;
          displayMenuAtaques();
          break;
          
        case 1:
          EraseData();
          tft.fillScreen(TFT_BLACK);
          tft.setTextSize(1);
          tft.setCursor(15, 70);
          tft.setTextColor(TFT_GREEN);
          tft.println("APAGADO!");
          delay(1500);
          displayAtaqueCaptivePortal();
          break;
          
        case 2:
          estadoAtual = VISUALIZAR_CREDENCIAIS;
          contarCredenciais();
          displayCredenciais();
          break;
      }
      lastDebounceTime = millis();
    }
  }
}

// ==================== DEAUTHER HANDLING ====================

void displayAtaqueDeauther() {
  tft.fillScreen(TFT_BLACK);
  
  tft.setTextSize(1);
  tft.setCursor(20, 10);
  tft.setTextColor(TFT_RED);
  tft.println("DEAUTHER");
  
  tft.setCursor(2, 28);
  tft.setTextColor(TFT_CYAN);
  String ssidTruncado = ssidSelecionado;
  if (ssidTruncado.length() > 20) {
    ssidTruncado = ssidTruncado.substring(0, 20) + "..";
  }
  tft.println(ssidTruncado);
  
  if (!deautherAtivo) {
    tft.setCursor(5, 50);
    tft.setTextColor(TFT_WHITE);
    tft.println("Modo de ataque:");
    
    tft.setCursor(10, 70);
    tft.setTextColor(deauthTipo == 0 ? TFT_RED : TFT_WHITE);
    tft.println("[ Broadcast ]");
    if (deauthTipo == 0) {
      tft.drawRect(5, 68, 118, 14, TFT_RED);
    }
    
    tft.setCursor(10, 90);
    tft.setTextColor(deauthTipo == 1 ? TFT_RED : TFT_WHITE);
    tft.println("[ Targeted ]");
    if (deauthTipo == 1) {
      tft.drawRect(5, 88, 118, 14, TFT_RED);
    }
    
    tft.setCursor(8, 115);
    tft.setTextColor(TFT_GREEN);
    tft.println("< > Selecionar");
    
    tft.setCursor(15, 130);
    tft.setTextColor(TFT_WHITE);
    tft.println("SEL = Iniciar");
    
    tft.setCursor(10, 145);
    tft.setTextColor(TFT_DARKGREY);
    tft.println("HOLD SEL = Voltar");
    
  } else {
    tft.setCursor(5, 50);
    tft.setTextColor(TFT_RED);
    tft.println("ATAQUE ATIVO!");
    
    tft.setCursor(5, 70);
    tft.setTextColor(TFT_WHITE);
    tft.printf("Modo: %s", deauthTipo == 0 ? "Broadcast" : "Targeted");
    
    tft.setCursor(5, 90);
    tft.setTextColor(TFT_YELLOW);
    tft.printf("Enviados: %lu", deauthCounter);
    
    tft.setCursor(5, 110);
    tft.setTextColor(TFT_GREEN);
    tft.println("Desconectando...");
    
    int dotPos = (millis() / 300) % 4;
    tft.setCursor(5, 125);
    for (int i = 0; i < 4; i++) {
      if (i <= dotPos) {
        tft.print(".");
      }
    }
    
    tft.setCursor(15, 145);
    tft.setTextColor(TFT_RED);
    tft.println("SEL = PARAR");
  }
}

void handleAtaqueDeauther() {
  static unsigned long holdStart = 0;
  static bool holdingSelect = false;
  bool selectPressed = (digitalRead(BUTTON_SELECT) == LOW);
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (!deautherAtivo) {
      if (digitalRead(BUTTON_LEFT) == LOW) {
        deauthTipo = 0;
        lastDebounceTime = millis();
        displayAtaqueDeauther();
      }
      
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        deauthTipo = 1;
        lastDebounceTime = millis();
        displayAtaqueDeauther();
      }
      
      if (selectPressed && !holdingSelect) {
        holdStart = millis();
        holdingSelect = true;
      }
      
      if (!selectPressed && holdingSelect) {
        unsigned long holdTime = millis() - holdStart;
        
        if (holdTime > 1000) {
          estadoAtual = MENU_ATAQUES;
          displayMenuAtaques();
        } else {
          if (initRadioForAttack(apRecordSelecionado.primary)) {
            deautherAtivo = true;
            deauthCounter = 0;
            
            if (attackTaskHandle == NULL) {
              attackTaskRunning = true;
              xTaskCreatePinnedToCore(
                attackTask,
                "AttackTask",
                4096,
                NULL,
                1,
                &attackTaskHandle,
                1
              );
            }
            displayAtaqueDeauther();
          } else {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED);
            tft.setCursor(20, 70);
            tft.println("ERRO: Radio!");
            delay(1500);
            displayAtaqueDeauther();
          }
        }
        holdingSelect = false;
        lastDebounceTime = millis();
      }
      
    } else {
      if (selectPressed && !holdingSelect) {
        deautherAtivo = false;
        deinitRadio();
        if (attackTaskHandle != NULL) {
          attackTaskRunning = false;
          vTaskDelay(pdMS_TO_TICKS(300));
          attackTaskHandle = NULL;
        }
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_YELLOW);
        tft.setCursor(30, 70);
        tft.println("PARADO");
        delay(1000);
        displayAtaqueDeauther();
        lastDebounceTime = millis();
        holdingSelect = true;
      }
      if (!selectPressed) {
        holdingSelect = false;
      }
    }
  }
}

// ==================== HANDSHAKE HANDLING ====================

void displayAtaqueHandshake() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 40);
  tft.setTextColor(TFT_MAGENTA);
  tft.println("CAP HANDSHAKE");
  
  tft.setCursor(5, 65);
  tft.setTextColor(TFT_YELLOW);
  tft.println("Em desenvolvimento");
  
  tft.setCursor(2, 85);
  tft.setTextColor(TFT_CYAN);
  String ssidTruncado = ssidSelecionado;
  if (ssidTruncado.length() > 20) {
    ssidTruncado = ssidTruncado.substring(0, 20) + "..";
  }
  tft.println(ssidTruncado);
  
  tft.setCursor(15, 130);
  tft.setTextColor(TFT_WHITE);
  tft.println("SEL = Voltar");
}

void handleAtaqueHandshake() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_SELECT) == LOW) {
      estadoAtual = MENU_ATAQUES;
      displayMenuAtaques();
      lastDebounceTime = millis();
    }
  }
}

// ==================== BEACON HANDLING ====================

void displayAtaqueBeacon() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(15, 10);
  tft.setTextColor(TFT_ORANGE);
  tft.println("BEACON SPAM");
  
  tft.setCursor(2, 28);
  tft.setTextColor(TFT_CYAN);
  String ssidTruncado = ssidSelecionado;
  if (ssidTruncado.length() > 20) {
    ssidTruncado = ssidTruncado.substring(0, 20) + "..";
  }
  tft.println(ssidTruncado);
  
  if (!beaconAtivo) {
    tft.setCursor(10, 50);
    tft.setTextColor(TFT_WHITE);
    tft.println("Redes clones:");
    tft.drawRect(25, 65, 78, 25, TFT_ORANGE);
    tft.drawRect(26, 66, 76, 23, TFT_ORANGE);
    tft.setTextSize(2);
    tft.setCursor(40, 72);
    tft.setTextColor(TFT_YELLOW);
    tft.println(beaconQuantidade);
    tft.setTextSize(1);
    tft.setCursor(8, 100);
    tft.setTextColor(TFT_GREEN);
    tft.println("< > Ajustar");
    tft.setCursor(15, 120);
    tft.setTextColor(TFT_WHITE);
    tft.println("SEL = Iniciar");
    tft.setCursor(10, 145);
    tft.setTextColor(TFT_DARKGREY);
    tft.println("HOLD SEL = Voltar");
  } else {
    tft.setCursor(5, 50);
    tft.setTextColor(TFT_RED);
    tft.println("ATAQUE ATIVO!");
    tft.setCursor(5, 70);
    tft.setTextColor(TFT_WHITE);
    tft.printf("Clones ativos: %d", beaconQuantidade);
    tft.setCursor(5, 90);
    tft.setTextColor(TFT_YELLOW);
    tft.printf("Beacons: %lu", beaconCounter);
    tft.setCursor(5, 110);
    tft.setTextColor(TFT_GREEN);
    tft.println("Transmitindo...");
    int dotPos = (millis() / 300) % 4;
    tft.setCursor(5, 125);
    for (int i = 0; i < 4; i++) {
      if (i <= dotPos) {
        tft.print(".");
      }
    }
    tft.setCursor(15, 145);
    tft.setTextColor(TFT_RED);
    tft.println("SEL = PARAR");
  }
}

void handleAtaqueBeacon() {
  static unsigned long holdStart = 0;
  static bool holdingSelect = false;
  bool selectPressed = (digitalRead(BUTTON_SELECT) == LOW);
  
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (!beaconAtivo) {
      if (digitalRead(BUTTON_LEFT) == LOW) {
        if (beaconQuantidade > 10) {
          beaconQuantidade -= 10;
        } else if (beaconQuantidade > 1) {
          beaconQuantidade -= 1;
        }
        lastDebounceTime = millis();
        displayAtaqueBeacon();
      }
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        if (beaconQuantidade >= 90) {
          beaconQuantidade += 10;
        } else {
          beaconQuantidade += 5;
        }
        if (beaconQuantidade > 200) beaconQuantidade = 200;
        lastDebounceTime = millis();
        displayAtaqueBeacon();
      }
      if (selectPressed && !holdingSelect) {
        holdStart = millis();
        holdingSelect = true;
      }
      if (!selectPressed && holdingSelect) {
        unsigned long holdTime = millis() - holdStart;
        if (holdTime > 1000) {
          estadoAtual = MENU_ATAQUES;
          beaconAtivo = false;
          beaconCounter = 0;
          displayMenuAtaques();
        } else {
          if (initRadioForAttack(apRecordSelecionado.primary)) {
            beaconAtivo = true;
            beaconCounter = 0;
            if (attackTaskHandle == NULL) {
              attackTaskRunning = true;
              xTaskCreatePinnedToCore(
                attackTask,
                "AttackTask",
                4096,
                NULL,
                1,
                &attackTaskHandle,
                1
              );
            }
            displayAtaqueBeacon();
          } else {
            tft.fillScreen(TFT_BLACK);
            tft.setTextColor(TFT_RED);
            tft.setCursor(20, 70);
            tft.println("ERRO: Radio!");
            delay(1500);
            displayAtaqueBeacon();
          }
        }
        holdingSelect = false;
        lastDebounceTime = millis();
      }
    } else {
      if (selectPressed && !holdingSelect) {
        beaconAtivo = false;
        beaconCounter = 0;
        deinitRadio();
        if (attackTaskHandle != NULL) {
          attackTaskRunning = false;
          vTaskDelay(pdMS_TO_TICKS(300));
          attackTaskHandle = NULL;
        }
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_YELLOW);
        tft.setCursor(25, 70);
        tft.println("ATAQUE PARADO");
        delay(1500);
        displayAtaqueBeacon();
        lastDebounceTime = millis();
        holdingSelect = true;
      }
      if (!selectPressed) {
        holdingSelect = false;
      }
    }
  }
}

// ==================== CREDENTIALS HANDLING ====================

void displayCredenciais() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.fillRect(0, 0, 128, 15, TFT_DARKGREY);
  drawText(40, 5, "< VOLTAR", TFT_YELLOW);

  int startY = 25;
  int lineHeight = 16;

  if (SPIFFS.exists("/credenciais.txt")) {
    File file = SPIFFS.open("/credenciais.txt", FILE_READ);
    if (file) {
      int linhaAtual = 0;
      int credencialInicio = paginaCredencialAtual * 5;
      String linha;

      while (file.available()) {
        linha = file.readStringUntil('\n');
        linha.trim();
        if (linha.length() > 0) {
          if (linhaAtual >= credencialInicio && linhaAtual < credencialInicio + 5) {
            tft.setCursor(2, startY + (linhaAtual - credencialInicio) * lineHeight);
            tft.setTextColor(TFT_WHITE);
            if (linha.length() > 20) {
              linha = linha.substring(0, 20) + "..";
            }
            tft.println(linha);
          }
          linhaAtual++;
        }
        if (linhaAtual >= credencialInicio + 5) {
          break;
        }
      }
      file.close();
    }
  } else {
    tft.setCursor(10, 60);
    tft.setTextColor(TFT_RED);
    tft.println("Nenhuma cred");
  }
  tft.setCursor(80, 150);
  tft.setTextColor(TFT_CYAN);
  tft.printf("%d/%d", paginaCredencialAtual + 1, totalCredenciais > 0 ? totalCredenciais : 1);
}

void handleVisualizarCredenciais() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      if (paginaCredencialAtual > 0) {
        paginaCredencialAtual--;
      } else {
        paginaCredencialAtual = totalCredenciais > 0 ? totalCredenciais - 1 : 0;
      }
      lastDebounceTime = millis();
      displayCredenciais();
    }

    if (digitalRead(BUTTON_RIGHT) == LOW) {
      if (paginaCredencialAtual < totalCredenciais - 1) {
        paginaCredencialAtual++;
      } else {
        paginaCredencialAtual = 0;
      }
      lastDebounceTime = millis();
      displayCredenciais();
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      estadoAtual = ATAQUE_CAPTIVE_PORTAL;
      displayAtaqueCaptivePortal();
      lastDebounceTime = millis();
    }
  }
}
