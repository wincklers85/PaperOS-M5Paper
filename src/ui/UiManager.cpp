#include "UiManager.h"
#include "PaperOS.h"
#include <WiFi.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <esp_system.h>
#include <time.h>
#include <sys/time.h>
#include <algorithm>

namespace paperos {

static String normalizedPhoneNumber(const String& raw) {
  String value;
  for (size_t i = 0; i < raw.length(); ++i) {
    char c = raw[i];
    if ((c >= '0' && c <= '9') || c == '*' || c == '#') value += c;
    else if (c == '+' && value.length() == 0) value += c;
  }
  return value;
}

void UiManager::preparePage(bool forceClean) {
  const bool pageChanged = hasRenderedPage_ && page_ != lastRenderedPage_;

  if (pageChanged) {
    wheelFocusVisible_ = false;
    wheelFocusIndex_ = 0;
    if (!navigatingBack_) {
      if (pageHistory_.empty() || pageHistory_.back() != lastRenderedPage_) {
        pageHistory_.push_back(lastRenderedPage_);
        if (pageHistory_.size() > 24) pageHistory_.erase(pageHistory_.begin());
      }
    } else {
      navigatingBack_ = false;
    }
  }

  if (forceClean || pageChanged) {
    display_.cleanRefresh();
    lastDeepClean_ = millis();
  }

  lastRenderedPage_ = page_;
  hasRenderedPage_ = true;
  quickPanelOpen_ = false;
  UiTheme::beginFrame();
}

void UiManager::commitPage() {
  if (wheelFocusVisible_ && !quickPanelOpen_ && !notificationPanelOpen_ && !locked_) {
    drawWheelFocus();
  }
  display_.pageRefresh();
}

void UiManager::showAppLoading(const String& titleText, const String& detailText, int percent) {
  UiTheme::beginFrame();
  statusBar();

  UiTheme::title(titleText, 18, 92);
  UiTheme::detail(detailText, 20, 136);

  UiTheme::card(42, 270, 456, 250, true);
  UiTheme::label("LOADING", 66, 300);
  UiTheme::value("Please wait", 66, 344, false);
  UiTheme::detail("PaperOS is preparing the app.", 66, 392);

  const int barX = 66, barY = 444, barW = 408, barH = 18;
  M5.Display.drawRoundRect(barX, barY, barW, barH, 9, TFT_BLACK);
  int fill = constrain((barW - 6) * constrain(percent, 0, 100) / 100, 0, barW - 6);
  if (fill > 0) M5.Display.fillRoundRect(barX + 3, barY + 3, fill, barH - 6, 6, TFT_BLACK);

  UiTheme::detail(String(constrain(percent, 0, 100)) + "%", 430, 478);
  display_.pageRefresh();
}

void UiManager::drawBatteryLiveArea() {
  const int pct = constrain(power_.batteryPercent(), 0, 100);
  const int mv = power_.batteryMillivolts();

  UiTheme::card(18, 145, 504, 178, true);
  UiTheme::label("BATTERY", 34, 162);
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(String(pct) + "%", 34, 201);
  UiTheme::resetFont();
  UiTheme::detail(String(mv) + " mV", 36, 260);

  const int bx = 238, by = 194, bw = 246, bh = 46;
  M5.Display.drawRoundRect(bx, by, bw, bh, 10, TFT_BLACK);
  M5.Display.fillRect(bx + bw, by + 13, 8, 20, TFT_BLACK);
  const int fill = (bw - 10) * pct / 100;
  if (fill > 0) M5.Display.fillRoundRect(bx + 5, by + 5, fill, bh - 10, 6, TFT_BLACK);

  UiTheme::detail(String("Voltage trend: ") + power_.batteryTrendLabel(), 238, 258);
  if (power_.chargingLikely()) {
    UiTheme::pill("CHARGE LIKELY", 336, 279, true);
  } else if (power_.batteryTrend() == BatteryTrend::Falling) {
    UiTheme::pill("ON BATTERY", 360, 279, false);
  } else {
    UiTheme::pill("MONITORING", 362, 279, false);
  }

  UiTheme::card(18, 338, 504, 132);
  UiTheme::label("LIVE POWER STATUS", 34, 355);
  UiTheme::value(power_.chargingLikely() ? "Probabile ricarica" : "Stato non misurabile", 34, 392, false);
  UiTheme::detail(String("Trend ") + String(power_.batteryTrendDeltaMv()) + " mV / finestra campioni", 34, 430);
  UiTheme::detail("USB/charger state is not exposed by M5Paper V1 hardware.", 34, 453);
}

void UiManager::begin() {
  display_.setProfile(static_cast<uint8_t>(config_.get().displayProfile));
  UiTheme::setStyle(static_cast<uint8_t>(config_.get().uiStyle));
  showHome(false);
}

void UiManager::statusBar() {
  const int w = M5.Display.width();
  auto dt = M5.Rtc.getDateTime();

  M5.Display.fillRect(0, 0, w, UiTheme::StatusH, TFT_WHITE);
  M5.Display.drawFastHLine(0, UiTheme::StatusH - 1, w, TFT_BLACK);

  const bool canBack = page_ != Page::Home && !pageHistory_.empty();
  if (canBack) {
    M5.Display.drawLine(24, 18, 12, 28, TFT_BLACK);
    M5.Display.drawLine(12, 28, 24, 38, TFT_BLACK);
    M5.Display.drawFastHLine(12, 28, 18, TFT_BLACK);
  }

  char clockText[8];
  snprintf(clockText, sizeof(clockText), "%02d:%02d", dt.time.hours, dt.time.minutes);
  M5.Display.setFont(&fonts::FreeSansBold12pt7b);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(clockText, canBack ? 42 : 14, 15);

  M5.Display.setFont(&fonts::FreeSans9pt7b);
  String dateText;
  if (dt.date.year >= 2020 && dt.date.year <= 2099) {
    char b[14];
    snprintf(b, sizeof(b), "%02d/%02d/%02d", dt.date.date, dt.date.month, dt.date.year % 100);
    dateText = b;
  } else {
    dateText = "RTC SET";
  }
  M5.Display.drawString(dateText, canBack ? 120 : 92, 20);

  const int iconY = 19;
  UiTheme::wifiIcon(350, iconY, wifi_.radioEnabled() && wifi_.isConnected());
  UiTheme::bluetoothIcon(382, iconY - 1, bluetoothActive_);
  UiTheme::sdIcon(416, iconY - 3, storage_.available());
  UiTheme::batteryIcon(493, iconY + 1, power_.batteryPercent());

  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.setTextDatum(top_right);
  M5.Display.drawString(String(power_.batteryPercent()) + "%", 486, 20);
  UiTheme::resetFont();
}

void UiManager::bottomNav(int active) {
  M5.Display.fillRect(0, UiTheme::NavY, UiTheme::ScreenW, UiTheme::NavH, TFT_WHITE);
  M5.Display.drawFastHLine(0, UiTheme::NavY, UiTheme::ScreenW, TFT_BLACK);
  const char* labels[5] = {"Home", "Notes", "Files", "Tools", "Apps"};
  for (int i = 0; i < 5; ++i) UiTheme::navItem(i, labels[i], i == active);
}

void UiManager::homeCard(int x, int y, int w, int h, const String& titleText, const String& mainValue, const String& detailText, bool withChevron) {
  UiTheme::card(x, y, w, h);
  UiTheme::label(titleText, x + 16, y + 14);
  if (withChevron) UiTheme::chevron(x + w - 28, y + 18);
  UiTheme::value(mainValue, x + 16, y + 47, false);
  UiTheme::detail(detailText, x + 16, y + h - 32);
}


void UiManager::renderPage(Page target) {
  switch (target) {
    case Page::Home: showHome(); break;
    case Page::Apps: showApps(); break;
    case Page::System: showSystem(); break;
    case Page::Settings: showSettings(); break;
    case Page::Notes: showNotes(); break;
    case Page::NoteView:
      if (selectedNoteIndex_ < noteIds_.size()) showNote(selectedNoteIndex_);
      else showNotes();
      break;
    case Page::Files: showFiles(currentFilePath_); break;
    case Page::LockScreen: showLockScreen(); break;
    case Page::FileView:
      if (currentPreviewPath_.length()) showFilePreview(currentPreviewPath_, currentPreviewName_, currentPreviewSize_);
      else showFiles(currentFilePath_);
      break;
    case Page::Tools: showTools(); break;
    case Page::NetworkTools: showNetworkTools(); break;
    case Page::PingTool: showPingTool(); break;
    case Page::DnsTool: showDnsTool(); break;
    case Page::LanScan: showLanScan(); break;
    case Page::ApiTester: showApiTester(); break;
    case Page::Mqtt: showMqtt(); break;
    case Page::WakeOnLan: showWakeOnLan(); break;
    case Page::WiFi: showWiFi(); break;
    case Page::WifiAudit: showWifiAudit(); break;
    case Page::Bluetooth: showBluetooth(); break;
    case Page::BleDetail:
      if (selectedBleIndex_ < bleScan_.size()) showBleDetail(selectedBleIndex_);
      else showBluetooth();
      break;
    case Page::General: showGeneral(); break;
    case Page::DateTime: showDateTime(); break;
    case Page::StorageTools: showStorageTools(); break;
    case Page::StorageFormat: showStorageFormat(); break;
    case Page::RecoveryHelp: showRecoveryHelp(); break;
    case Page::RecoveryPreview: showRecoveryPreview(selectedRecoveryIndex_); break;
    case Page::RecoveryConfirm: showRecoveryConfirm(); break;
    case Page::PhoneLink: showPhoneLink(); break;
    case Page::Phone: showPhone(); break;
    case Page::GpioLab: showGpioLab(); break;
    case Page::NfcLab: showNfcLab(); break;
    case Page::Labs: showLabs(); break;
    case Page::HidLab: showHidLab(); break;
    case Page::ClassicSplash: showClassicSplash(); break;
    case Page::ClassicDesktop: showClassicDesktop(); break;
    case Page::ClassicTerminal: showClassicTerminal(); break;
    case Page::ClassicHid: showClassicHid(); break;
    case Page::ClassicSki: showClassicSki(); break;
    case Page::ClassicSolitaire: showClassicSolitaire(); break;
    case Page::Calculator: showCalculator(); break;
    case Page::Battery: showBattery(); break;
    case Page::Clock: showClock(); break;
    case Page::Focus: showFocus(); break;
    case Page::Fun: showFun(); break;
    case Page::Browser: showBrowser(); break;
    case Page::Otp: showOtp(); break;
    case Page::ComingSoon: showComingSoon(comingTitle_, comingNav_); break;
    default: showHome(); break;
  }
}

void UiManager::navigateBack() {
  if (quickPanelOpen_) {
    closeQuickPanel();
    return;
  }
  if (pageHistory_.empty()) {
    showHome();
    return;
  }

  Page target = pageHistory_.back();
  pageHistory_.pop_back();
  navigatingBack_ = true;
  renderPage(target);
}

void UiManager::showQuickPanel() {
  quickPanelOpen_ = true;
  // E-paper has no alpha compositing. Render an opaque page after a clean
  // white refresh so the old app cannot ghost through this control panel.
  display_.cleanRefresh();
  UiTheme::beginFrame();
  UiTheme::shadowCard(10, 10, 520, 936, 4);
  M5.Display.fillRoundRect(218, 594, 104, 6, 3, TFT_BLACK);

  UiTheme::title("Quick Settings", 28, 28);
  UiTheme::detail("PaperOS Control Center  /  swipe up to close", 30, 69);

  UiTheme::iconButton(22, 104, 238, 106, "WF", wifi_.radioEnabled() ? "Wi-Fi ON" : "Wi-Fi OFF", wifi_.radioEnabled());
  UiTheme::iconButton(280, 104, 238, 106, "BT", bluetoothActive_ ? "Bluetooth ON" : "Bluetooth OFF", bluetoothActive_);

  UiTheme::iconButton(22, 226, 238, 106, "EPD", String("Display ") + display_.profileLabel(), false);
  UiTheme::iconButton(280, 226, 238, 106, "NTP", wifi_.timeSynced() ? "Time synced" : "Sync time", false);

  UiTheme::iconButton(22, 348, 238, 106, String(power_.batteryPercent()) + "%", "Battery statistics", false);
  UiTheme::iconButton(280, 348, 238, 106, "LOCK", "Blocca", true);

  UiTheme::card(22, 470, 496, 100);
  UiTheme::label("DISPLAY CLEANUP", 40, 487);
  UiTheme::detail("Tap here for a full white refresh to clear ghosting.", 40, 522);
  UiTheme::detail("Swipe up anywhere to return to the open app.", 40, 550);

  display_.pageRefresh();
}

void UiManager::closeQuickPanel() {
  if (!quickPanelOpen_) return;
  quickPanelOpen_ = false;
  Page keep = page_;
  renderPage(keep);
}

void UiManager::handleQuickPanelTap(int x, int y) {
  if (y >= 96 && y < 208) {
    if (x < 270) {
      wifi_.setRadioEnabled(!wifi_.radioEnabled());
      showQuickPanel();
    } else {
      if (bluetoothActive_) {
        phoneLink_.stop();
        hidInput_.stop();
        BLEDevice::deinit(true);
        bluetoothActive_ = false;
        bleScan_.clear();
      } else {
        BLEDevice::init("PaperOS");
        bluetoothActive_ = true;
      }
      showQuickPanel();
    }
    return;
  }

  if (y >= 222 && y < 334) {
    if (x < 270) {
      uint8_t next = (display_.profile() + 1) % 3;
      display_.setProfile(next);
      config_.edit().displayProfile = static_cast<DisplayProfile>(next);
      config_.save();
      showQuickPanel();
    } else {
      timeStatus_ = wifi_.syncClockNow() ? "NTP synchronization completed" : "Connect Wi-Fi to synchronize";
      showQuickPanel();
    }
    return;
  }

  if (y >= 348 && y < 460) {
    if (x < 270) {
      quickPanelOpen_ = false;
      showBattery();
    } else {
      quickPanelOpen_ = false;
      power_.sleepNow();
    }
    return;
  }

  if (y >= 470 && y < 570) {
    Page keep = page_;
    quickPanelOpen_ = false;
    display_.cleanRefresh();
    renderPage(keep);
  }
}


void UiManager::showLockScreen() {
  if (!locked_) pageBeforeLock_ = page_;
  locked_ = true;
  quickPanelOpen_ = false;
  notificationPanelOpen_ = false;
  wheelFocusVisible_ = false;
  page_ = Page::LockScreen;

  display_.cleanRefresh();
  UiTheme::beginFrame();

  auto dt = M5.Rtc.getDateTime();
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);

  M5.Display.setFont(&fonts::FreeSansBold18pt7b);
  M5.Display.drawString("PaperOS", 270, 96);

  char clockText[8];
  snprintf(clockText, sizeof(clockText), "%02d:%02d", dt.time.hours, dt.time.minutes);
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.drawString(clockText, 270, 202);

  char dateText[28];
  snprintf(dateText, sizeof(dateText), "%02d/%02d/%04d", dt.date.date, dt.date.month, dt.date.year);
  M5.Display.setFont(&fonts::FreeSans12pt7b);
  M5.Display.drawString(dateText, 270, 254);

  UiTheme::shadowCard(54, 330, 432, 146, 5);
  UiTheme::label("DEVICE", 78, 350);
  UiTheme::value(String("PaperOS  ") + power_.batteryPercent() + "%", 78, 386, false);
  String phoneState = phoneLink_.ancsReady() ? "iPhone ANCS connected" :
                      (phoneLink_.connected() ? "iPhone Bluetooth connected" : "Phone disconnected");
  UiTheme::detail(phoneState, 78, 430);

  UiTheme::shadowCard(54, 510, 432, 206, 5);
  if (!lockWakeArmed_) {
    UiTheme::label("STANDBY / LOCKED", 78, 532);
    UiTheme::value("Premi OK", 78, 578, true);
    UiTheme::detail("Premi la rotella del mouse verso l'interno.", 78, 638);
    UiTheme::detail("Fallback: premi il tasto centrale del M5Paper.", 78, 672);
  } else {
    UiTheme::label("READY TO UNLOCK", 78, 532);
    UiTheme::value("Swipe to unlock", 78, 578, true);
    M5.Display.drawLine(118, 660, 400, 660, TFT_BLACK);
    M5.Display.drawLine(400, 660, 378, 646, TFT_BLACK);
    M5.Display.drawLine(400, 660, 378, 674, TFT_BLACK);
    UiTheme::detail("Scorri verso destra sul touchscreen.", 78, 696);
  }

  UiTheme::detail("Wi-Fi, Bluetooth e Phone Link restano attivi in soft lock.", 270, 802);
  UiTheme::resetFont();
  display_.pageRefresh();
}

void UiManager::armUnlock() {
  if (!locked_) return;
  lockWakeArmed_ = true;
  power_.markActivity();
  showLockScreen();
}

void UiManager::unlockToMenu() {
  if (!locked_ || !lockWakeArmed_) return;
  locked_ = false;
  lockWakeArmed_ = false;
  pageHistory_.clear();
  hasRenderedPage_ = false;
  power_.markActivity();
  display_.cleanRefresh();
  showApps();
}

void UiManager::showNotificationCenter() {
  if (locked_) return;
  notificationPanelOpen_ = true;
  quickPanelOpen_ = false;

  UiTheme::ditherOverlay(0, 190, UiTheme::ScreenW, UiTheme::ScreenH - 190);
  UiTheme::shadowCard(10, 210, 520, 732, 7);
  M5.Display.fillRoundRect(218, 224, 104, 6, 3, TFT_BLACK);

  UiTheme::title("Notification Center", 28, 252);
  UiTheme::detail("Swipe down to close / wheel to browse", 30, 292);

  UiTheme::shadowCard(26, 322, 488, 118, 4);
  UiTheme::label("PHONE", 44, 338);
  String state = phoneLink_.ancsReady() ? "ANCS CONNECTED" :
                 (phoneLink_.connected() ? "BLUETOOTH CONNECTED" : "DISCONNECTED");
  UiTheme::value(phoneLink_.phoneName() + "  /  " + state, 44, 370, false);

  String battery = "Batteria telefono: N/D";
  if (phoneLink_.phoneBatteryKnown()) {
    battery = String("Batteria telefono: ") + phoneLink_.phoneBatteryPercent() + "%";
    if (phoneLink_.phoneChargingKnown()) battery += phoneLink_.phoneCharging() ? " / in carica" : " / non in carica";
  } else {
    battery += "  (serve companion/Shortcut)";
  }
  UiTheme::detail(battery, 44, 408);

  const int total = static_cast<int>(phoneLink_.notificationCount());
  const int visible = 5;
  notificationScroll_ = constrain(notificationScroll_, 0, max(0, total - visible));

  UiTheme::label("NOTIFICHE", 28, 462);
  if (!total) {
    UiTheme::shadowCard(26, 490, 488, 122, 3);
    UiTheme::value("Nessuna notifica", 44, 524, false);
    UiTheme::detail(phoneLink_.ancsReady() ? "Phone Link e attivo." : "Collega iPhone Link per ricevere ANCS.", 44, 566);
  } else {
    for (int row=0; row<visible && notificationScroll_+row<total; ++row) {
      const PhoneNotification* n=phoneLink_.notification(static_cast<size_t>(notificationScroll_+row));
      if (!n) continue;
      const int y=488+row*82;
      UiTheme::card(26,y,488,72);
      String category;
      switch(n->category){
        case 1: category="CALL"; break; case 2: category="MISSED CALL"; break;
        case 4: category="SOCIAL"; break; case 5: category="CALENDAR"; break;
        case 6: category="MAIL"; break; case 7: category="NEWS"; break;
        default: category=n->app.length()?n->app:"PHONE"; break;
      }
      String title=category+" / "+n->title;
      if(title.length()>45) title=title.substring(0,42)+"...";
      String body=n->body;
      if(body.length()>58) body=body.substring(0,55)+"...";
      UiTheme::value(title,42,y+9,false);
      UiTheme::detail(body,42,y+42);
    }
  }

  String pageInfo = total
    ? String(notificationScroll_+1) + "-" + String(min(total, notificationScroll_+visible)) + " / " + String(total)
    : String("0 / 0");
  UiTheme::detail(pageInfo, 396, 906);
  display_.partialRefresh(0, 190, UiTheme::ScreenW, UiTheme::ScreenH-190);
}

void UiManager::closeNotificationCenter() {
  if (!notificationPanelOpen_) return;
  notificationPanelOpen_ = false;
  renderPage(page_);
}

void UiManager::handleNotificationPanelTap(int x, int y) {
  if (y >= 322 && y < 440) {
    notificationPanelOpen_ = false;
    showPhoneLink();
    return;
  }
  if (y >= 488 && y < 898) {
    int row=(y-488)/82;
    int idx=notificationScroll_+row;
    if (idx >= 0 && idx < static_cast<int>(phoneLink_.notificationCount())) {
      phoneScroll_=idx;
      notificationPanelOpen_=false;
      showPhoneLink();
    }
  }
}


bool UiManager::isClassicPage() const {
  return page_ == Page::ClassicDesktop || page_ == Page::ClassicTerminal ||
         page_ == Page::ClassicHid || page_ == Page::ClassicSki ||
         page_ == Page::ClassicSolitaire || page_ == Page::ClassicSplash;
}

int UiManager::wheelItemCount() const {
  switch (page_) {
    case Page::Home: return 8;
    case Page::Apps: return 18;
    case Page::Settings: return 10;
    case Page::Files: return static_cast<int>(fileEntries_.size());
    case Page::Tools: return 6;
    case Page::NetworkTools: return 6;
    case Page::Labs: return 6;
    case Page::WiFi: return 1 + static_cast<int>(wifiScan_.size());
    case Page::Bluetooth: return static_cast<int>(bleScan_.size());
    case Page::ClassicDesktop: return 9;
    default: return 0;
  }
}

void UiManager::drawWheelFocus() {
  if (!wheelFocusVisible_) return;
  const int count=wheelItemCount();
  if (count<=0 || wheelFocusIndex_<0 || wheelFocusIndex_>=count) return;
  int x=0,y=0,w=0,h=0;
  const int idx=wheelFocusIndex_;

  if (page_ == Page::Home) {
    const int rects[8][4] = {
      {16,143,352,132},{364,143,160,132},
      {16,320,250,112},{274,320,250,112},
      {16,440,250,112},{274,440,250,112},
      {16,602,250,106},{274,602,250,106}
    };
    x=rects[idx][0];y=rects[idx][1];w=rects[idx][2];h=rects[idx][3];
  } else if (page_ == Page::Apps) {
    if (idx < 17) {
      int col=idx%4,row=idx/4;
      x=16+col*128; y=152+row*116; w=124; h=112;
    } else { x=16;y=744;w=508;h=108; }
  } else if (page_ == Page::Settings) {
    if (idx < settingsScroll_ || idx >= settingsScroll_+6) return;
    x=20;y=258+(idx-settingsScroll_)*82;w=500;h=82;
  } else if (page_ == Page::Files) {
    if (idx < fileScroll_ || idx >= fileScroll_+6) return;
    x=16;y=282+(idx-fileScroll_)*72;w=508;h=68;
  } else if (page_ == Page::Tools) {
    x=16;y=148+idx*92;w=508;h=92;
  } else if (page_ == Page::NetworkTools) {
    x=16;y=148+idx*90;w=508;h=82;
  } else if (page_ == Page::Labs) {
    x=16;y=250+idx*90;w=508;h=82;
  } else if (page_ == Page::WiFi) {
    if (idx == 0) { x=16;y=143;w=508;h=122; }
    else {
      int item=idx-1;
      if (item < wifiScroll_ || item >= wifiScroll_+5) return;
      x=20;y=309+(item-wifiScroll_)*88;w=500;h=82;
    }
  } else if (page_ == Page::Bluetooth) {
    if (idx < bleScroll_ || idx >= bleScroll_+5) return;
    x=20;y=305+(idx-bleScroll_)*88;w=500;h=82;
  } else if (page_ == Page::ClassicDesktop) {
    int col=idx%3,row=idx/3;
    x=50+col*154;y=174+row*152;w=108;h=98;
  } else return;

  M5.Display.drawRoundRect(x,y,w,h,8,TFT_BLACK);
  M5.Display.drawRoundRect(x+2,y+2,w-4,h-4,7,TFT_BLACK);
}

void UiManager::handleWheelNavigate(int direction) {
  if (!direction) return;
  power_.markActivity();

  if (locked_) return;

  if (notificationPanelOpen_) {
    int total=static_cast<int>(phoneLink_.notificationCount());
    notificationScroll_=constrain(notificationScroll_+direction,0,max(0,total-5));
    showNotificationCenter();
    return;
  }

  int count=wheelItemCount();
  if (count <= 0) {
    wheelFocusVisible_=false;
    handleScrollGesture(direction > 0 ? TouchGesture::SwipeUp : TouchGesture::SwipeDown);
    return;
  }

  if (!wheelFocusVisible_) {
    wheelFocusVisible_=true;
    wheelFocusIndex_=direction>0 ? 0 : count-1;
  } else {
    wheelFocusIndex_=constrain(wheelFocusIndex_+direction,0,count-1);
  }

  if (page_ == Page::Settings) {
    if (wheelFocusIndex_ < settingsScroll_) settingsScroll_=wheelFocusIndex_;
    if (wheelFocusIndex_ >= settingsScroll_+6) settingsScroll_=wheelFocusIndex_-5;
  } else if (page_ == Page::Files) {
    if (wheelFocusIndex_ < fileScroll_) fileScroll_=wheelFocusIndex_;
    if (wheelFocusIndex_ >= fileScroll_+6) fileScroll_=wheelFocusIndex_-5;
  } else if (page_ == Page::WiFi && wheelFocusIndex_ > 0) {
    int item=wheelFocusIndex_-1;
    if (item < wifiScroll_) wifiScroll_=item;
    if (item >= wifiScroll_+5) wifiScroll_=item-4;
  } else if (page_ == Page::Bluetooth) {
    if (wheelFocusIndex_ < bleScroll_) bleScroll_=wheelFocusIndex_;
    if (wheelFocusIndex_ >= bleScroll_+5) bleScroll_=wheelFocusIndex_-4;
  }

  renderPage(page_);
}

void UiManager::activateWheelFocus() {
  power_.markActivity();
  if (locked_) {
    armUnlock();
    return;
  }
  if (notificationPanelOpen_) {
    notificationPanelOpen_=false;
    showPhoneLink();
    return;
  }

  int count=wheelItemCount();
  if (count <= 0) return;
  if (!wheelFocusVisible_) {
    wheelFocusVisible_=true;
    wheelFocusIndex_=0;
    renderPage(page_);
    return;
  }

  const int idx=constrain(wheelFocusIndex_,0,count-1);

  if (page_ == Page::Home) {
    if(idx==0) showWiFi(); else if(idx==1) showBattery();
    else if(idx==2) showNotes(); else if(idx==3) showFiles();
    else if(idx==4) showSettings(); else if(idx==5) showSystem();
    else if(idx==6) showThermo(); else showSolar();
    return;
  }

  if (page_ == Page::Apps) {
    if(idx<17) openAppIndex(idx); else showClassicSplash();
    return;
  }

  if (page_ == Page::Settings) {
    if (idx==0) showGeneral();
    else if (idx==1) {
      uint8_t next=(static_cast<uint8_t>(config_.get().uiStyle)+1)%4;
      config_.edit().uiStyle=static_cast<UiStyle>(next);config_.save();UiTheme::setStyle(next);showSettings();
    } else if(idx==2) showWiFi();
    else if(idx==3) showBluetooth();
    else if(idx==4) showDateTime();
    else if(idx==5) showBattery();
    else if(idx==6) {
      uint8_t next=(display_.profile()+1)%3;
      display_.setProfile(next);config_.edit().displayProfile=static_cast<DisplayProfile>(next);config_.save();showSettings();
    } else if(idx==7 || idx==8) showStorageTools();
    else showLabs();
    return;
  }

  if (page_ == Page::Files) {
    if (idx >= static_cast<int>(fileEntries_.size())) return;
    const FileEntry& entry=fileEntries_[idx];
    if(entry.name=="..") { showFiles(parentPath(currentFilePath_)); return; }
    String full=joinPath(currentFilePath_,entry.name);
    if(fileSelectionMode_) {
      selectedFilePath_=full;selectedFileName_=entry.name;selectedFileDirectory_=entry.directory;
      fileStatus_=String("Selected: ")+entry.name;showFiles(currentFilePath_);
    } else if(entry.directory) showFiles(full);
    else showFilePreview(full,entry.name,entry.size);
    return;
  }

  if (page_ == Page::Tools) {
    if(idx==0) showNetworkTools(); else if(idx==1) showWiFi(); else if(idx==2) showBluetooth();
    else if(idx==3) showBrowser(); else if(idx==4) showOtp(); else showSystem();
    return;
  }

  if (page_ == Page::NetworkTools) {
    if(idx==0) showPingTool(); else if(idx==1) showDnsTool(); else if(idx==2) showLanScan();
    else if(idx==3) showApiTester(); else if(idx==4) showMqtt(); else showWakeOnLan();
    return;
  }

  if (page_ == Page::Labs) {
    if(idx==0) showHidLab(); else if(idx==1) showBluetooth(); else if(idx==2) showNfcLab();
    else if(idx==3) showGpioLab(); else if(idx==4) { display_.cleanRefresh(); showLabs(); }
    else showSystem();
    return;
  }

  if (page_ == Page::WiFi) {
    if(idx==0) {
      wifiScan_.clear();wifiScroll_=0;wifiScanRunning_=false;wifiStatusMessage_="";showWiFi();
      return;
    }
    int n=idx-1;
    if(n<0 || n>=static_cast<int>(wifiScan_.size())) return;
    const WiFiScanEntry entry=wifiScan_[n];
    if(!entry.ssid.length()) return;
    selectedWifiSsid_=entry.ssid;
    if(entry.encrypted) showKeyboard(InputTarget::WiFiPassword,String("Password for ")+entry.ssid,"",true);
    else {
      showAppLoading("Wi-Fi",String("Connecting to ")+entry.ssid,65);
      bool ok=wifi_.connect(entry.ssid,"",true);
      wifiStatusMessage_=ok?String("Connected to ")+entry.ssid:String("Connection failed: ")+entry.ssid;
      showWiFi();
    }
    return;
  }

  if (page_ == Page::Bluetooth) {
    if (idx>=0 && idx<static_cast<int>(bleScan_.size())) showBleDetail(static_cast<size_t>(idx));
    return;
  }

  if (page_ == Page::ClassicDesktop) {
    int col=idx%3,row=idx/3;
    classicHandlePointer(104+col*154,224+row*152,true);
  }
}

