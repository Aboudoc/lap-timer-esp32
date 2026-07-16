#include "pit.h"
#include <WiFi.h>
#include <Update.h>
#include "laptimer.h"

static const char* kCss =
    "<style>body{font-family:sans-serif;margin:16px;background:#111;color:#eee;max-width:480px}"
    "a{color:#4af}h2{margin:4px 0 12px}form{margin:8px 0}"
    "input,button{font-size:1em;padding:6px;margin:2px;border-radius:6px;border:1px solid #555;"
    "background:#222;color:#eee}button{cursor:pointer;background:#28a}"
    ".danger{background:#a33}.card{background:#1c1c1c;padding:10px;border-radius:8px;margin:10px 0}"
    "</style>";

void PitMode::sendPage(const String& body) {
  String page = String("<!DOCTYPE html><html><head><meta charset=utf-8>"
                       "<meta name=viewport content='width=device-width,initial-scale=1'>"
                       "<title>LapTimer</title>") + kCss + "</head><body><h2>&#127937; LapTimer</h2>" +
                body + "</body></html>";
  server_.send(200, "text/html", page);
}

void PitMode::handleRoot() {
  char tm[12];
  uint32_t best = act_.activeBestMs ? act_.activeBestMs() : 0;
  fmtLapTime(tm, sizeof(tm), best, true);
  String b = "<div class=card>Firmware " + String(version_) +
             "<br>Active track: <b>" + String(act_.activeTrackName ? act_.activeTrackName() : "-") +
             "</b><br>All-time best: <b>" + (best ? String(tm) : String("-")) + "</b></div>" +
             "<div class=card><a href=/laps.csv>&#11015; Download lap log (CSV)</a>"
             "<form method=post action=/clear onsubmit='return confirm(\"Erase the lap log?\")'>"
             "<button class=danger>Erase lap log</button></form></div>" +
             "<div class=card><a href=/tracks>Manage tracks</a></div>" +
             "<div class=card><a href=/update>Firmware update (OTA)</a></div>";
  sendPage(b);
}

void PitMode::handleTracks() {
  TrackMeta list[MAX_TRACKS];
  int cnt = st_->listTracks(list, MAX_TRACKS);
  String b = "<p><a href=/>&larr; back</a></p>";
  if (cnt == 0) b += "<div class=card>No stored track yet.</div>";
  char tm[12];
  for (int i = 0; i < cnt; i++) {
    fmtLapTime(tm, sizeof(tm), list[i].bestMs, true);
    b += "<div class=card><form method=post action=/track>"
         "<input type=hidden name=id value=" + String(list[i].id) + ">"
         "<input name=name maxlength=15 value=\"" + String(list[i].name) + "\"> "
         "best " + (list[i].bestMs ? String(tm) : String("-")) +
         "<br><button name=action value=select>Select</button>"
         "<button name=action value=rename>Rename</button>"
         "<button name=action value=del class=danger "
         "onclick='return confirm(\"Delete this track?\")'>Delete</button>"
         "</form></div>";
  }
  sendPage(b);
}

void PitMode::handleTrackAction() {
  uint8_t id = (uint8_t)server_.arg("id").toInt();
  String action = server_.arg("action");
  if (action == "select" && act_.selectTrack) {
    act_.selectTrack(id);
  } else if (action == "rename" && act_.renameTrack) {
    act_.renameTrack(id, server_.arg("name").c_str());
  } else if (action == "del" && act_.deleteTrack) {
    act_.deleteTrack(id);
  }
  server_.sendHeader("Location", "/tracks");
  server_.send(303);
}

void PitMode::begin(Storage* storage, const PitActions& actions, const char* version) {
  st_ = storage;
  act_ = actions;
  version_ = version;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(BT_DEVICE_NAME, PIT_WIFI_PASS);

  server_.on("/", HTTP_GET, [this]() { handleRoot(); });
  server_.on("/tracks", HTTP_GET, [this]() { handleTracks(); });
  server_.on("/track", HTTP_POST, [this]() { handleTrackAction(); });

  server_.on("/laps.csv", HTTP_GET, [this]() {
    File f = st_->openCsvRead();
    if (!f) {
      server_.send(404, "text/plain", "log empty");
      return;
    }
    server_.streamFile(f, "text/csv");
    f.close();
  });

  server_.on("/clear", HTTP_POST, [this]() {
    st_->clearCsv();
    server_.sendHeader("Location", "/");
    server_.send(303);
  });

  server_.on("/update", HTTP_GET, [this]() {
    sendPage("<p><a href=/>&larr; back</a></p><div class=card>"
             "<p>Upload a firmware built with <code>pio run</code><br>"
             "(file <code>.pio/build/esp32dev/firmware.bin</code>)</p>"
             "<form method=post action=/update enctype=multipart/form-data>"
             "<input type=file name=fw accept=.bin><button>Flash</button></form>"
             "<p>The device reboots when done.</p></div>");
  });
  server_.on("/update", HTTP_POST,
    [this]() {
      server_.send(200, "text/plain", Update.hasError() ? "FAILED" : "OK, rebooting...");
      delay(500);
      ESP.restart();
    },
    [this]() {
      HTTPUpload& up = server_.upload();
      if (up.status == UPLOAD_FILE_START) {
        Serial.printf("[OTA] start: %s\n", up.filename.c_str());
        Update.begin(UPDATE_SIZE_UNKNOWN);
      } else if (up.status == UPLOAD_FILE_WRITE) {
        Update.write(up.buf, up.currentSize);
      } else if (up.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
          Serial.printf("[OTA] done, %u bytes\n", up.totalSize);
        } else {
          Update.printError(Serial);
        }
      }
    });

  server_.onNotFound([this]() {
    server_.sendHeader("Location", "/");
    server_.send(303);
  });

  server_.begin();
  Serial.printf("[PIT] WiFi \"%s\" pass \"%s\" -> http://%s\n",
                BT_DEVICE_NAME, PIT_WIFI_PASS, WiFi.softAPIP().toString().c_str());
}

void PitMode::loop() {
  server_.handleClient();
}
