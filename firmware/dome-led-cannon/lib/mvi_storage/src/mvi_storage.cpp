#include "mvi_storage.h"

#include <EEPROM.h>

void StorageSetup() {
  EEPROM.begin(sizeof(Storage));
}

bool StorageRead(Storage& storage) {
  storage = {};
  EEPROM.get(0, storage);
  if (storage.version != Storage::VERSION) { return false; }
  return true;
}

void StorageWrite(const Storage& storage) {
  assert(storage.version == Storage::VERSION);
  EEPROM.put(0, storage);
  EEPROM.commit();
}
