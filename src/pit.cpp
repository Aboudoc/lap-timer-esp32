#include "pit.h"
#include <WiFi.h>
#include <Update.h>
#include "laptimer.h"
#include "web_app.h"
#include "web_icon.h"

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

// Minimal JSON string escaping (quotes, backslashes, control chars).
static void jsonEscape(const char* in, char* out, size_t n) {
  size_t o = 0;
  for (; *in && o + 2 < n; in++) {
    if (*in == '"' || *in == '\\') {
      out[o++] = '\\';
      out[o++] = *in;
    } else if ((uint8_t)*in >= 0x20) {
      out[o++] = *in;
    }
  }
  out[o] = 0;
}

void PitMode::handleTracksJson() {
  TrackMeta list[MAX_TRACKS];
  int cnt = st_->listTracks(list, MAX_TRACKS);
  const char* active = act_.activeTrackName ? act_.activeTrackName() : "";
  String out = "[";
  char esc[36], item[128];
  for (int i = 0; i < cnt; i++) {
    jsonEscape(list[i].name, esc, sizeof(esc));
    snprintf(item, sizeof(item), "%s{\"id\":%u,\"name\":\"%s\",\"best\":%lu,\"active\":%s}",
             i ? "," : "", list[i].id, esc, (unsigned long)list[i].bestMs,
             strcmp(list[i].name, active) == 0 ? "true" : "false");
    out += item;
  }
  out += "]";
  server_.send(200, "application/json", out);
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
  server_.send(200, "text/plain", "ok");
}

void PitMode::begin(Storage* storage, const PitActions& actions, const char* version) {
  st_ = storage;
  act_ = actions;
  version_ = version;

  WiFi.mode(WIFI_AP);
  WiFi.softAP(BT_DEVICE_NAME, PIT_WIFI_PASS);

  server_.on("/", HTTP_GET, [this]() {
    server_.send_P(200, "text/html", WEBAPP_HTML);
  });
  server_.on("/manifest.json", HTTP_GET, [this]() {
    server_.send_P(200, "application/json", MANIFEST_JSON);
  });
  server_.on("/apple-touch-icon.png", HTTP_GET, [this]() {
    server_.send_P(200, "image/png", (const char*)WEB_ICON_PNG, WEB_ICON_PNG_LEN);
  });
  server_.on("/api/status", HTTP_GET, [this]() {
    char b[512] = "{}";
    if (act_.statusJson) act_.statusJson(b, sizeof(b));
    server_.send(200, "application/json", b);
  });
  server_.on("/api/tracks", HTTP_GET, [this]() { handleTracksJson(); });
  server_.on("/track", HTTP_POST, [this]() { handleTrackAction(); });
  server_.on("/exitpit", HTTP_POST, [this]() {
    server_.send(200, "text/plain", "rebooting");
    delay(300);
    if (act_.exitPit) act_.exitPit();
  });

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
    server_.send(200, "text/plain", "ok");
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
