#pragma once

#include <Arduino.h>

#ifndef PAPEROS_VERSION
#define PAPEROS_VERSION "0.3.2-alpha"
#endif

namespace paperos {
static constexpr const char* NAME = "PaperOS";
static constexpr const char* VENDOR = "WinLabs Solutions";
static constexpr const char* VERSION = PAPEROS_VERSION;
static constexpr const char* HOSTNAME = "paperos";
static constexpr const char* SETUP_AP = "PaperOS-Setup";
static constexpr const char* ROOT_DIR = "/PaperOS";
// The original M5Paper target has an 8 MiB PSRAM chip. Classic ESP32 maps
// only 4 MiB into the normal address space; ESP.getPsramSize() reports that
// directly usable region for this Arduino build.
static constexpr uint32_t PSRAM_INSTALLED_BYTES = 8U * 1024U * 1024U;
static constexpr uint32_t SERIAL_BAUD = 115200;
}
