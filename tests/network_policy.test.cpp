#include <cassert>
#include <iostream>
#include "../sms_forwarder_esp32s3_sim7670g/src/network_policy.h"

int main() {
  using State = ObservedDataState;
  assert(observedDataState(false, false) == State::Unknown);
  assert(observedDataState(true, true) == State::Unknown);
  assert(observedDataState(true, false) == State::On);
  assert(!dataTransitionConfirmed(false, State::Unknown, State::Unknown));
  assert(!dataTransitionConfirmed(false, State::On, State::Unknown));
  assert(!dataTransitionConfirmed(false, State::Unknown, State::On));
  assert(dataTransitionConfirmed(false, State::On, State::Off));
  assert(!dataTransitionConfirmed(true, State::On, State::Unknown));
  assert(dataTransitionConfirmed(true, State::On, State::On));
  assert(!dataPolicyAllowsActivation(DATA_POLICY_ALWAYS_ON, false, false, false, true));
  assert(!dataPolicyAllowsActivation(DATA_POLICY_ALWAYS_OFF, true, false, false, true));
  assert(!dataPolicyAllowsActivation(DATA_POLICY_ROAMING_ONLY, true, true, true, false));
  assert(dataPolicyAllowsActivation(DATA_POLICY_ROAMING_ONLY, true, false, true, false));
  assert(dataPolicyAllowsActivation(DATA_POLICY_ALWAYS_ON, true, true, false, false));
  std::cout << "Network policy and unknown-state tests passed.\n";
}