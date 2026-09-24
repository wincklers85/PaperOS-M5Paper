#pragma once
#include <Arduino.h>
#include <vector>

namespace paperos {

struct BrowserPage {
  bool ok = false;
  int status = 0;
  String finalUrl;
  String title;
  String text;
  String error;
  std::vector<String> links;
};

class SimpleBrowser {
 public:
  BrowserPage fetch(const String& rawUrl);

 private:
  String normalizeUrl(const String& rawUrl) const;
  void parseHtml(const String& html, BrowserPage& page) const;
  String decodeEntities(String text) const;
  String stripTags(String html) const;
};

}
