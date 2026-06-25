#include "Language.h"

const LangPack langPT PROGMEM = {
    // ── Menu Principal ──
    "WiFi",
    "2.4GHz",
    "Sub GHz",
    "Ajustes",
    "R4BB1T FHC",
    "WIFI",
    "Escaneando...",

    // ── Menu Config ──
    "Voltar",
    "Sobre",
    "Mudar MAC",
    "Brilho",
    "Modo Menu",
    "Armazenamento",
    "Descanso Tela",
    "Idioma",
    "Desligar",
    "AJUSTES",
    "MODO MENU",
    "BLOCO",
    "LISTA",
    "IDIOMA / LANG",
    "Portugues (PT-BR)",
    "English (EN)",
    "",
    "Idioma salvo!",
    "MAC CHANGER",
    "MAC atual:",
    "Gerar MAC aleatorio",
    "SEL = Gerar",
    "Novo MAC:",
    "SEL = Aplicar",
    ">>> Aplicado! <<<",
    "SEL = Voltar",

    // ── Sobre ──
    "Chip:",
    "Cores:",
    "Rev:",
    "Flash:",
    "Heap:",
    "SDK:",
    "FW:",
    "MAC:",
    "Conectado",
    "Nao detectado",
    "NRF24L01",
    "Bus:",
    "CE:",
    "CSN:",
    "SCK:",
    "MISO:",
    "MOSI:",
    "HSPI",
    "NRF24L01 #2",
    "CC1101",
    "Freq:",
    "CS:",
    "GDO0:",
    "GDO2:",
    "BATERIA",
    "Tensao:",
    "ADC PIN:",
    "%",

    // ── Brilho ──
    "BRILHO",
    "min",
    "max",
    "",
    "",

    // ── Armazenamento ──
    "ARMAZENAMENTO",
    "USADO",
    "VOLTAR",
    "ARQUIVOS",
    "",
    "< VOLTAR",
    "",
    "< > galeria",
    "o sair",
    "",

    // ── Desligar ──
    "DESLIGAR",
    "Deseja desligar?",
    "<  CANCELAR",
    "o  CONFIRMAR",
    "DESLIGANDO SISTEMA",
    "Aguarde %d...",

    // ── Descanso Tela ──
    "DESCANSO TELA",
    "<- VOLTAR",
    " [ATIVO]",
    "",
    "",

    // ── Bateria Niveis ──
    "CRITICO",
    "BAIXO",
    "MEDIO",
    "BOM",
    "CHEIO",

    // ── Menu RF ──
    "< Voltar",
    "Capturar",
    "Sniffer",
    "Grafico",
    "Jammer",
    "Salvos",
    "SUB GHZ",

    // ── RF Replay ──
    "REPLAY",
    "Aguardando sinal...",
    "Aponte o controle e",
    "pressione o botao.",
    "Capturado:",
    "",
    "",
    ">>> ENVIADO! <<<",
    "Salvo no SPIFFS!",
    "Erro ao salvar!",

    // ── RF Raw ──
    "RAW  RX",
    "Escutando pulsos...",

    // ── RF Analyser ──
    "ANALISADOR",
    "scan...",

    // ── RF Jammer ──
    "JAMMER 433",
    "CC1101 indisponivel!",
    "Inunda o canal RF com",
    "sinais aleatorios,",
    "bloqueando recepcao.",
    "o = INICIAR JAMMER",
    "\xB7\xB7 JAMMER ATIVO \xB7\xB7",
    "o = PARAR",

    // ── RF Saved ──
    "SAVED RF",
    "Nenhum sinal salvo.",
    "Capture em Replay",
    "e pressione v",
    "< VOLTAR",
    "Acoes:",
    "Transmitir",
    "Excluir",
    "Repetir",
    "Voltar",
    ">> ENVIADO!",
    "Sinal Deletado",
    "Parar",
    "Pulsos:",

    // ── Menu Ataques ──
    "WIFI ATTACKS",
    "< VOLTAR",
    "Captive Portal",
    "Deauther",
    "NAV Jammer",
    "Beacon Spam",

    // ── Captive Portal ──
    "CAPTIVE PORTAL",
    "[ ATIVO ]",
    "Portal: 192.168.4.1",
    "+ Deauther Ativo",
    "Apagar dados",
    "Credenciais",

    // ── Apagar Dados ──
    "APAGAR DADOS",
    "Deseja mesmo apagar?",
    "Isso e irreversivel.",
    "< CANCELAR",
    "[ APAGAR ]",
    "Credenciais apagadas!",

    // ── Deauther ──
    "DEAUTHER",
    "< VOLTAR",
    "Broadcast",
    "Targeted",
    "[  INICIAR  ]",
    "",
    "BCAST",
    "TRGD",
    "[  PARAR  ]",
    "ATAQUE PARADO",
    "Enviados: %lu",
    "ERRO: Radio!",

    // ── Deauther Scan ──
    "CLIENTES",
    "[ PARAR SCAN ]",
    "[ ESCANEAR ]",
    "< VOLTAR",
    "Scan",
    "Nenhum cliente",
    "encontrado...",
    "MAC %d/%d  SEL=Atacar",
    "%d cliente(s)",
    "",

    // ── CTS Jammer ──
    "NAV JAMMER",
    "Mantem conexao ativa",
    "Congela canal (NAV)",
    "Mata velocidade/ping",
    "BSSID forjado do AP",
    "Iniciar Ataque",
    "Canal travado:",
    "NAV Flood:",
    "[  PARAR  ]",

    // ── Beacon Modo ──
    "BEACON MODO",
    "< VOLTAR",
    "Copia",
    "Aleatorio",
    "Personalizado",

    // ── Beacon Custom ──
    "NOME CUSTOM",

    // ── Beacon Spam ──
    "BEACON SPAM",
    "[ Aleatorio ]",
    "Pool de redes:",
    "",
    "",
    "",
    "[ ATIVO ]",
    "Beacons:",
    "SEL = PARAR",

    // ── Credenciais ──
    "CREDENCIAIS",
    "Nenhuma credencial",

    // ── Menu NRF24 ──
    "< Voltar",
    "BT Jammer",
    "Bluetooth Classic",
    "Drone Jammer",
    "Drones 2.4GHz",
    "BLE Adv Jammer",
    "BLE Adv Channels",
    "BLE Data Jammer",
    "BLE Data Channels",
    "Zigbee Jammer",
    "IEEE 802.15.4",
    "Misc Jammer",
    "Canal livre 0-124",
    "2.4GHz JAMMER",
    "2.4GHz ATTACK",
    "Modulos:",
    "init...",
    "  [ ATIVO ]  ",
    "  [INATIVO]  ",
    "Canal:",
    "Pacotes:",
    "",
    "",

    // ── Menu Networks ──
    "REDES WIFI",
    "< VOLTAR",

    // ── Main (low battery / errors) ──
    "BATERIA CRITICA",
    "Conecte ao carregador",
    "[ %d%% ]",
    "Desligando em %ds...",
    "ERRO: MUTEX",
    "SPIFFS FAIL",

    // ── Splash ──
    "nao encontrado",
    "BMP: must be 24-bit",
    "uncompressed",
    "Splash: sem memoria",

    // ── Misc ──
    "PKT:",
    "SHIFT",
    "DEL",
    "ENTER",
    "Pool:",

    // ── Hard Reset & First Boot ──
    "Hard Reset",
    "HARD RESET",
    "ATENCAO! Isso vai",
    "apagar todos os dados",
    "e configuracoes.",
    "Tem certeza?",
    "Acao IRREVERSIVEL.",
    "CANCELAR",
    "PROXIMO",
    "APAGAR",
    "Apagando sistema...",
    "Selecione o Idioma:",
    "BEM-VINDO AO R4BB1T",

    // ── Storage labels ──
    "FW",
    "SPIFFS",
    "LIVRE",
    "total:",

    // ── Screensaver anim names ──
    "Logo R4BB1T",
    "Matrix Rain",
    "Cubo 3D",
    "Plasma",
    "Tesseract 4D",
    "Corredor",
    "Onda Grade",
    "Aneis",
    "Quadrados",
    "Olho Magenta",
};