void UiManager::handleScrollGesture(TouchGesture gesture) {
  const int delta = gesture == TouchGesture::SwipeUp ? 1 : -1;

  if (page_ == Page::Files) {
    fileScroll_ = max(0, fileScroll_ + delta * 5);
    showFiles(currentFilePath_);
  } else if (page_ == Page::WiFi) {
    wifiScroll_ = max(0, wifiScroll_ + delta * 4);
    renderPage(Page::WiFi);
  } else if (page_ == Page::Bluetooth) {
    bleScroll_ = max(0, bleScroll_ + delta * 4);
    renderPage(Page::Bluetooth);
  } else if (page_ == Page::Settings) {
    settingsScroll_ = max(0, settingsScroll_ + delta * 3);
    showSettings();
  } else if (page_ == Page::FileView) {
    textScroll_ = max(0, textScroll_ + delta * 900);
    showFilePreview(currentPreviewPath_, currentPreviewName_, currentPreviewSize_);
  } else if (page_ == Page::PhoneLink) {
    int maxOffset = max(0, static_cast<int>(phoneLink_.notificationCount()) - 2);
    phoneScroll_ = constrain(phoneScroll_ + delta * 2, 0, maxOffset);
    showPhoneLink();
  } else if (page_ == Page::Phone) {
    int maxOffset = max(0, static_cast<int>(phoneLink_.notificationCount()) - 3);
    phoneScroll_ = constrain(phoneScroll_ + delta * 2, 0, maxOffset);
    showPhone();
  } else if (page_ == Page::RecoveryHelp) {
    fileScroll_ = constrain(fileScroll_ + delta * 3, 0, max(0, static_cast<int>(storage_.recoveredFileCount()) - 4));
    showRecoveryHelp();
  } else if (page_ == Page::Browser && browserPage_.ok) {
    browserTextScroll_ = max(0, browserTextScroll_ + delta * 850);
    browserLinkScroll_ = max(0, browserLinkScroll_ + delta * 2);
    showBrowser();
  }
}

void UiManager::showHome(bool forceClean) {
  page_ = Page::Home;
  preparePage(forceClean);
  statusBar();

  UiTheme::title("PaperOS", 18, 76);
  UiTheme::detail(config_.get().deviceName + "  /  " + VERSION, 20, 116);
  UiTheme::pill("ALPHA", 436, 79, true);

  UiTheme::card(18, 145, 504, 128, true);
  UiTheme::label("CONNECTION", 34, 160);
  String netName = wifi_.isConnected() ? WiFi.SSID() : String("PaperOS-Setup");
  if (netName.length() > 22) netName = netName.substring(0, 22);
  UiTheme::value(netName, 34, 194, false);
  UiTheme::detail(wifi_.isConnected() ? wifi_.ip().toString() : String("192.168.4.1 / setup"), 34, 232);

  M5.Display.drawFastVLine(366, 160, 94, TFT_BLACK);
  UiTheme::label("BATTERY", 386, 160);
  UiTheme::value(String(power_.batteryPercent()) + "%", 386, 194, false);
  UiTheme::detail(String(power_.batteryMillivolts()) + " mV", 386, 232);
  UiTheme::chevron(496, 190);

  UiTheme::label("QUICK ACCESS", 20, 294);
  homeCard(18, 322, 246, 108, "NOTES", storage_.available() ? "Open notes" : "Needs SD", "Notes on microSD", true);
  homeCard(276, 322, 246, 108, "FILES", storage_.available() ? "Browse files" : "Needs SD", "/PaperOS", true);
  homeCard(18, 442, 246, 108, "SETTINGS", "Configure", "Wi-Fi / display / power", true);
  homeCard(276, 442, 246, 108, "SYSTEM", String(ESP.getFreeHeap() / 1024) + " KB free", "Diagnostics", true);

  UiTheme::label("MODULES", 20, 574);
  homeCard(18, 604, 246, 102, "TERMO", "Coming Soon", "Heating dashboard", true);
  homeCard(276, 604, 246, 102, "SOLAR", "Coming Soon", "Energy dashboard", true);

  UiTheme::card(18, 720, 504, 130);
  UiTheme::label("WEB CONSOLE", 34, 737);
  UiTheme::value("paperos.local", 34, 770, false);
  UiTheme::detail("Files / Notes / Wi-Fi / Settings / OTA", 34, 809);
  UiTheme::pill("WEB", 440, 735, true);

  bottomNav(0);
  commitPage();
}

void UiManager::showApps() {
  page_ = Page::Apps;
  preparePage();
  statusBar();

  UiTheme::title("Apps", 18, 76);
  UiTheme::detail("PaperOS tools  /  touch an app to open", 20, 116);

  const char* names[17] = {
    "Notes", "Files", "Calculator",
    "Wi-Fi", "Bluetooth", "Browser",
    "Battery", "Clock", "Focus",
    "OTP", "Network", "Settings",
    "System", "Labs", "Phone Link", "Phone", "Recovery"
  };
  const char* glyphs[17] = {
    "NT", "FL", "CAL",
    "WF", "BT", "WEB",
    "BAT", "CK", "25",
    "OTP", "NET", "ST",
    "SYS", "LAB", "PH", "TEL", "SD"
  };

  const int tileW = 120;
  const int tileH = 108;
  const int startY = 154;
  const int pitchY = 116;
  for (int i = 0; i < 17; ++i) {
    int col = i % 4;
    int row = i / 4;
    int x = 16 + col * 128;
    int y = startY + row * pitchY;
    UiTheme::appTile(x, y, tileW, tileH, glyphs[i], names[i], false);
  }

  UiTheme::shadowCard(18, 746, 504, 104, 4);
  UiTheme::label("CLASSIC DESKTOP", 34, 762);
  UiTheme::value("Windows 3.11 Mode", 34, 792, false);
  UiTheme::detail("Program Manager / Terminal / Bluetooth HID / Games", 34, 826);
  UiTheme::pill("OPEN", 438, 765, true);

  bottomNav(4);
  commitPage();
}

void UiManager::settingRow(int y, const String& titleText, const String& detailText) {
  UiTheme::card(18, y, 504, 88);
  UiTheme::value(titleText, 34, y + 17, false);
  UiTheme::detail(detailText, 34, y + 56);
  UiTheme::chevron(491, y + 34);
}


static void drawSettingsSymbol(const String& glyph, int cx, int cy) {
  const uint32_t c = TFT_WHITE;
  if (glyph == "GN") {
    M5.Display.drawCircle(cx, cy, 9, c);
    M5.Display.drawCircle(cx, cy, 3, c);
    M5.Display.drawFastHLine(cx-13, cy, 26, c);
    M5.Display.drawFastVLine(cx, cy-13, 26, c);
  } else if (glyph == "TH") {
    M5.Display.drawCircle(cx, cy, 7, c);
    M5.Display.drawCircle(cx, cy, 12, c);
    M5.Display.fillCircle(cx, cy, 2, c);
  } else if (glyph == "WF") {
    M5.Display.fillCircle(cx, cy+9, 2, c);
    M5.Display.drawLine(cx-5,cy+4,cx,cy+1,c);
    M5.Display.drawLine(cx,cy+1,cx+5,cy+4,c);
    M5.Display.drawLine(cx-10,cy-2,cx,cy-7,c);
    M5.Display.drawLine(cx,cy-7,cx+10,cy-2,c);
  } else if (glyph == "BT") {
    M5.Display.drawLine(cx,cy-13,cx,cy+13,c);
    M5.Display.drawLine(cx,cy-13,cx+8,cy-6,c);
    M5.Display.drawLine(cx+8,cy-6,cx-7,cy+7,c);
    M5.Display.drawLine(cx-7,cy-7,cx+8,cy+6,c);
    M5.Display.drawLine(cx+8,cy+6,cx,cy+13,c);
  } else if (glyph == "TM") {
    M5.Display.drawCircle(cx,cy,12,c);
    M5.Display.drawLine(cx,cy,cx,cy-7,c);
    M5.Display.drawLine(cx,cy,cx+6,cy+4,c);
  } else if (glyph == "PW") {
    M5.Display.drawRoundRect(cx-13,cy-7,24,14,3,c);
    M5.Display.fillRect(cx+11,cy-3,3,6,c);
    M5.Display.fillRect(cx-9,cy-3,13,6,c);
  } else if (glyph == "EP") {
    M5.Display.drawRoundRect(cx-13,cy-10,26,20,4,c);
    M5.Display.drawFastHLine(cx-7,cy-4,14,c);
    M5.Display.drawFastHLine(cx-7,cy+1,10,c);
    M5.Display.fillCircle(cx+8,cy+6,2,c);
  } else if (glyph == "SD") {
    M5.Display.drawRoundRect(cx-10,cy-13,20,26,3,c);
    M5.Display.drawLine(cx+3,cy-13,cx+10,cy-6,c);
    M5.Display.drawFastVLine(cx-5,cy-8,7,c);
    M5.Display.drawFastVLine(cx,cy-8,7,c);
  } else if (glyph == "BK") {
    M5.Display.drawRoundRect(cx-12,cy-9,24,19,3,c);
    M5.Display.drawLine(cx,cy-13,cx,cy+2,c);
    M5.Display.drawLine(cx,cy+2,cx-5,cy-3,c);
    M5.Display.drawLine(cx,cy+2,cx+5,cy-3,c);
  } else if (glyph == "LB") {
    M5.Display.drawLine(cx-5,cy-13,cx-5,cy-3,c);
    M5.Display.drawLine(cx+5,cy-13,cx+5,cy-3,c);
    M5.Display.drawLine(cx-5,cy-3,cx-11,cy+11,c);
    M5.Display.drawLine(cx+5,cy-3,cx+11,cy+11,c);
    M5.Display.drawFastHLine(cx-11,cy+11,22,c);
    M5.Display.drawFastHLine(cx-7,cy+4,14,c);
  } else {
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(TFT_WHITE,TFT_BLACK);
    M5.Display.drawString(glyph,cx,cy);
  }
}

void UiManager::settingsRow(int y, const String& glyph, const String& titleText, const String& detailText, bool withChevron) {
  M5.Display.fillRect(22, y, 496, 78, TFT_WHITE);
  const int r = UiTheme::style() == 2 ? 8 : 15;
  M5.Display.fillRoundRect(34, y + 15, 46, 46, r, TFT_BLACK);
  drawSettingsSymbol(glyph, 57, y + 38);
  UiTheme::resetFont();

  UiTheme::value(titleText, 96, y + 11, false);
  if (detailText.length()) UiTheme::detail(detailText, 96, y + 47);
  M5.Display.drawFastHLine(96, y + 77, 404, TFT_BLACK);
  if (withChevron) UiTheme::chevron(490, y + 29);
}

