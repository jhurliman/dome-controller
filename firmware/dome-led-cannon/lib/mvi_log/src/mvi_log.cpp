#include "mvi_log.h"

void print(const char* str) {
  Serial.print(str);
}

void println(const char* str) {
  Serial.println(str);
}

void println(const String& str) {
  Serial.println(str);
}
