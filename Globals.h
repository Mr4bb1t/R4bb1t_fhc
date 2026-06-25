#ifndef GLOBALS_H
#define GLOBALS_H

#include "Config.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <Arduino.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <Preferences.h>

// Estados da máquina de estados
enum EstadoTela {
  MENU_INICIAL,
  SELECAO_REDES,
  MENU_ATAQUES,
  ATAQUE_CAPTIVE_PORTAL,
  ATAQUE_DEAUTHER,
  ATAQUE_DEAUTHER_SCAN,
  MENU_WIFI_ANALYZER,
  TELA_PACKET_MONITOR,
  TELA_NETWORK_COMPARE,
  ATAQUE_BEACON_MODO,
  ATAQUE_BEACON_CUSTOM,
  ATAQUE_BEACON,
  ATAQUE_INFO_REDE,
  VISUALIZAR_CREDENCIAIS,

  MENU_CONFIGURACOES,
  TELA_ARMAZENAMENTO,
  TELA_SOBRE,
  TELA_MAC_CHANGER,
  TELA_BRILHO,
  TELA_MODO_MENU,
  TELA_SCREENSAVER,
  MENU_RF,
  TELA_RF_REPLAY,
  TELA_RF_RAW,
  TELA_RF_ANALYSER,
  TELA_RF_RANDOM,
  TELA_RF_SAVED,
  CONFIRMA_APAGAR_CREDENCIAIS,
  MENU_NRF24,
  TELA_NRF_ATTACK,
  TELA_DESLIGAR,
  TELA_SCREENSAVER_TEST,
  TELA_IDIOMA,
  TELA_HARDRESET,
  TELA_PRIMEIRO_BOOT,
  TELA_BEM_VINDO
};

// Task handles e mutex
extern TaskHandle_t attackTaskHandle;
extern SemaphoreHandle_t wifiMutex;
extern volatile bool attackTaskRunning;
extern volatile bool systemInitialized;

// Objetos Globais
extern TFT_eSPI tft;
extern AsyncWebServer server;
extern DNSServer dnsServer;
extern Preferences prefs;

// Dados de rede
extern String ssidSelecionado;
extern String macSelecionado;
extern String ipSelecionado;

extern String redes[MAX_REDES];
extern wifi_ap_record_t ap_records[MAX_REDES];
extern wifi_ap_record_t apRecordSelecionado;

extern int numRedes;
extern int redeSelecionada;
extern int opcaoAtaqueSelecionada;
extern int opcaoMenuInicial;
extern int menuStyle; // 0 = Quadradinho (Grid), 1 = Lista


// Controle de botões
extern unsigned long lastDebounceTime;
extern unsigned long debounceDelay;

// Credenciais
extern int totalCredenciais;
extern int paginaCredencialAtual;
extern int contadorLinhas;
extern int opcaoSubMenuAtaque;

// Deauther
extern bool deautherAtivo;
extern volatile unsigned long deauthCounter;
extern int deauthTipo;
extern int clientScanBtnSel;  // declaração

// Clientes descobertos (Deauther Targeted)
#define MAX_CLIENTS 16
struct ClientInfo {
  uint8_t mac[6];
  int8_t  rssi;
};
extern ClientInfo clientList[MAX_CLIENTS];
extern int clientCount;
extern int clientSelected;
extern bool clientScanRunning;
extern uint8_t targetClientMac[6]; // MAC do cliente alvo para deauth unicast

// Packet Monitor
extern volatile unsigned long pmTotalPackets;
extern volatile unsigned long pmPacketsPerSec;
extern volatile unsigned long pmUniqueClients;
extern unsigned long pmStartTime;


// Beacon Spam
extern int beaconModo;
extern String beaconCustomSSID;
extern int beaconQuantidade;
extern bool beaconAtivo;
extern volatile unsigned long beaconCounter;

// Estado do rádio
extern volatile bool radioLocked;
extern volatile uint8_t canalTravado;

// RF — Frequência detectada automaticamente
extern float rfDetectedMHz;     // frequência com maior RSSI (0 = não detectada)
extern int   rfDetectedRSSI;    // RSSI da frequência detectada

// Estado atual
extern EstadoTela estadoAtual;
extern unsigned long lastActivityTime;
extern EstadoTela estadoAnteriorScreensaver;

#endif
