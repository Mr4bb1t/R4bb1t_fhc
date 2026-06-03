#include "Menu_Attacks.h"
#include "Globals.h"
#include "Config.h"
#include "Battery.h"
#include "UI.h"
#include "Radio.h"
#include "Attacks.h"
#include "Captive.h"
#include "Menu_Networks.h"
#include <SPIFFS.h>

// ── Helper: trunca SSID para caber na tela ─────
static String truncSSID(const String &s, int maxLen = 18) {
  if ((int)s.length() > maxLen) return s.substring(0, maxLen - 1) + ".";
  return s;
}

// ═══════════════════════════════════════════════
//  MENU ATAQUES
// ═══════════════════════════════════════════════
void displayMenuAtaques() {
  tft.fillScreen(C_BG);
  tft.setTextSize(1);

  // Header com SSID truncado como subtítulo
  drawHeader("WIFI ATTACKS", true);

  // SSID alvo em dourado escuro — linha abaixo do header
  tft.setTextColor(C_GOLD_DIM);
  String ssid = truncSSID(ssidSelecionado, 21);
  tft.setCursor(4, 17);
  tft.print(ssid);
  tft.drawFastHLine(0, 25, 128, C_GREY);

  // Lista de opções
  const char *items[] = {"< VOLTAR", "Captive Portal", "Deauther",
                          "Cap Handshake", "Beacon Spam"};
  for (int i = 0; i < 5; i++) {
    drawMenuItem(0, 27 + i * 20, 128, 19, items[i],
                 opcaoAtaqueSelecionada == i);
  }

  batteryDraw();
}

void handleMenuAtaques() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      opcaoAtaqueSelecionada = (opcaoAtaqueSelecionada > 0)
                               ? opcaoAtaqueSelecionada - 1 : 4;
      lastDebounceTime = millis();
      displayMenuAtaques();
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      opcaoAtaqueSelecionada = (opcaoAtaqueSelecionada < 4)
                               ? opcaoAtaqueSelecionada + 1 : 0;
      lastDebounceTime = millis();
      displayMenuAtaques();
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      switch (opcaoAtaqueSelecionada) {
        case 0: estadoAtual = SELECAO_REDES; displayNetworks(); break;
        case 1: estadoAtual = ATAQUE_CAPTIVE_PORTAL;
                createCaptivePortal(ssidSelecionado);
                displayAtaqueCaptivePortal(); break;
        case 2: estadoAtual = ATAQUE_DEAUTHER; displayAtaqueDeauther(); break;
        case 3: estadoAtual = ATAQUE_HANDSHAKE; displayAtaqueHandshake(); break;
        case 4: estadoAtual = ATAQUE_BEACON; displayAtaqueBeacon(); break;
      }
      lastDebounceTime = millis();
    }
  }
}

// ═══════════════════════════════════════════════
//  CAPTIVE PORTAL
// ═══════════════════════════════════════════════
void displayAtaqueCaptivePortal() {
  tft.fillScreen(C_BG);
  drawHeader("CAPTIVE PORTAL", true);

  // Status ATIVO
  tft.setTextSize(1);
  tft.setTextColor(C_RED);
  tft.setCursor(40, 20);
  tft.print("[ ATIVO ]");

  // SSID
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 33);
  tft.print(truncSSID(ssidSelecionado, 21));
  drawSeparator(43, C_GREY);

  // Mensagem
  tft.setTextColor(C_WHITE);
  tft.setCursor(8, 50);
  tft.print("Aguardando credenciais");
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(8, 63);
  tft.print("Portal: 192.168.4.1");
  drawSeparator(78, C_GREY);

  // Opções
  drawMenuItem(0, 80, 128, 19, "< VOLTAR",    opcaoSubMenuAtaque == 0);
  drawMenuItem(0, 100, 128, 19, "Apagar dados", opcaoSubMenuAtaque == 1);
  drawMenuItem(0, 120, 128, 19, "Credenciais",  opcaoSubMenuAtaque == 2);

  batteryDraw();
}

