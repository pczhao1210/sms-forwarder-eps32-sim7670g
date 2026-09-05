const fs = require('node:fs');
const path = require('node:path');
const { extractFunction, runCpp } = require('./host_cpp');
const source = fs.readFileSync(path.join(__dirname, '../sms_forwarder_esp32s3_sim7670g/src/web_server.cpp'), 'utf8');
runCpp(`
#include <Arduino.h>
#include <cassert>
#include <iostream>
#include <map>
struct {
  std::map<std::string, String> args;
  String arg(const String& key) { auto found = args.find(key); return found == args.end() ? String("") : found->second; }
  bool hasArg(const String& key) { return args.count(key); }
} server;
bool invalid = false;
void sendAllowedValueError(const char*) { invalid = true; }
${extractFunction(source, 'static bool readSecretArg(')}
int main() {
  String result;
  assert(readSecretArg("token", "saved", result, 12) && result == "saved");
  server.args["token"] = "";
  assert(readSecretArg("token", "saved", result, 12) && result == "saved");
  server.args["token"] = "replacement";
  assert(readSecretArg("token", "saved", result, 12) && result == "replacement");
  server.args["tokenAction"] = "keep";
  assert(readSecretArg("token", "saved", result, 12) && result == "saved");
  server.args["tokenAction"] = "clear";
  assert(readSecretArg("token", "saved", result, 12) && result.isEmpty());
  server.args["tokenAction"] = "replace";
  assert(!readSecretArg("token", "saved", result, 3) && invalid);
  server.args["token"] = "";
  assert(!readSecretArg("token", "saved", result, 12));
  server.args["tokenAction"] = "unexpected";
  assert(!readSecretArg("token", "saved", result, 12));
  std::cout << "Secret keep, replace and clear tests passed.\\n";
}
`);