#include <cassert>
#include <iostream>
#include "../sms_forwarder_esp32s3_sim7670g/src/sms_storage.cpp"

static void reset() {
  FakeFS::writeRemaining = std::numeric_limits<size_t>::max();
  FakeFS::failOpen = false;
  FakeFS::corruptOnFlush = false;
  FakeFS::failRenameTo.clear();
  SPIFFS.files.clear();
  smsStorage.init();
}

int main() {
  reset();
  for (int index = 1; index <= MAX_SMS_COUNT; index++) {
    assert(smsStorage.saveSMS("sender", "pending", "123", SMSStatus::PENDING_FORWARD) == index);
  }
  assert(smsStorage.saveSMS("sender", "overflow", "123", SMSStatus::PENDING_FORWARD) == 0);
  smsStorage.init();
  assert(smsStorage.getSMSCount() == MAX_SMS_COUNT);
  assert(smsStorage.getSMSById(1).status == SMSStatus::PENDING_FORWARD);
  assert(smsStorage.updateSMSStatus(2, SMSStatus::FORWARD_SUCCESS));
  assert(smsStorage.saveSMS("sender", "accepted", "123", SMSStatus::PENDING_FORWARD) == 51);
  assert(smsStorage.getSMSById(1).id == 1);
  assert(smsStorage.getSMSById(2).id == 0);

  for (int failure = 0; failure < 4; failure++) {
    reset();
    assert(smsStorage.saveSMS("sender", "old", "123", SMSStatus::PENDING_FORWARD) == 1);
    const auto original = *SPIFFS.files.at("/sms.json");
    if (failure == 0) FakeFS::writeRemaining = 20;
    if (failure == 1) FakeFS::corruptOnFlush = true;
    if (failure == 2) FakeFS::failRenameTo = "/sms.bak";
    if (failure == 3) FakeFS::failRenameTo = "/sms.json";
    assert(smsStorage.saveSMS("sender", "new", "123", SMSStatus::PENDING_FORWARD) == 0);
    assert(smsStorage.getSMSCount() == 1);
    assert(SPIFFS.exists("/sms.json") || SPIFFS.exists("/sms.bak"));
    FakeFS::writeRemaining = std::numeric_limits<size_t>::max();
    FakeFS::corruptOnFlush = false;
    FakeFS::failRenameTo.clear();
    smsStorage.init();
    assert(smsStorage.getSMSById(1).content == "old");
    assert(*SPIFFS.files.at("/sms.json") == original);
  }

  reset();
  int accepted = 0;
  String large(4000, 'x');
  while (smsStorage.saveSMS("sender", large, "123", SMSStatus::RETRY_SCHEDULED)) accepted++;
  assert(accepted > 0 && accepted < MAX_SMS_COUNT);
  smsStorage.init();
  assert(smsStorage.getSMSCount() == accepted);
  assert(smsStorage.getSMSById(1).content == large);
  assert(smsStorage.updateSMSStatus(1, SMSStatus::FORWARD_SUCCESS));
  FakeFS::failOpen = true;
  assert(smsStorage.saveSMS("sender", large, "123", SMSStatus::PENDING_FORWARD) == 0);
  assert(smsStorage.getSMSById(1).status == SMSStatus::FORWARD_SUCCESS);
  FakeFS::failOpen = false;
  assert(smsStorage.saveSMS("sender", large, "123", SMSStatus::PENDING_FORWARD) > 0);
  assert(smsStorage.getSMSById(1).id == 0);
  std::cout << "SMS storage tests passed (production C++).\n";
}