const LangPack langEN PROGMEM = {
    // ── Menu Principal ──
    "WiFi",
    "2.4GHz",
    "Sub GHz",
    "Settings",
    "R4BB1T FHC",
    "WIFI",
    "Scanning...",

    // ── Menu Config ──
    "Back",
    "About",
    "Change MAC",
    "Brightness",
    "Menu Style",
    "Storage",
    "Screensaver",
    "Language",
    "Power Off",
    "SETTINGS",
    "MENU STYLE",
    "GRID",
    "LIST",
    "LANGUAGE / LANG",
    "Portugues (PT-BR)",
    "English (EN)",
    "",
    "Language saved!",
    "MAC CHANGER",
    "Current MAC:",
    "Generate random MAC",
    "SEL = Generate",
    "New MAC:",
    "SEL = Apply",
    ">>> Applied! <<<",
    "SEL = Back",

    // ── Sobre ──
    "Chip:",
    "Cores:",
    "Rev:",
    "Flash:",
    "Heap:",
    "SDK:",
    "FW:",
    "MAC:",
    "Connected",
    "Not detected",
    "NRF24L01",
    "Bus:",
    "CE:",
    "CSN:",
    "SCK:",
    "MISO:",
    "MOSI:",
    "HSPI",
    "NRF24L01 #2",
    "CC1101",
    "Freq:",
    "CS:",
    "GDO0:",
    "GDO2:",
    "BATTERY",
    "Voltage:",
    "ADC PIN:",
    "%",

    // ── Brilho ──
    "BRIGHTNESS",
    "min",
    "max",
    "",
    "",

    // ── Armazenamento ──
    "STORAGE",
    "USED",
    "BACK",
    "FILES",
    "",
    "< BACK",
    "",
    "< > gallery",
    "o exit",
    "",

    // ── Desligar ──
    "POWER OFF",
    "Turn off device?",
    "<  CANCEL",
    "o  CONFIRM",
    "SHUTTING DOWN",
    "Wait %d...",

    // ── Descanso Tela ──
    "SCREENSAVER",
    "<- BACK",
    " [ACTIVE]",
    "",
    "",

    // ── Bateria Niveis ──
    "CRITICAL",
    "LOW",
    "MED",
    "GOOD",
    "FULL",

    // ── Menu RF ──
    "< Back",
    "Capture",
    "Sniffer",
    "Graph",
    "Jammer",
    "Saved",
    "SUB GHZ",

    // ── RF Replay ──
    "REPLAY",
    "Waiting for signal...",
    "Point remote and",
    "press button.",
    "Captured:",
    "",
    "",
    ">>> SENT! <<<",
    "Saved to SPIFFS!",
    "Error saving!",

    // ── RF Raw ──
    "RAW  RX",
    "Listening pulses...",

    // ── RF Analyser ──
    "ANALYZER",
    "scan...",

    // ── RF Jammer ──
    "JAMMER 433",
    "CC1101 unavailable!",
    "Floods RF channel with",
    "random signals,",
    "blocking reception.",
    "o = START JAMMER",
    "JAMMER ACTIVE",
    "o = STOP",

    // ── RF Saved ──
    "SAVED RF",
    "No saved signals.",
    "Capture in Replay",
    "and press v",
    "< BACK",
    "Actions:",
    "Transmit",
    "Delete",
    "Repeat",
    "Back",
    ">> SENT!",
    "Signal Deleted",
    "Stop",
    "Pulses:",

    // ── Menu Ataques ──
    "WIFI ATTACKS",
    "< BACK",
    "Captive Portal",
    "Deauther",
    "NAV Jammer",
    "Beacon Spam",

    // ── Captive Portal ──
    "CAPTIVE PORTAL",
    "[ ACTIVE ]",
    "Portal: 192.168.4.1",
    "+ Deauther Active",
    "Clear data",
    "Credentials",

    // ── Apagar Dados ──
    "CLEAR DATA",
    "Delete all data?",
    "This is irreversible.",
    "< CANCEL",
    "[ DELETE ]",
    "Data cleared!",

    // ── Deauther ──
    "DEAUTHER",
    "< BACK",
    "Broadcast",
    "Targeted",
    "[  START  ]",
    "",
    "BCAST",
    "TARG",
    "[  STOP  ]",
    "ATTACK STOPPED",
    "Sent: %lu",
    "ERROR: Radio!",

    // ── Deauther Scan ──
    "CLIENTS",
    "[ STOP SCAN ]",
    "[ SCAN ]",
    "< BACK",
    "Scan",
    "No clients",
    "found...",
    "MAC %d/%d  SEL=Attack",
    "%d client(s)",
    "",

    // ── CTS Jammer ──
    "NAV JAMMER",
    "Maintains active conn",
    "Freezes channel (NAV)",
    "Kills speed/ping",
    "Forged AP BSSID",
    "Start Attack",
    "Locked channel:",
    "NAV Flood:",
    "[  STOP  ]",

    // ── Beacon Modo ──
    "BEACON MODE",
    "< BACK",
    "Clone",
    "Random",
    "Custom",

    // ── Beacon Custom ──
    "CUSTOM NAME",

    // ── Beacon Spam ──
    "BEACON SPAM",
    "[ Random ]",
    "Network pool:",
    "",
    "",
    "",
    "[ ACTIVE ]",
    "Beacons:",
    "SEL = STOP",

    // ── Credenciais ──
    "CREDENTIALS",
    "No credentials",

    // ── Menu NRF24 ──
    "< Back",
    "BT Jammer",
    "Bluetooth Classic",
    "Drone Jammer",
    "Drones 2.4GHz",
    "BLE Adv Jammer",
    "BLE Adv Channels",
    "BLE Data Jammer",
    "BLE Data Channels",
    "Zigbee Jammer",
    "IEEE 802.15.4",
    "Misc Jammer",
    "Free ch 0-124",
    "2.4GHz JAMMER",
    "2.4GHz ATTACK",
    "Modules:",
    "init...",
    "  [ ACTIVE ]  ",
    "  [INACTIVE]  ",
    "Channel:",
    "Packets:",
    "",
    "",

    // ── Menu Networks ──
    "WIFI NETWORKS",
    "< BACK",

    // ── Main (low battery / errors) ──
    "LOW BATTERY",
    "Connect charger",
    "[ %d%% ]",
    "Shutting down in %ds...",
    "ERROR: MUTEX",
    "SPIFFS FAIL",

    // ── Splash ──
    "not found",
    "BMP: must be 24-bit",
    "uncompressed",
    "Splash: out of memory",

    // ── Misc ──
    "PKT:",
    "SHIFT",
    "DEL",
    "ENTER",
    "Pool:",

    // ── Hard Reset & First Boot ──
    "Factory Reset",
    "hard reset",
    "WARNING! This will",
    "erase ALL data and",
    "Settings.",
    "Are you sure?",
    "NO going back.",
    "CANCEL",
    "NEXT",
    "ERASE",
    "Wiping system...",
    "Select Language:",
    "WELCOME TO R4BB1T",

    // ── Storage labels ──
    "FW",
    "SPIFFS",
    "FREE",
    "total:",

    // ── Screensaver anim names ──
    "Logo R4BB1T",
    "Matrix Rain",
    "3D Cube",
    "Plasma",
    "Tesseract 4D",
    "Corridor",
    "Wave Grid",
    "Rings",
    "Squares",
    "Magenta Eye",
};

const LangPack *lang = &langPT;

void setLanguage(int langId) {
  if (langId == 1) {
    lang = &langEN;
  } else {
    lang = &langPT;
  }
}
