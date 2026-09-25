#include "NotesService.h"
#include <ArduinoJson.h>
#include <SD.h>

namespace paperos {
String NotesService::sanitizeId(const String& id) const {
  String out;
  for (size_t i=0;i<id.length();++i) {
    char c=id[i]; if (isalnum(static_cast<unsigned char>(c)) || c=='-' || c=='_') out += c;
  }
  return out.length() ? out : String(millis());
}

String NotesService::listJson() {
  DynamicJsonDocument d(8192); JsonArray a=d.createNestedArray("notes"); d["ok"]=storage_.available();
  if (storage_.available()) {
    File root=SD.open("/PaperOS/Notes");
    for (File f=root.openNextFile(); f; f=root.openNextFile()) {
      if (!f.isDirectory()) {
        DynamicJsonDocument n(4096);
        if (!deserializeJson(n,f)) {
          JsonObject o=a.createNestedObject();
          o["id"] = n["id"] | String(f.name());
          o["title"] = n["title"] | "Untitled";
          o["category"] = n["category"] | "";
          o["favorite"] = n["favorite"] | false;
          o["updated"] = n["updated"] | 0;
        }
      }
      f.close();
    }
    root.close();
  }
  String out; serializeJson(d,out); return out;
}

String NotesService::get(const String& id) {
  String sid=sanitizeId(id); String p="/PaperOS/Notes/"+sid+".json";
  if (!storage_.available() || !SD.exists(p)) return "{}";
  File f=SD.open(p,FILE_READ); String out=f.readString(); f.close(); return out;
}

bool NotesService::save(const String& id, const String& json) {
  if (storage_.recoveryScanning()) return false;
  if (!storage_.available()) return false;
  if (!SD.exists("/PaperOS")) SD.mkdir("/PaperOS");
  if (!SD.exists("/PaperOS/Notes")) SD.mkdir("/PaperOS/Notes");
  String sid=sanitizeId(id); String p="/PaperOS/Notes/"+sid+".json"; String tmp=p+".tmp";
  DynamicJsonDocument d(8192); if (deserializeJson(d,json)) return false;
  d["id"]=sid; d["updated"]=millis();
  File f=SD.open(tmp,FILE_WRITE); if (!f) return false;
  bool ok=serializeJson(d,f)>0; f.flush(); f.close(); if (!ok) { SD.remove(tmp); return false; }
  if (SD.exists(p)) SD.remove(p); return SD.rename(tmp,p);
}

bool NotesService::remove(const String& id) {
  if (storage_.recoveryScanning()) return false;
  if (!storage_.available()) return false;
  String p="/PaperOS/Notes/"+sanitizeId(id)+".json"; return !SD.exists(p) || SD.remove(p);
}
}
