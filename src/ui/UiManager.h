#pragma once
#include <Arduino.h>
#include <vector>
#include "UiTheme.h"
#include "../drivers/DisplayManager.h"
#include "../drivers/TouchManager.h"
#include "../network/WiFiManager.h"
#include "../storage/StorageManager.h"
#include "../services/PowerManager.h"
#include "../services/SimpleBrowser.h"
#include "../services/OtpService.h"
#include "../services/NetworkToolsService.h"
#include "../services/PhoneLinkService.h"
#include "../core/ConfigManager.h"

namespace paperos {

class UiManager {
 public:
  UiManager(DisplayManager& d, TouchManager& t, WiFiManager& w, StorageManager& s, PowerManager& p, ConfigManager& c, PhoneLinkService& phone)
   : display_(d), touch_(t), wifi_(w), storage_(s), power_(p), config_(c), phoneLink_(phone) {}

  void begin();
  void loop();

  void showHome(bool forceClean = false);
  void showApps();
  void showSystem();
  void showSettings();
  void showNotes();
  void showFiles(const String& path = "/PaperOS");
  void showTools();
  void showThermo();
  void showSolar();
  void showWiFi();
  void showWifiAudit();
  void showBluetooth();
  void showGeneral();
  void showLabs();
  void showHidLab();
  void showCalculator();
  void showBattery();
  void showClock();
  void showFocus();
  void showFun();
  void showBrowser();
  void showOtp();
  void showNetworkTools();
  void showPingTool();
  void showDnsTool();
  void showLanScan();
  void showApiTester();
  void showMqtt();
  void showWakeOnLan();
  void showDateTime();
  void showStorageTools();
  void showStorageFormat();
  void showPhoneLink();
  void showGpioLab();

 private:
  enum class Page {
    Home, Apps, System, Settings, Notes, NoteView,
    Files, FileView, Tools, NetworkTools, PingTool, DnsTool, LanScan, ApiTester, Mqtt, WakeOnLan, WiFi, WifiAudit, Bluetooth, BleDetail, BleGatt, General, DateTime, StorageTools, StorageFormat, PhoneLink, GpioLab, Labs, HidLab, Calculator, Battery, Clock, Focus, Fun, Browser, Otp, Keyboard, ComingSoon
  };

  struct FileEntry {
    String name;
    bool directory = false;
    uint64_t size = 0;
  };

  struct WiFiScanEntry {
    String ssid;
    int32_t rssi = 0;
    int32_t channel = 0;
    bool encrypted = true;
    int authMode = 0;
  };

  struct BleScanEntry {
    String name;
    String address;
    String serviceUuid;
    String manufacturerHex;
    String manufacturerName;
    String beaconType;
    int32_t rssi = 0;
    int32_t txPower = 0;
    bool hasTxPower = false;
  };

  enum class InputTarget : uint8_t {
    None = 0,
    WiFiPassword,
    BrowserUrl,
    OtpSecret,
    PingHost,
    DnsHost,
    ApiUrl,
    ApiBody,
    MqttHost,
    MqttTopic,
    MqttPayload,
    WolMac,
    BrowserSearch,
    ManualTime,
    PhoneCommand
  };

  Page page_ = Page::Home;
  DisplayManager& display_;
  TouchManager& touch_;
  WiFiManager& wifi_;
  StorageManager& storage_;
  PowerManager& power_;
  ConfigManager& config_;
  PhoneLinkService& phoneLink_;

  uint32_t lastClock_ = 0;
  uint32_t lastDeepClean_ = 0;
  uint32_t lastBatteryUiRefresh_ = 0;
  uint32_t lastClockPageRefresh_ = 0;
  uint32_t lastFocusUiRefresh_ = 0;
  uint64_t lastOtpStep_ = 0;
  bool hasRenderedPage_ = false;
  Page lastRenderedPage_ = Page::Home;
  std::vector<Page> pageHistory_;
  bool navigatingBack_ = false;
  bool quickPanelOpen_ = false;
  bool keyboardShowSecret_ = false;
  int settingsScroll_ = 0;
  int wifiScroll_ = 0;
  bool wifiScanRunning_ = false;
  int bleScroll_ = 0;
  int fileScroll_ = 0;
  int textScroll_ = 0;
  int phoneScroll_ = 0;
  int browserTextScroll_ = 0;
  int browserLinkScroll_ = 0;
  uint8_t lanScanSegment_ = 0;
  int comingNav_ = 4;
  String comingTitle_;

