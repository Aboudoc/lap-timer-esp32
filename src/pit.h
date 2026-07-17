#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include "config.h"
#include "storage.h"

// Callbacks into main.cpp, so web actions update the live timer state.
struct PitActions {
  void (*selectTrack)(uint8_t id);
  void (*renameTrack)(uint8_t id, const char* name);
  void (*deleteTrack)(uint8_t id);
  const char* (*activeTrackName)();
  void (*statusJson)(char* buf, size_t n);  // live state for /api/status
  void (*exitPit)();                        // clear the flag and reboot
  void (*traceCsv)(int which, Print& out);  // 0 = best lap, 1 = last lap
};

// Pit mode: WiFi hotspot + embedded web app (installable on the phone's home
// screen). Live dashboard, session browser with charts, lap log download,
// track manager and OTA firmware updates — no cable, no computer.
// Entered/left by a long press on the GPS page (reboots).
class PitMode {
 public:
  void begin(Storage* storage, const PitActions& actions, const char* version);
  void loop();

 private:
  void sendPage(const String& body);
  void handleTracksJson();
  void handleTrackAction();

  WebServer  server_{80};
  Storage*   st_ = nullptr;
  PitActions act_{};
  const char* version_ = "";
};
