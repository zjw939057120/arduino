#ifndef MODBUS_TCP_H
#define MODBUS_TCP_H

#include <Arduino.h>
#include "NetworkServer.h"

extern NetworkServer server;

void ServerStart();

void ModbusTCPHandler();

#endif // MODBUS_TCP_H
