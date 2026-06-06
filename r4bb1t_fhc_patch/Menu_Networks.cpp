#include "Menu_Networks.h"
#include "Globals.h"
#include "UI.h"
#include "Menu_Main.h"
#include "Menu_Attacks.h"

void displayNetworks() {
  tft.fillScreen(C_BG);
  tft.setTextSize(1);

  // Header com back arrow
  drawHeader("REDES WIFI", true);

  // Lógica de scroll para as redes (mantém o cursor na tela antes de rolar)
  static int scrollOffset = 0;
  const int MAX_VISIBLE = 7;

  if (redeSelecionada == 0) {
    scrollOffset = 0; // Se "Voltar" estiver selecionado, volta pro topo
  } else {
    int listIndex = redeSelecionada - 1; // Índice na array de redes (0 a numRedes-1)
    if (listIndex < scrollOffset) {
      scrollOffset = listIndex;
    } else if (listIndex >= scrollOffset + MAX_VISIBLE) {
      scrollOffset = listIndex - MAX_VISIBLE + 1;
    }
  }

  // Item fixo "Voltar" no topo
  bool voltarSel = (redeSelecionada == 0);
  drawMenuItem(0, 16, 128, 18, "< VOLTAR", voltarSel, false);

  // Desenha os itens visíveis
  for (int i = 0; i < MAX_VISIBLE; i++) {
    int netIdx = scrollOffset + i;
    if (netIdx >= numRedes) break;
    
    String networkName = redes[netIdx];
    if (networkName.length() > 17) {
      networkName = networkName.substring(0, 17) + ".";
    }
    
    bool sel = (redeSelecionada == (netIdx + 1));
    drawMenuItem(0, 34 + i * 18, 128, 18, networkName.c_str(), sel, false);
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
