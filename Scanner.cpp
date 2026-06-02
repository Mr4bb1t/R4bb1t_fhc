#include "Scanner.h"
#include "Globals.h"

void scanNetworks() {
  Serial.println("Escaneando redes Wi-Fi...");
  
  numRedes = 0;
  memset(ap_records, 0, sizeof(ap_records));
  
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);
  
  int n = WiFi.scanNetworks(false, false);
  numRedes = (n < MAX_REDES) ? n : MAX_REDES;
  
  Serial.printf("Número de redes encontradas: %d\n", numRedes);
  
  for (int i = 0; i < numRedes; i++) {
    redes[i] = WiFi.SSID(i);
    
    String ssid = WiFi.SSID(i);
    strncpy((char*)ap_records[i].ssid, ssid.c_str(), 32);
    ap_records[i].ssid[31] = '\0';
    
    uint8_t* bssid_ptr = WiFi.BSSID(i);
    if (bssid_ptr != nullptr) {
      for (int j = 0; j < 6; j++) {
        ap_records[i].bssid[j] = bssid_ptr[j];
      }
    } else {
      memset(ap_records[i].bssid, 0, 6);
    }
    
    int canal = WiFi.channel(i);
    ap_records[i].primary = (canal > 0 && canal <= 14) ? canal : 1;
    ap_records[i].rssi = WiFi.RSSI(i);
    ap_records[i].authmode = WiFi.encryptionType(i);
    ap_records[i].second = WIFI_SECOND_CHAN_NONE;
    
    Serial.printf("Rede %d: %s (Canal: %d, BSSID: %02X:%02X:%02X:%02X:%02X:%02X, RSSI: %d dBm)\n",
                  i, redes[i].c_str(), ap_records[i].primary,
                  ap_records[i].bssid[0], ap_records[i].bssid[1], 
                  ap_records[i].bssid[2], ap_records[i].bssid[3],
                  ap_records[i].bssid[4], ap_records[i].bssid[5],
                  ap_records[i].rssi);
  }
  
  Serial.println("Scan completo");
}