  std::vector<FileEntry> fileEntries_;
  String currentFilePath_ = "/PaperOS";
  bool bluetoothActive_ = false;
  String calcDisplay_ = "0";
  double calcAccumulator_ = 0.0;
  char calcPendingOp_ = 0;
  bool calcResetInput_ = true;
  std::vector<String> noteIds_;
  std::vector<String> noteTitles_;
  std::vector<WiFiScanEntry> wifiScan_;
  std::vector<BleScanEntry> bleScan_;
  size_t selectedBleIndex_ = 0;
  String currentPreviewPath_;
  String currentPreviewName_;
  uint64_t currentPreviewSize_ = 0;
  size_t selectedNoteIndex_ = 0;
  bool focusRunning_ = false;
  uint32_t focusEndMs_ = 0;
  uint32_t focusRemainingSec_ = 25UL * 60UL;
  String funResult_ = "Tap a game";
  String selectedWifiSsid_;
  String wifiStatusMessage_;
  InputTarget inputTarget_ = InputTarget::None;
  String inputValue_;
  String inputPrompt_;
  bool inputMasked_ = false;
  bool keyboardShift_ = false;
  bool keyboardSymbols_ = false;
  SimpleBrowser browserService_;
  NetworkToolsService networkTools_;
  BrowserPage browserPage_;
  String browserUrl_ = "https://";
  String browserSearchQuery_;
  bool browserSearchMode_ = true;
  String otpSecret_;
  String otpCode_;
  String pingHost_ = "8.8.8.8";
  String pingResultText_;
  String dnsHost_ = "example.com";
  String dnsResultText_;
  std::vector<LanServiceHost> lanHosts_;
  String apiUrl_ = "http://";
  String apiMethod_ = "GET";
  String apiBody_;
  ApiResult apiResult_;
  String mqttHost_;
  String mqttTopic_ = "paperos/test";
  String mqttPayload_ = "hello from PaperOS";
  String mqttStatus_;
  String wolMac_;
  String wolStatus_;
  String timeStatus_;
  String storageStatus_;
  String pendingFormatType_;
  bool formatConfirmArmed_ = false;
  String phoneLinkStatus_ = "Bridge not connected";
  String phoneCommandDraft_ = "camera:shutter";
  bool gpioInitialized_ = false;
  uint8_t gpioModeState_[6] = {0,0,0,0,0,0};
  bool gpioOutputLevel_[6] = {false,false,false,false,false,false};
  String gpioStatus_;
  std::vector<String> gattRows_;

  void preparePage(bool forceClean = false);
  void renderPage(Page target);
  void navigateBack();
  void showQuickPanel();
  void closeQuickPanel();
  void handleQuickPanelTap(int x, int y);
  void handleScrollGesture(TouchGesture gesture);
  void commitPage();
  void statusBar();
  void bottomNav(int active);
  void homeCard(int x, int y, int w, int h, const String& title, const String& value, const String& detail, bool chevron = true);
  void settingRow(int y, const String& title, const String& detail);
  void showComingSoon(const String& title, int activeNav = 4);
  void showNote(size_t index);
  void showFilePreview(const String& fullPath, const String& name, uint64_t size);
  void handleBottomNav(int x);
  void openAppIndex(int index);
  String parentPath(const String& path) const;
  String joinPath(const String& base, const String& name) const;
  bool isTextFile(const String& name) const;
  bool isImageFile(const String& name) const;
  bool isPdfFile(const String& name) const;
  String extractPdfText(const String& path, size_t offset = 0) const;
  void settingsRow(int y, const String& glyph, const String& title, const String& detail, bool chevron = true);
  void calcKey(const String& key);
  void showAppLoading(const String& title, const String& detail, int percent = 55);
  void drawBatteryLiveArea();
  void drawBatteryGraph();
  void drawBatteryImpactEstimate();
  void drawClockLiveArea();
  void drawFocusLiveArea();
  void drawFunResult();
  void showKeyboard(InputTarget target, const String& prompt, const String& initial = "", bool masked = false);
  void handleKeyboardTap(int x, int y);
  void finishKeyboard();
  void drawKeyboard();
  void fetchBrowserUrl(const String& url);
  void drawOtpCode();
  void showBleDetail(size_t index);
  String bleManufacturerName(const String& bytes) const;
  String bleManufacturerHex(const String& bytes) const;
  String bytesHex(const String& bytes, size_t maxBytes = 18) const;
  void readBleGatt(size_t index);
};

}
