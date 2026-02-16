#ifndef OPERATOR_DB_H
#define OPERATOR_DB_H

#include <Arduino.h>

String getOperatorNameByCode(const String& code, const char* lang);
bool isKnownOperatorCode(const String& code);

#endif