void handleAtaqueCaptivePortal() {
  dnsServer.processNextRequest();
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      opcaoSubMenuAtaque = (opcaoSubMenuAtaque > 0) ? opcaoSubMenuAtaque - 1 : 2;
      lastDebounceTime = millis();
      displayAtaqueCaptivePortal();
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      opcaoSubMenuAtaque = (opcaoSubMenuAtaque < 2) ? opcaoSubMenuAtaque + 1 : 0;
      lastDebounceTime = millis();
      displayAtaqueCaptivePortal();
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      switch (opcaoSubMenuAtaque) {
        case 0:
          stopCaptivePortal();
          estadoAtual = MENU_ATAQUES;
          displayMenuAtaques();
          break;
        case 1:
          EraseData();
          tft.fillScreen(C_BG);
          drawHeader("CAPTIVE PORTAL", true);
          tft.setTextColor(C_GREEN);
          tft.setCursor(30, 80);
          tft.print("Dados apagados!");
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

// ═══════════════════════════════════════════════
//  DEAUTHER
// ═══════════════════════════════════════════════
void displayAtaqueDeauther() {
  tft.fillScreen(C_BG);
  drawHeader("DEAUTHER", true);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 17);
  tft.print(truncSSID(ssidSelecionado, 21));
  drawSeparator(26, C_GREY);

  if (!deautherAtivo) {
    tft.setTextColor(C_WHITE);
    tft.setCursor(4, 32);
    tft.print("Modo de ataque:");

    drawMenuItem(0, 44, 128, 19, "Broadcast", deauthTipo == 0, false);
    drawMenuItem(0, 64, 128, 19, "Targeted",  deauthTipo == 1, false);

    drawSeparator(86, C_GREY);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4,  90); tft.print("< > Selecionar modo");
    tft.setCursor(4, 102); tft.print("SEL = Iniciar");
    tft.setCursor(4, 114); tft.print("HOLD SEL = Voltar");
  } else {
    // Ataque ativo
    tft.setTextColor(C_RED);
    tft.setCursor(38, 32);
    tft.print("[ ATIVO ]");

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 50); tft.print("Modo:");
    tft.setTextColor(C_GOLD);
    tft.setCursor(40, 50); tft.print(deauthTipo == 0 ? "Broadcast" : "Targeted");

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 65); tft.print("Enviados:");
    tft.setTextColor(C_WHITE);
    tft.setCursor(60, 65); tft.printf("%lu", deauthCounter);

    // Animação de pontos
    int dotPos = (millis() / 300) % 4;
    tft.setTextColor(C_GREEN);
    tft.setCursor(4, 80);
    for (int i = 0; i < 4; i++) {
      if (i <= dotPos) tft.print(".");
    }

    drawSeparator(92, C_GREY);
    tft.setTextColor(C_RED);
    tft.setCursor(38, 96); tft.print("SEL = PARAR");
  }

  batteryDraw();
}

void handleAtaqueDeauther() {
  static unsigned long holdStart = 0;
  static bool holdingSelect = false;
  bool selectPressed = (digitalRead(BUTTON_SELECT) == LOW);

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (!deautherAtivo) {
      if (digitalRead(BUTTON_LEFT) == LOW) {
        deauthTipo = 0; lastDebounceTime = millis(); displayAtaqueDeauther();
      }
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        deauthTipo = 1; lastDebounceTime = millis(); displayAtaqueDeauther();
      }
      if (selectPressed && !holdingSelect) {
        holdStart = millis(); holdingSelect = true;
      }
      if (!selectPressed && holdingSelect) {
        unsigned long holdTime = millis() - holdStart;
        if (holdTime > 1000) {
          estadoAtual = MENU_ATAQUES; displayMenuAtaques();
        } else {
          if (initRadioForAttack(apRecordSelecionado.primary)) {
            deautherAtivo = true; deauthCounter = 0;
            if (attackTaskHandle == NULL) {
              attackTaskRunning = true;
              xTaskCreatePinnedToCore(attackTask, "AttackTask", 4096,
                                      NULL, 1, &attackTaskHandle, 1);
            }
            displayAtaqueDeauther();
          } else {
            tft.fillScreen(C_BG); drawHeader("DEAUTHER", true);
            tft.setTextColor(C_RED); tft.setCursor(20, 80);
            tft.println("ERRO: Radio!"); delay(1500); displayAtaqueDeauther();
          }
        }
        holdingSelect = false; lastDebounceTime = millis();
      }
    } else {
      if (selectPressed && !holdingSelect) {
        deautherAtivo = false; deinitRadio();
        if (attackTaskHandle != NULL) {
          attackTaskRunning = false;
          vTaskDelay(pdMS_TO_TICKS(300)); attackTaskHandle = NULL;
        }
        tft.fillScreen(C_BG); drawHeader("DEAUTHER", true);
        tft.setTextColor(C_GOLD); tft.setCursor(42, 80); tft.println("PARADO");
        delay(1000); displayAtaqueDeauther();
        lastDebounceTime = millis(); holdingSelect = true;
      }
      if (!selectPressed) holdingSelect = false;
    }
  }
}

