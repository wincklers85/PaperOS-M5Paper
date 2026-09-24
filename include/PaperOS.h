#pragma once

#include <Arduino.h>

#ifndef PAPEROS_VERSION
#define PAPEROS_VERSION "0.3.0-alpha"
#endif

namespace paperos {
static constexpr const char* NAME = "PaperOS";
static constexpr const char* VENDOR = "WinLabs Solutions";
static constexpr const char* VERSION = PAPEROS_VERSION;
static constexpr const char* HOSTNAME = "paperos";
static constexpr const char* SETUP_AP = "PaperOS-Setup";
static constexpr const char* ROOT_DIR = "/PaperOS";
static constexpr uint32_t SERIAL_BAUD = 115200;
}
