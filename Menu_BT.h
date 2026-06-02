#ifndef MENU_BT_H
#define MENU_BT_H

#include <Arduino.h>

// ── Main BT menu (MODO_BLUETOOTH) ────────────
void displayMenuBT();
void handleMenuBT();

// ── Sub-screens (TELA_BT_SUBMENU / TELA_BT_SCAN / TELA_BT_ATTACK) ──
void displayBT_SubMenu();
void handleBT_SubMenu();

void displayBT_Scan();
void handleBT_Scan();

void displayBT_Attack();
void handleBT_Attack();

#endif