// ═══════════════════════════════════════════════
//  HANDSHAKE
// ═══════════════════════════════════════════════
void displayAtaqueHandshake() {
  tft.fillScreen(C_BG);
  drawHeader("CAP HANDSHAKE", true);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 17);
  tft.print(truncSSID(ssidSelecionado, 21));
  drawSeparator(27, C_GREY);

  tft.setTextColor(C_GOLD);
  tft.setCursor(20, 60);
  tft.print("Em desenvolvimento");
  tft.setTextColor(C_GREY);
  tft.setCursor(4, 80);
  tft.print("WPA2 handshake capture");

  drawSeparator(130, C_GREY);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(28, 135);
  tft.print("SEL = Voltar");

  batteryDraw();
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

// ═══════════════════════════════════════════════
//  BEACON SPAM
// ═══════════════════════════════════════════════
void displayAtaqueBeacon() {
  tft.fillScreen(C_BG);
  drawHeader("BEACON SPAM", true);

  tft.setTextSize(1);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(4, 17);
  tft.print(truncSSID(ssidSelecionado, 21));
  drawSeparator(26, C_GREY);

  if (!beaconAtivo) {
    tft.setTextColor(C_WHITE);
    tft.setCursor(8, 34);
    tft.print("Redes clones:");

    // Caixa do contador
    tft.drawRect(38, 46, 52, 26, C_GOLD);
    tft.drawRect(39, 47, 50, 24, C_GOLD_SEL);
    tft.setTextSize(2);
    int numX = 38 + (52 - (int)(String(beaconQuantidade).length()) * 12) / 2;
    tft.setTextColor(C_GOLD);
    tft.setCursor(numX, 53);
    tft.print(beaconQuantidade);
    tft.setTextSize(1);

    drawSeparator(78, C_GREY);
    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(8,  84); tft.print("< > Ajustar quantidade");
    tft.setCursor(8,  96); tft.print("SEL = Iniciar ataque");
    tft.setCursor(8, 108); tft.print("HOLD SEL = Voltar");
  } else {
    tft.setTextColor(C_RED);
    tft.setCursor(38, 32);
    tft.print("[ ATIVO ]");

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 50); tft.print("Clones:");
    tft.setTextColor(C_GOLD);
    tft.setCursor(50, 50); tft.print(beaconQuantidade);

    tft.setTextColor(C_GOLD_DIM);
    tft.setCursor(4, 65); tft.print("Beacons:");
    tft.setTextColor(C_WHITE);
    tft.setCursor(56, 65); tft.printf("%lu", beaconCounter);

    int dotPos = (millis() / 300) % 4;
    tft.setTextColor(C_GREEN);
    tft.setCursor(4, 80);
    for (int i = 0; i < 4; i++) { if (i <= dotPos) tft.print("."); }

    drawSeparator(92, C_GREY);
    tft.setTextColor(C_RED);
    tft.setCursor(34, 96); tft.print("SEL = PARAR");
  }

  batteryDraw();
}

