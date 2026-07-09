#ifndef MODBUS_SERVER_H
#define MODBUS_SERVER_H

#include <Arduino.h>
#include "NetworkServer.h"

#define MODBUS_PORT 502

extern NetworkServer modbusServer;

void ModbusServerStart();

void ModbusServerHandler();

#endif // MODBUS_SERVER_H
