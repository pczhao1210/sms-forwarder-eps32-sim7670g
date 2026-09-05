#include <cassert>
#include <iostream>
#include "../sms_forwarder_esp32s3_sim7670g/src/pdp_auth.h"

int main() {
  std::string command;
  assert(buildPdpAuthCommand("", "", command) && command == "AT+CGAUTH=1,0");
  assert(buildPdpAuthCommand("user", "password", command));
  assert(command == "AT+CGAUTH=1,1,\"password\",\"user\"");
  assert(buildPdpAuthCommand("user", "", command) && command == "AT+CGAUTH=1,1,\"\",\"user\"");
  assert(!buildPdpAuthCommand("", "password", command));
  assert(!buildPdpAuthCommand("user", "bad\"password", command));
  assert(!buildPdpAuthCommand("user\r\nAT", "password", command));
  assert(!buildPdpAuthCommand(std::string(65, 'x'), "password", command));
  assert(redactPdpCredentials("AT+CGAUTH=1,1,\"secret\",\"user\"").find("secret") == std::string::npos);
  assert(redactPdpCredentials("+CGAUTH: 1,1,\"secret\",\"user\"").find("secret") == std::string::npos);
  assert(redactPdpCredentials("at+cgauth?") == "[PDP credentials redacted]");
  assert(redactPdpCredentials("AT+CEREG?") == "AT+CEREG?");
  std::cout << "SIM767XX PDP authentication and log redaction tests passed.\n";
}