void handleAtaqueBeacon() {
  static unsigned long holdStart = 0;
  static bool holdingSelect = false;
  bool selectPressed = (digitalRead(BUTTON_SELECT) == LOW);

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (!beaconAtivo) {
      if (digitalRead(BUTTON_LEFT) == LOW) {
        if (beaconQuantidade > 10) beaconQuantidade -= 10;
        else if (beaconQuantidade > 1) beaconQuantidade -= 1;
        lastDebounceTime = millis(); displayAtaqueBeacon();
      }
      if (digitalRead(BUTTON_RIGHT) == LOW) {
        beaconQuantidade += (beaconQuantidade >= 90) ? 10 : 5;
        if (beaconQuantidade > 200) beaconQuantidade = 200;
        lastDebounceTime = millis(); displayAtaqueBeacon();
      }
      if (selectPressed && !holdingSelect) {
        holdStart = millis(); holdingSelect = true;
      }
      if (!selectPressed && holdingSelect) {
        unsigned long holdTime = millis() - holdStart;
        if (holdTime > 1000) {
          estadoAtual = MENU_ATAQUES; beaconAtivo = false;
          beaconCounter = 0; displayMenuAtaques();
        } else {
          if (initRadioForAttack(apRecordSelecionado.primary)) {
            beaconAtivo = true; beaconCounter = 0;
            if (attackTaskHandle == NULL) {
              attackTaskRunning = true;
              xTaskCreatePinnedToCore(attackTask, "AttackTask", 4096,
                                      NULL, 1, &attackTaskHandle, 1);
            }
            displayAtaqueBeacon();
          } else {
            tft.fillScreen(C_BG); drawHeader("BEACON SPAM", true);
            tft.setTextColor(C_RED); tft.setCursor(20, 80);
            tft.println("ERRO: Radio!"); delay(1500); displayAtaqueBeacon();
          }
        }
        holdingSelect = false; lastDebounceTime = millis();
      }
    } else {
      if (selectPressed && !holdingSelect) {
        beaconAtivo = false; beaconCounter = 0; deinitRadio();
        if (attackTaskHandle != NULL) {
          attackTaskRunning = false;
          vTaskDelay(pdMS_TO_TICKS(300)); attackTaskHandle = NULL;
        }
        tft.fillScreen(C_BG); drawHeader("BEACON SPAM", true);
        tft.setTextColor(C_GOLD); tft.setCursor(28, 80);
        tft.println("ATAQUE PARADO"); delay(1500); displayAtaqueBeacon();
        lastDebounceTime = millis(); holdingSelect = true;
      }
      if (!selectPressed) holdingSelect = false;
    }
  }
}

// ═══════════════════════════════════════════════
//  CREDENCIAIS
// ═══════════════════════════════════════════════
void displayCredenciais() {
  tft.fillScreen(C_BG);
  drawHeader("CREDENCIAIS", true);
  tft.setTextSize(1);

  int startY    = 18;
  int lineHeight = 16;

  if (SPIFFS.exists("/credenciais.txt")) {
    File file = SPIFFS.open("/credenciais.txt", FILE_READ);
    if (file) {
      int linhaAtual      = 0;
      int credencialInicio = paginaCredencialAtual * 5;
      String linha;
      while (file.available()) {
        linha = file.readStringUntil('\n');
        linha.trim();
        if (linha.length() > 0) {
          if (linhaAtual >= credencialInicio && linhaAtual < credencialInicio + 5) {
            tft.setCursor(4, startY + (linhaAtual - credencialInicio) * lineHeight);
            tft.setTextColor(linhaAtual % 2 == 0 ? C_GOLD : C_WHITE);
            if (linha.length() > 20) linha = linha.substring(0, 20) + ".";
            tft.println(linha);
          }
          linhaAtual++;
        }
        if (linhaAtual >= credencialInicio + 5) break;
      }
      file.close();
    }
  } else {
    tft.setTextColor(C_GREY);
    tft.setCursor(20, 70);
    tft.println("Nenhuma credencial");
  }

  drawSeparator(138, C_GREY);
  tft.setTextColor(C_GOLD_DIM);
  tft.setCursor(76, 142);
  tft.printf("%d/%d", paginaCredencialAtual + 1,
             totalCredenciais > 0 ? totalCredenciais : 1);
}

void handleVisualizarCredenciais() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      if (paginaCredencialAtual > 0)
        paginaCredencialAtual--;
      else
        paginaCredencialAtual = totalCredenciais > 0 ? totalCredenciais - 1 : 0;
      lastDebounceTime = millis(); displayCredenciais();
    }
    if (digitalRead(BUTTON_RIGHT) == LOW) {
      if (paginaCredencialAtual < totalCredenciais - 1)
        paginaCredencialAtual++;
      else
        paginaCredencialAtual = 0;
      lastDebounceTime = millis(); displayCredenciais();
    }
    if (digitalRead(BUTTON_SELECT) == LOW) {
      estadoAtual = ATAQUE_CAPTIVE_PORTAL;
      displayAtaqueCaptivePortal();
      lastDebounceTime = millis();
    }
  }
}
