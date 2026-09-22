#pragma once
#include <Arduino.h>
#include "StorageManager.h"

namespace paperos {
class NotesService {
 public:
  explicit NotesService(StorageManager& storage) : storage_(storage) {}
  String listJson();
  String get(const String& id);
  bool save(const String& id, const String& json);
  bool remove(const String& id);
 private:
  StorageManager& storage_;
  String sanitizeId(const String& id) const;
};
}
