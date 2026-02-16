#ifndef I18N_H
#define I18N_H

#include <Arduino.h>

const char* normalizeLangCode(const String& lang);
const char* getCurrentLangCode();
const char* i18nGet(const char* key);
const char* i18nGet(const char* key, const char* lang);
String i18nFormat(const char* key, ...);

#endif
