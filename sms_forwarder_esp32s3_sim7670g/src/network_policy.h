#ifndef NETWORK_POLICY_H
#define NETWORK_POLICY_H

enum DataPolicy {
  DATA_POLICY_ALWAYS_OFF = 0,
  DATA_POLICY_ROAMING_ONLY = 1,
  DATA_POLICY_ALWAYS_ON = 2
};

enum class ObservedDataState { Unknown, Off, On };

inline ObservedDataState observedDataState(bool sawOn, bool sawOff) {
  if (sawOn == sawOff) return ObservedDataState::Unknown;
  return sawOn ? ObservedDataState::On : ObservedDataState::Off;
}

inline bool dataTransitionConfirmed(bool enable, ObservedDataState attached, ObservedDataState active) {
  return enable ? attached == ObservedDataState::On && active == ObservedDataState::On
                : attached == ObservedDataState::Off || active == ObservedDataState::Off;
}

inline bool dataPolicyAllowsActivation(int policy, bool registered, bool roaming, bool autoDisableRoaming, bool allowRoaming) {
  if (!registered || policy == DATA_POLICY_ALWAYS_OFF) return false;
  if (roaming && autoDisableRoaming && !allowRoaming) return false;
  if (policy == DATA_POLICY_ALWAYS_ON) return true;
  return !roaming || allowRoaming;
}

#endif