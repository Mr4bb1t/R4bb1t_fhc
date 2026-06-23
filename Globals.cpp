#include "Globals.h"

// Task handles e mutex
TaskHandle_t attackTaskHandle = NULL;
SemaphoreHandle_t wifiMutex = NULL;
volatile bool attackTaskRunning = false;
volatile bool systemInitialized = false;

// Objetos Globais
TFT_eSPI tft = TFT_eSPI();
AsyncWebServer server(80);
DNSServer dnsServer;
Preferences prefs;

// Dados de rede
String ssidSelecionado = "";
String macSelecionado = "";
String ipSelecionado = "";

String redes[MAX_REDES];
wifi_ap_record_t ap_records[MAX_REDES];
wifi_ap_record_t apRecordSelecionado;

int numRedes = 0;
int redeSelecionada = 1;
int opcaoAtaqueSelecionada = 0;
int opcaoMenuInicial = 0;
int menuStyle = 0; // 0 = Quadradinho (Grid), 1 = Lista


// Controle de botões
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = DEBOUNCE_DELAY;

// Credenciais
int totalCredenciais = 0;
int paginaCredencialAtual = 0;
int contadorLinhas = 0;
int opcaoSubMenuAtaque = 0;

// Deauther
bool deautherAtivo = false;
volatile unsigned long deauthCounter = 0;
int deauthTipo = 0; // 0 = Broadcast, 1 = Targeted
int clientScanBtnSel = 0;     // definição

// Clientes descobertos (Deauther Targeted)
ClientInfo clientList[MAX_CLIENTS];
int clientCount = 0;
int clientSelected = 0;
bool clientScanRunning = false;
uint8_t targetClientMac[6] = {0};

// CTS Jammer
volatile bool ctsAtivo = false;
volatile unsigned long ctsCounter = 0;

// Beacon Spam
int beaconModo = 0;
String beaconCustomSSID = "";
int beaconQuantidade = 50;
bool beaconAtivo = false;
volatile unsigned long beaconCounter = 0;

// Estado do rádio
volatile bool radioLocked = false;
volatile uint8_t canalTravado = 0;

// RF — Frequência detectada automaticamente
float rfDetectedMHz = 0;      // 0 = não detectada ainda
int   rfDetectedRSSI = -100;  // RSSI da frequência detectada

// Estado Atual
EstadoTela estadoAtual = MENU_INICIAL;
