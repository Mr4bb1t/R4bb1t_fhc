#include "Menu_Networks.h"
#include "Globals.h"
#include "UI.h"
#include "Menu_Main.h"
#include "Menu_Attacks.h"

void displayNetworks() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);

  if (redeSelecionada == 0) {
    tft.fillRect(0, 0, 128, 15, TFT_DARKGREY);
    drawText(5, 5, "< VOLTAR", TFT_YELLOW);
  } else {
    drawText(5, 5, "< Voltar", TFT_DARKGREY);
  }

  int startIndex = redeSelecionada - 1;
  if (startIndex < 0) startIndex = 0;
  
  for (int i = startIndex; i < numRedes && i < startIndex + 8; i++) {
    String networkName = redes[i];
    if (networkName.length() > 18) {
      networkName = networkName.substring(0, 18) + "..";
    }
    drawText(5, 20 + (i - startIndex) * 16, networkName.c_str(), 
             redeSelecionada == (i + 1) ? TFT_RED : TFT_WHITE);
    if (redeSelecionada == (i + 1)) {
      tft.drawRect(0, 18 + (i - startIndex) * 16, 128, 14, TFT_RED);
    }
  }
}

void handleSelecaoRedes() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      if (redeSelecionada > 0) {
        redeSelecionada--;
      } else {
        redeSelecionada = numRedes;
      }
      lastDebounceTime = millis();
      displayNetworks();
    }

    if (digitalRead(BUTTON_RIGHT) == LOW) {
      if (redeSelecionada < numRedes) {
        redeSelecionada++;
      } else {
        redeSelecionada = 0;
      }
      lastDebounceTime = millis();
      displayNetworks();
    }

    if (digitalRead(BUTTON_SELECT) == LOW) {
      if (redeSelecionada == 0) {
        estadoAtual = MENU_INICIAL;
        displayMenuInicial();
      } else {
        int redeAtual = redeSelecionada - 1;
        
        if (redeAtual >= 0 && redeAtual < numRedes) {
          ssidSelecionado = redes[redeAtual];
          macSelecionado = WiFi.BSSIDstr(redeAtual);
          
          memcpy(&apRecordSelecionado, &ap_records[redeAtual], sizeof(wifi_ap_record_t));
          
          Serial.printf("\n=== REDE SELECIONADA ===\n");
          Serial.printf("SSID: %s\n", ssidSelecionado.c_str());
          Serial.printf("BSSID: %s\n", macSelecionado.c_str());
          Serial.printf("Canal: %d\n", apRecordSelecionado.primary);
          Serial.printf("RSSI: %d dBm\n", apRecordSelecionado.rssi);
          
          estadoAtual = MENU_ATAQUES;
          opcaoAtaqueSelecionada = 0;
          displayMenuAtaques();
        }
      }
      lastDebounceTime = millis();
    }
  }
}
