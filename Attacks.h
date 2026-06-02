#ifndef ATTACKS_H
#define ATTACKS_H

#include <Arduino.h>

void attackTask(void* parameter);
void sendBeacon(const char* ssid, uint8_t* bssid, uint8_t channel, uint8_t seq_num);

#endif
