#include "OtpService.h"
#include <vector>
#include <mbedtls/md.h>

namespace paperos {

int OtpService::base32Value(char c) {
  if (c >= 'A' && c <= 'Z') return c - 'A';
  if (c >= '2' && c <= '7') return 26 + (c - '2');
  return -1;
}

String OtpService::normalizeSecret(const String& secret) {
  String out;
  out.reserve(secret.length());
  for (size_t i = 0; i < secret.length(); ++i) {
    char c = secret[i];
    if (c == ' ' || c == '-' || c == '=') continue;
    if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    if ((c >= 'A' && c <= 'Z') || (c >= '2' && c <= '7')) out += c;
  }
  return out;
}

bool OtpService::validSecret(const String& secret) {
  String s = normalizeSecret(secret);
  if (s.length() < 8) return false;
  for (size_t i = 0; i < s.length(); ++i) if (base32Value(s[i]) < 0) return false;
  return true;
}

String OtpService::generate(const String& secret, uint64_t unixTime, uint8_t digits, uint32_t periodSeconds) {
  String s = normalizeSecret(secret);
  if (!validSecret(s) || !periodSeconds) return "";

  std::vector<uint8_t> key;
  key.reserve((s.length() * 5 + 7) / 8);

  uint32_t buffer = 0;
  int bits = 0;
  for (size_t i = 0; i < s.length(); ++i) {
    int v = base32Value(s[i]);
    if (v < 0) return "";
    buffer = (buffer << 5) | static_cast<uint32_t>(v);
    bits += 5;
    if (bits >= 8) {
      bits -= 8;
      key.push_back(static_cast<uint8_t>((buffer >> bits) & 0xFF));
    }
  }
  if (key.empty()) return "";

  uint64_t counter = unixTime / periodSeconds;
  uint8_t msg[8];
  for (int i = 7; i >= 0; --i) {
    msg[i] = static_cast<uint8_t>(counter & 0xFF);
    counter >>= 8;
  }

  uint8_t digest[20] = {0};
  const mbedtls_md_info_t* info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA1);
  if (!info) return "";

  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  if (mbedtls_md_setup(&ctx, info, 1) != 0 ||
      mbedtls_md_hmac_starts(&ctx, key.data(), key.size()) != 0 ||
      mbedtls_md_hmac_update(&ctx, msg, sizeof(msg)) != 0 ||
      mbedtls_md_hmac_finish(&ctx, digest) != 0) {
    mbedtls_md_free(&ctx);
    return "";
  }
  mbedtls_md_free(&ctx);

  int offset = digest[19] & 0x0F;
  uint32_t binary =
      ((digest[offset] & 0x7F) << 24) |
      ((digest[offset + 1] & 0xFF) << 16) |
      ((digest[offset + 2] & 0xFF) << 8) |
      (digest[offset + 3] & 0xFF);

  uint32_t mod = digits == 8 ? 100000000UL : 1000000UL;
  uint32_t code = binary % mod;
  char out[10];
  snprintf(out, sizeof(out), digits == 8 ? "%08lu" : "%06lu", static_cast<unsigned long>(code));
  return String(out);
}

}