void UiManager::showSettings() {
  page_ = Page::Settings;
  preparePage();
  statusBar();

  UiTheme::title("Impostazioni", 18, 76);
  UiTheme::detail("PaperOS / swipe up-down", 20, 116);

  UiTheme::shadowCard(18, 145, 504, 96, 4);
  M5.Display.fillRoundRect(38, 166, 54, 54, 18, TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold18pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString("P", 65, 193);
  UiTheme::resetFont();
  UiTheme::value(config_.get().deviceName, 112, 164, false);
  UiTheme::detail(String("PaperOS ") + VERSION + "  /  " + UiTheme::styleName(), 112, 202);

  const char* glyphs[10] = {"GN","TH","WF","BT","TM","PW","EP","SD","BK","LB"};
  const char* titles[10] = {"Generali","Tema","Wi-Fi","Bluetooth","Data e ora","Batteria e Lock","Display / EPD","Storage / SD","Backup & Restore","Labs"};
  String details[10] = {
    "Info dispositivo, firmware e memoria",
    String(UiTheme::styleName()) + " / tap to change",
    wifi_.isConnected() ? WiFi.SSID() : (wifi_.radioEnabled() ? "Non connesso" : "Radio OFF"),
    phoneLink_.ancsReady() ? "iPhone ANCS connected" : (bluetoothActive_ ? "BLE attivo" : "BLE disattivato"),
    wifi_.timeSynced() ? "NTP sincronizzato" : "RTC / sincronizzazione manuale",
    String(power_.batteryPercent()) + "% / " + String(power_.batteryMillivolts()) + " mV",
    String("Qualita ") + display_.profileLabel(),
    storage_.available() ? String("microSD / ") + String((uint32_t)(storage_.freeBytes()/1048576ULL)) + " MB liberi" : "microSD non disponibile",
    "Configurazione e reti Wi-Fi su SD",
    "NFC / GPIO / BLE / strumenti beta"
  };

  const int visible = 6;
  settingsScroll_ = constrain(settingsScroll_, 0, 10 - visible);
  UiTheme::shadowCard(18, 257, 504, 510, 4);
  for (int row = 0; row < visible; ++row) {
    int idx = settingsScroll_ + row;
    settingsRow(260 + row * 82, glyphs[idx], titles[idx], details[idx]);
  }

  UiTheme::card(18, 782, 504, 68);
  UiTheme::detail(String(settingsScroll_ + 1) + "-" + String(settingsScroll_ + visible) + " / 10  -  swipe", 34, 806);

  bottomNav(4);
  commitPage();
}




void UiManager::showDateTime() {
  page_ = Page::DateTime;
  preparePage();
  statusBar();

  auto dt = M5.Rtc.getDateTime();
  char current[32];
  snprintf(current, sizeof(current), "%04d-%02d-%02d  %02d:%02d:%02d",
           dt.date.year, dt.date.month, dt.date.date,
           dt.time.hours, dt.time.minutes, dt.time.seconds);

  UiTheme::title("Data e ora", 18, 76);
  UiTheme::detail("RTC + NTP synchronization", 20, 116);

  UiTheme::card(18, 145, 504, 150, true);
  UiTheme::label("CURRENT", 34, 163);
  UiTheme::value(current, 34, 205, false);
  UiTheme::detail(String("Timezone: ") + config_.get().timezone, 34, 252);

  UiTheme::card(18, 316, 246, 112, true);
  UiTheme::value("SYNC NTP", 70, 354, false);
  UiTheme::detail(wifi_.isConnected() ? "Use network time" : "Wi-Fi required", 58, 394);

  UiTheme::card(276, 316, 246, 112);
  UiTheme::value("SET MANUAL", 326, 354, false);
  UiTheme::detail("YYYY-MM-DD HH:MM", 323, 394);

  UiTheme::card(18, 450, 504, 168);
  UiTheme::label("STATUS", 34, 468);
  UiTheme::value(timeStatus_.length() ? timeStatus_ : (wifi_.timeSynced() ? "NTP synchronized" : "RTC active"), 34, 505, false);
  UiTheme::detail("A successful NTP sync also updates the hardware RTC.", 34, 553);
  UiTheme::detail("TOTP can continue offline while RTC time remains correct.", 34, 586);

  UiTheme::card(18, 640, 504, 118);
  UiTheme::label("QUICK ACCESS", 34, 658);
  UiTheme::detail("You can also trigger NTP sync from the top Quick Settings drawer.", 34, 697);
  UiTheme::detail("Swipe down from the status bar to open it.", 34, 729);

  bottomNav(4);
  commitPage();
}

void UiManager::showStorageTools() {
  page_ = Page::StorageTools;
  preparePage();
  statusBar();

  UiTheme::title("Storage & Backup", 18, 76);
  UiTheme::detail(storage_.available() ? "microSD mounted" : "microSD unavailable", 20, 116);

  UiTheme::card(18, 145, 504, 100, true);
  UiTheme::label("SD STATUS", 34, 161);
  UiTheme::value(storage_.available() ? String((uint32_t)(storage_.freeBytes()/1048576ULL)) + " MB free" : String("No card"), 34, 198, false);
  UiTheme::detail(storage_.available() ? String((uint32_t)(storage_.totalBytes()/1048576ULL)) + " MB total" : String("Insert microSD"), 34, 230);

  settingsRow(266, "BK", "Backup settings", "/PaperOS/Backup/settings.json");
  settingsRow(348, "RS", "Restore settings", "Load settings.json from SD");
  settingsRow(430, "WF", "Export Wi-Fi text", "/PaperOS/Config/wifi_networks.txt");
  settingsRow(512, "IM", "Import Wi-Fi text", "Read edited SSID/Password blocks");
  settingsRow(594, "FM", "Format / partition", "Single-volume SD tools");
  settingsRow(676, "RC", "Recover deleted files", "Read this before using the card again");

  UiTheme::card(18, 770, 504, 80);
  UiTheme::detail(storageStatus_.length() ? storageStatus_ : String("Wi-Fi text stores passwords in plain text by explicit choice."), 34, 797);
  UiTheme::detail("Backup/restore and format actions require the microSD.", 34, 826);

  bottomNav(4);
  commitPage();
}

void UiManager::showGeneral() {
  page_ = Page::General;
  preparePage();
  statusBar();

  UiTheme::title("Generali", 18, 78);
  UiTheme::detail("Info dispositivo", 20, 119);

  settingsRow(150, "IN", "Info", String("PaperOS ") + VERSION, false);
  settingsRow(240, "HW", "Modello", "M5Stack M5Paper original", false);
  settingsRow(330, "CPU", "Processore", "ESP32-D0WDQ6-V3 / 240 MHz", false);
  settingsRow(420, "RM", "Memoria", String(ESP.getPsramSize() / 1048576.0, 0) + " MB PSRAM", false);
  settingsRow(510, "FL", "Flash", "16 MB", false);
  settingsRow(600, "DP", "Display", "540 x 960 e-paper / 4.7 inch", false);
  settingsRow(690, "WL", "Software", "Created by WinLabs Solutions", false);

  UiTheme::card(18, 790, 504, 60);
  UiTheme::detail(String("Build: ") + VERSION + "  /  " + VENDOR, 34, 811);

  bottomNav(4);
  commitPage();
}

void UiManager::showWiFi() {
  page_ = Page::WiFi;
  preparePage();
  statusBar();

  UiTheme::title("Wi-Fi Analyzer", 18, 76);
  UiTheme::detail(wifi_.radioEnabled() ? (wifi_.isConnected() ? WiFi.SSID() : String("Not connected")) : String("Wi-Fi radio OFF"), 20, 116);

  UiTheme::card(18, 145, 504, 118, true);
  UiTheme::label("CURRENT NETWORK", 34, 161);
  String current = wifi_.isConnected() ? WiFi.SSID() : (wifi_.radioEnabled() ? String("PaperOS-Setup") : String("Wi-Fi disabled"));
  if (current.length() > 23) current = current.substring(0, 23);
  UiTheme::value(current, 34, 197, false);
  String networkDetail = wifi_.isConnected()
    ? wifi_.ip().toString() + "  /  CH " + String(WiFi.channel())
    : String("Tap here to rescan / toggle from Quick Settings");
  UiTheme::detail(networkDetail, 34, 235);
  UiTheme::pill(wifi_.isConnected() ? String(WiFi.RSSI()) + " dBm" : (wifi_.radioEnabled() ? "OFFLINE" : "RADIO OFF"), 382, 160, false);

  UiTheme::label("NEARBY NETWORKS", 20, 286);
  UiTheme::card(18, 310, 504, 466);

  if (!wifi_.radioEnabled()) {
    UiTheme::value("Wi-Fi radio is OFF", 44, 354, false);
    UiTheme::detail("Swipe down from the top to enable Wi-Fi.", 44, 398);
  } else if (wifiScan_.empty()) {
    UiTheme::value(wifiScanRunning_ ? "Scanning..." : "No networks cached", 44, 354, false);
    UiTheme::detail("SSID / signal / channel / security", 44, 398);
    if (wifiScanRunning_) {
      M5.Display.drawRoundRect(44, 448, 438, 18, 9, TFT_BLACK);
      M5.Display.fillRoundRect(47, 451, 250, 12, 6, TFT_BLACK);
    }
  } else {
    const int visible = 5;
    int maxOffset = max(0, static_cast<int>(wifiScan_.size()) - visible);
    wifiScroll_ = constrain(wifiScroll_, 0, maxOffset);
    int shown = min(visible, static_cast<int>(wifiScan_.size()) - wifiScroll_);

    for (int row = 0; row < shown; ++row) {
      const WiFiScanEntry& entry = wifiScan_[wifiScroll_ + row];
      String title = entry.ssid.length() ? entry.ssid : String("<hidden>");
      if (title.length() > 24) title = title.substring(0, 24);
      String detail = String(entry.rssi) + " dBm  /  CH " + String(entry.channel) + "  /  " +
                      (entry.encrypted ? "SECURE" : "OPEN");
      settingsRow(311 + row * 88, "WF", title, detail, true);
    }

    UiTheme::detail(String(wifiScroll_ + 1) + "-" + String(wifiScroll_ + shown) + " / " + String(wifiScan_.size()) +
                    "  -  swipe up/down", 44, 754);
  }

  UiTheme::card(18, 792, 504, 58);
  UiTheme::detail(wifiStatusMessage_.length() ? wifiStatusMessage_ : String("Saved networks reconnect automatically."), 34, 811);
  UiTheme::pill("AUDIT", 438, 796, true);

  bottomNav(4);
  commitPage();

  if (wifi_.radioEnabled() && wifiScan_.empty() && !wifiScanRunning_ && wifiStatusMessage_ != "No networks found") {
    WiFi.scanDelete();
    WiFi.scanNetworks(true, true);
    wifiScanRunning_ = true;
  }
}


void UiManager::showWifiAudit() {
  page_ = Page::WifiAudit;
  preparePage();
  statusBar();

  UiTheme::title("Wi-Fi Audit", 18, 76);
  UiTheme::detail("Authorized 2.4 GHz configuration audit", 20, 116);

  int openCount = 0, legacyCount = 0, modernCount = 0, hiddenCount = 0;
  int channelCounts[14] = {0};
  for (const auto& n : wifiScan_) {
    if (!n.ssid.length()) hiddenCount++;
    if (n.authMode == 0) openCount++;
    else if (n.authMode == 1 || n.authMode == 2 || n.authMode == 4) legacyCount++;
    else modernCount++;
    if (n.channel >= 1 && n.channel <= 13) channelCounts[n.channel]++;
  }

  int busiestChannel = 0, busiestCount = 0;
  for (int ch = 1; ch <= 13; ++ch) {
    if (channelCounts[ch] > busiestCount) {
      busiestCount = channelCounts[ch];
      busiestChannel = ch;
    }
  }

  homeCard(18, 145, 160, 112, "OPEN", String(openCount), "No encryption", false);
  homeCard(190, 145, 160, 112, "LEGACY", String(legacyCount), "WEP/WPA/mixed", false);
  homeCard(362, 145, 160, 112, "MODERN", String(modernCount), "WPA2/3/enterprise", false);

  UiTheme::card(18, 276, 504, 126);
  UiTheme::label("CHANNEL CONGESTION", 34, 293);
  UiTheme::value(busiestChannel ? String("Busiest CH ") + busiestChannel + " / " + busiestCount + " APs" : String("No channel data"), 34, 330, false);
  UiTheme::detail(String("Hidden SSIDs: ") + hiddenCount + " / total " + wifiScan_.size(), 34, 370);

  UiTheme::label("STRONGEST NETWORKS", 20, 424);
  std::vector<WiFiScanEntry> sorted = wifiScan_;
  std::sort(sorted.begin(), sorted.end(), [](const WiFiScanEntry& a, const WiFiScanEntry& b){ return a.rssi > b.rssi; });
  int shown = min(4, static_cast<int>(sorted.size()));
  for (int i = 0; i < shown; ++i) {
    int y = 450 + i * 76;
    const auto& n = sorted[i];
    String auth;
    switch (n.authMode) {
      case 0: auth = "OPEN"; break;
      case 1: auth = "WEP"; break;
      case 2: auth = "WPA"; break;
      case 3: auth = "WPA2"; break;
      case 4: auth = "WPA/WPA2"; break;
      case 5: auth = "WPA2 ENT"; break;
      case 6: auth = "WPA3"; break;
      case 7: auth = "WPA2/3"; break;
      default: auth = String("AUTH ") + n.authMode; break;
    }
    String title = n.ssid.length() ? n.ssid : String("<hidden>");
    if (title.length() > 25) title = title.substring(0, 25);
    settingsRow(y, "WF", title, String(n.rssi) + " dBm / CH " + n.channel + " / " + auth, false);
  }

  UiTheme::card(18, 770, 504, 80);
  UiTheme::detail("Diagnostic only: no deauth, cracking or credential capture.", 34, 793);
  UiTheme::detail("Run a fresh scan from Wi-Fi Analyzer for current results.", 34, 824);

  bottomNav(3);
  commitPage();
}

void UiManager::showBluetooth() {
  page_ = Page::Bluetooth;
  preparePage();
  statusBar();

  UiTheme::title("BLE Inspector", 18, 76);
  UiTheme::detail("Bluetooth LE scanner / beacon / GATT", 20, 116);

  if (!bluetoothActive_) {
    BLEDevice::init("PaperOS");
    bluetoothActive_ = true;
  }

  UiTheme::card(18, 145, 504, 112, true);
  UiTheme::label("SCANNER", 34, 161);
  UiTheme::value(bleScan_.empty() ? "Ready" : String(bleScan_.size()) + " devices", 34, 197, false);
  UiTheme::detail("Tap SCAN, then swipe the results and open a device.", 34, 232);
  UiTheme::pill("SCAN", 430, 160, true);

  UiTheme::label("NEARBY BLE DEVICES", 20, 282);
  UiTheme::card(18, 306, 504, 470);

  if (bleScan_.empty()) {
    UiTheme::value("No scan yet", 44, 350, false);
    UiTheme::detail("Name / manufacturer / RSSI / beacon type.", 44, 392);
  } else {
    const int visible = 5;
    int maxOffset = max(0, static_cast<int>(bleScan_.size()) - visible);
    bleScroll_ = constrain(bleScroll_, 0, maxOffset);
    int shown = min(visible, static_cast<int>(bleScan_.size()) - bleScroll_);

    for (int row = 0; row < shown; ++row) {
      const BleScanEntry& entry = bleScan_[bleScroll_ + row];
      String title = entry.name.length() ? entry.name : entry.address;
      if (title.length() > 24) title = title.substring(0, 24);
      String detail = entry.manufacturerName.length() ? entry.manufacturerName : String(entry.rssi) + " dBm";
      if (entry.manufacturerName.length()) detail += " / " + String(entry.rssi) + " dBm";
      if (entry.beaconType.length()) detail += " / " + entry.beaconType;
      if (detail.length() > 48) detail = detail.substring(0, 48);
      settingsRow(307 + row * 88, "BT", title, detail, true);
    }

    UiTheme::detail(String(bleScroll_ + 1) + "-" + String(bleScroll_ + shown) + " / " + String(bleScan_.size()) +
                    "  -  swipe up/down", 44, 754);
  }

  UiTheme::card(18, 792, 504, 58);
  UiTheme::detail("Your AirTag: iPhone Find My > Items > Play Sound.", 34, 811);
  UiTheme::detail("PaperOS BLE cannot read owner details or sound it.", 34, 835);

  bottomNav(4);
  commitPage();
}



String UiManager::bleManufacturerName(const String& bytes) const {
  if (bytes.length() < 2) return "";
  uint16_t id = static_cast<uint8_t>(bytes[0]) |
                (static_cast<uint16_t>(static_cast<uint8_t>(bytes[1])) << 8);
  switch (id) {
    case 0x0002: return "Intel";
    case 0x0006: return "Microsoft";
    case 0x000D: return "Texas Instruments";
    case 0x000F: return "Broadcom";
    case 0x003A: return "Panasonic";
    case 0x0046: return "MediaTek";
    case 0x004C: return "Apple";
    case 0x0057: return "Harman";
    case 0x0059: return "Nordic Semiconductor";
    case 0x005D: return "Realtek";
    case 0x0065: return "HP";
    case 0x0067: return "GN Audio";
    case 0x0068: return "General Motors";
    case 0x006B: return "Polar";
    case 0x0075: return "Samsung";
    case 0x0078: return "Nike";
    case 0x00E0: return "Google";
    default: {
      char buf[24];
      snprintf(buf, sizeof(buf), "Company ID 0x%04X", id);
      return String(buf);
    }
  }
}

String UiManager::bleManufacturerHex(const String& bytes) const {
  return bytesHex(bytes, 18);
}


String UiManager::bytesHex(const String& bytes, size_t maxBytes) const {
  static const char* hex = "0123456789ABCDEF";
  String out;
  size_t limit = bytes.length() > maxBytes ? maxBytes : bytes.length();
  out.reserve(limit * 3 + 4);
  for (size_t i = 0; i < limit; ++i) {
    uint8_t b = static_cast<uint8_t>(bytes[i]);
    if (i) out += ' ';
    out += hex[(b >> 4) & 0x0F];
    out += hex[b & 0x0F];
  }
  if (bytes.length() > limit) out += " ...";
  return out;
}

void UiManager::readBleGatt(size_t index) {
  gattRows_.clear();
  if (index >= bleScan_.size()) return;

  const BleScanEntry target = bleScan_[index];
  showAppLoading("BLE GATT", "Connecting to " + target.address, 45);

  BLEClient* client = BLEDevice::createClient();
  if (!client) {
    gattRows_.push_back("Unable to create BLE client");
    return;
  }

  bool connected = client->connect(BLEAddress(target.address.c_str()));
  if (!connected) {
    gattRows_.push_back("Connection failed");
    delete client;
    return;
  }

  std::map<std::string, BLERemoteService*>* services = client->getServices();
  if (!services) {
    gattRows_.push_back("No GATT services discovered");
  } else {
    for (auto& svcPair : *services) {
      BLERemoteService* svc = svcPair.second;
      if (!svc) continue;
      std::map<std::string, BLERemoteCharacteristic*>* chars = svc->getCharacteristics();
      if (!chars) continue;

      for (auto& chPair : *chars) {
        BLERemoteCharacteristic* ch = chPair.second;
        if (!ch || !ch->canRead()) continue;

        std::string raw = ch->readValue();
        String rawArduino;
        rawArduino.reserve(raw.length());
        for (size_t i = 0; i < raw.length(); ++i) rawArduino += static_cast<char>(raw[i]);
        String row = String(ch->getUUID().toString().c_str()) + " = " + bytesHex(rawArduino, 12);
        gattRows_.push_back(row);
        if (gattRows_.size() >= 6) break;
      }
      if (gattRows_.size() >= 6) break;
    }
  }

  client->disconnect();
  delete client;
  if (gattRows_.empty()) gattRows_.push_back("No readable characteristics");
}

void UiManager::showBleDetail(size_t index) {
  if (index >= bleScan_.size()) return;
  selectedBleIndex_ = index;
  page_ = Page::BleDetail;
  preparePage();
  statusBar();

  const BleScanEntry& d = bleScan_[index];
  UiTheme::title("BLE Device", 18, 76);
  UiTheme::detail(d.name.length() ? d.name : d.address, 20, 116);

  settingRow(150, "Address", d.address);
  settingRow(232, "Signal", String(d.rssi) + " dBm");
  settingRow(314, "Beacon", d.beaconType.length() ? d.beaconType : String("Generic BLE advertising"));
  settingRow(396, "Service UUID", d.serviceUuid.length() ? d.serviceUuid : String("Not advertised"));
  settingRow(478, "Manufacturer", d.manufacturerHex.length() ? d.manufacturerHex : String("No manufacturer data"));

  UiTheme::card(18, 574, 504, 92, true);
  UiTheme::value("TEST + READ GATT", 145, 606, false);
  UiTheme::detail("Connection test + services + readable characteristics", 76, 642);

  UiTheme::card(18, 684, 504, 158);
  UiTheme::label("GATT RESULT", 34, 702);
  if (gattRows_.empty()) {
    UiTheme::detail("Tap TEST + READ GATT for connectable BLE sensors/devices.", 34, 742);
    UiTheme::detail("Beacon-only devices may reject connections.", 34, 776);
  } else {
    size_t shown = gattRows_.size() > 3 ? 3 : gattRows_.size();
    for (size_t i = 0; i < shown; ++i) {
      String row = gattRows_[i];
      if (row.length() > 64) row = row.substring(0, 61) + "...";
      UiTheme::detail(row, 34, 734 + static_cast<int>(i) * 30);
    }
  }

  bottomNav(4);
  commitPage();
}


void UiManager::drawBatteryGraph() {
  UiTheme::card(18, 486, 504, 170);
  UiTheme::label("VOLTAGE HISTORY", 34, 503);

  const int gx = 42, gy = 542, gw = 454, gh = 84;
  M5.Display.drawRect(gx, gy, gw, gh, TFT_BLACK);

  const uint8_t count = power_.voltageHistoryCount();
  if (count < 2) {
    UiTheme::detail("Collecting real voltage samples...", 96, 575);
    return;
  }

  int minMv = 9999, maxMv = 0;
  for (uint8_t i = 0; i < count; ++i) {
    int mv = power_.voltageHistoryMv(i);
    if (mv <= 0) continue;
    minMv = min(minMv, mv);
    maxMv = max(maxMv, mv);
  }
  if (minMv > maxMv) return;
  if (maxMv - minMv < 40) {
    minMv -= 20;
    maxMv += 20;
  }

  int px = gx;
  int py = gy + gh / 2;
  for (uint8_t i = 0; i < count; ++i) {
    int mv = power_.voltageHistoryMv(i);
    int x = gx + (count <= 1 ? 0 : (static_cast<int>(i) * (gw - 1) / (count - 1)));
    int y = gy + gh - 1 - ((mv - minMv) * (gh - 2) / max(1, maxMv - minMv));
    if (i) M5.Display.drawLine(px, py, x, y, TFT_BLACK);
    M5.Display.fillCircle(x, y, 1, TFT_BLACK);
    px = x; py = y;
  }

  UiTheme::detail(String(minMv) + "-" + String(maxMv) + " mV / " + String(count) + " samples", 34, 630);
}

void UiManager::drawBatteryImpactEstimate() {
  UiTheme::card(18, 670, 504, 132);
  UiTheme::label("ESTIMATED IMPACT", 34, 687);

  struct Impact { String name; int score; };
  Impact items[4] = {
    {"Wi-Fi", wifi_.radioEnabled() ? (wifi_.isConnected() ? 3 : 2) : 0},
    {"Bluetooth", bluetoothActive_ ? 2 : 0},
    {"E-paper", display_.profile() == 2 ? 3 : (display_.profile() == 1 ? 2 : 1)},
    {"CPU / active UI", 2}
  };

  for (int i = 0; i < 4; ++i) {
    for (int j = i + 1; j < 4; ++j) {
      if (items[j].score > items[i].score) {
        Impact t = items[i]; items[i] = items[j]; items[j] = t;
      }
    }
  }

  String line;
  for (int i = 0; i < 4; ++i) {
    if (i) line += "  >  ";
    line += items[i].name;
  }
  UiTheme::value(line, 34, 722, false);
  UiTheme::detail("Relative software estimate only - M5Paper V1 has no current sensor.", 34, 770);
}

void UiManager::showBattery() {
  page_ = Page::Battery;
  preparePage();
  statusBar();

  UiTheme::title("Batteria", 18, 76);
  UiTheme::detail("Power Center / real voltage + estimated subsystem impact", 20, 116);

  drawBatteryLiveArea();
  drawBatteryGraph();
  drawBatteryImpactEstimate();

  UiTheme::card(18, 816, 504, 34);
  UiTheme::detail("Swipe down for Quick Settings / battery current mA unavailable on V1", 34, 824);

  bottomNav(3);
  commitPage();
  lastBatteryUiRefresh_ = millis();
}


void UiManager::drawClockLiveArea() {
  auto dt = M5.Rtc.getDateTime();
  M5.Display.fillRect(18, 145, 504, 410, TFT_WHITE);
  UiTheme::card(18, 145, 504, 410, true);

  char timeText[8];
  char dateText[32];
  if (dt.date.year >= 2020 && dt.date.year <= 2099) {
    snprintf(timeText, sizeof(timeText), "%02d:%02d", dt.time.hours, dt.time.minutes);
    snprintf(dateText, sizeof(dateText), "%02d/%02d/%04d", dt.date.date, dt.date.month, dt.date.year);
  } else {
    snprintf(timeText, sizeof(timeText), "--:--");
    snprintf(dateText, sizeof(dateText), "RTC not set");
  }

  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(timeText, 270, 285);
  UiTheme::resetFont();

  M5.Display.setFont(&fonts::FreeSansBold18pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(dateText, 270, 365);
  UiTheme::resetFont();

  UiTheme::detail(String("Battery ") + power_.batteryPercent() + "%  /  " + power_.batteryMillivolts() + " mV", 168, 440);
  UiTheme::pill(wifi_.isConnected() ? "ONLINE" : "OFFLINE", 205, 480, wifi_.isConnected());
}

void UiManager::showClock() {
  page_ = Page::Clock;
  preparePage();
  statusBar();
  UiTheme::title("Clock", 18, 76);
  UiTheme::detail("Desk clock  /  minute refresh", 20, 116);
  drawClockLiveArea();

  UiTheme::card(18, 575, 504, 160);
  UiTheme::label("E-PAPER MODE", 34, 593);
  UiTheme::value("Quiet clock", 34, 630, false);
  UiTheme::detail("Only the clock panel refreshes once per minute.", 34, 675);
  UiTheme::detail("Ideal as a desk display without constant screen activity.", 34, 708);

  bottomNav(4);
  commitPage();
  lastClockPageRefresh_ = millis();
}

void UiManager::drawFocusLiveArea() {
  uint32_t remaining = focusRemainingSec_;
  if (focusRunning_) {
    int32_t diff = static_cast<int32_t>(focusEndMs_ - millis());
    remaining = diff > 0 ? static_cast<uint32_t>(diff) / 1000UL : 0;
  }

  const uint32_t mins = remaining / 60UL;
  const uint32_t secs = remaining % 60UL;
  char timerText[12];
  snprintf(timerText, sizeof(timerText), "%02lu:%02lu",
           static_cast<unsigned long>(mins),
           static_cast<unsigned long>(secs));

  M5.Display.fillRect(18, 145, 504, 350, TFT_WHITE);
  UiTheme::card(18, 145, 504, 350, true);
  UiTheme::label("FOCUS SESSION", 34, 164);
  UiTheme::pill(focusRunning_ ? "RUNNING" : (remaining == 0 ? "DONE" : "READY"), 382, 158, focusRunning_);

  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(timerText, 270, 292);
  UiTheme::resetFont();

  UiTheme::detail("25 minute focus timer designed for low-refresh e-paper.", 73, 385);
  UiTheme::detail(focusRunning_ ? "PaperOS updates this timer roughly once per minute." : "Tap START to begin.", 79, 425);
}

void UiManager::showFocus() {
  page_ = Page::Focus;
  preparePage();
  statusBar();
  UiTheme::title("Focus", 18, 76);
  UiTheme::detail("25 minute concentration timer", 20, 116);

  drawFocusLiveArea();

  UiTheme::card(18, 515, 246, 112, true);
  UiTheme::value(focusRunning_ ? "PAUSE" : "START", 79, 553, false);
  UiTheme::detail("Tap", 121, 592);

  UiTheme::card(276, 515, 246, 112);
  UiTheme::value("RESET", 345, 553, false);
  UiTheme::detail("25:00", 379, 592);

  UiTheme::card(18, 646, 504, 118);
  UiTheme::label("TIP", 34, 664);
  UiTheme::detail("Use the full 25 minutes on one task, then take a short break.", 34, 703);
  UiTheme::detail("The timer keeps running while you open other PaperOS apps.", 34, 735);

  bottomNav(4);
  commitPage();
  lastFocusUiRefresh_ = millis();
}

void UiManager::drawFunResult() {
  M5.Display.fillRect(18, 145, 504, 230, TFT_WHITE);
  UiTheme::card(18, 145, 504, 230, true);
  UiTheme::label("RESULT", 34, 164);

  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(funResult_, 270, 270);
  UiTheme::resetFont();
}

void UiManager::showFun() {
  page_ = Page::Fun;
  preparePage();
  statusBar();
  UiTheme::title("Fun", 18, 76);
  UiTheme::detail("Tiny random tools", 20, 116);

  drawFunResult();

  UiTheme::card(18, 400, 160, 118, true);
  UiTheme::value("D6", 74, 438, false);
  UiTheme::detail("Roll dice", 54, 480);

  UiTheme::card(190, 400, 160, 118, true);
  UiTheme::value("COIN", 232, 438, false);
  UiTheme::detail("Heads / tails", 214, 480);

  UiTheme::card(362, 400, 160, 118, true);
  UiTheme::value("1-100", 397, 438, false);
  UiTheme::detail("Random", 408, 480);

  UiTheme::card(18, 545, 504, 150);
  UiTheme::label("QUICK DECIDER", 34, 564);
  UiTheme::detail("Useful when nobody wants to choose.", 34, 606);
  UiTheme::detail("Uses the ESP32 hardware random source.", 34, 641);
  UiTheme::detail("Only the result card refreshes after a tap.", 34, 675);

  bottomNav(4);
  commitPage();
}


void UiManager::showKeyboard(InputTarget target, const String& prompt, const String& initial, bool masked) {
  inputTarget_ = target;
  inputPrompt_ = prompt;
  inputValue_ = initial;
  inputMasked_ = masked;
  keyboardShowSecret_ = false;
  keyboardShift_ = false;
  keyboardSymbols_ = false;
  page_ = Page::Keyboard;
  preparePage();
  drawKeyboard();
  commitPage();
}

void UiManager::drawKeyboard() {
  statusBar();
  UiTheme::title("Keyboard", 18, 76);
  UiTheme::detail(inputPrompt_, 20, 116);

  UiTheme::card(18, 145, 504, 95, true);
  String shown = inputValue_;
  if (inputMasked_ && !keyboardShowSecret_) {
    shown = "";
    int n = inputValue_.length() < 26 ? inputValue_.length() : 26;
    for (int i = 0; i < n; ++i) shown += '*';
  } else if (shown.length() > 38) {
    shown = "..." + shown.substring(shown.length() - 35);
  }
  UiTheme::value(shown.length() ? shown : String("_"), 34, 178, false);
  UiTheme::detail(String(inputValue_.length()) + " chars", 34, 213);
  if (inputMasked_) UiTheme::pill(keyboardShowSecret_ ? "HIDE" : "SHOW", 442, 161, true);

  const int x0 = 15, keyW = 47, gap = 4, keyH = 58;
  const int ys[4] = {260, 330, 400, 470};
  String rows[4];
  rows[0] = "1234567890";
  if (!keyboardSymbols_) {
    rows[1] = "qwertyuiop";
    rows[2] = "asdfghjkl-";
    rows[3] = "zxcvbnm./_";
  } else {
    rows[1] = "!@#$%^&*()";
    rows[2] = "[]{}<>+=?;";
    rows[3] = "_-./:;,'\"?";
  }

  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 10; ++col) {
      int x = x0 + col * (keyW + gap);
      UiTheme::card(x, ys[row], keyW, keyH, false);
      String key = String(rows[row][col]);
      if (!keyboardSymbols_ && keyboardShift_ && row > 0 && key[0] >= 'a' && key[0] <= 'z') {
        key[0] = static_cast<char>(key[0] - 'a' + 'A');
      }
      M5.Display.setFont(&fonts::FreeSansBold9pt7b);
      M5.Display.setTextDatum(middle_center);
      M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
      M5.Display.drawString(key, x + keyW / 2, ys[row] + keyH / 2);
      UiTheme::resetFont();
    }
  }

  const int ay = 550, ah = 70;
  UiTheme::card(18, ay, 90, ah, keyboardShift_);
  UiTheme::value("SHIFT", 34, ay + 22, false);
  UiTheme::card(116, ay, 88, ah, keyboardSymbols_);
  UiTheme::value("SYM", 137, ay + 22, false);
  UiTheme::card(212, ay, 124, ah);
  UiTheme::value("SPACE", 235, ay + 22, false);
  UiTheme::card(344, ay, 80, ah);
  UiTheme::value("BS", 366, ay + 22, false);
  UiTheme::card(432, ay, 90, ah, true);
  UiTheme::value("OK", 455, ay + 22, false);

  UiTheme::card(18, 642, 504, 72);
  UiTheme::value("CANCEL", 210, 664, false);

  UiTheme::card(18, 730, 504, 112);
  UiTheme::label("INPUT", 34, 747);
  UiTheme::detail("Keys flash dark when accepted. SHIFT changes letter case.", 34, 782);
  UiTheme::detail(inputMasked_ ? "SHOW/HIDE reveals the current password temporarily." : "SYM opens common URL and symbol characters.", 34, 816);
}

