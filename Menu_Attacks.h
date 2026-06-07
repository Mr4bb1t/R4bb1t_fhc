#ifndef MENU_ATTACKS_H
#define MENU_ATTACKS_H

#include <Arduino.h>

void displayMenuAtaques();
void handleMenuAtaques();

void displayAtaqueCaptivePortal();
void handleAtaqueCaptivePortal();

void displayAtaqueDeauther();
void handleAtaqueDeauther();

void displayAtaqueDeautherScan(bool init = false);
void handleAtaqueDeautherScan();

void displayAtaqueCtsJammer();
void handleAtaqueCtsJammer();

void displayAtaqueBeaconModo();
void handleAtaqueBeaconModo();

void displayAtaqueBeaconCustom();
void handleAtaqueBeaconCustom();

void displayAtaqueBeacon();
void handleAtaqueBeacon();

void displayCredenciais();
void handleVisualizarCredenciais();

void displayConfirmaApagar();
void handleConfirmaApagar();

#endif
