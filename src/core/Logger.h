#pragma once
#include <Arduino.h>
#include <FS.h>

namespace paperos {
class Logger {
 public:
  void begin(fs::FS* fs = nullptr);
  void info(const String& message);
  void warn(const String& message);
  void error(const String& message);
 private:
  fs::FS* fs_ = nullptr;
  void write(const char* level, const String& message);
};
}
