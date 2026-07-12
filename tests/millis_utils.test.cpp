#include <cstdlib>
#include <iostream>
#include <limits>

#include "../sms_forwarder_esp32s3_sim7670g/src/millis_utils.h"

namespace {
void expect(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
}

int main() {
  const uint32_t nearWrap = std::numeric_limits<uint32_t>::max() - 9U;

  expect(millisSince(5U, nearWrap) == 15U,
         "elapsed duration crosses wraparound");
  expect(!millisElapsed(4U, nearWrap, 15U),
         "interval is not elapsed before wrapped boundary");
  expect(millisElapsed(5U, nearWrap, 15U),
         "interval is elapsed at wrapped boundary");

  const uint32_t deadline = millisDeadlineAfter(nearWrap, 15U);
  expect(deadline == 5U, "deadline creation wraps safely");
  expect(!millisDeadlineReached(4U, deadline),
         "deadline is not reached one tick before");
  expect(millisDeadlineReached(5U, deadline),
         "deadline is reached at the exact tick");
  expect(millisDeadlineReached(6U, deadline),
         "deadline remains reached after the tick");

  const uint32_t batchStart = nearWrap;
  expect(!millisElapsed(4U, batchStart, 15U),
         "SMS batch remains open before merge boundary");
  expect(millisElapsed(5U, batchStart, 15U),
         "SMS batch closes at merge boundary");

  const uint32_t retryBase = std::numeric_limits<uint32_t>::max() - 29999U;
  const uint32_t firstRetry = millisDeadlineAfter(retryBase, 60000U);
  expect(!millisDeadlineReached(firstRetry - 1U, firstRetry),
         "retry backoff waits for the complete interval");
  expect(millisDeadlineReached(firstRetry, firstRetry),
         "retry backoff fires at its wrapped deadline");

  const uint32_t secondRetry = millisDeadlineAfter(firstRetry, 120000U);
  expect(!millisDeadlineReached(secondRetry - 1U, secondRetry),
         "increased retry backoff waits until its deadline");
  expect(millisDeadlineReached(secondRetry, secondRetry),
         "increased retry backoff fires at its deadline");

  std::cout << "All millis utility tests passed.\n";
  return 0;
}
