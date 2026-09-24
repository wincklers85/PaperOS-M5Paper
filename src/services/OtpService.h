#pragma once
#include <Arduino.h>

namespace paperos {

class OtpService {
 public:
  static String normalizeSecret(const String& secret);
  static bool validSecret(const String& secret);
  static String generate(const String& secret, uint64_t unixTime, uint8_t digits = 6, uint32_t periodSeconds = 30);

 private:
  static int base32Value(char c);
};

}
