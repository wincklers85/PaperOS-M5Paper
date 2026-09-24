#include "SimpleBrowser.h"
#include <HTTPClient.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

namespace paperos {

String SimpleBrowser::normalizeUrl(const String& rawUrl) const {
  String url = rawUrl;
  url.trim();
  if (!url.length()) return "";
  if (!url.startsWith("http://") && !url.startsWith("https://")) url = "https://" + url;
  return url;
}

String SimpleBrowser::decodeEntities(String text) const {
  text.replace("&nbsp;", " ");
  text.replace("&amp;", "&");
  text.replace("&lt;", "<");
  text.replace("&gt;", ">");
  text.replace("&quot;", String((char)34));
  text.replace("&#39;", "'");
  return text;
}

String SimpleBrowser::stripTags(String html) const {
  String lower = html;
  lower.toLowerCase();

  for (const char* tag : {"script", "style", "svg"}) {
    String openTag = String("<") + tag;
    String closeTag = String("</") + tag + ">";
    while (true) {
      lower = html;
      lower.toLowerCase();
      int a = lower.indexOf(openTag);
      if (a < 0) break;
      int b = lower.indexOf(closeTag, a);
      if (b < 0) {
        html.remove(a);
        break;
      }
      html.remove(a, b + closeTag.length() - a);
    }
  }

  String out;
  size_t reserveLen = html.length() < 12000 ? html.length() : 12000;
  out.reserve(reserveLen);
  bool inTag = false;
  bool lastSpace = false;
  for (size_t i = 0; i < html.length() && out.length() < 12000; ++i) {
    char c = html[i];
    if (c == '<') { inTag = true; if (!lastSpace) { out += ' '; lastSpace = true; } continue; }
    if (c == '>') { inTag = false; continue; }
    if (inTag) continue;
    bool space = c == '\r' || c == '\n' || c == '\t' || c == ' ';
    if (space) {
      if (!lastSpace) out += ' ';
      lastSpace = true;
    } else {
      out += c;
      lastSpace = false;
    }
  }
  out.trim();
  return decodeEntities(out);
}

void SimpleBrowser::parseHtml(const String& html, BrowserPage& page) const {
  String lower = html;
  lower.toLowerCase();

  int ta = lower.indexOf("<title");
  if (ta >= 0) {
    ta = lower.indexOf('>', ta);
    int tb = lower.indexOf("</title>", ta + 1);
    if (ta >= 0 && tb > ta) {
      page.title = stripTags(html.substring(ta + 1, tb));
      if (page.title.length() > 80) page.title = page.title.substring(0, 80);
    }
  }
  if (!page.title.length()) page.title = page.finalUrl;

  int pos = 0;
  while (page.links.size() < 8) {
    int href = lower.indexOf("href", pos);
    if (href < 0) break;
    int eq = lower.indexOf('=', href + 4);
    if (eq < 0) break;
    int p = eq + 1;
    while (p < static_cast<int>(html.length()) && (html[p] == ' ' || html[p] == '\t')) ++p;
    if (p >= static_cast<int>(html.length())) break;

    char quote = html[p];
    int end = -1;
    String link;
    if (quote == '"' || quote == '\'') {
      end = html.indexOf(quote, p + 1);
      if (end > p) link = html.substring(p + 1, end);
    } else {
      end = p;
      while (end < static_cast<int>(html.length()) && html[end] != ' ' && html[end] != '>') ++end;
      link = html.substring(p, end);
    }

    if (link.startsWith("http://") || link.startsWith("https://")) {
      bool duplicate = false;
      for (const auto& existing : page.links) if (existing == link) duplicate = true;
      if (!duplicate) page.links.push_back(link);
    }
    pos = end > p ? end + 1 : p + 1;
  }

  page.text = stripTags(html);
  if (page.text.length() > 7000) page.text = page.text.substring(0, 7000);
}

BrowserPage SimpleBrowser::fetch(const String& rawUrl) {
  BrowserPage page;
  String url = normalizeUrl(rawUrl);
  page.finalUrl = url;
  if (!url.length()) { page.error = "Empty URL"; return page; }

  HTTPClient http;
  http.setConnectTimeout(6000);
  http.setTimeout(8000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("PaperOS/0.1 M5Paper Reader");

  WiFiClient plain;
  WiFiClientSecure secure;
  bool begun = false;
  if (url.startsWith("https://")) {
    secure.setInsecure();
    begun = http.begin(secure, url);
  } else {
    begun = http.begin(plain, url);
  }

  if (!begun) { page.error = "Unable to open URL"; return page; }

  int code = http.GET();
  page.status = code;
  if (code <= 0) {
    page.error = http.errorToString(code);
    http.end();
    return page;
  }

  String contentType = http.header("Content-Type");
  if (contentType.length() && contentType.indexOf("text/") < 0 &&
      contentType.indexOf("html") < 0 && contentType.indexOf("json") < 0) {
    page.error = "Unsupported content type: " + contentType;
    http.end();
    return page;
  }

  WiFiClient* stream = http.getStreamPtr();
  String payload;
  payload.reserve(16000);
  uint32_t lastData = millis();
  while (http.connected() && payload.length() < 40000 && millis() - lastData < 2500) {
    int available = stream->available();
    if (available > 0) {
      char buf[513];
      int readNow = stream->readBytes(buf, min(available, 512));
      if (readNow > 0) {
        buf[readNow] = 0;
        payload.concat(buf, readNow);
        lastData = millis();
      }
    } else {
      delay(5);
    }
  }

  page.finalUrl = http.getLocation().length() ? http.getLocation() : url;
  http.end();

  if (code >= 400) {
    page.error = String("HTTP ") + code;
    return page;
  }

  parseHtml(payload, page);
  page.ok = true;
  return page;
}

}
