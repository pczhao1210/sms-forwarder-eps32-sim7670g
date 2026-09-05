#include <cassert>
#include <iostream>
#include "../sms_forwarder_esp32s3_sim7670g/src/at_response.h"

int main() {
  assert(classifyAtResult("OK") == AtResult::Ok);
  assert(classifyAtResult("ERROR") == AtResult::Error);
  assert(classifyAtResult("+CMS ERROR: 321") == AtResult::Error);
  assert(classifyAtResult("+CME ERROR: SIM not inserted") == AtResult::Error);
  assert(classifyAtResult("+CMGR: 0,,42") == AtResult::Pending);
  assert(classifyAtResult("+CMTI: \"SM\",7") == AtResult::Pending);
  assert(classifyAtResult("NOT OK") == AtResult::Pending);
  assert(classifyAtResult("") == AtResult::Pending);
  std::cout << "AT terminal response tests passed.\n";
}