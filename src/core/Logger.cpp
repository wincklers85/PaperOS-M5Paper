#include "Logger.h"

namespace paperos {
void Logger::begin(fs::FS* fs) { fs_ = fs; }
void Logger::info(const String& m) { write("INFO", m); }
void Logger::warn(const String& m) { write("WARNING", m); }
void Logger::error(const String& m) { write("ERROR", m); }
void Logger::write(const char* level, const String& message) {
  String line = String(millis()) + " [" + level + "] " + message;
  Serial.println(line);
  if (!fs_) return;
  const char* p = "/PaperOS/Logs/system.log";
  File probe = fs_->open(p, FILE_READ);
  if (probe && probe.size() > 262144) {
    probe.close();
    fs_->remove("/PaperOS/Logs/system.1.log");
    fs_->rename(p, "/PaperOS/Logs/system.1.log");
  } else if (probe) { probe.close(); }
  File f = fs_->open(p, FILE_APPEND);
  if (!f) return;
  f.println(line);
  f.close();
}
}
