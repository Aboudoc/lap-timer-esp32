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
  uint32_t (*activeBestMs)();
};

// Pit mode: WiFi hotspot + embedded web app. Download the lap log, manage
// tracks, and flash firmware updates (OTA) from a phone browser — no cable,
// no computer. Entered/left by a long press on the GPS page (reboots).
class PitMode {
 public:
  void begin(Storage* storage, const PitActions& actions, const char* version);
  void loop();

 private:
  void sendPage(const String& body);
  void handleRoot();
  void handleTracks();
  void handleTrackAction();

  WebServer  server_{80};
  Storage*   st_ = nullptr;
  PitActions act_{};
  const char* version_ = "";
};
