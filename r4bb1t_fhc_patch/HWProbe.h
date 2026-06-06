
#ifndef HWPROBE_H
#define HWPROBE_H

// Flags de estado dos módulos — setadas true quando a init bem-sucede.
// Cada menu faz init lazy e atualiza a flag correspondente.
extern bool hwNRF24_ok;
extern bool hwCC1101_ok;
extern bool hwBLE_ok;

#endif
