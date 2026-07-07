#ifndef MODBUS_SERVER_H
#define MODBUS_SERVER_H

#include <Arduino.h>
#include "NetworkServer.h"

#define MODBUS_PORT 502
#define HTTP_PORT 80

extern NetworkServer modbusServer;
extern NetworkServer httpServer;

void ServerStart();

void ModbusServerHandler();

#endif // MODBUS_SERVER_H