void UiManager::handleKeyboardTap(int x, int y) {
  if (inputMasked_ && y >= 145 && y < 240 && x >= 405) {
    keyboardShowSecret_ = !keyboardShowSecret_;
    M5.Display.fillRect(18, 145, 504, 95, TFT_WHITE);
    UiTheme::card(18, 145, 504, 95, true);
    String shown = inputValue_;
    if (!keyboardShowSecret_) {
      shown = "";
      int n = inputValue_.length() < 26 ? inputValue_.length() : 26;
      for (int i = 0; i < n; ++i) shown += '*';
    } else if (shown.length() > 38) {
      shown = "..." + shown.substring(shown.length() - 35);
    }
    UiTheme::value(shown.length() ? shown : String("_"), 34, 178, false);
    UiTheme::detail(String(inputValue_.length()) + " chars", 34, 213);
    UiTheme::pill(keyboardShowSecret_ ? "HIDE" : "SHOW", 442, 161, true);
    display_.partialRefresh(18, 145, 504, 95);
    return;
  }

  const int x0 = 15, keyW = 47, gap = 4, keyH = 58;
  const int ys[4] = {260, 330, 400, 470};
  String rows[4];
  rows[0] = "1234567890";
  if (!keyboardSymbols_) {
    rows[1] = "qwertyuiop";
    rows[2] = "asdfghjkl-";
    rows[3] = "zxcvbnm./_";
  } else {
    rows[1] = "!@#$%^&*()";
    rows[2] = "[]{}<>+=?;";
    rows[3] = "_-./:;,'\"?";
  }

  bool changed = false;
  for (int row = 0; row < 4 && !changed; ++row) {
    if (y < ys[row] || y >= ys[row] + keyH) continue;
    int col = (x - x0) / (keyW + gap);
    int lx = (x - x0) % (keyW + gap);
    if (col >= 0 && col < 10 && lx >= 0 && lx < keyW) {
      char c = rows[row][col];
      if (!keyboardSymbols_ && keyboardShift_ && row > 0 && c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');

      const int keyX = x0 + col * (keyW + gap);
      M5.Display.fillRoundRect(keyX, ys[row], keyW, keyH, 10, TFT_BLACK);
      M5.Display.setFont(&fonts::FreeSansBold9pt7b);
      M5.Display.setTextDatum(middle_center);
      M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
      M5.Display.drawString(String(c), keyX + keyW / 2, ys[row] + keyH / 2);
      UiTheme::resetFont();
      display_.partialRefresh(keyX, ys[row], keyW, keyH);

      if (inputValue_.length() < 160) inputValue_ += c;
      changed = true;
    }
  }

  if (y >= 550 && y < 620) {
    if (x >= 18 && x < 108) {
      keyboardShift_ = !keyboardShift_;
      UiTheme::beginFrame();
      drawKeyboard();
      display_.pageRefresh();
      return;
    }
    if (x >= 116 && x < 204) {
      keyboardSymbols_ = !keyboardSymbols_;
      UiTheme::beginFrame();
      drawKeyboard();
      display_.pageRefresh();
      return;
    }
    if (x >= 212 && x < 336) {
      if (inputValue_.length() < 160) inputValue_ += ' ';
      changed = true;
    } else if (x >= 344 && x < 424) {
      if (inputValue_.length()) inputValue_.remove(inputValue_.length() - 1);
      changed = true;
    } else if (x >= 432 && x < 522) {
      finishKeyboard();
      return;
    }
  }

  if (y >= 642 && y < 714) {
    inputTarget_ = InputTarget::None;
    navigateBack();
    return;
  }

  if (changed) {
    M5.Display.fillRect(18, 145, 504, 95, TFT_WHITE);
    UiTheme::card(18, 145, 504, 95, true);
    String shown = inputValue_;
    if (inputMasked_ && !keyboardShowSecret_) {
      shown = "";
      int n = inputValue_.length() < 26 ? inputValue_.length() : 26;
      for (int i = 0; i < n; ++i) shown += '*';
    } else if (shown.length() > 38) {
      shown = "..." + shown.substring(shown.length() - 35);
    }
    UiTheme::value(shown.length() ? shown : String("_"), 34, 178, false);
    UiTheme::detail(String(inputValue_.length()) + " chars", 34, 213);
    if (inputMasked_) UiTheme::pill(keyboardShowSecret_ ? "HIDE" : "SHOW", 442, 161, true);
    display_.partialRefresh(18, 145, 504, 95);

    UiTheme::beginFrame();
    drawKeyboard();
    display_.partialRefresh(15, 260, 507, 280);
  }
}

void UiManager::finishKeyboard() {
  InputTarget target = inputTarget_;
  inputTarget_ = InputTarget::None;

  if (target == InputTarget::WiFiPassword) {
    String ssid = selectedWifiSsid_;
    String password = inputValue_;
    showAppLoading("Wi-Fi", "Connecting to " + ssid, 65);
    bool ok = wifi_.connect(ssid, password, true);
    wifiStatusMessage_ = ok ? String("Connected to ") + ssid : String("Connection failed: ") + ssid;
    showWiFi();
    return;
  }

  if (target == InputTarget::BrowserSearch) {
    browserSearchQuery_ = inputValue_;
    browserSearchMode_ = true;
    browserTextScroll_ = 0;
    browserLinkScroll_ = 0;
    if (!wifi_.isConnected()) {
      browserPage_ = BrowserPage();
      browserPage_.error = "Connect Wi-Fi first";
    } else {
          showAppLoading("Web Search", "Trying lightweight search providers...", 55);
      browserPage_ = browserService_.search(browserSearchQuery_);
      if (browserPage_.finalUrl.length()) browserUrl_ = browserPage_.finalUrl;
    }
    showBrowser();
    return;
  }

  if (target == InputTarget::BrowserUrl) {
    browserUrl_ = inputValue_;
    fetchBrowserUrl(browserUrl_);
    return;
  }

  if (target == InputTarget::OtpSecret) {
    String normalized = OtpService::normalizeSecret(inputValue_);
    if (OtpService::validSecret(normalized)) {
      otpSecret_ = normalized;
      otpCode_ = "";
    } else {
      otpSecret_ = "";
      otpCode_ = "INVALID";
    }
    showOtp();
    return;
  }

  if (target == InputTarget::PingHost) {
    pingHost_ = inputValue_;
    showPingTool();
    return;
  }
  if (target == InputTarget::DnsHost) {
    dnsHost_ = inputValue_;
    showDnsTool();
    return;
  }
  if (target == InputTarget::ApiUrl) {
    apiUrl_ = inputValue_;
    showApiTester();
    return;
  }
  if (target == InputTarget::ApiBody) {
    apiBody_ = inputValue_;
    showApiTester();
    return;
  }
  if (target == InputTarget::MqttHost) {
    mqttHost_ = inputValue_;
    showMqtt();
    return;
  }
  if (target == InputTarget::MqttTopic) {
    mqttTopic_ = inputValue_;
    showMqtt();
    return;
  }
  if (target == InputTarget::MqttPayload) {
    mqttPayload_ = inputValue_;
    showMqtt();
    return;
  }
  if (target == InputTarget::WolMac) {
    wolMac_ = inputValue_;
    showWakeOnLan();
    return;
  }

  if (target == InputTarget::PhoneCommand) {
    phoneCommandDraft_ = inputValue_;
    bool ok = phoneLink_.sendCommand(phoneCommandDraft_);
    phoneLinkStatus_ = ok ? String("Command sent: ") + phoneCommandDraft_ : String("Start Phone Link bridge first");
    showPhoneLink();
    return;
  }

  if (target == InputTarget::PhoneNumber) {
    phoneNumberDraft_ = normalizedPhoneNumber(inputValue_);
    phoneAppStatus_ = phoneNumberDraft_.length() ? "Number ready" : "Enter a valid phone number";
    showPhone();
    return;
  }

  if (target == InputTarget::PhoneMessage) {
    phoneMessageDraft_ = inputValue_;
    const String number = normalizedPhoneNumber(phoneNumberDraft_);
    if (!number.length() || !phoneMessageDraft_.length()) {
      phoneAppStatus_ = "Enter a number and message first";
    } else {
      String message = phoneMessageDraft_;
      message.replace("|", " ");
      phoneAppStatus_ = phoneLink_.sendCommand(String("message:") + number + "|" + message)
        ? "SMS request sent; approve it on the iPhone"
        : "No companion connected; install/open the iPhone companion";
    }
    showPhone();
    return;
  }

  if (target == InputTarget::ClassicTerminal) {
    classicTerminalInput_ = inputValue_;
    String cmd=classicTerminalInput_;
    classicTerminalInput_="";
    classicTerminalExecute(cmd);
    showClassicTerminal();
    return;
  }

  if (target == InputTarget::ManualTime) {
    int yy=0, mo=0, dd=0, hh=0, mm=0;
    String v = inputValue_;
    v.replace("T", " ");
    if (sscanf(v.c_str(), "%d-%d-%d %d:%d", &yy, &mo, &dd, &hh, &mm) == 5 &&
        yy >= 2020 && yy <= 2099 && mo >= 1 && mo <= 12 && dd >= 1 && dd <= 31 &&
        hh >= 0 && hh <= 23 && mm >= 0 && mm <= 59) {

      const char* tz = config_.get().timezone == "Europe/Rome"
        ? "CET-1CEST,M3.5.0,M10.5.0/3"
        : "UTC0";
      setenv("TZ", tz, 1);
      tzset();

      struct tm local = {};
      local.tm_year = yy - 1900;
      local.tm_mon = mo - 1;
      local.tm_mday = dd;
      local.tm_hour = hh;
      local.tm_min = mm;
      local.tm_sec = 0;
      local.tm_isdst = -1;
      time_t epoch = mktime(&local);
      if (epoch > 0) {
        struct timeval tv = {epoch, 0};
        settimeofday(&tv, nullptr);
        M5.Rtc.setDateTime({{
          static_cast<int16_t>(yy), static_cast<int8_t>(mo), static_cast<int8_t>(dd)
        }, {
          static_cast<int8_t>(hh), static_cast<int8_t>(mm), static_cast<int8_t>(0)
        }});
        timeStatus_ = "Manual time updated";
      } else {
        timeStatus_ = "Invalid date/time";
      }
    } else {
      timeStatus_ = "Use YYYY-MM-DD HH:MM";
    }
    showDateTime();
    return;
  }

  showApps();
}

void UiManager::fetchBrowserUrl(const String& url) {
  browserUrl_ = url;
  browserTextScroll_ = 0;
  browserLinkScroll_ = 0;
  browserSearchMode_ = false;

  if (!wifi_.isConnected()) {
    browserPage_ = BrowserPage();
    browserPage_.error = "Connect Wi-Fi first";
    showBrowser();
    return;
  }

  showAppLoading("Web Reader", "Fetching page...", 55);
  browserPage_ = browserService_.fetch(browserUrl_);
  if (browserPage_.finalUrl.length()) browserUrl_ = browserPage_.finalUrl;
  showBrowser();
}

void UiManager::showBrowser() {
  page_ = Page::Browser;
  preparePage();
  statusBar();

  UiTheme::title("Web Reader", 18, 76);
  UiTheme::detail("Search + HTTP/HTTPS text reader", 20, 116);

  UiTheme::card(18, 145, 504, 88, true);
  UiTheme::label("SEARCH", 34, 160);
  String q = browserSearchQuery_.length() ? browserSearchQuery_ : String("Search the web");
  if (q.length() > 45) q = q.substring(0, 42) + "...";
  UiTheme::value(q, 34, 191, false);
  UiTheme::pill("GO", 458, 158, true);

  UiTheme::card(18, 246, 504, 92);
  UiTheme::label("ADDRESS", 34, 261);
  String u = browserUrl_.length() ? browserUrl_ : String("https://");
  if (u.length() > 50) u = "..." + u.substring(u.length() - 47);
  UiTheme::value(u, 34, 292, false);
  UiTheme::pill("EDIT", 438, 259, true);
  UiTheme::detail(wifi_.isConnected() ? String("Direct URL / relative links supported") : String("Wi-Fi required"), 34, 320);

  if (!browserPage_.ok) {
    UiTheme::card(18, 354, 504, 360);
    String error = browserPage_.error.length() ? browserPage_.error : String("Search or enter a URL");
    if (error.length() > 46) error = error.substring(0, 43) + "...";
    UiTheme::value(error, 40, 398, false);
    if (browserPage_.error.length() > 46) {
      UiTheme::detail(browserPage_.error.substring(43, min(103, static_cast<int>(browserPage_.error.length()))), 40, 442);
      if (browserPage_.error.length() > 103) UiTheme::detail(browserPage_.error.substring(103, min(163, static_cast<int>(browserPage_.error.length()))), 40, 469);
    }
    UiTheme::detail("Search tries DuckDuckGo, Lite and Google HTML.", 40, 516);
    UiTheme::detail("Reader mode extracts title, text and internal links.", 40, 548);
    UiTheme::detail("JavaScript, video and complex CSS are not rendered.", 40, 580);
    UiTheme::detail("HTTPS uses lightweight transport without CA validation.", 40, 612);
  } else {
    UiTheme::card(18, 354, 504, 76, true);
    String title = browserPage_.title;
    if (title.length() > 52) title = title.substring(0, 49) + "...";
    UiTheme::value(title, 34, 374, false);
    UiTheme::detail(String("HTTP ") + browserPage_.status, 34, 407);

    UiTheme::card(18, 444, 504, 238);
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.setTextWrap(true, true);
    M5.Display.setCursor(34, 466);
    int start = constrain(browserTextScroll_, 0, max(0, static_cast<int>(browserPage_.text.length()) - 1));
    String excerpt = browserPage_.text.substring(start, min(start + 1350, static_cast<int>(browserPage_.text.length())));
    M5.Display.print(excerpt);
    M5.Display.setTextWrap(false);
    UiTheme::resetFont();

    UiTheme::label("LINKS", 20, 697);
    int linkCount = static_cast<int>(browserPage_.links.size());
    browserLinkScroll_ = constrain(browserLinkScroll_, 0, max(0, linkCount - 2));
    for (int i = 0; i < 2 && browserLinkScroll_ + i < linkCount; ++i) {
      int y = 721 + i * 57;
      UiTheme::card(18, y, 504, 50);
      String link = browserPage_.links[browserLinkScroll_ + i];
      if (link.length() > 58) link = link.substring(0, 55) + "...";
      UiTheme::detail(String(browserLinkScroll_ + i + 1) + ". " + link, 30, y + 15);
    }
    UiTheme::detail(String("Text ") + (browserTextScroll_ + 1) + " / " + browserPage_.text.length() +
                    "  - swipe up/down", 34, 834);
  }

  bottomNav(4);
  commitPage();
}

void UiManager::drawOtpCode() {
  M5.Display.fillRect(18, 145, 504, 250, TFT_WHITE);
  UiTheme::card(18, 145, 504, 250, true);
  UiTheme::label("TOTP", 34, 163);

  time_t now = time(nullptr);
  String value;
  String detail;

  if (!otpSecret_.length()) {
    value = otpCode_ == "INVALID" ? "INVALID" : "SET SECRET";
    detail = "Base32 secret is kept only in RAM.";
  } else if (now < 1700000000) {
    value = "SYNC TIME";
    detail = "Connect Wi-Fi once after boot to synchronize NTP.";
  } else {
    value = OtpService::generate(otpSecret_, static_cast<uint64_t>(now), 6, 30);
    uint32_t remaining = 30U - static_cast<uint32_t>(now % 30);
    detail = String("RFC6238 / SHA1 / 6 digits / valid ~") + remaining + " sec";
    otpCode_ = value;
    lastOtpStep_ = static_cast<uint64_t>(now) / 30ULL;
  }

  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(value, 270, 255);
  UiTheme::resetFont();

  UiTheme::detail(detail, 64, 332);
}

void UiManager::showOtp() {
  page_ = Page::Otp;
  preparePage();
  statusBar();

  UiTheme::title("OTP Authenticator", 18, 76);
  UiTheme::detail("TOTP calculator / RFC6238", 20, 116);

  drawOtpCode();

  UiTheme::card(18, 414, 246, 112, true);
  UiTheme::value("SET SECRET", 58, 452, false);
  UiTheme::detail("Base32 key", 82, 491);

  UiTheme::card(276, 414, 246, 112);
  UiTheme::value("CLEAR", 347, 452, false);
  UiTheme::detail("Forget from RAM", 330, 491);

  UiTheme::card(18, 546, 504, 166);
  UiTheme::label("SECURITY", 34, 564);
  UiTheme::detail("The OTP secret is not written to LittleFS or microSD.", 34, 604);
  UiTheme::detail("It disappears on reboot/deep power loss.", 34, 638);
  UiTheme::detail("Time is synchronized from NTP after a Wi-Fi connection.", 34, 672);

  UiTheme::card(18, 728, 504, 114);
  UiTheme::label("STATUS", 34, 746);
  UiTheme::detail(wifi_.timeSynced() ? "System clock synchronized" : "NTP not synchronized in this boot", 34, 783);
  UiTheme::detail(otpSecret_.length() ? "Secret loaded in volatile memory" : "No secret loaded", 34, 816);

  bottomNav(4);
  commitPage();
}



void UiManager::showStorageFormat() {
  page_ = Page::StorageFormat;
  preparePage();
  statusBar();

  UiTheme::title("Format microSD", 18, 76);
  UiTheme::detail("DESTRUCTIVE / single PaperOS volume", 20, 116);

  UiTheme::card(18, 145, 504, 132, true);
  UiTheme::label("WARNING", 34, 162);
  UiTheme::value("All files on the card will be erased", 34, 199, false);
  UiTheme::detail("Backup anything important before formatting.", 34, 239);

  UiTheme::card(18, 300, 160, 116);
  UiTheme::value("AUTO", 65, 336, false);
  UiTheme::detail("FAT/exFAT by size", 37, 378);

  UiTheme::card(190, 300, 160, 116);
  UiTheme::value("FAT", 248, 336, false);
  UiTheme::detail("FAT16/32", 233, 378);

  UiTheme::card(362, 300, 160, 116);
  UiTheme::value("exFAT", 404, 336, false);
  UiTheme::detail("SDXC / large", 391, 378);

  UiTheme::card(18, 442, 504, 150);
  UiTheme::label("CONFIRMATION", 34, 459);
  if (formatConfirmArmed_) {
    UiTheme::value(String("Tap ") + pendingFormatType_ + " again to ERASE", 34, 499, false);
    UiTheme::detail("A different button cancels the previous choice and arms that format.", 34, 542);
  } else {
    UiTheme::value("Choose a format", 34, 499, false);
    UiTheme::detail("First tap arms the action. Second identical tap performs it.", 34, 542);
  }

  UiTheme::card(18, 615, 504, 168);
  UiTheme::label("PARTITION MODEL", 34, 633);
  UiTheme::detail("Formatting creates one whole-card volume that PaperOS can mount.", 34, 671);
  UiTheme::detail("Multi-partition editing is intentionally disabled in this alpha:", 34, 704);
  UiTheme::detail("the current ESP32 SD mount layer exposes one mounted volume.", 34, 737);
  UiTheme::detail(storage_.filesystemHint(), 34, 770);

  bottomNav(4);
  commitPage();
}

void UiManager::showRecoveryHelp() {
  page_ = Page::RecoveryHelp;
  preparePage();
  statusBar();
  UiTheme::title("SD Recovery", 18, 76);
  UiTheme::detail("Read-only FAT32 scan / best-effort file export", 20, 116);

  UiTheme::shadowCard(18, 145, 504, 94, 4);
  UiTheme::label("DELETED FILE SCAN", 34, 160);
  UiTheme::detail("FAT32 only / does not write to the SD card", 34, 197);
  UiTheme::pill("SCAN", 430, 159, true);

  UiTheme::card(18, 254, 504, 452);
  UiTheme::label("FOUND CANDIDATES", 34, 270);
  UiTheme::detail(String(storage_.recoveredFileCount()) + " file(s)  /  swipe to browse", 34, 302);
  const size_t count = storage_.recoveredFileCount();
  fileScroll_ = constrain(fileScroll_, 0, max(0, static_cast<int>(count) - 4));
  if (!count) {
    String scanStatus = storage_.recoveryStatus().length() ? storage_.recoveryStatus() : String("Tap SCAN to check the card");
    if (scanStatus.length() > 40) scanStatus = scanStatus.substring(0, 37) + "...";
    UiTheme::value(scanStatus, 34, 354, false);
    UiTheme::detail("Only FAT32 deleted entries are listed by this first version.", 34, 402);
  } else {
    const size_t shown = min<size_t>(4, count - static_cast<size_t>(fileScroll_));
    for (size_t i = 0; i < shown; ++i) {
      const size_t idx = static_cast<size_t>(fileScroll_) + i;
      const RecoveredSdFile* f = storage_.recoveredFile(idx);
      if (!f) continue;
      const int y = 324 + static_cast<int>(i) * 86;
      UiTheme::value(String(idx + 1) + ". " + f->name, 34, y, false);
      UiTheme::detail(String(f->size / 1024) + " KB  /  " + (f->fatChainAvailable ? "FAT chain hint" : "contiguous guess"), 34, y + 34);
      if (i + 1 < shown) M5.Display.drawFastHLine(34, y + 64, 454, TFT_BLACK);
    }
  }

  UiTheme::card(18, 724, 504, 126);
  UiTheme::label("EXPORT TO A COMPUTER", 34, 740);
  UiTheme::detail("Sign in to the Web Console, then open /recovery", 34, 776);
  UiTheme::detail("Download recovered files there; never save them to this SD.", 34, 810);
  String status = storage_.recoveryStatus();
  if (status.length() > 58) status = status.substring(0, 55) + "...";
  UiTheme::detail(status, 34, 842);
  bottomNav(4);
  commitPage();
}

void UiManager::showRecoveryPreview(size_t index) {
  const RecoveredSdFile* file = storage_.recoveredFile(index);
  if (!file) { showRecoveryHelp(); return; }
  selectedRecoveryIndex_ = index;
  page_ = Page::RecoveryPreview;
  preparePage();
  statusBar();
  UiTheme::title(file->name, 18, 76);
  UiTheme::detail(String(file->size) + " bytes / candidate data preview", 20, 116);
  UiTheme::card(18, 145, 504, 570, true);
  String upper = file->name;
  upper.toUpperCase();
  const bool jpeg = upper.endsWith(".JPG") || upper.endsWith(".JPEG");
  const bool png = upper.endsWith(".PNG");
  const bool bmp = upper.endsWith(".BMP");
  const bool text = upper.endsWith(".TXT") || upper.endsWith(".MD") || upper.endsWith(".CSV") ||
                    upper.endsWith(".JSON") || upper.endsWith(".LOG");
  if (jpeg || png || bmp) {
    const size_t maxPreview = 2U * 1024U * 1024U;
    if (file->size > maxPreview) {
      UiTheme::value("Image too large for on-device preview", 38, 210, false);
    } else {
      uint8_t* bytes = static_cast<uint8_t*>(ps_malloc(file->size));
      if (!bytes) {
        UiTheme::value("Not enough memory for preview", 38, 210, false);
      } else {
        size_t got = storage_.readRecoveredFile(index, 0, bytes, file->size);
        if (got == file->size) {
          if (jpeg) M5.Display.drawJpg(bytes, got, 38, 170, 464, 520);
          else if (png) M5.Display.drawPng(bytes, got, 38, 170, 464, 520);
          else M5.Display.drawBmp(bytes, got, 38, 170, 464, 520);
        } else {
          UiTheme::value("Preview read failed", 38, 210, false);
        }
        free(bytes);
      }
    }
  } else if (text) {
    char bytes[769];
    size_t got = storage_.readRecoveredFile(index, 0, reinterpret_cast<uint8_t*>(bytes), sizeof(bytes) - 1);
    bytes[got] = 0;
    String preview;
    for (size_t i = 0; i < got && preview.length() < 680; ++i) {
      const uint8_t ch = static_cast<uint8_t>(bytes[i]);
      if (ch == '\n' || ch == '\r' || ch == '\t' || ch >= 32) preview += static_cast<char>(ch);
    }
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.setTextWrap(true, true);
    M5.Display.setCursor(36, 178);
    M5.Display.print(preview);
    UiTheme::resetFont();
  } else {
    UiTheme::value("No on-device preview for this type", 38, 210, false);
    UiTheme::detail("Use Export to open or save it on a computer.", 38, 252);
  }
  UiTheme::card(18, 744, 504, 78);
  UiTheme::value("EXPORT FILE", 182, 762, false);
  UiTheme::detail("Opens a risk confirmation first", 125, 794);
  UiTheme::detail("Preview may be incomplete if sectors were reused or the file was fragmented.", 26, 838);
  bottomNav(4);
  commitPage();
}

void UiManager::showRecoveryConfirm() {
  page_ = Page::RecoveryConfirm;
  preparePage();
  statusBar();
  const RecoveredSdFile* file = storage_.recoveredFile(selectedRecoveryIndex_);
  UiTheme::title("Recover file?", 18, 76);
  UiTheme::detail(file ? file->name : "File no longer available", 20, 116);
  UiTheme::card(18, 150, 504, 440, true);
  UiTheme::label("POSSIBLE RISKS", 36, 172);
  UiTheme::detail("The directory entry survived, but some file data may", 36, 218);
  UiTheme::detail("already have been reused. Fragmented files can export", 36, 254);
  UiTheme::detail("incomplete or damaged. Previewing does not verify every byte.", 36, 290);
  UiTheme::detail("PaperOS will stream the file to your signed-in browser.", 36, 350);
  UiTheme::detail("It will not write a recovered copy onto this SD card.", 36, 386);
  if (recoveryExportConfirmed_) {
    UiTheme::card(34, 436, 472, 112);
    UiTheme::label("NEXT", 50, 451);
    UiTheme::detail("On a computer/phone, sign in to Web Console and open:", 50, 484);
    UiTheme::value("http://paperos.local/recovery", 50, 516, false);
    UiTheme::detail("Then select the candidate and download it to that device.", 36, 568);
    UiTheme::iconButton(18, 744, 246, 78, "OK", "Return to preview", false);
    UiTheme::iconButton(276, 744, 246, 78, "LIST", "Back to results", false);
  } else {
    UiTheme::detail("Continue only if you accept these limitations.", 36, 520);
    UiTheme::iconButton(18, 744, 246, 78, "YES", "Continue to export", true);
    UiTheme::iconButton(276, 744, 246, 78, "NO", "Cancel", false);
  }
  bottomNav(4);
  commitPage();
}

void UiManager::showPhone() {
  page_ = Page::Phone;
  preparePage();
  statusBar();

  UiTheme::title("Phone", 18, 76);
  UiTheme::detail("iPhone notifications and companion dialer", 20, 116);
  UiTheme::shadowCard(18, 145, 504, 86, 4);
  UiTheme::label("IPHONE LINK", 34, 158);
  UiTheme::value(phoneLink_.statusText(), 34, 188, false);
  UiTheme::pill(phoneLink_.active() ? "STOP" : "PAIR", 430, 159, true);

  UiTheme::card(18, 244, 504, 102);
  UiTheme::label("PHONE NUMBER", 34, 258);
  String number = phoneNumberDraft_.length() ? phoneNumberDraft_ : String("Tap to enter a number");
  UiTheme::value(number, 34, 293, false);
  UiTheme::pill("EDIT", 438, 255, false);
  UiTheme::iconButton(18, 358, 246, 86, "CALL", "Call via iPhone app", false);
  UiTheme::iconButton(276, 358, 246, 86, "SMS", "Compose message", false);

  UiTheme::label("RECENT PHONE NOTIFICATIONS", 20, 466);
  UiTheme::pill("CLEAR", 444, 458, false);
  UiTheme::card(18, 490, 504, 322);
  const size_t count = phoneLink_.notificationCount();
  phoneScroll_ = constrain(phoneScroll_, 0, max(0, static_cast<int>(count) - 3));
  if (!count) {
    UiTheme::value("No notifications received", 38, 530, false);
    UiTheme::detail(phoneLink_.ancsReady() ? "New calls and messages will appear here." : "Pair the iPhone in iPhone Link first.", 38, 570);
  } else {
    const size_t shown = min<size_t>(3, count - static_cast<size_t>(phoneScroll_));
    for (size_t i = 0; i < shown; ++i) {
      const PhoneNotification* n = phoneLink_.notification(static_cast<size_t>(phoneScroll_) + i);
      if (!n) continue;
      const int y = 508 + static_cast<int>(i) * 94;
      String title = n->title.length() ? n->title : n->app;
      if (title.length() > 47) title = title.substring(0, 44) + "...";
      String body = n->body;
      if (body.length() > 64) body = body.substring(0, 61) + "...";
      UiTheme::value(title, 34, y, false);
      UiTheme::detail(n->app + (n->category == 1 ? " / CALL" : (n->category == 2 ? " / MISSED CALL" : "")), 34, y + 31);
      UiTheme::detail(body, 34, y + 57);
      if (i + 1 < shown) M5.Display.drawFastHLine(34, y + 80, 454, TFT_BLACK);
    }
  }

  UiTheme::card(18, 824, 504, 42);
  String status = phoneAppStatus_.length() ? phoneAppStatus_ :
    (phoneLink_.connected() ? "Commands go to the connected companion for approval." :
                              "Calling and SMS need a compatible iPhone companion app.");
  if (status.length() > 62) status = status.substring(0, 59) + "...";
  UiTheme::detail(status, 28, 836);
  bottomNav(4);
  commitPage();
}

void UiManager::showPhoneLink() {
  page_ = Page::PhoneLink;
  preparePage();
  statusBar();

  UiTheme::title("iPhone Link", 18, 76);
  UiTheme::detail("Native Apple ANCS + optional companion bridge", 20, 116);

  UiTheme::shadowCard(18, 145, 504, 112, 4);
  UiTheme::label("IPHONE ACCESSORY", 34, 161);
  UiTheme::value(phoneLink_.statusText(), 34, 196, false);
  UiTheme::pill(phoneLink_.active() ? "STOP" : "PAIR", 430, 160, true);
  String peer = phoneLink_.peerAddress();
  if (peer.length() > 20) peer = peer.substring(0, 20);
  UiTheme::detail(phoneLink_.ancsReady() ? String("ANCS READY / ") + peer :
                  (phoneLink_.bonded() ? "Bond saved / waiting for ANCS" : "Secure BLE bonding / ANCS solicitation"), 34, 232);

  UiTheme::shadowCard(18, 274, 504, 176, 4);
  UiTheme::label(phoneLink_.ancsReady() ? "IOS CONNECTION" : "FIRST PAIRING", 34, 291);
  if (phoneLink_.ancsReady()) {
    UiTheme::value("Notifications from iPhone are live", 34, 326, false);
    UiTheme::detail("Incoming/missed calls, messages, mail and app notifications", 34, 365);
    UiTheme::detail("arrive through Apple's ANCS service. Full Contacts / message", 34, 396);
    UiTheme::detail("history databases are not exposed by ANCS.", 34, 426);
  } else {
    UiTheme::detail("1. Tap PAIR above.", 34, 323);
    UiTheme::detail("2. On iPhone open nRF Connect > Scan > PaperOS > Connect.", 34, 352);
    UiTheme::detail("3. Accept the iOS Bluetooth pairing request.", 34, 381);
    UiTheme::detail("4. Allow notifications for PaperOS when iOS asks.", 34, 410);
    UiTheme::detail("Direct visibility in Settings > Bluetooth can vary on DIY BLE.", 34, 436);
  }

  UiTheme::label("IPHONE NOTIFICATIONS", 20, 470);
  UiTheme::shadowCard(18, 494, 504, 170, 4);
  if (phoneLink_.notificationCount() == 0) {
    UiTheme::value(phoneLink_.ancsReady() ? "No current notifications" : "Waiting for iPhone", 38, 530, false);
    UiTheme::detail("ANCS events will appear here automatically after pairing.", 38, 568);
    UiTheme::detail("Swipe this page to browse more notifications.", 38, 600);
  } else {
    const int visible = 2;
    int maxOffset = max(0, static_cast<int>(phoneLink_.notificationCount()) - visible);
    phoneScroll_ = constrain(phoneScroll_, 0, maxOffset);
    int shown = min(visible, static_cast<int>(phoneLink_.notificationCount()) - phoneScroll_);
    for (int row = 0; row < shown; ++row) {
      const PhoneNotification* n = phoneLink_.notification(static_cast<size_t>(phoneScroll_ + row));
      if (!n) continue;
      String cat;
      switch (n->category) {
        case 1: cat="CALL"; break; case 2: cat="MISSED"; break; case 3: cat="VOICEMAIL"; break;
        case 4: cat="SOCIAL"; break; case 5: cat="CALENDAR"; break; case 6: cat="MAIL"; break;
        case 7: cat="NEWS"; break; case 8: cat="HEALTH"; break; case 9: cat="BUSINESS"; break;
        default: cat="IOS"; break;
      }
      String title = cat + " / " + (n->title.length() ? n->title : n->app);
      if (title.length() > 45) title = title.substring(0, 42) + "...";
      String body = n->body;
      if (body.length() > 62) body = body.substring(0, 59) + "...";
      UiTheme::value(title, 34, 511 + row * 74, false);
      UiTheme::detail(body, 34, 546 + row * 74);
      if (row == 0 && shown > 1) M5.Display.drawFastHLine(34, 574, 454, TFT_BLACK);
    }
  }

  const PhoneNotification* first = phoneLink_.notification(static_cast<size_t>(phoneScroll_));
  UiTheme::iconButton(18, 680, 160, 76, "+", "ANCS action +", first && first->positiveAction);
  UiTheme::iconButton(190, 680, 160, 76, "-", "ANCS action -", first && first->negativeAction);
  UiTheme::iconButton(362, 680, 160, 76, "CLR", "Clear list", false);

  UiTheme::iconButton(18, 772, 160, 70, "CAM", "Companion camera", false);
  UiTheme::iconButton(190, 772, 160, 70, "PLAY", "Media control", false);
  UiTheme::iconButton(362, 772, 160, 70, "...", "Custom command", false);

  bottomNav(4);
  commitPage();
}

void UiManager::showGpioLab() {
  nfcService_.stop();
  page_ = Page::GpioLab;

  static const int pins[6] = {25, 32, 26, 33, 18, 19};
  static const char* labels[6] = {
    "Port A Yellow / G25", "Port A White / G32",
    "Port B Yellow / G26", "Port B White / G33",
    "Port C Yellow / G18", "Port C White / G19"
  };

  if (!gpioInitialized_) {
    for (int i = 0; i < 6; ++i) {
      pinMode(pins[i], INPUT);
      gpioModeState_[i] = 0;
      gpioOutputLevel_[i] = false;
    }
    gpioInitialized_ = true;
  }

  preparePage();
  statusBar();

  UiTheme::title("GPIO Lab", 18, 76);
  UiTheme::detail("M5Paper Port A / B / C", 20, 116);

  for (int i = 0; i < 6; ++i) {
    int y = 145 + i * 94;
    UiTheme::card(18, y, 504, 82);
    String mode;
    if (gpioModeState_[i] == 0) mode = String("INPUT / ") + (digitalRead(pins[i]) ? "HIGH" : "LOW");
    else mode = String("OUTPUT / ") + (gpioOutputLevel_[i] ? "HIGH" : "LOW");
    UiTheme::value(labels[i], 34, y + 14, false);
    UiTheme::detail(mode + "  - tap to cycle INPUT -> LOW -> HIGH", 34, y + 49);
    UiTheme::chevron(491, y + 28);
  }

  UiTheme::card(18, 720, 504, 122);
  UiTheme::label("PIN MAP / SAFETY", 34, 738);
  UiTheme::detail("A: G25/G32  B: G26/G33  C: G18/G19", 34, 774);
  UiTheme::detail("Signal pins are 3.3 V logic. Grove red wire is 5 V.", 34, 806);
  UiTheme::detail("Default is INPUT; do not drive unknown external hardware.", 34, 833);

  bottomNav(4);
  commitPage();
}


void UiManager::showNfcLab() {
  page_ = Page::NfcLab;
  preparePage();
  statusBar();

  UiTheme::title("NFC Lab", 18, 76);
  UiTheme::detail("PN532 / Port C UART / ISO14443A", 20, 116);

  UiTheme::shadowCard(18, 145, 504, 128, 4);
  UiTheme::label("PN532 MODULE", 34, 162);
  UiTheme::value(nfcService_.ready() ? nfcService_.firmwareText() : String("Not initialized"), 34, 198, false);
  UiTheme::pill(nfcService_.ready() ? "READY" : "TEST", 420, 160, true);
  UiTheme::detail("Port C: G18 RX <- PN532 TX / G19 TX -> PN532 RX", 34, 235);

  UiTheme::iconButton(18, 294, 246, 106, "NFC", "Test module", false);
  UiTheme::iconButton(276, 294, 246, 106, "TAG", "Scan tag", true);

  UiTheme::shadowCard(18, 420, 504, 190, 4);
  UiTheme::label("LAST TAG", 34, 438);
  if (lastNfcTag_.found) {
    UiTheme::value(lastNfcTag_.uid, 34, 477, false);
    UiTheme::detail(lastNfcTag_.type, 34, 520);
    UiTheme::detail(String("UID length: ") + lastNfcTag_.uidLength + " bytes", 34, 554);
  } else {
    UiTheme::value(nfcStatus_.length() ? nfcStatus_ : String("No tag scanned"), 34, 477, false);
    UiTheme::detail("Place an ISO14443A / MIFARE / NTAG-compatible tag near PN532.", 34, 520);
  }

  UiTheme::shadowCard(18, 630, 504, 190, 4);
  UiTheme::label("WIRING / SAFETY", 34, 648);
  UiTheme::detail("Set the PN532 board switches/jumpers to HSU / UART mode.", 34, 684);
  UiTheme::detail("PN532 TX -> Port C G18 (RX)", 34, 718);
  UiTheme::detail("PN532 RX -> Port C G19 (TX) / GND -> GND", 34, 750);
  UiTheme::detail("Port C red wire is 5 V: power only if your PN532 board accepts it.", 34, 782);
  UiTheme::detail("This Lab reads tags only; no write/emulation is performed.", 34, 810);

  bottomNav(4);
  commitPage();
}

void UiManager::showLabs() {
  page_ = Page::Labs;
  preparePage();
  statusBar();

  UiTheme::title("Labs", 18, 78);
  UiTheme::detail("WinLabs Solutions / hardware & beta features", 20, 119);

  UiTheme::shadowCard(18, 150, 504, 84, 4);
  UiTheme::pill("LABS", 34, 172, true);
  UiTheme::detail("Experimental tools with explicit hardware limits.", 130, 180);

  settingsRow(252, "USB", "USB HID Lab", "Script library / external HID adapter");
  settingsRow(342, "BT", "BLE Explorer", "Advertising / GATT diagnostics");
  settingsRow(432, "NFC", "NFC / PN532", "Port C UART / tag UID reader");
  settingsRow(522, "IO", "GPIO Lab", "Port A / B / C pin controls");
  settingsRow(612, "EPD", "Display Test", "Refresh / ghosting diagnostics");
  settingsRow(702, "DEV", "Developer", "System / logs / experimental tools");

  bottomNav(4);
  commitPage();
}

void UiManager::showHidLab() {
  page_ = Page::HidLab;
  preparePage();
  statusBar();

  UiTheme::title("USB HID Lab", 18, 78);
  UiTheme::detail("Labs / script library", 20, 119);

  UiTheme::card(18, 150, 504, 150, true);
  UiTheme::label("HARDWARE LIMIT", 34, 168);
  UiTheme::value("External HID adapter required", 34, 204, false);
  UiTheme::detail("M5Paper USB-C is USB-to-serial, not native HID.", 34, 246);
  UiTheme::detail("Scripts can be stored here but are not executed over USB-C.", 34, 274);

  UiTheme::label("SCRIPT LIBRARY", 20, 326);
  int row = 0;
  if (storage_.available()) {
    File root = SD.open("/PaperOS/Labs/HID");
    for (File file = root.openNextFile(); file && row < 5; file = root.openNextFile()) {
      if (!file.isDirectory()) {
        String n = String(file.name());
        int slash = n.lastIndexOf('/');
        if (slash >= 0) n = n.substring(slash + 1);
        settingsRow(356 + row * 82, "SC", n, String((uint32_t)file.size()) + " bytes", false);
        ++row;
      }
      file.close();
    }
    root.close();
  }
  if (row == 0) {
    UiTheme::card(18, 356, 504, 118);
    UiTheme::value("Nessuno script", 34, 390, false);
    UiTheme::detail("Carica file in /PaperOS/Labs/HID dal File Manager web.", 34, 432);
  }

  UiTheme::card(18, 786, 504, 64);
  UiTheme::detail("Execution on internal USB-C: unavailable on this hardware", 34, 808);

  bottomNav(4);
  commitPage();
}


void UiManager::classicDrawChrome(const String& titleText) {
  M5.Display.fillScreen(TFT_WHITE);

  // Windows 3.x-style monochrome desktop texture.
  for (int y = 0; y < UiTheme::ScreenH; y += 6) {
    for (int x = ((y / 6) & 1) ? 3 : 0; x < UiTheme::ScreenW; x += 6) {
      M5.Display.drawPixel(x, y, TFT_BLACK);
    }
  }

  M5.Display.fillRect(8, 8, 524, 902, TFT_WHITE);
  M5.Display.drawRect(8, 8, 524, 902, TFT_BLACK);
  M5.Display.drawRect(10, 10, 520, 898, TFT_BLACK);

  M5.Display.fillRect(14, 14, 512, 42, TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold12pt7b);
  M5.Display.setTextDatum(middle_left);
  M5.Display.drawString(titleText, 26, 35);

  M5.Display.fillRect(486, 20, 30, 28, TFT_WHITE);
  M5.Display.drawRect(486, 20, 30, 28, TFT_BLACK);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString("X", 501, 34);

  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.setTextDatum(middle_left);
  M5.Display.drawString("File", 24, 76);
  M5.Display.drawString("Options", 78, 76);
  M5.Display.drawString("Window", 160, 76);
  M5.Display.drawString("Help", 250, 76);
  M5.Display.drawFastHLine(14, 95, 512, TFT_BLACK);
  UiTheme::resetFont();
}

void UiManager::classicDrawCursor() {
  int x = constrain(classicMouseX_, 2, UiTheme::ScreenW - 18);
  int y = constrain(classicMouseY_, 2, UiTheme::ScreenH - 24);
  M5.Display.drawLine(x, y, x, y + 20, TFT_BLACK);
  M5.Display.drawLine(x, y, x + 13, y + 13, TFT_BLACK);
  M5.Display.drawLine(x + 1, y + 1, x + 1, y + 17, TFT_WHITE);
  M5.Display.drawLine(x + 2, y + 2, x + 11, y + 11, TFT_WHITE);
  M5.Display.drawLine(x + 4, y + 13, x + 9, y + 20, TFT_BLACK);
}

void UiManager::showClassicSplash() {
  page_ = Page::ClassicSplash;
  preparePage(true);
  M5.Display.fillScreen(TFT_WHITE);

  M5.Display.drawRect(25, 90, 490, 690, TFT_BLACK);
  M5.Display.drawRect(29, 94, 482, 682, TFT_BLACK);

  // Period-correct geometric flag/splash, rendered from vectors.
  const int ox = 78, oy = 190, cell = 62;
  M5.Display.fillRect(ox, oy, cell, cell, TFT_BLACK);
  for (int y = 0; y < cell; y += 6)
    for (int x = 0; x < cell; x += 6)
      M5.Display.drawPixel(ox + x, oy + y, TFT_WHITE);
  M5.Display.drawRect(ox + 76, oy, cell, cell, TFT_BLACK);
  M5.Display.fillRect(ox, oy + 76, cell, cell, TFT_BLACK);
  M5.Display.drawRect(ox + 76, oy + 76, cell, cell, TFT_BLACK);

  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.drawString("Microsoft Windows", 270, 410);
  M5.Display.setFont(&fonts::FreeSansBold18pt7b);
  M5.Display.drawString("Version 3.11", 270, 472);

  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.drawString("Classic Desktop for PaperOS", 270, 550);
  M5.Display.drawString("WinLabs Solutions", 270, 585);
  M5.Display.drawString("Starting Program Manager...", 270, 690);
  UiTheme::resetFont();

  display_.pageRefresh();
  delay(1300);
  navigatingBack_ = true;
  showClassicDesktop();
}

void UiManager::showClassicDesktop() {
  page_ = Page::ClassicDesktop;
  preparePage();
  classicDrawChrome("Program Manager - Windows 3.11");

  M5.Display.fillRect(28, 112, 484, 620, TFT_WHITE);
  M5.Display.drawRect(28, 112, 484, 620, TFT_BLACK);
  M5.Display.fillRect(32, 116, 476, 34, TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.setTextDatum(middle_left);
  M5.Display.drawString("Main", 44, 133);
  UiTheme::resetFont();

  struct Icon { int x; int y; const char* name; const char* glyph; };
  const Icon icons[] = {
    {54, 178, "Terminal", "C:\\>"},
    {208, 178, "File Manager", "FILE"},
    {362, 178, "Control Panel", "CTRL"},
    {54, 330, "Bluetooth Input", "HID"},
    {208, 330, "Ski", "SKI"},
    {362, 330, "Solitaire", "SOL"},
    {54, 482, "Network", "NET"},
    {208, 482, "NFC / GPIO", "I/O"},
    {362, 482, "Exit to PaperOS", "EXIT"}
  };

  for (const auto& icon : icons) {
    M5.Display.fillRect(icon.x, icon.y, 100, 82, TFT_WHITE);
    M5.Display.drawRect(icon.x + 20, icon.y, 60, 50, TFT_BLACK);
    M5.Display.fillRect(icon.x + 24, icon.y + 4, 52, 42, TFT_BLACK);
    M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(icon.glyph, icon.x + 50, icon.y + 25);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.setFont(&fonts::Font2);
    M5.Display.drawString(icon.name, icon.x + 50, icon.y + 66);
  }

  M5.Display.drawRect(28, 750, 484, 120, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(top_left);
  M5.Display.drawString("PaperOS Classic Desktop", 44, 766);
  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.drawString(hidInput_.statusText(), 44, 802);
  M5.Display.drawString("Touch, BLE mouse and BLE keyboard supported", 44, 836);
  UiTheme::resetFont();

  classicDrawCursor();
  commitPage();
}

void UiManager::showClassicHid() {
  page_ = Page::ClassicHid;
  preparePage();
  classicDrawChrome("Bluetooth Input Devices");

  M5.Display.drawRect(28, 118, 484, 108, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString("BLE HID Host", 44, 136);
  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.drawString(classicHidStatus_.length() ? classicHidStatus_ : hidInput_.statusText(), 44, 170);
  M5.Display.drawString("Mouse / Keyboard service 0x1812 / BLE HID", 44, 200);

  M5.Display.fillRect(392, 140, 96, 54, TFT_WHITE);
  M5.Display.drawRect(392, 140, 96, 54, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString("SCAN", 440, 167);

  const auto& devices = hidInput_.devices();
  int shown = min(7, static_cast<int>(devices.size()));
  for (int i = 0; i < shown; ++i) {
    int y = 248 + i * 76;
    M5.Display.drawRect(28, y, 484, 66, TFT_BLACK);
    String n = devices[i].name;
    if (n.length() > 31) n = n.substring(0, 28) + "...";
    M5.Display.setTextDatum(top_left);
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.drawString(n, 44, y + 9);
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    String kind = devices[i].likelyKeyboard ? "Keyboard" : (devices[i].likelyMouse ? "Mouse" : "HID");
    M5.Display.drawString(kind + " / " + String(devices[i].rssi) + " dBm / " + devices[i].address, 44, y + 37);
    M5.Display.drawRect(430, y + 12, 64, 38, TFT_BLACK);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("LINK", 462, y + 31);
  }

  if (!shown) {
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString("Press SCAN with your mouse/keyboard in pairing mode.", 270, 360);
  }

  UiTheme::resetFont();
  classicDrawCursor();
  commitPage();
}


std::vector<String> UiManager::classicTokenize(const String& command) const {
  std::vector<String> out;
  String current;
  bool quoted = false;
  for (size_t i = 0; i < command.length(); ++i) {
    char c = command[i];
    if (c == '"') {
      quoted = !quoted;
      continue;
    }
    if (!quoted && (c == ' ' || c == '\t')) {
      if (current.length()) {
        out.push_back(current);
        current = "";
      }
    } else {
      current += c;
    }
  }
  if (current.length()) out.push_back(current);
  return out;
}

void UiManager::classicTerminalPrint(const String& text) {
  int start = 0;
  while (start <= (int)text.length()) {
    int nl = text.indexOf('\n', start);
    String line = nl >= 0 ? text.substring(start, nl) : text.substring(start);

    while (line.length() > 64) {
      classicTerminalLines_.push_back(line.substring(0, 64));
      line = line.substring(64);
    }
    classicTerminalLines_.push_back(line);

    if (nl < 0) break;
    start = nl + 1;
  }
  while (classicTerminalLines_.size() > 80) classicTerminalLines_.erase(classicTerminalLines_.begin());
}

bool UiManager::classicPinAllowed(int pin) const {
  if (classicUnsafeGpio_) return pin >= 0 && pin <= 39;
  return pin == 18 || pin == 19 || pin == 25 || pin == 26 || pin == 32 || pin == 33;
}

void UiManager::classicTerminalExecute(const String& commandRaw) {
  String command = commandRaw;
  command.trim();
  if (!command.length()) return;

  classicTerminalPrint(classicCwd_ + "> " + command);
  auto a = classicTokenize(command);
  if (a.empty()) return;
  String cmd = a[0];
  cmd.toLowerCase();

  auto resolve = [this](const String& p) {
    if (!p.length()) return classicCwd_;
    if (p == "C:" || p == "c:") return String("/PaperOS");
    if (p == "D:" || p == "d:") return String("/");
    if (p.startsWith("/")) return p;
    return joinPath(classicCwd_, p);
  };

  if (cmd == "help" || cmd == "?") {
    classicTerminalPrint("PaperOS ROOT Terminal commands:");
    classicTerminalPrint("ver sysinfo heap battery time sd cls pwd cd dir ls type cat");
    classicTerminalPrint("mkdir md del rm copy cp move touch write append gpio wifi hid bt nfc");
    classicTerminalPrint("reboot lock sleep deepsleep");
    classicTerminalPrint("unsafe on|off  - unlock internal GPIO 0..39");
    return;
  }

  if (cmd == "cls" || cmd == "clear") {
    classicTerminalLines_.clear();
    return;
  }

  if (cmd == "ver") {
    classicTerminalPrint(String("PaperOS ") + VERSION + " / Classic Desktop 3.11");
    return;
  }

  if (cmd == "sysinfo") {
    classicTerminalPrint(String("CPU ESP32 @ ") + getCpuFrequencyMhz() + " MHz");
    classicTerminalPrint(String("Heap free: ") + ESP.getFreeHeap() + " / PSRAM free: " + ESP.getFreePsram());
    classicTerminalPrint(String("Flash: ") + ESP.getFlashChipSize() + " bytes");
    classicTerminalPrint(String("Wi-Fi: ") + (wifi_.isConnected() ? WiFi.SSID() : "offline"));
    classicTerminalPrint(String("SD: ") + (storage_.available() ? "mounted" : "not mounted"));
    return;
  }

  if (cmd == "heap") {
    classicTerminalPrint(String("heap=") + ESP.getFreeHeap() + " min=" + ESP.getMinFreeHeap() +
                         " psram=" + ESP.getFreePsram());
    return;
  }

  if (cmd == "battery") {
    classicTerminalPrint(String("battery=") + power_.batteryPercent() + "%  " +
                         power_.batteryMillivolts() + "mV  trend=" + power_.batteryTrendLabel());
    return;
  }

  if (cmd == "time") {
    auto dt = M5.Rtc.getDateTime();
    char b[40];
    snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d:%02d",
             dt.date.year, dt.date.month, dt.date.date,
             dt.time.hours, dt.time.minutes, dt.time.seconds);
    classicTerminalPrint(String(b));
    return;
  }

  if (cmd == "sd") {
    classicTerminalPrint(storage_.available()
      ? String("SD mounted total=") + (uint32_t)(storage_.totalBytes()/1048576ULL) +
        "MB free=" + (uint32_t)(storage_.freeBytes()/1048576ULL) + "MB " + storage_.filesystemHint()
      : String("SD not mounted"));
    return;
  }

  if (cmd == "pwd") {
    classicTerminalPrint(classicCwd_);
    return;
  }

  if (cmd == "cd") {
    String p = a.size() >= 2 ? resolve(a[1]) : String("/PaperOS");
    if (storage_.isDirectory(p)) {
      classicCwd_ = p;
      classicTerminalPrint(classicCwd_);
    } else classicTerminalPrint("Path not found");
    return;
  }

  if (cmd == "dir" || cmd == "ls") {
    String p = a.size() >= 2 ? resolve(a[1]) : classicCwd_;
    File d = SD.open(p);
    if (!d || !d.isDirectory()) {
      if (d) d.close();
      classicTerminalPrint("Directory not found");
      return;
    }
    int count = 0;
    for (File x = d.openNextFile(); x && count < 40; x = d.openNextFile()) {
      String n = String(x.name());
      int slash = n.lastIndexOf('/');
      if (slash >= 0) n = n.substring(slash + 1);
      classicTerminalPrint((x.isDirectory() ? "<DIR> " : "      ") + n +
                           (x.isDirectory() ? "" : String("  ") + (uint32_t)x.size()));
      x.close();
      ++count;
    }
    d.close();
    if (!count) classicTerminalPrint("<empty>");
    return;
  }

  if (cmd == "type" || cmd == "cat") {
    if (a.size() < 2) { classicTerminalPrint("Usage: type <file>"); return; }
    String p = resolve(a[1]);
    File f = SD.open(p, FILE_READ);
    if (!f || f.isDirectory()) { if (f) f.close(); classicTerminalPrint("File not found"); return; }
    String content;
    while (f.available() && content.length() < 2048) content += (char)f.read();
    f.close();
    classicTerminalPrint(content);
    if (storage_.exists(p) && content.length() >= 2048) classicTerminalPrint("[output clipped at 2048 bytes]");
    return;
  }

  if (cmd == "mkdir" || cmd == "md") {
    if (a.size() < 2) { classicTerminalPrint("Usage: mkdir <path>"); return; }
    String p = resolve(a[1]);
    classicTerminalPrint(storage_.makeDir(p) ? "Directory created" : "mkdir failed");
    return;
  }

  if (cmd == "del" || cmd == "rm") {
    if (a.size() < 2) { classicTerminalPrint("Usage: del <path>"); return; }
    String p = resolve(a[1]);
    classicTerminalPrint(storage_.removePath(p) ? "Deleted" : "delete failed/protected");
    return;
  }

  if (cmd == "copy" || cmd == "cp") {
    if (a.size() < 3) { classicTerminalPrint("Usage: copy <source> <destination>"); return; }
    String from = resolve(a[1]), to = resolve(a[2]);
    classicTerminalPrint(storage_.copyPath(from, to) ? "Copied" : "copy failed");
    return;
  }

  if (cmd == "touch") {
    if (a.size() < 2) { classicTerminalPrint("Usage: touch <file>"); return; }
    String p=resolve(a[1]);
    File f=SD.open(p, FILE_APPEND);
    bool ok=(bool)f;
    if(f) f.close();
    classicTerminalPrint(ok ? "File ready" : "touch failed");
    return;
  }

  if (cmd == "write" || cmd == "append") {
    if (a.size() < 3) { classicTerminalPrint(String("Usage: ")+cmd+" <file> <text>"); return; }
    String p=resolve(a[1]);
    String textValue;
    for(size_t i=2;i<a.size();++i){ if(i>2) textValue+=" "; textValue+=a[i]; }
    if (cmd == "write" && SD.exists(p)) SD.remove(p);
    File f=SD.open(p, cmd=="append" ? FILE_APPEND : FILE_WRITE);
    if(!f){ classicTerminalPrint("open failed"); return; }
    size_t n=f.print(textValue);
    if(cmd=="append") f.print("\n");
    f.flush(); f.close();
    classicTerminalPrint(n ? "Written" : "write failed");
    return;
  }

  if (cmd == "move" || cmd == "mv") {
    if (a.size() < 3) { classicTerminalPrint("Usage: move <source> <destination>"); return; }
    String from = resolve(a[1]), to = resolve(a[2]);
    classicTerminalPrint(storage_.movePath(from, to) ? "Moved" : "move failed");
    return;
  }

  if (cmd == "unsafe") {
    if (a.size() >= 2) {
      String v = a[1]; v.toLowerCase();
      classicUnsafeGpio_ = v == "on" || v == "1" || v == "true";
    }
    classicTerminalPrint(String("unsafe GPIO=") + (classicUnsafeGpio_ ? "ON" : "OFF"));
    if (classicUnsafeGpio_) classicTerminalPrint("WARNING: internal display/SD pins can be disrupted.");
    return;
  }

  if (cmd == "gpio") {
    if (a.size() < 2 || a[1] == "list") {
      classicTerminalPrint("Expansion pins: A=25,32  B=26,33  C=18,19");
      classicTerminalPrint(String("Internal pins: ") + (classicUnsafeGpio_ ? "UNLOCKED" : "locked; use unsafe on"));
      return;
    }
    String sub = a[1]; sub.toLowerCase();
    if (a.size() < 3) { classicTerminalPrint("gpio read|write|mode|adc <pin> [value]"); return; }
    int pin = a[2].toInt();
    if (!classicPinAllowed(pin)) { classicTerminalPrint("Pin locked. Use expansion GPIO or 'unsafe on'."); return; }

    if (sub == "read") {
      pinMode(pin, INPUT);
      classicTerminalPrint(String("GPIO") + pin + "=" + digitalRead(pin));
    } else if (sub == "write") {
      if (a.size() < 4) { classicTerminalPrint("gpio write <pin> 0|1"); return; }
      pinMode(pin, OUTPUT);
      int v = a[3].toInt() ? HIGH : LOW;
      digitalWrite(pin, v);
      classicTerminalPrint(String("GPIO") + pin + "=" + (v == HIGH ? "HIGH" : "LOW"));
    } else if (sub == "mode") {
      if (a.size() < 4) { classicTerminalPrint("gpio mode <pin> in|out|pullup"); return; }
      String m=a[3]; m.toLowerCase();
      if (m == "out") pinMode(pin, OUTPUT);
      else if (m == "pullup") pinMode(pin, INPUT_PULLUP);
      else pinMode(pin, INPUT);
      classicTerminalPrint("Mode updated");
    } else if (sub == "adc") {
      classicTerminalPrint(String("ADC GPIO") + pin + "=" + analogRead(pin));
    } else classicTerminalPrint("Unknown gpio subcommand");
    return;
  }

  if (cmd == "wifi") {
    String sub = a.size() >= 2 ? a[1] : "status";
    sub.toLowerCase();
    if (sub == "on") {
      wifi_.setRadioEnabled(true);
      classicTerminalPrint("Wi-Fi radio ON");
    } else if (sub == "off") {
      wifi_.setRadioEnabled(false);
      classicTerminalPrint("Wi-Fi radio OFF");
    } else if (sub == "scan") {
      int n = WiFi.scanNetworks(false, true);
      classicTerminalPrint(String("Networks: ") + n);
      for (int i=0; i<n && i<20; ++i)
        classicTerminalPrint(WiFi.SSID(i) + "  " + WiFi.RSSI(i) + "dBm CH" + WiFi.channel(i));
      WiFi.scanDelete();
    } else {
      classicTerminalPrint(String("radio=") + (wifi_.radioEnabled() ? "on" : "off") +
                           " connected=" + (wifi_.isConnected() ? "yes" : "no"));
      if (wifi_.isConnected()) classicTerminalPrint(WiFi.SSID() + " / " + WiFi.localIP().toString());
    }
    return;
  }

  if (cmd == "hid" || cmd == "bt") {
    String sub = a.size() >= 2 ? a[1] : "status";
    sub.toLowerCase();
    if (!bluetoothActive_) {
      BLEDevice::init("PaperOS");
      bluetoothActive_ = true;
    }
    hidInput_.begin();

    if (sub == "scan") {
      classicTerminalPrint("Scanning BLE HID...");
      hidInput_.scan(4);
      const auto& d = hidInput_.devices();
      for (size_t i=0;i<d.size() && i<15;++i)
        classicTerminalPrint(String(i) + ": " + d[i].name + " / " + d[i].address);
      if (d.empty()) classicTerminalPrint("No BLE HID found");
    } else if (sub == "connect") {
      if (a.size() < 3) { classicTerminalPrint("hid connect <index>"); return; }
      int idx = a[2].toInt();
      classicTerminalPrint(hidInput_.connect(idx) ? "HID connected" : "HID connect failed");
    } else {
      classicTerminalPrint(hidInput_.statusText());
    }
    return;
  }

  if (cmd == "nfc") {
    String sub = a.size() >= 2 ? a[1] : "scan";
    sub.toLowerCase();
    if (sub == "scan") {
      NfcTagInfo tag = nfcService_.scan(1600);
      classicTerminalPrint(tag.found ? String("NFC UID=") + tag.uid + " / " + tag.type : String("No NFC tag"));
    } else classicTerminalPrint(nfcService_.statusText());
    return;
  }

  if (cmd == "reboot") {
    classicTerminalPrint("Rebooting...");
    showClassicTerminal();
    delay(250);
    ESP.restart();
    return;
  }

  if (cmd == "sleep" || cmd == "lock") {
    classicTerminalPrint("Locking PaperOS...");
    showClassicTerminal();
    delay(120);
    power_.sleepNow();
    return;
  }

  if (cmd == "deepsleep") {
    classicTerminalPrint("Entering hardware deep sleep...");
    showClassicTerminal();
    delay(180);
    power_.deepSleepNow(0);
    return;
  }

  classicTerminalPrint("Bad command or file name");
}

void UiManager::showClassicTerminal() {
  page_ = Page::ClassicTerminal;
  preparePage();
  classicDrawChrome("MS-DOS Prompt - PaperOS ROOT Terminal");

  M5.Display.fillRect(24, 108, 492, 696, TFT_BLACK);
  M5.Display.drawRect(22, 106, 496, 700, TFT_BLACK);

  M5.Display.setFont(&fonts::FreeMono9pt7b);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextDatum(top_left);

  const int visible = 22;
  int start = max(0, static_cast<int>(classicTerminalLines_.size()) - visible);
  int y = 118;
  for (int i = start; i < (int)classicTerminalLines_.size(); ++i) {
    String line = classicTerminalLines_[i];
    if (line.length() > 66) line = line.substring(0, 66);
    M5.Display.drawString(line, 32, y);
    y += 27;
  }

  M5.Display.fillRect(24, 816, 492, 64, TFT_WHITE);
  M5.Display.drawRect(24, 816, 492, 64, TFT_BLACK);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  String prompt = classicCwd_ + "> " + classicTerminalInput_;
  if (prompt.length() > 63) prompt = "..." + prompt.substring(prompt.length() - 60);
  M5.Display.drawString(prompt + "_", 32, 835);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.drawString("Tap prompt for touch keyboard / BLE keyboard types directly", 32, 864);
  UiTheme::resetFont();

  classicDrawCursor();
  commitPage();
}


void UiManager::classicInitSki() {
  skiPlayerX_ = 270;
  skiScore_ = 0;
  skiRunning_ = true;
  classicLastGameTick_ = millis();
  for (int i = 0; i < 9; ++i) {
    skiObstacles_[i].x = 35 + (esp_random() % 470);
    skiObstacles_[i].y = 120 + (esp_random() % 620);
    skiObstacles_[i].type = esp_random() & 1U;
  }
}

void UiManager::classicStepSki() {
  if (!skiRunning_) return;
  for (int i = 0; i < 9; ++i) {
    skiObstacles_[i].y += 52;
    if (skiObstacles_[i].y > 830) {
      skiObstacles_[i].y = 120 - (esp_random() % 220);
      skiObstacles_[i].x = 35 + (esp_random() % 470);
      skiObstacles_[i].type = esp_random() & 1U;
      ++skiScore_;
    }

    if (abs(skiObstacles_[i].x - skiPlayerX_) < 28 &&
        skiObstacles_[i].y > 720 && skiObstacles_[i].y < 800) {
      skiRunning_ = false;
    }
  }
}

void UiManager::showClassicSki() {
  page_ = Page::ClassicSki;
  if (!skiRunning_ && skiScore_ == 0) classicInitSki();
  preparePage();
  classicDrawChrome("Ski - Windows Entertainment Pack");

  M5.Display.fillRect(24, 108, 492, 748, TFT_WHITE);
  M5.Display.drawRect(24, 108, 492, 748, TFT_BLACK);

  // Mountain/snow field.
  for (int y=125; y<830; y+=34) {
    int x = 35 + ((y * 17) % 450);
    M5.Display.drawPixel(x, y, TFT_BLACK);
  }

  for (int i=0; i<9; ++i) {
    int x=skiObstacles_[i].x, y=skiObstacles_[i].y;
    if (y < 120 || y > 825) continue;
    if (skiObstacles_[i].type == 0) {
      M5.Display.drawLine(x, y-16, x-13, y+10, TFT_BLACK);
      M5.Display.drawLine(x, y-16, x+13, y+10, TFT_BLACK);
      M5.Display.drawFastHLine(x-13, y+10, 26, TFT_BLACK);
      M5.Display.drawFastVLine(x, y+10, 10, TFT_BLACK);
    } else {
      M5.Display.drawCircle(x, y, 10, TFT_BLACK);
      M5.Display.drawLine(x-7,y+6,x+7,y-6,TFT_BLACK);
    }
  }

  const int sx=skiPlayerX_, sy=770;
  M5.Display.drawCircle(sx, sy-18, 5, TFT_BLACK);
  M5.Display.drawLine(sx, sy-13, sx, sy+5, TFT_BLACK);
  M5.Display.drawLine(sx, sy-7, sx-12, sy, TFT_BLACK);
  M5.Display.drawLine(sx, sy-7, sx+12, sy, TFT_BLACK);
  M5.Display.drawLine(sx, sy+5, sx-14, sy+18, TFT_BLACK);
  M5.Display.drawLine(sx, sy+5, sx+14, sy+18, TFT_BLACK);
  M5.Display.drawLine(sx-20, sy+21, sx-4, sy+17, TFT_BLACK);
  M5.Display.drawLine(sx+4, sy+17, sx+20, sy+21, TFT_BLACK);

  M5.Display.fillRect(34, 120, 180, 54, TFT_WHITE);
  M5.Display.drawRect(34, 120, 180, 54, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.setTextColor(TFT_BLACK,TFT_WHITE);
  M5.Display.drawString(String("Score: ") + skiScore_, 48, 138);

  M5.Display.fillRect(332, 120, 160, 54, TFT_WHITE);
  M5.Display.drawRect(332, 120, 160, 54, TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(skiRunning_ ? "SKIING" : "CRASH - TAP", 412, 147);

  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString("Touch left/right or use keyboard arrows / A-D", 270, 838);
  UiTheme::resetFont();

  classicDrawCursor();
  commitPage();
}

void UiManager::classicInitSolitaire() {
  solStock_.clear(); solWaste_.clear();
  for (auto& p : solTableau_) p.clear();
  for (auto& f : solFoundation_) f.clear();
  solSelectedType_ = -1; solSelectedPile_ = -1; solScore_ = 0;

  std::vector<ClassicCard> deck;
  for (uint8_t suit=0; suit<4; ++suit) {
    for (uint8_t rank=1; rank<=13; ++rank) {
      ClassicCard card;
      card.rank=rank;
      card.suit=suit;
      card.faceUp=false;
      deck.push_back(card);
    }
  }

  for (int i=(int)deck.size()-1; i>0; --i) {
    int j = esp_random() % (i+1);
    ClassicCard t=deck[i]; deck[i]=deck[j]; deck[j]=t;
  }

  size_t at=0;
  for (int col=0; col<7; ++col) {
    for (int row=0; row<=col; ++row) {
      ClassicCard c=deck[at++];
      c.faceUp = row == col;
      solTableau_[col].push_back(c);
    }
  }
  while (at<deck.size()) {
    ClassicCard c=deck[at++];
    c.faceUp=false;
    solStock_.push_back(c);
  }
}

bool UiManager::classicSolitaireMoveToTableau(int target) {
  if (target < 0 || target > 6 || solSelectedType_ < 0) return false;
  ClassicCard card;
  if (solSelectedType_ == 0) {
    if (solWaste_.empty()) return false;
    card=solWaste_.back();
  } else {
    if (solSelectedPile_ < 0 || solSelectedPile_ > 6 || solTableau_[solSelectedPile_].empty()) return false;
    card=solTableau_[solSelectedPile_].back();
    if (!card.faceUp) return false;
  }

  bool valid=false;
  if (solTableau_[target].empty()) valid=card.rank==13;
  else {
    ClassicCard top=solTableau_[target].back();
    bool redCard = card.suit==1 || card.suit==2;
    bool redTop = top.suit==1 || top.suit==2;
    valid = top.faceUp && top.rank == card.rank + 1 && redCard != redTop;
  }
  if (!valid) return false;

  if (solSelectedType_ == 0) solWaste_.pop_back();
  else {
    solTableau_[solSelectedPile_].pop_back();
    if (!solTableau_[solSelectedPile_].empty()) solTableau_[solSelectedPile_].back().faceUp=true;
  }
  card.faceUp=true;
  solTableau_[target].push_back(card);
  solSelectedType_=-1; solSelectedPile_=-1;
  return true;
}

bool UiManager::classicSolitaireMoveToFoundation(int foundation) {
  if (foundation < 0 || foundation > 3 || solSelectedType_ < 0) return false;
  ClassicCard card;
  if (solSelectedType_ == 0) {
    if (solWaste_.empty()) return false;
    card=solWaste_.back();
  } else {
    if (solSelectedPile_ < 0 || solTableau_[solSelectedPile_].empty()) return false;
    card=solTableau_[solSelectedPile_].back();
  }

  if (card.suit != foundation) return false;
  uint8_t needed = solFoundation_[foundation].empty() ? 1 : solFoundation_[foundation].back().rank + 1;
  if (card.rank != needed) return false;

  if (solSelectedType_ == 0) solWaste_.pop_back();
  else {
    solTableau_[solSelectedPile_].pop_back();
    if (!solTableau_[solSelectedPile_].empty()) solTableau_[solSelectedPile_].back().faceUp=true;
  }
  card.faceUp=true;
  solFoundation_[foundation].push_back(card);
  solSelectedType_=-1; solSelectedPile_=-1;
  solScore_ += 5;
  return true;
}

void UiManager::showClassicSolitaire() {
  page_ = Page::ClassicSolitaire;
  if (solStock_.empty() && solWaste_.empty() && solTableau_[0].empty()) classicInitSolitaire();
  preparePage();
  classicDrawChrome("Solitaire");

  auto drawCard=[this](int x,int y,const ClassicCard& c,bool selected) {
    const int w=58,h=78;
    M5.Display.fillRect(x,y,w,h,TFT_WHITE);
    M5.Display.drawRect(x,y,w,h,TFT_BLACK);
    if (selected) M5.Display.drawRect(x+2,y+2,w-4,h-4,TFT_BLACK);
    if (!c.faceUp) {
      for(int yy=y+6;yy<y+h-5;yy+=7)
        for(int xx=x+6;xx<x+w-5;xx+=7)
          M5.Display.drawPixel(xx,yy,TFT_BLACK);
      return;
    }
    String rank;
    if(c.rank==1) rank="A"; else if(c.rank==11) rank="J"; else if(c.rank==12) rank="Q"; else if(c.rank==13) rank="K"; else rank=String(c.rank);
    const char* suitNames[4]={"C","D","H","S"};
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.setTextColor(TFT_BLACK,TFT_WHITE);
    M5.Display.setTextDatum(top_left);
    M5.Display.drawString(rank+String(suitNames[c.suit]),x+5,y+5);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.drawString(suitNames[c.suit],x+w/2,y+h/2+5);
    UiTheme::resetFont();
  };

  // Stock and waste.
  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.setTextColor(TFT_BLACK,TFT_WHITE);
  M5.Display.drawString("Stock",24,112);
  M5.Display.drawString("Waste",94,112);
  if (!solStock_.empty()) {
    ClassicCard back=solStock_.back(); back.faceUp=false; drawCard(22,142,back,false);
  } else {
    M5.Display.drawRect(22,142,58,78,TFT_BLACK);
    M5.Display.drawString("RESET",26,170);
  }
  if (!solWaste_.empty()) drawCard(92,142,solWaste_.back(),solSelectedType_==0);
  else M5.Display.drawRect(92,142,58,78,TFT_BLACK);

  // Foundations C/D/H/S.
  for(int f=0;f<4;++f){
    int x=230+72*f;
    M5.Display.drawRect(x,142,58,78,TFT_BLACK);
    const char* sn[4]={"C","D","H","S"};
    if(solFoundation_[f].empty()) {
      M5.Display.setTextDatum(middle_center);
      M5.Display.drawString(sn[f],x+29,181);
    } else drawCard(x,142,solFoundation_[f].back(),false);
  }

  // Tableau.
  for(int col=0;col<7;++col){
    int x=15+75*col;
    int y=260;
    if(solTableau_[col].empty()) {
      M5.Display.drawRect(x,y,58,78,TFT_BLACK);
      M5.Display.setTextDatum(middle_center);
      M5.Display.drawString("K",x+29,y+39);
      continue;
    }
    for(size_t i=0;i<solTableau_[col].size();++i){
      int cy=y+(int)i*31;
      bool selected=solSelectedType_==1 && solSelectedPile_==col && i==solTableau_[col].size()-1;
      drawCard(x,cy,solTableau_[col][i],selected);
    }
  }

  M5.Display.fillRect(24,820,492,42,TFT_WHITE);
  M5.Display.drawRect(24,820,492,42,TFT_BLACK);
  M5.Display.setTextDatum(middle_left);
  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.drawString(String("Score: ")+solScore_+"   Tap stock/waste/card then destination   N = new game",36,841);
  UiTheme::resetFont();

  classicDrawCursor();
  commitPage();
}

void UiManager::showCalculator() {
  page_ = Page::Calculator;
  preparePage();
  statusBar();

  UiTheme::title("Calculator", 18, 78);
  UiTheme::detail("PaperOS utility", 20, 119);

  UiTheme::card(18, 145, 504, 100, true);
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextDatum(middle_right);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(calcDisplay_, 500, 195);
  UiTheme::resetFont();

  const char* keys[20] = {
    "C", "+/-", "%", "/",
    "7", "8", "9", "*",
    "4", "5", "6", "-",
    "1", "2", "3", "+",
    "0", ".", "=", "BS"
  };
  const int sx = 18, sy = 266, kw = 117, kh = 92, gap = 12;
  for (int i = 0; i < 20; ++i) {
    int col = i % 4, row = i / 4;
    int x = sx + col * (kw + gap);
    int y = sy + row * (kh + gap);
    UiTheme::card(x, y, kw, kh, keys[i][0] == '=');
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(keys[i], x + kw/2, y + kh/2);
    UiTheme::resetFont();
  }

  bottomNav(4);
  commitPage();
}

void UiManager::calcKey(const String& key) {
  if (key == "C") {
    calcDisplay_ = "0"; calcAccumulator_ = 0; calcPendingOp_ = 0; calcResetInput_ = true; return;
  }
  if (key == "BS") {
    if (calcDisplay_.length() > 1) calcDisplay_.remove(calcDisplay_.length()-1);
    else calcDisplay_ = "0";
    return;
  }
  if (key == "+/-") {
    if (calcDisplay_.startsWith("-")) calcDisplay_.remove(0,1);
    else if (calcDisplay_ != "0") calcDisplay_ = "-" + calcDisplay_;
    return;
  }
  if (key == "%") {
    double v = calcDisplay_.toDouble() / 100.0;
    calcDisplay_ = String(v, 6);
    while (calcDisplay_.endsWith("0")) calcDisplay_.remove(calcDisplay_.length()-1);
    if (calcDisplay_.endsWith(".")) calcDisplay_.remove(calcDisplay_.length()-1);
    return;
  }
  if (key == "+" || key == "-" || key == "*" || key == "/" || key == "=") {
    double current = calcDisplay_.toDouble();
    if (calcPendingOp_) {
      if (calcPendingOp_ == '+') calcAccumulator_ += current;
      else if (calcPendingOp_ == '-') calcAccumulator_ -= current;
      else if (calcPendingOp_ == '*') calcAccumulator_ *= current;
      else if (calcPendingOp_ == '/' && current != 0) calcAccumulator_ /= current;
    } else calcAccumulator_ = current;
    calcDisplay_ = String(calcAccumulator_, 6);
    while (calcDisplay_.endsWith("0")) calcDisplay_.remove(calcDisplay_.length()-1);
    if (calcDisplay_.endsWith(".")) calcDisplay_.remove(calcDisplay_.length()-1);
    calcPendingOp_ = key == "=" ? 0 : key[0];
    calcResetInput_ = true;
    return;
  }

  if (calcResetInput_) { calcDisplay_ = "0"; calcResetInput_ = false; }
  if (key == "." && calcDisplay_.indexOf('.') >= 0) return;
  if (calcDisplay_ == "0" && key != ".") calcDisplay_ = key;
  else if (calcDisplay_.length() < 14) calcDisplay_ += key;
}

void UiManager::showSystem() {
  page_ = Page::System;
  preparePage();
  statusBar();

  UiTheme::title("System", 18, 78);
  UiTheme::detail(String("PaperOS ") + VERSION, 20, 119);

  homeCard(18, 150, 246, 126, "HEAP", String(ESP.getFreeHeap() / 1024) + " KB", String("Min ") + ESP.getMinFreeHeap() / 1024 + " KB", false);
  homeCard(276, 150, 246, 126, "PSRAM", String(ESP.getFreePsram() / 1048576.0, 1) + " MB", String(ESP.getPsramSize() / 1048576.0, 1) + " MB total", false);
  homeCard(18, 290, 246, 126, "BATTERY", String(power_.batteryPercent()) + "%", String(power_.batteryMillivolts()) + " mV", false);
  homeCard(276, 290, 246, 126, "UPTIME", String(millis() / 60000UL) + " min", "Since boot", false);

  UiTheme::card(18, 430, 504, 126);
  UiTheme::label("NETWORK", 34, 447);
  UiTheme::value(wifi_.isConnected() ? WiFi.SSID() : String(SETUP_AP), 34, 480, false);
  UiTheme::detail(wifi_.ip().toString(), 34, 523);
  UiTheme::pill(wifi_.isConnected() ? String(WiFi.RSSI()) + " dBm" : "AP MODE", 392, 442, false);

  UiTheme::card(18, 570, 504, 126);
  UiTheme::label("STORAGE", 34, 587);
  UiTheme::value(storage_.available() ? "microSD mounted" : "microSD unavailable", 34, 620, false);
  UiTheme::detail(storage_.available() ? String(storage_.freeBytes() / 1048576.0, 1) + " MB free" : "Insert a microSD for Files and Notes", 34, 663);

  UiTheme::card(18, 710, 504, 132);
  UiTheme::label("DISPLAY", 34, 727);
  UiTheme::value(String(display_.pageChangesSinceClean()) + " page changes", 34, 760, false);
  UiTheme::detail("Tools > Clean Display performs an anti-ghosting wipe.", 34, 802);

  bottomNav(3);
  commitPage();
}

void UiManager::showNotes() {
  page_ = Page::Notes;
  noteIds_.clear();
  noteTitles_.clear();
  preparePage();
  statusBar();

  UiTheme::title("Notes", 18, 78);
  UiTheme::detail("Stored on microSD", 20, 119);

  // Auto-display is disabled: microSD is read while the previous physical
  // frame remains visible, then this complete page is committed once.
  if (!storage_.available()) {
    UiTheme::card(18, 160, 504, 260, true);
    UiTheme::value("microSD required", 40, 205, true);
    UiTheme::detail("Insert a card, then reopen Notes.", 40, 270);
    UiTheme::detail("PaperOS will create /PaperOS/Notes automatically.", 40, 304);
  } else {
    File root = SD.open("/PaperOS/Notes");
    int row = 0;
    for (File f = root.openNextFile(); f && row < 8; f = root.openNextFile()) {
      if (!f.isDirectory()) {
        DynamicJsonDocument doc(4096);
        if (!deserializeJson(doc, f)) {
          String id = doc["id"] | String(f.name());
          String title = doc["title"] | "Untitled";
          noteIds_.push_back(id);
          noteTitles_.push_back(title);
          int y = 150 + row * 82;
          UiTheme::card(18, y, 504, 70);
          UiTheme::value(title, 34, y + 13, false);
          String cat = doc["category"] | "";
          UiTheme::detail(cat.length() ? cat : "Note", 34, y + 46);
          UiTheme::chevron(491, y + 25);
          ++row;
        }
      }
      f.close();
    }
    root.close();

    if (noteTitles_.empty()) {
      UiTheme::card(18, 160, 504, 220);
      UiTheme::value("No notes yet", 40, 205, true);
      UiTheme::detail("Create your first note from paperos.local.", 40, 270);
    }
  }

  bottomNav(1);
  commitPage();
}

void UiManager::showNote(size_t index) {
  if (index >= noteIds_.size() || !storage_.available()) return;
  page_ = Page::NoteView;
  preparePage();
  statusBar();

  String id = noteIds_[index];
  String path = "/PaperOS/Notes/" + id + ".json";
  File f = SD.open(path, FILE_READ);

  String titleText = noteTitles_[index];
  String bodyText;
  String categoryText;

  if (f) {
    DynamicJsonDocument doc(8192);
    if (!deserializeJson(doc, f)) {
      titleText = String((const char*)(doc["title"] | "Untitled"));
      bodyText = String((const char*)(doc["body"] | ""));
      categoryText = String((const char*)(doc["category"] | ""));
    }
    f.close();
  }

  UiTheme::title(titleText, 18, 78);
  UiTheme::detail(categoryText.length() ? categoryText : "Note", 20, 119);

  UiTheme::card(18, 150, 504, 690);
  M5.Display.setFont(&fonts::FreeSans12pt7b);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextWrap(true, true);
  M5.Display.setCursor(36, 180);
  String clipped = bodyText.substring(0, 1400);
  M5.Display.print(clipped);
  M5.Display.setTextWrap(false);
  UiTheme::resetFont();

  bottomNav(1);
  commitPage();
}

String UiManager::parentPath(const String& path) const {
  if (path == "/" || !path.length()) return "/";
  int pos = path.lastIndexOf('/');
  if (pos <= 0) return "/";
  return path.substring(0, pos);
}

String UiManager::joinPath(const String& base, const String& name) const {
  if (base == "/") return "/" + name;
  return base + (base.endsWith("/") ? "" : "/") + name;
}

bool UiManager::isTextFile(const String& name) const {
  String n = name;
  n.toLowerCase();
  return n.endsWith(".txt") || n.endsWith(".md") || n.endsWith(".json") ||
         n.endsWith(".log") || n.endsWith(".csv") || n.endsWith(".ini") ||
         n.endsWith(".cfg") || n.endsWith(".xml") || n.endsWith(".html") ||
         n.endsWith(".htm") || n.endsWith(".yaml") || n.endsWith(".yml");
}

bool UiManager::isImageFile(const String& name) const {
  String n = name;
  n.toLowerCase();
  return n.endsWith(".jpg") || n.endsWith(".jpeg") || n.endsWith(".png") || n.endsWith(".bmp");
}

bool UiManager::isPdfFile(const String& name) const {
  String n = name;
  n.toLowerCase();
  return n.endsWith(".pdf");
}

String UiManager::extractPdfText(const String& path, size_t offset) const {
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory()) { if (f) f.close(); return ""; }

  if (offset < f.size()) f.seek(offset);
  String out;
  out.reserve(2200);

  bool inText = false;
  bool escaped = false;
  while (f.available() && out.length() < 2000) {
    char c = static_cast<char>(f.read());
    if (!inText) {
      if (c == '(') {
        inText = true;
        escaped = false;
      }
      continue;
    }

    if (escaped) {
      if (c == 'n' || c == 'r') out += '\n';
      else if (c == 't') out += ' ';
      else out += c;
      escaped = false;
      continue;
    }

    if (c == '\\') {
      escaped = true;
      continue;
    }

    if (c == ')') {
      inText = false;
      out += ' ';
      continue;
    }

    uint8_t b = static_cast<uint8_t>(c);
    if (b >= 32 && b < 127) out += c;
    else if (c == '\n' || c == '\r') out += ' ';
  }
  f.close();
  out.trim();
  return out;
}

void UiManager::showFiles(const String& path) {
  page_ = Page::Files;
  String nextPath = storage_.validPath(path) ? path : String("/PaperOS");
  bool pathChanged = nextPath != currentFilePath_;

  if (pathChanged || fileEntries_.empty()) {
    currentFilePath_ = nextPath;
    fileScroll_ = 0;
    fileEntries_.clear();

    if (storage_.available()) {
      if (currentFilePath_ != "/") {
        FileEntry up;
        up.name = "..";
        up.directory = true;
        fileEntries_.push_back(up);
      }

      File root = SD.open(currentFilePath_);
      if (root && root.isDirectory()) {
        for (File f = root.openNextFile(); f; f = root.openNextFile()) {
          FileEntry e;
          e.name = String(f.name());
          int slash = e.name.lastIndexOf('/');
          if (slash >= 0) e.name = e.name.substring(slash + 1);
          e.directory = f.isDirectory();
          e.size = f.size();
          fileEntries_.push_back(e);
          f.close();
          if (fileEntries_.size() >= 220) break;
        }
      }
      if (root) root.close();
    }
  } else {
    currentFilePath_ = nextPath;
  }

  preparePage();
  statusBar();
  UiTheme::title("File Manager", 18, 72);
  UiTheme::detail("PaperOS + full microSD", 20, 111);

  const bool inPaperOs = currentFilePath_.startsWith("/PaperOS");
  UiTheme::iconButton(18, 137, 246, 72, "P", inPaperOs ? "PaperOS  /  OPEN" : "PaperOS", false);
  UiTheme::iconButton(276, 137, 246, 72, "SD", !inPaperOs ? "SD Card /  /  OPEN" : "SD Card /", false);
  if (inPaperOs) {
    M5.Display.drawRoundRect(20, 139, 242, 68, 18, TFT_BLACK);
    M5.Display.drawRoundRect(21, 140, 240, 66, 17, TFT_BLACK);
  } else {
    M5.Display.drawRoundRect(278, 139, 242, 68, 18, TFT_BLACK);
    M5.Display.drawRoundRect(279, 140, 240, 66, 17, TFT_BLACK);
  }

  UiTheme::card(18, 220, 504, 54);
  String shownPath = currentFilePath_;
  if (shownPath.length() > 34) shownPath = "..." + shownPath.substring(shownPath.length() - 31);
  UiTheme::detail(shownPath, 34, 239);
  UiTheme::pill("SD TOOLS", 404, 228, false);
  if (fileSelectionMode_) UiTheme::pill("SELECT", 286, 230, true);

  if (!storage_.available()) {
    UiTheme::card(18, 286, 504, 260, true);
    UiTheme::value("microSD required", 40, 325, true);
    UiTheme::detail("Insert a card to browse PaperOS or the SD root.", 40, 390);
  } else if (fileEntries_.empty()) {
    UiTheme::card(18, 286, 504, 210);
    UiTheme::value("Empty folder", 40, 325, true);
    UiTheme::detail("Use Paste or upload files from paperos.local.", 40, 384);
  } else {
    const int visible = 6;
    int maxOffset = max(0, static_cast<int>(fileEntries_.size()) - visible);
    fileScroll_ = constrain(fileScroll_, 0, maxOffset);
    int shown = min(visible, static_cast<int>(fileEntries_.size()) - fileScroll_);

    for (int row = 0; row < shown; ++row) {
      const FileEntry& e = fileEntries_[fileScroll_ + row];
      int y = 284 + row * 72;
      String full = e.name == ".." ? parentPath(currentFilePath_) : joinPath(currentFilePath_, e.name);
      bool selected = selectedFilePath_.length() && full == selectedFilePath_;
      UiTheme::card(18, y, 504, 64, selected);
      String title = (e.directory ? "[DIR] " : "") + e.name;
      if (title.length() > 35) title = title.substring(0, 32) + "...";
      UiTheme::value(title, 34, y + 9, false);
      String detail = e.name == ".." ? "Parent folder" : (e.directory ? "Folder" : String((uint32_t)e.size) + " bytes");
      if (selected) detail = "SELECTED / " + detail;
      UiTheme::detail(detail, 34, y + 39);
      if (!fileSelectionMode_) UiTheme::chevron(491, y + 21);
    }
    UiTheme::detail(String(fileScroll_ + 1) + "-" + String(fileScroll_ + shown) + " / " + String(fileEntries_.size()) + "  swipe", 344, 720);
  }

  UiTheme::iconButton(18, 744, 116, 76, "SEL", fileSelectionMode_ ? "Cancel" : "Select", fileSelectionMode_);
  UiTheme::iconButton(144, 744, 116, 76, "CP", "Copy", selectedFilePath_.length());
  UiTheme::iconButton(270, 744, 116, 76, "CUT", "Cut", selectedFilePath_.length());
  UiTheme::iconButton(396, 744, 126, 76, "PST", "Paste", fileClipboardPath_.length());

  UiTheme::card(18, 828, 504, 32);
  String status = fileStatus_;
  if (!status.length() && fileClipboardPath_.length()) status = String(fileClipboardCut_ ? "Cut: " : "Copied: ") + fileClipboardName_;
  if (!status.length()) status = "Select a file/folder for Copy or Cut";
  if (status.length() > 62) status = status.substring(0, 59) + "...";
  UiTheme::detail(status, 28, 836);

  bottomNav(2);
  commitPage();
}

void UiManager::showFilePreview(const String& fullPath, const String& name, uint64_t size) {
  bool newFile = currentPreviewPath_ != fullPath;
  currentPreviewPath_ = fullPath;
  currentPreviewName_ = name;
  currentPreviewSize_ = size;
  if (newFile) textScroll_ = 0;

  page_ = Page::FileView;
  preparePage();
  statusBar();

  UiTheme::title(name, 18, 78);
  UiTheme::detail(String((uint32_t)size) + " bytes", 20, 119);
  UiTheme::card(18, 150, 504, 690);

  if (isImageFile(name)) {
    UiTheme::label("IMAGE READER", 34, 168);
    bool ok = false;
    String lower = name;
    lower.toLowerCase();

    // M5GFX decodes files directly from the mounted microSD. The original
    // M5Paper framebuffer is monochrome, so color images are quantized by
    // the display path rather than pretending to preserve LCD color.
    File imageFile = SD.open(fullPath, FILE_READ);
    if (imageFile && !imageFile.isDirectory()) {
      if (lower.endsWith(".jpg") || lower.endsWith(".jpeg")) {
        ok = M5.Display.drawJpg(&imageFile, 42, 205, 454, 550);
      } else if (lower.endsWith(".png")) {
        ok = M5.Display.drawPng(&imageFile, 42, 205, 454, 550);
      } else if (lower.endsWith(".bmp")) {
        ok = M5.Display.drawBmp(&imageFile, 42, 205, 454, 550);
      }
      imageFile.close();
    }

    UiTheme::detail(ok ? "Rendered from microSD / e-paper monochrome" : "Image decode failed or unsupported variant", 34, 786);
    UiTheme::detail("JPEG / PNG / BMP", 34, 816);
  } else if (isPdfFile(name)) {
    UiTheme::label("PDF READER LITE", 34, 168);
    String content = extractPdfText(fullPath, static_cast<size_t>(textScroll_));
    if (!content.length()) {
      UiTheme::value("No plain text extracted", 40, 220, false);
      UiTheme::detail("Many PDFs compress/font-encode their page streams.", 40, 272);
      UiTheme::detail("This ESP32 reader does not fake full PDF rendering.", 40, 308);
      UiTheme::detail("Use paperos.local to download complex PDFs.", 40, 344);
    } else {
      M5.Display.setFont(&fonts::FreeMono9pt7b);
      M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
      M5.Display.setTextWrap(true, true);
      M5.Display.setCursor(34, 205);
      M5.Display.print(content);
      M5.Display.setTextWrap(false);
      UiTheme::resetFont();
    }
    UiTheme::detail(String("PDF byte window ") + textScroll_ + " / " + String((uint32_t)size) + " - swipe", 34, 812);
  } else if (isTextFile(name)) {
    File f = SD.open(fullPath, FILE_READ);
    String content;
    if (f) {
      size_t safeOffset = min<size_t>(textScroll_, f.size());
      f.seek(safeOffset);
      while (f.available() && content.length() < 1800) content += static_cast<char>(f.read());
      f.close();
    }
    M5.Display.setFont(&fonts::FreeMono9pt7b);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.setTextWrap(true, true);
    M5.Display.setCursor(34, 178);
    M5.Display.print(content.length() ? content : String("[end of file]"));
    M5.Display.setTextWrap(false);
    UiTheme::resetFont();
    UiTheme::detail(String("Offset ") + textScroll_ + " / " + String((uint32_t)size) + " - swipe up/down", 34, 812);
  } else {
    UiTheme::value("Preview unavailable", 40, 205, true);
    UiTheme::detail("Supported: TXT/MD/JSON/CSV/XML/HTML/YAML/INI/CFG", 40, 270);
    UiTheme::detail("Images: JPEG/PNG/BMP. PDF: basic text extraction.", 40, 306);
    UiTheme::detail("Other binary files remain accessible from paperos.local.", 40, 342);
  }

  bottomNav(2);
  commitPage();
}


void UiManager::showNetworkTools() {
  page_ = Page::NetworkTools;
  preparePage();
  statusBar();

  UiTheme::title("Network Toolkit", 18, 76);
  UiTheme::detail(wifi_.isConnected() ? WiFi.SSID() : String("Wi-Fi required"), 20, 116);

  settingsRow(150, "PG", "Ping", "ICMP reachability + average latency");
  settingsRow(240, "DNS", "DNS Lookup", "Resolve hostname to IPv4");
  settingsRow(330, "LAN", "LAN Discovery", "Fing-style /24 segments + common services");
  settingsRow(420, "API", "HTTP / API Tester", "GET / POST / PUT + response body");
  settingsRow(510, "MQ", "MQTT Client", "Connect / subscribe / publish");
  settingsRow(600, "WOL", "Wake-on-LAN", "Send magic packet to a MAC address");

  UiTheme::card(18, 704, 504, 138);
  UiTheme::label("NETWORK", 34, 722);
  UiTheme::detail(wifi_.isConnected() ? String("IP: ") + WiFi.localIP().toString() : String("Connect from Wi-Fi Analyzer first."), 34, 758);
  UiTheme::detail(wifi_.isConnected() ? String("Gateway: ") + WiFi.gatewayIP().toString() : String("Offline"), 34, 792);
  UiTheme::detail("Tools run locally on the ESP32; no cloud service required.", 34, 823);

  bottomNav(3);
  commitPage();
}

void UiManager::showPingTool() {
  page_ = Page::PingTool;
  preparePage();
  statusBar();
  UiTheme::title("Ping", 18, 76);
  UiTheme::detail("ICMP network test", 20, 116);

  UiTheme::card(18, 145, 504, 110, true);
  UiTheme::label("TARGET", 34, 162);
  UiTheme::value(pingHost_, 34, 198, false);
  UiTheme::pill("EDIT", 438, 160, true);
  UiTheme::detail("Hostname or IPv4 address", 34, 232);

  UiTheme::card(18, 276, 504, 100, true);
  UiTheme::value("RUN PING", 190, 310, false);

  UiTheme::card(18, 397, 504, 250);
  UiTheme::label("RESULT", 34, 415);
  UiTheme::value(pingResultText_.length() ? pingResultText_ : String("Not run yet"), 34, 456, false);
  UiTheme::detail("PaperOS sends two ICMP echo requests.", 34, 510);
  UiTheme::detail("Average latency is reported when replies are received.", 34, 545);

  bottomNav(3);
  commitPage();
}

void UiManager::showDnsTool() {
  page_ = Page::DnsTool;
  preparePage();
  statusBar();
  UiTheme::title("DNS Lookup", 18, 76);
  UiTheme::detail("Resolve hostnames", 20, 116);

  UiTheme::card(18, 145, 504, 110, true);
  UiTheme::label("HOSTNAME", 34, 162);
  UiTheme::value(dnsHost_, 34, 198, false);
  UiTheme::pill("EDIT", 438, 160, true);
  UiTheme::detail("Example: example.com", 34, 232);

  UiTheme::card(18, 276, 504, 100, true);
  UiTheme::value("RESOLVE", 204, 310, false);

  UiTheme::card(18, 397, 504, 220);
  UiTheme::label("RESULT", 34, 415);
  UiTheme::value(dnsResultText_.length() ? dnsResultText_ : String("Not run yet"), 34, 456, false);
  UiTheme::detail("Uses the DNS server supplied by the active Wi-Fi network.", 34, 515);

  bottomNav(3);
  commitPage();
}

void UiManager::showLanScan() {
  page_ = Page::LanScan;
  preparePage();
  statusBar();
  UiTheme::title("LAN Discovery", 18, 76);
  UiTheme::detail("Fing-style local service discovery", 20, 116);

  int segStart = lanScanSegment_ == 0 ? 1 : (lanScanSegment_ * 64 + 1);
  int segEnd = lanScanSegment_ == 3 ? 254 : min(254, segStart + 63);
  UiTheme::card(18, 145, 246, 92, true);
  UiTheme::value(String("SCAN .") + segStart + "-." + segEnd, 44, 176, false);
  UiTheme::detail("Probe selected range", 58, 211);
  UiTheme::card(276, 145, 246, 92);
  UiTheme::value("NEXT RANGE", 326, 176, false);
  UiTheme::detail("Cycle /24 segment", 330, 211);

  UiTheme::label("DISCOVERED SERVICES", 20, 260);
  if (lanHosts_.empty()) {
    UiTheme::card(18, 288, 504, 350);
    UiTheme::value("No scan results yet", 120, 342, false);
    UiTheme::detail("Scan the current /24 in four short segments.", 52, 396);
    UiTheme::detail("This detects hosts exposing common TCP services.", 52, 432);
  } else {
    size_t shown = lanHosts_.size() > 6 ? 6 : lanHosts_.size();
    for (size_t i = 0; i < shown; ++i) {
      int y = 288 + static_cast<int>(i) * 78;
      settingsRow(y, "IP", lanHosts_[i].ip.toString(), String("Open TCP port ") + lanHosts_[i].port, false);
    }
  }

  bottomNav(3);
  commitPage();
}

void UiManager::showApiTester() {
  page_ = Page::ApiTester;
  preparePage();
  statusBar();
  UiTheme::title("HTTP / API Tester", 18, 76);
  UiTheme::detail("Local and internet endpoints", 20, 116);

  UiTheme::card(18, 145, 504, 105, true);
  UiTheme::label("URL", 34, 160);
  String shownUrl = apiUrl_;
  if (shownUrl.length() > 48) shownUrl = "..." + shownUrl.substring(shownUrl.length() - 45);
  UiTheme::value(shownUrl, 34, 195, false);
  UiTheme::pill("EDIT", 438, 158, true);
  UiTheme::detail("Tap URL to edit", 34, 227);

  UiTheme::card(18, 268, 160, 92, true);
  UiTheme::value(apiMethod_, 67, 300, false);
  UiTheme::detail("Tap method", 49, 335);

  UiTheme::card(190, 268, 160, 92);
  UiTheme::value("BODY", 240, 300, false);
  UiTheme::detail(apiBody_.length() ? "JSON set" : "Empty", 235, 335);

  UiTheme::card(362, 268, 160, 92, true);
  UiTheme::value("SEND", 414, 300, false);

  UiTheme::card(18, 380, 504, 350);
  UiTheme::label("RESPONSE", 34, 398);
  if (apiResult_.status) {
    UiTheme::value(String("HTTP ") + apiResult_.status, 34, 433, false);
    String body = apiResult_.body;
    if (body.length() > 950) body = body.substring(0, 950);
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.setTextWrap(true, true);
    M5.Display.setCursor(34, 480);
    M5.Display.print(body.length() ? body : apiResult_.error);
    M5.Display.setTextWrap(false);
    UiTheme::resetFont();
  } else {
    UiTheme::value(apiResult_.error.length() ? apiResult_.error : String("No request yet"), 34, 433, false);
    UiTheme::detail("GET, POST and PUT are supported.", 34, 488);
  }

  bottomNav(3);
  commitPage();
}

void UiManager::showMqtt() {
  page_ = Page::Mqtt;
  preparePage();
  statusBar();
  UiTheme::title("MQTT Client", 18, 76);
  UiTheme::detail(networkTools_.mqttConnected() ? String("CONNECTED") : String("DISCONNECTED"), 20, 116);

  settingsRow(145, "BR", "Broker", mqttHost_.length() ? mqttHost_ : String("Tap to set host"));
  settingsRow(235, "TP", "Topic", mqttTopic_);
  settingsRow(325, "PL", "Payload", mqttPayload_.length() > 34 ? mqttPayload_.substring(0, 34) + "..." : mqttPayload_);

  UiTheme::card(18, 430, 160, 95, true);
  UiTheme::value(networkTools_.mqttConnected() ? "DROP" : "CONNECT", 41, 463, false);
  UiTheme::card(190, 430, 160, 95);
  UiTheme::value("SUB", 247, 463, false);
  UiTheme::card(362, 430, 160, 95);
  UiTheme::value("PUB", 416, 463, false);

  UiTheme::card(18, 545, 504, 190);
  UiTheme::label("LAST MESSAGE", 34, 563);
  UiTheme::value(networkTools_.mqttLastTopic().length() ? networkTools_.mqttLastTopic() : String("No message"), 34, 599, false);
  String p = networkTools_.mqttLastPayload();
  if (p.length() > 130) p = p.substring(0, 130) + "...";
  UiTheme::detail(p.length() ? p : mqttStatus_, 34, 642);
  UiTheme::detail(String("State: ") + networkTools_.mqttState() + "  /  Port 1883", 34, 700);

  bottomNav(3);
  commitPage();
}

void UiManager::showWakeOnLan() {
  page_ = Page::WakeOnLan;
  preparePage();
  statusBar();
  UiTheme::title("Wake-on-LAN", 18, 76);
  UiTheme::detail("Magic packet sender", 20, 116);

  UiTheme::card(18, 145, 504, 110, true);
  UiTheme::label("TARGET MAC", 34, 162);
  UiTheme::value(wolMac_.length() ? wolMac_ : String("00:00:00:00:00:00"), 34, 198, false);
  UiTheme::pill("EDIT", 438, 160, true);
  UiTheme::detail("Format AA:BB:CC:DD:EE:FF", 34, 232);

  UiTheme::card(18, 276, 504, 100, true);
  UiTheme::value("SEND MAGIC PACKET", 132, 310, false);

  UiTheme::card(18, 397, 504, 210);
  UiTheme::label("STATUS", 34, 415);
  UiTheme::value(wolStatus_.length() ? wolStatus_ : String("Ready"), 34, 456, false);
  UiTheme::detail("UDP broadcast on the current subnet, port 9.", 34, 510);
  UiTheme::detail("The target machine must have Wake-on-LAN enabled.", 34, 546);

  bottomNav(3);
  commitPage();
}

void UiManager::showTools() {
  page_ = Page::Tools;
  preparePage();
  statusBar();

  UiTheme::title("Tools", 18, 78);
  UiTheme::detail("Useful native utilities", 20, 119);

  settingRow(150, "Network Toolkit", "Ping / DNS / LAN / API / MQTT / Wake-on-LAN");
  settingRow(242, "Wi-Fi Analyzer", "Networks / RSSI / channel / connect");
  settingRow(334, "BLE Inspector", "Beacon scan / UUID / manufacturer / GATT");
  settingRow(426, "Web Reader", "HTTP / HTTPS reader mode");
  settingRow(518, "OTP Authenticator", "RFC6238 TOTP / 6 digits / 30 sec");
  settingRow(610, "System Monitor", "Memory / battery / network");

  UiTheme::card(18, 714, 504, 128);
  UiTheme::label("PAGE TRANSITIONS", 34, 730);
  UiTheme::detail("Every full page change performs a clean white refresh.", 34, 765);
  UiTheme::detail("The next page is then committed as one buffered frame.", 34, 798);
  UiTheme::detail("Live widgets continue to use fast regional updates.", 34, 828);

  bottomNav(3);
  commitPage();
}

void UiManager::showThermo() {
  page_ = Page::ComingSoon;
  comingTitle_ = "Termo";
  comingNav_ = 4;

  preparePage();
  statusBar();
  UiTheme::title("Termo", 18, 78);
  UiTheme::detail("Heating & thermal dashboard", 20, 119);

  UiTheme::card(18, 150, 504, 118, true);
  UiTheme::label("STATUS", 34, 166);
  UiTheme::pill("COMING SOON", 374, 160, false);
  UiTheme::value("Thermal control center", 34, 203, false);
  UiTheme::detail("No simulated temperatures are shown.", 34, 238);

  homeCard(18, 282, 246, 120, "ROOM", "--.- C", "SHT30 / remote sensor", false);
  homeCard(276, 282, 246, 120, "HUMIDITY", "-- %", "Local / remote sensor", false);
  homeCard(18, 416, 246, 120, "PUFFER TOP", "--.- C", "Planned telemetry", false);
  homeCard(276, 416, 246, 120, "PUFFER BOTTOM", "--.- C", "Planned telemetry", false);

  UiTheme::card(18, 550, 504, 154);
  UiTheme::label("PLANNED CONNECTIONS", 34, 568);
  UiTheme::detail("ESP32 / MQTT / HTTP / serial bridge", 34, 606);
  UiTheme::detail("Pellet boiler state + puffer resistances", 34, 640);
  UiTheme::detail("Automations will activate only after real sensor binding.", 34, 674);

  UiTheme::card(18, 718, 504, 124);
  UiTheme::label("SAFETY", 34, 736);
  UiTheme::detail("Control outputs remain disabled in this alpha.", 34, 774);
  UiTheme::detail("Placeholders show -- instead of invented measurements.", 34, 808);

  bottomNav(4);
  commitPage();
}

void UiManager::showSolar() {
  page_ = Page::ComingSoon;
  comingTitle_ = "Solar";
  comingNav_ = 4;

  preparePage();
  statusBar();
  UiTheme::title("Solar", 18, 78);
  UiTheme::detail("Energy dashboard", 20, 119);

  UiTheme::card(18, 150, 504, 118, true);
  UiTheme::label("STATUS", 34, 166);
  UiTheme::pill("COMING SOON", 374, 160, false);
  UiTheme::value("PV energy center", 34, 203, false);
  UiTheme::detail("Waiting for a real inverter data source.", 34, 238);

  homeCard(18, 282, 246, 120, "PV POWER", "--.- kW", "Planned live value", false);
  homeCard(276, 282, 246, 120, "HOME", "--.- kW", "Planned consumption", false);
  homeCard(18, 416, 246, 120, "BATTERY SOC", "-- %", "Planned inverter / BMS", false);
  homeCard(276, 416, 246, 120, "GRID", "--.- kW", "Import / export", false);

  UiTheme::card(18, 550, 504, 154);
  UiTheme::label("PLANNED DETAIL", 34, 568);
  UiTheme::detail("MPPT1 / MPPT2 / battery V+A / daily yield", 34, 606);
  UiTheme::detail("MQTT / HTTP / serial sources supported", 34, 640);
  UiTheme::detail("No fake production numbers are displayed.", 34, 674);

  UiTheme::card(18, 718, 504, 124);
  UiTheme::label("AUTOMATION READY", 34, 736);
  UiTheme::detail("Future surplus rules can drive puffer heating.", 34, 774);
  UiTheme::detail("Rules stay disabled until telemetry is connected.", 34, 808);

  bottomNav(4);
  commitPage();
}

void UiManager::showComingSoon(const String& titleText, int activeNav) {
  page_ = Page::ComingSoon;
  comingTitle_ = titleText;
  comingNav_ = activeNav;

  preparePage();
  statusBar();
  UiTheme::title(titleText, 18, 78);
  UiTheme::detail("PaperOS module", 20, 119);

  UiTheme::card(18, 170, 504, 340, true);
  UiTheme::value("Coming Soon", 150, 255, true);
  UiTheme::detail("This feature is not simulated.", 140, 325);
  UiTheme::detail("It will be enabled when its backend is real.", 92, 365);

  UiTheme::card(18, 530, 504, 180);
  UiTheme::label("AVAILABLE NOW", 34, 550);
  UiTheme::detail("Notes / Files / Wi-Fi / Settings / System / Tools", 34, 590);
  UiTheme::detail("Browser console also provides OTA and file management.", 34, 628);

  bottomNav(activeNav);
  commitPage();
}

void UiManager::handleBottomNav(int x) {
  int index = constrain(x / (UiTheme::ScreenW / 5), 0, 4);
  if (index == 0) showHome();
  else if (index == 1) showNotes();
  else if (index == 2) showFiles();
  else if (index == 3) showTools();
  else showApps();
}


void UiManager::classicHandlePointer(int x, int y, bool click) {
  classicMouseX_ = constrain(x, 0, UiTheme::ScreenW - 1);
  classicMouseY_ = constrain(y, 0, UiTheme::ScreenH - 1);
  if (!click) return;

  if (x >= 480 && y >= 12 && y <= 58) {
    if (page_ == Page::ClassicDesktop) showApps();
    else showClassicDesktop();
    return;
  }

  if (page_ == Page::ClassicDesktop) {
    struct Hit { int x; int y; int w; int h; int app; };
    const Hit hits[] = {
      {54,178,100,92,0}, {208,178,100,92,1}, {362,178,100,92,2},
      {54,330,100,92,3}, {208,330,100,92,4}, {362,330,100,92,5},
      {54,482,100,92,6}, {208,482,100,92,7}, {362,482,100,92,8}
    };
    for (const auto& h : hits) {
      if (x < h.x || x >= h.x+h.w || y < h.y || y >= h.y+h.h) continue;
      if (h.app == 0) {
        if (classicTerminalLines_.empty()) {
          classicTerminalPrint(String("PaperOS ROOT Terminal ") + VERSION);
          classicTerminalPrint("Type HELP for available commands.");
        }
        showClassicTerminal();
      } else if (h.app == 1) {
        showFiles("/");
      } else if (h.app == 2) {
        showSettings();
      } else if (h.app == 3) {
        if (!bluetoothActive_) {
          BLEDevice::init("PaperOS");
          bluetoothActive_ = true;
        }
        hidInput_.begin();
        showClassicHid();
      } else if (h.app == 4) {
        classicInitSki();
        showClassicSki();
      } else if (h.app == 5) {
        classicInitSolitaire();
        showClassicSolitaire();
      } else if (h.app == 6) {
        showNetworkTools();
      } else if (h.app == 7) {
        showGpioLab();
      } else {
        showApps();
      }
      return;
    }
  }

  if (page_ == Page::ClassicTerminal) {
    if (y >= 810 && y <= 890) {
      showKeyboard(InputTarget::ClassicTerminal, "PaperOS ROOT command", classicTerminalInput_, false);
    }
    return;
  }

  if (page_ == Page::ClassicHid) {
    if (x >= 380 && y >= 128 && y <= 210) {
      if (!bluetoothActive_) {
        BLEDevice::init("PaperOS");
        bluetoothActive_ = true;
      }
      hidInput_.begin();
      classicHidStatus_ = "Scanning BLE HID...";
      showClassicHid();
      hidInput_.scan(4);
      classicHidStatus_ = hidInput_.statusText();
      showClassicHid();
      return;
    }

    if (y >= 248 && y < 248 + 7 * 76) {
      int idx = (y - 248) / 76;
      if (idx >= 0 && idx < (int)hidInput_.devices().size()) {
        classicHidStatus_ = hidInput_.connect((size_t)idx) ? "Connected" : "Connection failed";
        showClassicHid();
      }
    }
    return;
  }

  if (page_ == Page::ClassicSki) {
    if (!skiRunning_) {
      classicInitSki();
    } else {
      skiPlayerX_ += x < UiTheme::ScreenW/2 ? -32 : 32;
      skiPlayerX_ = constrain(skiPlayerX_, 35, 505);
    }
    showClassicSki();
    return;
  }

  if (page_ == Page::ClassicSolitaire) {
    if (y >= 138 && y <= 225) {
      if (x >= 18 && x < 86) {
        if (solStock_.empty()) {
          while (!solWaste_.empty()) {
            ClassicCard c=solWaste_.back(); solWaste_.pop_back(); c.faceUp=false; solStock_.push_back(c);
          }
        } else {
          ClassicCard c=solStock_.back(); solStock_.pop_back(); c.faceUp=true; solWaste_.push_back(c);
        }
        solSelectedType_=-1; solSelectedPile_=-1;
      } else if (x >= 88 && x < 160) {
        if (!solWaste_.empty()) { solSelectedType_=0; solSelectedPile_=-1; }
      } else if (x >= 225) {
        int foundation=(x-230)/72;
        if (foundation>=0 && foundation<4) classicSolitaireMoveToFoundation(foundation);
      }
      showClassicSolitaire();
      return;
    }

    if (y >= 245 && y < 810) {
      int col=(x-15)/75;
      if (col>=0 && col<7) {
        if (solSelectedType_ >= 0) {
          if (!classicSolitaireMoveToTableau(col)) {
            solSelectedType_=-1; solSelectedPile_=-1;
          }
        } else if (!solTableau_[col].empty() && solTableau_[col].back().faceUp) {
          solSelectedType_=1; solSelectedPile_=col;
        }
        showClassicSolitaire();
      }
    }
    return;
  }
}

void UiManager::classicHandleKey(const HidKeyEvent& key) {
  if ((key.alt && key.keycode == 0x3D) || key.keycode == 0x29) {
    if (page_ == Page::ClassicDesktop) showApps();
    else showClassicDesktop();
    return;
  }

  if (page_ == Page::ClassicTerminal) {
    if (key.ascii == '\n') {
      String cmd=classicTerminalInput_;
      classicTerminalInput_="";
      classicTerminalExecute(cmd);
    } else if (key.ascii == '\b') {
      if (classicTerminalInput_.length()) classicTerminalInput_.remove(classicTerminalInput_.length()-1);
    } else if (key.ascii >= 32 && key.ascii < 127 && classicTerminalInput_.length() < 160) {
      classicTerminalInput_ += key.ascii;
    }
    showClassicTerminal();
    return;
  }

  if (page_ == Page::ClassicDesktop) {
    if (key.keycode == 0x4F) classicMouseX_ += 28;
    else if (key.keycode == 0x50) classicMouseX_ -= 28;
    else if (key.keycode == 0x51) classicMouseY_ += 28;
    else if (key.keycode == 0x52) classicMouseY_ -= 28;
    else if (key.keycode == 0x28) { classicHandlePointer(classicMouseX_, classicMouseY_, true); return; }
    classicMouseX_=constrain(classicMouseX_,0,539);
    classicMouseY_=constrain(classicMouseY_,0,959);
    showClassicDesktop();
    return;
  }

  if (page_ == Page::ClassicSki) {
    if (key.keycode == 0x50 || key.ascii == 'a' || key.ascii == 'A') skiPlayerX_ -= 36;
    else if (key.keycode == 0x4F || key.ascii == 'd' || key.ascii == 'D') skiPlayerX_ += 36;
    else if (key.ascii == 'n' || key.ascii == 'N' || key.keycode == 0x28) classicInitSki();
    skiPlayerX_=constrain(skiPlayerX_,35,505);
    showClassicSki();
    return;
  }

  if (page_ == Page::ClassicSolitaire) {
    if (key.ascii == 'n' || key.ascii == 'N') {
      classicInitSolitaire();
      showClassicSolitaire();
    }
  }
}

void UiManager::classicProcessHidInput() {
  if (!hidInput_.active()) return;
  const bool classic=isClassicPage() && page_!=Page::ClassicSplash;

  HidKeyEvent key;
  while (hidInput_.popKey(key)) {
    if (locked_) {
      if (key.keycode==0x28) armUnlock();
      continue;
    }

    if (classic) {
      classicHandleKey(key);
      continue;
    }

    if (key.keycode==0x52) handleWheelNavigate(-1);       // Up
    else if (key.keycode==0x51) handleWheelNavigate(1);  // Down
    else if (key.keycode==0x28) activateWheelFocus();    // Enter
    else if (key.keycode==0x29) navigateBack();          // Esc
  }

  HidMouseEvent mouse;
  bool moved=false;
  while (hidInput_.popMouse(mouse)) {
    const uint8_t oldButtons=classicMouseButtons_;
    const bool leftClick=(mouse.buttons&0x01U) && !(oldButtons&0x01U);
    const bool wheelClick=(mouse.buttons&0x04U) && !(oldButtons&0x04U);
    classicMouseButtons_=mouse.buttons;

    if (wheelClick) {
      if (locked_) armUnlock();
      else activateWheelFocus();
    }

    if (mouse.wheel) {
      handleWheelNavigate(mouse.wheel>0 ? -1 : 1);
    }

    if (locked_) continue;

    if (classic) {
      classicMouseX_=constrain(classicMouseX_+(int)mouse.dx*2,0,539);
      classicMouseY_=constrain(classicMouseY_+(int)mouse.dy*2,0,959);
      moved = moved || mouse.dx || mouse.dy;
      if (leftClick) {
        classicHandlePointer(classicMouseX_,classicMouseY_,true);
        moved=false;
      }
    }
  }

  if (classic && moved && millis()-classicLastPointerRefresh_>170) {
    classicLastPointerRefresh_=millis();
    renderPage(page_);
  }
}

void UiManager::openAppIndex(int index) {
  if (index == 0) showNotes();
  else if (index == 1) showFiles();
  else if (index == 2) showCalculator();
  else if (index == 3) showWiFi();
  else if (index == 4) showBluetooth();
  else if (index == 5) showBrowser();
  else if (index == 6) showBattery();
  else if (index == 7) showClock();
  else if (index == 8) showFocus();
  else if (index == 9) showOtp();
  else if (index == 10) showNetworkTools();
  else if (index == 11) showSettings();
  else if (index == 12) showSystem();
  else if (index == 13) showLabs();
  else if (index == 14) showPhoneLink();
  else if (index == 15) showPhone();
  else if (index == 16) showRecoveryHelp();
}

void UiManager::loop() {
  M5.update();
  networkTools_.loop();

  if (power_.consumeLockRequest() && !locked_) {
    showLockScreen();
  }

  classicProcessHidInput();

  if (locked_) {
    if (M5.BtnB.wasClicked()) {
      armUnlock();
      return;
    }

    auto lockTouch=touch_.poll();
    if (lockTouch.released && lockWakeArmed_ && lockTouch.gesture==TouchGesture::SwipeRight) {
      unlockToMenu();
    }
    return;
  }

  if (M5.BtnC.wasHold()) {
    power_.sleepNow();
    return;
  }
  if (M5.BtnA.wasClicked()) {
    power_.markActivity();
    wheelFocusVisible_=false;
    showHome();
    return;
  }
  if (M5.BtnB.wasClicked()) {
    power_.markActivity();
    wheelFocusVisible_=false;
    showApps();
    return;
  }

  auto e = touch_.poll();

  if (e.active) power_.markActivity();

  if (e.released && notificationPanelOpen_) {
    if (e.gesture==TouchGesture::SwipeDown) closeNotificationCenter();
    else if (e.gesture==TouchGesture::SwipeUp) {
      notificationScroll_=constrain(notificationScroll_+2,0,max(0,(int)phoneLink_.notificationCount()-5));
      showNotificationCenter();
    } else if (e.clicked) handleNotificationPanelTap(e.x,e.y);
    return;
  }

  if (e.released && quickPanelOpen_) {
    if (e.gesture == TouchGesture::SwipeUp) closeQuickPanel();
    else if (e.clicked) handleQuickPanelTap(e.x, e.y);
    return;
  }

  if (e.released && e.gesture==TouchGesture::SwipeUp && e.startY>820) {
    power_.markActivity();
    showNotificationCenter();
    return;
  }

  if (e.released && e.gesture == TouchGesture::SwipeDown && e.startY < 120) {
    power_.markActivity();
    showQuickPanel();
    return;
  }

  if (e.released && (e.gesture == TouchGesture::SwipeUp || e.gesture == TouchGesture::SwipeDown)) {
    power_.markActivity();
    wheelFocusVisible_=false;
    handleScrollGesture(e.gesture);
    return;
  }

  if (e.released && e.gesture == TouchGesture::SwipeRight && e.startX < 80) {
    power_.markActivity();
    wheelFocusVisible_=false;
    navigateBack();
    return;
  }

  if (e.clicked) {
    power_.markActivity();

    bool classicPage = page_ == Page::ClassicDesktop || page_ == Page::ClassicTerminal ||
                       page_ == Page::ClassicHid || page_ == Page::ClassicSki ||
                       page_ == Page::ClassicSolitaire || page_ == Page::ClassicSplash;

    if (classicPage && page_ != Page::ClassicSplash) {
      classicHandlePointer(e.x, e.y, true);
      return;
    }

    if (page_ != Page::Keyboard && e.y < UiTheme::StatusH && e.x < 42 && page_ != Page::Home) {
      navigateBack();
      return;
    }

    if (page_ != Page::Keyboard && e.y < UiTheme::StatusH && e.x >= 430) {
      showBattery();
      return;
    }

    if (page_ != Page::Keyboard && e.y >= UiTheme::NavY) {
      handleBottomNav(e.x);
      return;
    }

    if (page_ == Page::Keyboard) {
      handleKeyboardTap(e.x, e.y);
      return;
    }

    if (page_ == Page::Home) {
      if (e.y >= 145 && e.y < 273) {
        if (e.x >= 366) showBattery();
        else showWiFi();
      } else if (e.y >= 322 && e.y < 430) {
        if (e.x < 270) showNotes();
        else showFiles();
      } else if (e.y >= 442 && e.y < 550) {
        if (e.x < 270) showSettings();
        else showSystem();
      } else if (e.y >= 604 && e.y < 706) {
        if (e.x < 270) showThermo();
        else showSolar();
      } else if (e.y >= 720 && e.y < 850) {
        showSettings();
      }
      return;
    }

    if (page_ == Page::Apps) {
      const int startY = 154;
      const int pitchY = 116;
      if (e.y >= startY && e.y < startY + 5 * pitchY && e.x >= 16) {
        int row = (e.y - startY) / pitchY;
        int col = (e.x - 16) / 128;
        int localX = (e.x - 16) % 128;
        int localY = (e.y - startY) % pitchY;
        if (col >= 0 && col < 4 && localX < 120 && localY < 108) openAppIndex(row * 4 + col);
      } else if (e.y >= 746 && e.y < 850) {
        showClassicSplash();
      }
      return;
    }

    if (page_ == Page::Notes) {
      if (e.y >= 150 && e.y < 150 + 8 * 82) {
        size_t idx = (e.y - 150) / 82;
        if (idx < noteIds_.size()) showNote(idx);
      }
      return;
    }

    if (page_ == Page::Files) {
      if (e.y >= 220 && e.y < 274 && e.x >= 390) {
        showStorageTools();
        return;
      }
      if (e.y >= 137 && e.y < 209) {
        fileSelectionMode_ = false;
        selectedFilePath_ = "";
        fileStatus_ = "";
        showFiles(e.x < 270 ? "/PaperOS" : "/");
        return;
      }

      if (e.y >= 284 && e.y < 284 + 6 * 72) {
        size_t local = static_cast<size_t>((e.y - 284) / 72);
        size_t idx = static_cast<size_t>(fileScroll_) + local;
        if (idx < fileEntries_.size()) {
          const FileEntry& entry = fileEntries_[idx];
          if (entry.name == "..") {
            if (!fileSelectionMode_) showFiles(parentPath(currentFilePath_));
          } else {
            String full = joinPath(currentFilePath_, entry.name);
            if (fileSelectionMode_) {
              selectedFilePath_ = full;
              selectedFileName_ = entry.name;
              selectedFileDirectory_ = entry.directory;
              fileStatus_ = String("Selected: ") + entry.name;
              showFiles(currentFilePath_);
            } else if (entry.directory) {
              showFiles(full);
            } else {
              showFilePreview(full, entry.name, entry.size);
            }
          }
        }
        return;
      }

      if (e.y >= 744 && e.y < 820) {
        if (e.x < 139) {
          fileSelectionMode_ = !fileSelectionMode_;
          if (!fileSelectionMode_) {
            selectedFilePath_ = "";
            selectedFileName_ = "";
          }
          fileStatus_ = fileSelectionMode_ ? "Tap a file or folder to select it" : "";
        } else if (e.x < 265) {
          if (selectedFilePath_.length()) {
            fileClipboardPath_ = selectedFilePath_;
            fileClipboardName_ = selectedFileName_;
            fileClipboardCut_ = false;
            fileSelectionMode_ = false;
            selectedFilePath_ = "";
            fileStatus_ = String("Copied to clipboard: ") + fileClipboardName_;
          } else fileStatus_ = "Select a file or folder first";
        } else if (e.x < 391) {
          if (selectedFilePath_.length()) {
            fileClipboardPath_ = selectedFilePath_;
            fileClipboardName_ = selectedFileName_;
            fileClipboardCut_ = true;
            fileSelectionMode_ = false;
            selectedFilePath_ = "";
            fileStatus_ = String("Cut to clipboard: ") + fileClipboardName_;
          } else fileStatus_ = "Select a file or folder first";
        } else {
          if (!fileClipboardPath_.length()) {
            fileStatus_ = "Clipboard is empty";
          } else {
            String target = storage_.uniqueDestination(currentFilePath_, fileClipboardName_);
            showAppLoading(fileClipboardCut_ ? "Moving" : "Copying", fileClipboardName_, 55);
            bool ok = fileClipboardCut_
              ? storage_.movePath(fileClipboardPath_, target)
              : storage_.copyPath(fileClipboardPath_, target);
            if (ok) {
              fileStatus_ = String(fileClipboardCut_ ? "Moved: " : "Pasted: ") + fileClipboardName_;
              if (fileClipboardCut_) {
                fileClipboardPath_ = "";
                fileClipboardName_ = "";
                fileClipboardCut_ = false;
              }
              fileEntries_.clear();
            } else {
              fileStatus_ = "Paste failed";
            }
          }
        }
        showFiles(currentFilePath_);
        return;
      }
      return;
    }

    if (page_ == Page::Phone) {
      if (e.y >= 145 && e.y < 232) {
        if (phoneLink_.active()) {
          phoneLink_.stop();
          phoneAppStatus_ = "Phone Link stopped";
        } else {
          bool ok = phoneLink_.begin();
          bluetoothActive_ = bluetoothActive_ || ok;
          phoneAppStatus_ = ok ? "Pairing mode started; connect the iPhone" : "Bluetooth could not start";
        }
        showPhone();
      } else if (e.y >= 244 && e.y < 346) {
        showKeyboard(InputTarget::PhoneNumber, "Phone number", phoneNumberDraft_, false);
      } else if (e.y >= 358 && e.y < 444) {
        String number = normalizedPhoneNumber(phoneNumberDraft_);
        if (!number.length()) {
          phoneAppStatus_ = "Enter a phone number first";
        } else if (e.x < 270) {
          phoneAppStatus_ = phoneLink_.sendCommand(String("call:") + number)
            ? "Call request sent; approve it on the iPhone"
            : "No companion connected; install/open the iPhone companion";
        } else {
          showKeyboard(InputTarget::PhoneMessage, "Message to " + number, phoneMessageDraft_, false);
          return;
        }
        showPhone();
      } else if (e.y >= 458 && e.y < 490 && e.x >= 420) {
        phoneLink_.clearNotifications();
        phoneAppStatus_ = "Notification list cleared";
        showPhone();
      }
      return;
    }

    if (page_ == Page::RecoveryHelp) {
      if (e.y >= 145 && e.y < 252) {
        showAppLoading("SD Recovery", "Scanning deleted FAT32 entries (read only)...", 45);
        storage_.scanDeletedFiles();
        fileScroll_ = 0;
        showRecoveryHelp();
      } else if (e.y >= 324 && e.y < 668 && storage_.recoveredFileCount()) {
        size_t row = (e.y - 324) / 86;
        size_t index = static_cast<size_t>(fileScroll_) + row;
        if (index < storage_.recoveredFileCount()) showRecoveryPreview(index);
      }
      return;
    }

    if (page_ == Page::Tools) {
      if (e.y >= 150 && e.y < 238) showNetworkTools();
      else if (e.y >= 242 && e.y < 330) showWiFi();
      else if (e.y >= 334 && e.y < 422) showBluetooth();
      else if (e.y >= 426 && e.y < 514) showBrowser();
      else if (e.y >= 518 && e.y < 606) showOtp();
      else if (e.y >= 610 && e.y < 698) showSystem();
      return;
    }

    if (page_ == Page::NetworkTools) {
      if (e.y >= 150 && e.y < 228) showPingTool();
      else if (e.y >= 240 && e.y < 318) showDnsTool();
      else if (e.y >= 330 && e.y < 408) showLanScan();
      else if (e.y >= 420 && e.y < 498) showApiTester();
      else if (e.y >= 510 && e.y < 588) showMqtt();
      else if (e.y >= 600 && e.y < 678) showWakeOnLan();
      return;
    }

    if (page_ == Page::PingTool) {
      if (e.y >= 145 && e.y < 255) {
        showKeyboard(InputTarget::PingHost, "Ping target", pingHost_, false);
      } else if (e.y >= 276 && e.y < 376) {
        showAppLoading("Ping", String("Testing ") + pingHost_, 55);
        PingResult r = networkTools_.ping(pingHost_, 2);
        if (r.ok) pingResultText_ = r.ip.toString() + "  /  " + String(r.averageMs, 1) + " ms";
        else pingResultText_ = r.error;
        showPingTool();
      }
      return;
    }

    if (page_ == Page::DnsTool) {
      if (e.y >= 145 && e.y < 255) {
        showKeyboard(InputTarget::DnsHost, "Hostname", dnsHost_, false);
      } else if (e.y >= 276 && e.y < 376) {
        IPAddress ip;
        dnsResultText_ = networkTools_.dnsLookup(dnsHost_, ip) ? ip.toString() : String("Lookup failed");
        M5.Display.fillRect(18, 397, 504, 220, TFT_WHITE);
        UiTheme::card(18, 397, 504, 220);
        UiTheme::label("RESULT", 34, 415);
        UiTheme::value(dnsResultText_, 34, 456, false);
        UiTheme::detail("Uses the DNS server supplied by the active Wi-Fi network.", 34, 515);
        display_.partialRefresh(18, 397, 504, 220);
      }
      return;
    }

    if (page_ == Page::LanScan) {
      if (e.y >= 145 && e.y < 237) {
        if (e.x < 270) {
          uint8_t startHost = lanScanSegment_ == 0 ? 1 : static_cast<uint8_t>(lanScanSegment_ * 64 + 1);
          uint8_t endHost = lanScanSegment_ == 3 ? 254 : static_cast<uint8_t>(min(254, static_cast<int>(startHost) + 63));
          showAppLoading("LAN Discovery", String("Scanning .") + startHost + "-." + endHost, 45);
          lanHosts_ = networkTools_.quickLanScan(startHost, endHost);
          showLanScan();
        } else {
          lanScanSegment_ = (lanScanSegment_ + 1) % 4;
          lanHosts_.clear();
          showLanScan();
        }
      }
      return;
    }

    if (page_ == Page::ApiTester) {
      if (e.y >= 145 && e.y < 250) {
        showKeyboard(InputTarget::ApiUrl, "HTTP / API URL", apiUrl_, false);
      } else if (e.y >= 268 && e.y < 360) {
        if (e.x < 178) {
          if (apiMethod_ == "GET") apiMethod_ = "POST";
          else if (apiMethod_ == "POST") apiMethod_ = "PUT";
          else apiMethod_ = "GET";
          M5.Display.fillRect(18, 268, 160, 92, TFT_WHITE);
          UiTheme::card(18, 268, 160, 92, true);
          UiTheme::value(apiMethod_, 67, 300, false);
          UiTheme::detail("Tap method", 49, 335);
          display_.partialRefresh(18, 268, 160, 92);
        } else if (e.x < 350) {
          showKeyboard(InputTarget::ApiBody, "JSON / request body", apiBody_, false);
        } else {
          showAppLoading("API Tester", String(apiMethod_) + " request", 60);
          apiResult_ = networkTools_.httpRequest(apiMethod_, apiUrl_, apiBody_);
          showApiTester();
        }
      }
      return;
    }

    if (page_ == Page::Mqtt) {
      if (e.y >= 145 && e.y < 223) {
        showKeyboard(InputTarget::MqttHost, "MQTT broker host", mqttHost_, false);
      } else if (e.y >= 235 && e.y < 313) {
        showKeyboard(InputTarget::MqttTopic, "MQTT topic", mqttTopic_, false);
      } else if (e.y >= 325 && e.y < 403) {
        showKeyboard(InputTarget::MqttPayload, "MQTT payload", mqttPayload_, false);
      } else if (e.y >= 430 && e.y < 525) {
        if (e.x < 178) {
          if (networkTools_.mqttConnected()) {
            networkTools_.mqttDisconnect();
            mqttStatus_ = "Disconnected";
          } else {
            mqttStatus_ = networkTools_.mqttConnect(mqttHost_, 1883) ? "Connected" : String("Connect failed / state ") + networkTools_.mqttState();
          }
        } else if (e.x < 350) {
          mqttStatus_ = networkTools_.mqttSubscribe(mqttTopic_) ? String("Subscribed: ") + mqttTopic_ : String("Subscribe failed");
        } else {
          mqttStatus_ = networkTools_.mqttPublish(mqttTopic_, mqttPayload_) ? String("Published") : String("Publish failed");
        }
        showMqtt();
      }
      return;
    }

    if (page_ == Page::WakeOnLan) {
      if (e.y >= 145 && e.y < 255) {
        showKeyboard(InputTarget::WolMac, "Wake-on-LAN MAC", wolMac_, false);
      } else if (e.y >= 276 && e.y < 376) {
        wolStatus_ = networkTools_.wakeOnLan(wolMac_) ? "Magic packet sent" : "Send failed / invalid MAC";
        M5.Display.fillRect(18, 397, 504, 210, TFT_WHITE);
        UiTheme::card(18, 397, 504, 210);
        UiTheme::label("STATUS", 34, 415);
        UiTheme::value(wolStatus_, 34, 456, false);
        UiTheme::detail("UDP broadcast on the current subnet, port 9.", 34, 510);
        UiTheme::detail("The target machine must have Wake-on-LAN enabled.", 34, 546);
        display_.partialRefresh(18, 397, 504, 210);
      }
      return;
    }

    if (page_ == Page::Settings) {
      if (e.y >= 260 && e.y < 752) {
        int local = (e.y - 260) / 82;
        int idx = settingsScroll_ + local;
        if (idx == 0) showGeneral();
        else if (idx == 1) {
          uint8_t next = (static_cast<uint8_t>(config_.get().uiStyle) + 1) % 4;
          config_.edit().uiStyle = static_cast<UiStyle>(next);
          config_.save();
          UiTheme::setStyle(next);
          showSettings();
        }
        else if (idx == 2) showWiFi();
        else if (idx == 3) showBluetooth();
        else if (idx == 4) showDateTime();
        else if (idx == 5) showBattery();
        else if (idx == 6) {
          uint8_t next = (display_.profile() + 1) % 3;
          display_.setProfile(next);
          config_.edit().displayProfile = static_cast<DisplayProfile>(next);
          config_.save();
          showSettings();
        }
        else if (idx == 7 || idx == 8) showStorageTools();
        else if (idx == 9) showLabs();
      }
      return;
    }

    if (page_ == Page::DateTime) {
      if (e.y >= 316 && e.y < 428) {
        if (e.x < 270) {
          timeStatus_ = wifi_.syncClockNow() ? "NTP synchronization completed" : "Connect Wi-Fi first";
          showDateTime();
        } else {
          auto dt = M5.Rtc.getDateTime();
          char b[24];
          snprintf(b, sizeof(b), "%04d-%02d-%02d %02d:%02d",
                   dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes);
          showKeyboard(InputTarget::ManualTime, "YYYY-MM-DD HH:MM", String(b), false);
        }
      }
      return;
    }

    if (page_ == Page::StorageTools) {
      if (e.y >= 266 && e.y < 344) {
        storageStatus_ = config_.exportBackupToSd() ? "Settings backup written to SD" : "Backup failed";
        showStorageTools();
      } else if (e.y >= 348 && e.y < 426) {
        bool ok = config_.restoreBackupFromSd();
        if (ok) {
          display_.setProfile(static_cast<uint8_t>(config_.get().displayProfile));
          UiTheme::setStyle(static_cast<uint8_t>(config_.get().uiStyle));
        }
        storageStatus_ = ok ? "Settings restored from SD" : "Restore failed / invalid backup";
        showStorageTools();
      } else if (e.y >= 430 && e.y < 508) {
        storageStatus_ = config_.exportWifiTextToSd() ? "Wi-Fi text file exported" : "Wi-Fi export failed";
        showStorageTools();
      } else if (e.y >= 512 && e.y < 590) {
        storageStatus_ = config_.importWifiTextFromSd() ? "Wi-Fi networks imported" : "Wi-Fi import failed";
        showStorageTools();
      } else if (e.y >= 594 && e.y < 672) {
        formatConfirmArmed_ = false;
        pendingFormatType_ = "";
        showStorageFormat();
      } else if (e.y >= 676 && e.y < 754) {
        showRecoveryHelp();
      }
      return;
    }

    if (page_ == Page::WiFi) {
      if (e.y >= 145 && e.y < 263) {
        wifiScan_.clear();
        wifiScroll_ = 0;
        wifiScanRunning_ = false;
        wifiStatusMessage_ = "";
        showWiFi();
        return;
      }
      if (e.y >= 792 && e.y < 850 && e.x >= 400) {
        showWifiAudit();
        return;
      }
      if (e.y >= 310 && e.y < 776) {
        int local = (e.y - 310) / 88;
        int idx = wifiScroll_ + local;
        if (local >= 0 && local < 5 && idx >= 0 && idx < static_cast<int>(wifiScan_.size())) {
          const WiFiScanEntry entry = wifiScan_[idx];
          if (!entry.ssid.length()) return;
          selectedWifiSsid_ = entry.ssid;
          if (entry.encrypted) {
            showKeyboard(InputTarget::WiFiPassword, String("Password for ") + entry.ssid, "", true);
          } else {
            showAppLoading("Wi-Fi", String("Connecting to ") + entry.ssid, 65);
            bool ok = wifi_.connect(entry.ssid, "", true);
            wifiStatusMessage_ = ok ? String("Connected to ") + entry.ssid : String("Connection failed: ") + entry.ssid;
            showWiFi();
          }
        }
      }
      return;
    }

    if (page_ == Page::Bluetooth) {
      if (e.y >= 145 && e.y < 257) {
        bleScan_.clear();
        bleScroll_ = 0;

        M5.Display.fillRect(18, 306, 504, 470, TFT_WHITE);
        UiTheme::card(18, 306, 504, 470);
        UiTheme::value("Scanning BLE...", 44, 350, false);
        UiTheme::detail("Collecting advertising data and company IDs.", 44, 392);
        M5.Display.drawRoundRect(44, 442, 438, 18, 9, TFT_BLACK);
        M5.Display.fillRoundRect(47, 445, 292, 12, 6, TFT_BLACK);
        display_.partialRefresh(18, 306, 504, 470);

        BLEScan* scanner = BLEDevice::getScan();
        scanner->setActiveScan(true);
        BLEScanResults results = scanner->start(3, false);
        int total = results.getCount();

        for (int i = 0; i < total; ++i) {
          BLEAdvertisedDevice d = results.getDevice(i);
          BleScanEntry entry;
          entry.address = String(d.getAddress().toString().c_str());
          entry.name = d.haveName() ? String(d.getName().c_str()) : entry.address;
          entry.rssi = d.getRSSI();
          entry.hasTxPower = d.haveTXPower();
          entry.txPower = entry.hasTxPower ? d.getTXPower() : 0;
          entry.serviceUuid = d.haveServiceUUID() ? String(d.getServiceUUID().toString().c_str()) : String();

          if (d.haveManufacturerData()) {
            std::string raw = d.getManufacturerData();
            String rawArduino;
            rawArduino.reserve(raw.length());
            for (size_t b = 0; b < raw.length(); ++b) rawArduino += static_cast<char>(raw[b]);
            entry.manufacturerHex = bleManufacturerHex(rawArduino);
            entry.manufacturerName = bleManufacturerName(rawArduino);

            if (raw.length() >= 4 &&
                static_cast<uint8_t>(raw[0]) == 0x4C &&
                static_cast<uint8_t>(raw[1]) == 0x00 &&
                static_cast<uint8_t>(raw[2]) == 0x02 &&
                static_cast<uint8_t>(raw[3]) == 0x15) {
              entry.beaconType = "iBeacon";
            }
          }

          String uuidLower = entry.serviceUuid;
          uuidLower.toLowerCase();
          if (uuidLower.indexOf("feaa") >= 0) entry.beaconType = "Eddystone";
          if (!entry.beaconType.length() && entry.manufacturerHex.length()) entry.beaconType = "Manufacturer beacon";
          bleScan_.push_back(entry);
        }

        scanner->clearResults();
        showBluetooth();
        return;
      }

      if (e.y >= 306 && e.y < 776) {
        int local = (e.y - 306) / 88;
        int idx = bleScroll_ + local;
        if (local >= 0 && local < 5 && idx >= 0 && idx < static_cast<int>(bleScan_.size())) {
          showBleDetail(static_cast<size_t>(idx));
        }
      }
      return;
    }

    if (page_ == Page::BleDetail) {
      if (e.y >= 574 && e.y < 666) {
        readBleGatt(selectedBleIndex_);
        showBleDetail(selectedBleIndex_);
      }
      return;
    }

    if (page_ == Page::StorageFormat) {
      if (e.y >= 300 && e.y < 416) {
        String type;
        if (e.x < 178) type = "AUTO";
        else if (e.x < 350) type = "FAT";
        else type = "exFAT";

        if (!formatConfirmArmed_ || pendingFormatType_ != type) {
          pendingFormatType_ = type;
          formatConfirmArmed_ = true;
          showStorageFormat();
        } else {
          showAppLoading("Format microSD", String("Formatting ") + type + "...", 55);
          bool ok = storage_.formatCard(type);
          storageStatus_ = ok ? String("microSD formatted as ") + type + " and PaperOS layout recreated"
                              : String("Format failed - card may need desktop recovery");
          formatConfirmArmed_ = false;
          pendingFormatType_ = "";
          showStorageTools();
        }
      }
      return;
    }

    if (page_ == Page::RecoveryHelp) {
      if (e.y >= 145 && e.y < 252) {
        showAppLoading("SD Recovery", "Scanning deleted FAT32 entries (read only)...", 45);
        storage_.scanDeletedFiles();
        fileScroll_ = 0;
        showRecoveryHelp();
      } else if (e.y >= 324 && e.y < 668 && storage_.recoveredFileCount()) {
        size_t row = (e.y - 324) / 86;
        size_t index = static_cast<size_t>(fileScroll_) + row;
        if (index < storage_.recoveredFileCount()) showRecoveryPreview(index);
      }
      return;
    }

    if (page_ == Page::RecoveryPreview) {
      if (e.y >= 744 && e.y < 832) {
        recoveryExportConfirmed_ = false;
        page_ = Page::RecoveryConfirm;
        showRecoveryConfirm();
      }
      return;
    }

    if (page_ == Page::RecoveryConfirm) {
      if (e.y >= 744 && e.y < 832) {
        if (recoveryExportConfirmed_) {
          if (e.x < 270) showRecoveryPreview(selectedRecoveryIndex_);
          else showRecoveryHelp();
        } else if (e.x < 270) {
          recoveryExportConfirmed_ = true;
          showRecoveryConfirm();
        } else showRecoveryPreview(selectedRecoveryIndex_);
      }
      return;
    }

    if (page_ == Page::PhoneLink) {
      if (e.y >= 145 && e.y < 257) {
        if (phoneLink_.active()) {
          phoneLink_.stop();
          phoneLinkStatus_ = "Phone Link stopped";
        } else {
          bool ok = phoneLink_.begin();
          bluetoothActive_ = bluetoothActive_ || ok;
          phoneLinkStatus_ = ok ? "Pairing mode started" : "Bluetooth/ANCS start failed";
        }
        showPhoneLink();
      } else if (e.y >= 680 && e.y < 756) {
        const PhoneNotification* n = phoneLink_.notification(static_cast<size_t>(phoneScroll_));
        if (e.x < 178) {
          phoneLinkStatus_ = (n && n->positiveAction && phoneLink_.performNotificationAction(n->uid, true))
            ? "Positive ANCS action sent" : "Positive action unavailable";
        } else if (e.x < 350) {
          phoneLinkStatus_ = (n && n->negativeAction && phoneLink_.performNotificationAction(n->uid, false))
            ? "Negative ANCS action sent" : "Negative action unavailable";
        } else {
          phoneLink_.clearNotifications();
          phoneScroll_ = 0;
          phoneLinkStatus_ = "Notification list cleared";
        }
        showPhoneLink();
      } else if (e.y >= 772 && e.y < 842) {
        if (e.x < 178) {
          phoneLinkStatus_ = phoneLink_.sendCommand("camera:shutter") ? "Companion camera command sent" : "Companion bridge inactive";
          showPhoneLink();
        } else if (e.x < 350) {
          phoneLinkStatus_ = phoneLink_.sendCommand("media:playpause") ? "Companion media command sent" : "Companion bridge inactive";
          showPhoneLink();
        } else {
          showKeyboard(InputTarget::PhoneCommand, "Companion command", phoneCommandDraft_, false);
        }
      }
      return;
    }

    if (page_ == Page::NfcLab) {
      if (e.y >= 145 && e.y < 273) {
        bool ok = nfcService_.begin();
        nfcStatus_ = ok ? nfcService_.firmwareText() : "PN532 not detected - check HSU wiring/mode";
        showNfcLab();
      } else if (e.y >= 294 && e.y < 400) {
        if (e.x < 270) {
          bool ok = nfcService_.begin();
          nfcStatus_ = ok ? nfcService_.firmwareText() : "PN532 not detected";
        } else {
          showAppLoading("NFC", "Waiting for ISO14443A tag...", 55);
          lastNfcTag_ = nfcService_.scan(1800);
          nfcStatus_ = lastNfcTag_.found ? String("Tag detected: ") + lastNfcTag_.uid : "No tag detected";
        }
        showNfcLab();
      }
      return;
    }

    if (page_ == Page::GpioLab) {
      if (e.y >= 145 && e.y < 145 + 6 * 94) {
        static const int pins[6] = {25, 32, 26, 33, 18, 19};
        int idx = (e.y - 145) / 94;
        if (idx >= 0 && idx < 6) {
          gpioModeState_[idx] = (gpioModeState_[idx] + 1) % 3;
          if (gpioModeState_[idx] == 0) {
            pinMode(pins[idx], INPUT);
            gpioOutputLevel_[idx] = false;
          } else {
            pinMode(pins[idx], OUTPUT);
            gpioOutputLevel_[idx] = gpioModeState_[idx] == 2;
            digitalWrite(pins[idx], gpioOutputLevel_[idx] ? HIGH : LOW);
          }
          gpioStatus_ = String("GPIO") + pins[idx] + " updated";
          showGpioLab();
        }
      }
      return;
    }

    if (page_ == Page::Browser) {
      if (e.y >= 145 && e.y < 233) {
        showKeyboard(InputTarget::BrowserSearch, "Search the web", browserSearchQuery_, false);
        return;
      }
      if (e.y >= 246 && e.y < 338) {
        showKeyboard(InputTarget::BrowserUrl, "Web address", browserUrl_.length() ? browserUrl_ : String("https://"), false);
        return;
      }
      if (browserPage_.ok && e.y >= 721 && e.y < 835) {
        int local = (e.y - 721) / 57;
        int ly = (e.y - 721) % 57;
        int idx = browserLinkScroll_ + local;
        if (local >= 0 && local < 2 && idx >= 0 && idx < static_cast<int>(browserPage_.links.size()) && ly < 50) {
          fetchBrowserUrl(browserPage_.links[idx]);
        }
      }
      return;
    }

    if (page_ == Page::Otp) {
      if (e.y >= 414 && e.y < 526) {
        if (e.x < 270) {
          showKeyboard(InputTarget::OtpSecret, "Base32 OTP secret", otpSecret_, false);
        } else {
          otpSecret_ = "";
          otpCode_ = "";
          lastOtpStep_ = 0;
          drawOtpCode();
          display_.partialRefresh(18, 145, 504, 250);
        }
      }
      return;
    }

    if (page_ == Page::Focus) {
      if (e.y >= 515 && e.y < 627) {
        if (e.x < 270) {
          if (focusRunning_) {
            int32_t diff = static_cast<int32_t>(focusEndMs_ - millis());
            focusRemainingSec_ = diff > 0 ? static_cast<uint32_t>(diff) / 1000UL : 0;
            focusRunning_ = false;
          } else {
            if (focusRemainingSec_ == 0) focusRemainingSec_ = 25UL * 60UL;
            focusEndMs_ = millis() + focusRemainingSec_ * 1000UL;
            focusRunning_ = true;
          }
        } else {
          focusRunning_ = false;
          focusRemainingSec_ = 25UL * 60UL;
        }

        drawFocusLiveArea();
        M5.Display.fillRect(18, 515, 504, 112, TFT_WHITE);
        UiTheme::card(18, 515, 246, 112, true);
        UiTheme::value(focusRunning_ ? "PAUSE" : "START", 79, 553, false);
        UiTheme::detail("Tap", 121, 592);
        UiTheme::card(276, 515, 246, 112);
        UiTheme::value("RESET", 345, 553, false);
        UiTheme::detail("25:00", 379, 592);
        display_.partialRefresh(18, 145, 504, 482);
      }
      return;
    }

    if (page_ == Page::Fun) {
      if (e.y >= 400 && e.y < 518) {
        if (e.x < 178) funResult_ = String("D6: ") + String((esp_random() % 6U) + 1U);
        else if (e.x < 350) funResult_ = (esp_random() & 1U) ? "HEADS" : "TAILS";
        else funResult_ = String("Number: ") + String((esp_random() % 100U) + 1U);
        drawFunResult();
        display_.partialRefresh(18, 145, 504, 230);
      }
      return;
    }

    if (page_ == Page::Labs) {
      if (e.y >= 252 && e.y < 330) showHidLab();
      else if (e.y >= 342 && e.y < 420) showBluetooth();
      else if (e.y >= 432 && e.y < 510) showNfcLab();
      else if (e.y >= 522 && e.y < 600) showGpioLab();
      else if (e.y >= 612 && e.y < 690) {
        display_.cleanRefresh();
        showLabs();
      }
      else if (e.y >= 702 && e.y < 780) showSystem();
      return;
    }

    if (page_ == Page::Calculator) {
      const int sx = 18, sy = 266, kw = 117, kh = 92, gap = 12;
      if (e.x >= sx && e.y >= sy) {
        int col = (e.x - sx) / (kw + gap);
        int row = (e.y - sy) / (kh + gap);
        int lx = (e.x - sx) % (kw + gap);
        int ly = (e.y - sy) % (kh + gap);
        if (col >= 0 && col < 4 && row >= 0 && row < 5 && lx < kw && ly < kh) {
          const char* keys[20] = {
            "C", "+/-", "%", "/",
            "7", "8", "9", "*",
            "4", "5", "6", "-",
            "1", "2", "3", "+",
            "0", ".", "=", "BS"
          };
          calcKey(keys[row * 4 + col]);

          M5.Display.fillRect(18, 145, 504, 100, TFT_WHITE);
          UiTheme::card(18, 145, 504, 100, true);
          M5.Display.setFont(&fonts::FreeSansBold24pt7b);
          M5.Display.setTextDatum(middle_right);
          M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
          M5.Display.drawString(calcDisplay_, 500, 195);
          UiTheme::resetFont();
          display_.partialRefresh(18, 145, 504, 100);
        }
      }
      return;
    }
  }

  if (focusRunning_) {
    int32_t diff = static_cast<int32_t>(focusEndMs_ - millis());
    if (diff <= 0) {
      focusRunning_ = false;
      focusRemainingSec_ = 0;
      if (page_ == Page::Focus) {
        drawFocusLiveArea();
        display_.partialRefresh(18, 145, 504, 350);
      }
    }
  }

  if (page_ == Page::WiFi && wifiScanRunning_) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      wifiScan_.clear();
      wifiScroll_ = 0;
      int total = min(n, 40);
      for (int i = 0; i < total; ++i) {
        WiFiScanEntry entry;
        entry.ssid = WiFi.SSID(i);
        entry.rssi = WiFi.RSSI(i);
        entry.channel = WiFi.channel(i);
        entry.authMode = static_cast<int>(WiFi.encryptionType(i));
        entry.encrypted = entry.authMode != 0;
        wifiScan_.push_back(entry);
      }
      wifiScanRunning_ = false;
      wifiStatusMessage_ = total ? String("Scan complete: ") + total + " networks" : String("No networks found");
      WiFi.scanDelete();
      showWiFi();
    }
  }

  if (page_ == Page::Battery && millis() - lastBatteryUiRefresh_ > 30000UL) {
    lastBatteryUiRefresh_ = millis();
    M5.Display.fillRect(18, 145, 504, 325, TFT_WHITE);
    drawBatteryLiveArea();
    display_.partialRefresh(18, 145, 504, 325);
  }

  if (page_ == Page::Clock && millis() - lastClockPageRefresh_ > 60000UL) {
    lastClockPageRefresh_ = millis();
    drawClockLiveArea();
    display_.partialRefresh(18, 145, 504, 410);
  }

  if (page_ == Page::Focus && focusRunning_ && millis() - lastFocusUiRefresh_ > 60000UL) {
    lastFocusUiRefresh_ = millis();
    drawFocusLiveArea();
    display_.partialRefresh(18, 145, 504, 350);
  }

  if (page_ == Page::ClassicSki && skiRunning_ && millis() - classicLastGameTick_ > 650UL) {
    classicLastGameTick_ = millis();
    classicStepSki();
    showClassicSki();
  }

  if (page_ == Page::Otp && otpSecret_.length()) {
    time_t now = time(nullptr);
    if (now >= 1700000000) {
      uint64_t step = static_cast<uint64_t>(now) / 30ULL;
      if (step != lastOtpStep_) {
        drawOtpCode();
        display_.partialRefresh(18, 145, 504, 250);
      }
    }
  }

  if (millis() - lastClock_ > 60000UL) {
    lastClock_ = millis();
    statusBar();
    display_.partialRefresh(0, 0, M5.Display.width(), UiTheme::StatusH);
  }

  if (millis() - lastDeepClean_ > 25UL * 60UL * 1000UL && page_ == Page::Home) {
    showHome(true);
  }
}

}
