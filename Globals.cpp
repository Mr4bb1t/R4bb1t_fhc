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
unsigned long deauthCounter = 0;
int deauthTipo = 0; // 0 = Broadcast, 1 = Targeted

// Beacon Spam
int beaconQuantidade = 50;
bool beaconAtivo = false;
unsigned long beaconCounter = 0;

// Estado do rádio
volatile bool radioLocked = false;
volatile uint8_t canalTravado = 0;

// Estado Atual
EstadoTela estadoAtual = MENU_INICIAL;
