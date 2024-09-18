#pragma once

#include <cstdint>

// Uses EEPROM to read and write persistent data

struct Storage {
  constexpr static uint16_t VERSION = 1;

  uint16_t version = VERSION;
  uint16_t light_id;
};

// One-time initialization of EEPROM
void StorageSetup();

// Read the storage data from EEPROM, if it exists
bool StorageRead(Storage& storage);

// Write the storage data to EEPROM
void StorageWrite(const Storage& storage);
