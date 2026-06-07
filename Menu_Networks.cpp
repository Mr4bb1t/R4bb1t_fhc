#include "Menu_Networks.h"
#include "Globals.h"
#include "UI.h"
#include "Menu_Main.h"
#include "Menu_Attacks.h"

static String getAuthShort(wifi_auth_mode_t auth) {
  switch (auth) {
    case WIFI_AUTH_OPEN: return "OPN";
    case WIFI_AUTH_WEP: return "WEP";
    case WIFI_AUTH_WPA_PSK: return "WPA";
    case WIFI_AUTH_WPA2_PSK: return "WP2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WP2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "ENT";
    case WIFI_AUTH_WPA3_PSK: return "WP3";
    case WIFI_AUTH_WPA2_WPA3_PSK: return "WP3";
    case WIFI_AUTH_OWE: return "OWE";
    default: return "UNK";
  }
}

static int scrollOffset = 0;
const int MAX_VISIBLE = 7;

static void drawNetworkItem(int i, bool sel) {
  if (i == 0) {
    drawMenuItem(0, 16, 128, 18, "< VOLTAR", sel, false);
  } else {
    int listIndex = i - 1;
    if (listIndex >= scrollOffset && listIndex < scrollOffset + MAX_VISIBLE && listIndex < numRedes) {
      int screenRow = listIndex - scrollOffset;
      String sec = getAuthShort(ap_records[listIndex].authmode);
      String displayName = "[" + sec + "] " + redes[listIndex];
      if (displayName.length() > 21) {
        displayName = displayName.substring(0, 20) + ".";
      }
      drawMenuItem(0, 34 + screenRow * 18, 128, 18, displayName.c_str(), sel, false);
    }
  }
}

void displayNetworks() {
  tft.fillScreen(C_BG);
  tft.setTextSize(1);
  drawHeader("REDES WIFI", true);

  if (redeSelecionada == 0) {
    scrollOffset = 0;
  } else {
    int listIndex = redeSelecionada - 1;
    if (listIndex < scrollOffset) {
      scrollOffset = listIndex;
    } else if (listIndex >= scrollOffset + MAX_VISIBLE) {
      scrollOffset = listIndex - MAX_VISIBLE + 1;
    }
  }

  drawNetworkItem(0, redeSelecionada == 0);

  for (int i = 0; i < MAX_VISIBLE; i++) {
    int netIdx = scrollOffset + i;
    if (netIdx >= numRedes) break;
    drawNetworkItem(netIdx + 1, redeSelecionada == (netIdx + 1));
  }
}

void handleSelecaoRedes() {
  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (digitalRead(BUTTON_LEFT) == LOW) {
      int oldSel = redeSelecionada;
      if (redeSelecionada > 0) {
        redeSelecionada--;
      } else {
        redeSelecionada = numRedes;
      }
      lastDebounceTime = millis();
      
      int newScrollOffset = scrollOffset;
      if (redeSelecionada == 0) {
        newScrollOffset = 0;
      } else {
        int listIndex = redeSelecionada - 1;
        if (listIndex < scrollOffset) newScrollOffset = listIndex;
        else if (listIndex >= scrollOffset + MAX_VISIBLE) newScrollOffset = listIndex - MAX_VISIBLE + 1;
      }
      
      if (newScrollOffset != scrollOffset) {
        displayNetworks();
      } else {
        drawNetworkItem(oldSel, false);
        drawNetworkItem(redeSelecionada, true);
      }
    }

    if (digitalRead(BUTTON_RIGHT) == LOW) {
      int oldSel = redeSelecionada;
      if (redeSelecionada < numRedes) {
        redeSelecionada++;
      } else {
        redeSelecionada = 0;
      }
      lastDebounceTime = millis();
      
      int newScrollOffset = scrollOffset;
      if (redeSelecionada == 0) {
        newScrollOffset = 0;
      } else {
        int listIndex = redeSelecionada - 1;
        if (listIndex < scrollOffset) newScrollOffset = listIndex;
        else if (listIndex >= scrollOffset + MAX_VISIBLE) newScrollOffset = listIndex - MAX_VISIBLE + 1;
      }
      
      if (newScrollOffset != scrollOffset) {
        displayNetworks();
      } else {
        drawNetworkItem(oldSel, false);
        drawNetworkItem(redeSelecionada, true);
      }
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
