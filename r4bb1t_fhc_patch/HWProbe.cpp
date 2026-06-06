// HWProbe.cpp — Flags de estado dos módulos de hardware.
//
// Os módulos são inicializados de forma LAZY — apenas quando o usuário
// entra no menu correspondente pela primeira vez. Se o módulo não for
// encontrado, o menu exibe mensagem de erro sem travar o sistema.
//
// NÃO há tela de probe no boot. A splash screen normal é exibida.

#include "HWProbe.h"

// Todas as flags começam como false.
// Cada menu seta a flag como true assim que a init bem-sucede.
bool hwNRF24_ok = false;
bool hwCC1101_ok = false;
bool hwBLE_ok = false;
