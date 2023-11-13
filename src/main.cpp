/*
MIT License

Copyright (c) 2021-2023 riraosan.github.io

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <memory>
#include <Arduino.h>
#include <SD.h>
#include <SPIFFS.h>
#include <M5ModuleRCA.h>
#include <M5Unified.h>

// basic
#include <WiFi.h>
#include <HTTPClient.h>
#include <Ticker.h>

#define HTTP_BUFFER 1024 * 2

// github
#include <ArduinoJson.h>
#include <StreamUtils.h>

// yahoo finance (yahoo.com) Root Certificate
constexpr const char *ca = R"(-----BEGIN CERTIFICATE-----
MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBs
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5j
ZSBFViBSb290IENBMB4XDTA2MTExMDAwMDAwMFoXDTMxMTExMDAwMDAwMFowbDEL
MAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3
LmRpZ2ljZXJ0LmNvbTErMCkGA1UEAxMiRGlnaUNlcnQgSGlnaCBBc3N1cmFuY2Ug
RVYgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMbM5XPm
+9S75S0tMqbf5YE/yc0lSbZxKsPVlDRnogocsF9ppkCxxLeyj9CYpKlBWTrT3JTW
PNt0OKRKzE0lgvdKpVMSOO7zSW1xkX5jtqumX8OkhPhPYlG++MXs2ziS4wblCJEM
xChBVfvLWokVfnHoNb9Ncgk9vjo4UFt3MRuNs8ckRZqnrG0AFFoEt7oT61EKmEFB
Ik5lYYeBQVCmeVyJ3hlKV9Uu5l0cUyx+mM0aBhakaHPQNAQTXKFx01p8VdteZOE3
hzBWBOURtCmAEvF5OYiiAhF8J2a3iLd48soKqDirCmTCv2ZdlYTBoSUeh10aUAsg
EsxBu24LUTi4S8sCAwEAAaNjMGEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQF
MAMBAf8wHQYDVR0OBBYEFLE+w2kD+L9HAdSYJhoIAu9jZCvDMB8GA1UdIwQYMBaA
FLE+w2kD+L9HAdSYJhoIAu9jZCvDMA0GCSqGSIb3DQEBBQUAA4IBAQAcGgaX3Nec
nzyIZgYIVyHbIUf4KmeqvxgydkAQV8GK83rZEWWONfqe/EW1ntlMMUu4kehDLI6z
eM7b41N5cdblIZQB2lWHmiRk9opmzN6cN82oNLFpmyPInngiK3BD41VHMWEZ71jF
hS9OMPagMRYjyOfiZRYzy78aG6A9+MpeizGLYAiJLQwGXFK3xPkKmNEVX58Svnw2
Yzi9RKR/5CYrCsSXaQ3pjOLAEFe4yHYSkVXySGnYvCoCWw9E1CAx2/S6cCZdkGCe
vEsXCS+0yx5DaMkHJ8HSXPfqIbloEpw8nL+e/IBcm2PN7EeqJSdnoDfzAIJ9VNep
+OkuE6N36B9K
-----END CERTIFICATE-----
)";

const char *ssid     = "Buffalo-C130";   // Enter SSID
const char *password = "nnkxnpshmhai6";  // Enter Password

// Yahoo! finance API endpoint
String              endpoint("https://query1.finance.yahoo.com/v8/finance/chart/{code}?interval=1d");
DynamicJsonDocument _doc(HTTP_BUFFER);

M5Canvas canvas;

Ticker timer;
bool   flag = false;

typedef struct tagDATA {
  String name;
  float  value;
} DATA;

DATA data[3];

void secondTimer(void) {
  flag = true;
}

// APIにリクエストを送る→Jsonレスポンス取得
long doHttpGet(String url, const char *ca) {
  log_i("[HTTP] GET begin...\n");
  HTTPClient http;

  http.begin(url, ca);

  log_i("[HTTP] GET...");
  // start connection and send HTTP header
  int           httpCode = http.GET();
  unsigned long index    = 0;

  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    log_i("[HTTP] GET... code: %d", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK) {
      ReadLoggingStream    loggingStream(http.getStream(), Serial);
      DeserializationError error = deserializeJson(_doc, loggingStream);

      if (error) {
        log_e("deserializeJson() failed: %s", error.f_str());
        http.end();
        return -1;
      }
    } else {
      http.end();
      log_e("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      return -1;
    }
  } else {
    http.end();
    log_e("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    return -1;
  }

  http.end();

  return 0;
}

// Mony order code is ex. USDJPY=X. etc...
float getMonyOrder(String monyordercode) {
  String api(endpoint);
  api.replace("{code}", monyordercode);

  if (doHttpGet(api, ca)) {
    log_e("fail to load json data.");
  }

  JsonObject chart_result_0 = _doc["chart"]["result"][0];
  if (chart_result_0.isNull() == false) {
    JsonObject chart_result_0_meta = chart_result_0["meta"];
    if (chart_result_0_meta.isNull() == false) {
      return chart_result_0_meta["regularMarketPrice"];
    }
  }

  return -1;
}

float getStockPrice(String stockcode) {
  String api(endpoint);
  api.replace("{code}", stockcode);

  if (doHttpGet(api, ca)) {
    log_e("fail to load json data.");
  }

  JsonObject chart_result_0 = _doc["chart"]["result"][0];
  if (chart_result_0.isNull() == false) {
    JsonObject chart_result_0_meta = chart_result_0["meta"];
    if (chart_result_0_meta.isNull() == false) {
      return chart_result_0_meta["regularMarketPrice"];
    }
  }

  return -1;
}

void initWifi(void) {
  // Connect to wifi)
  WiFi.begin(ssid, password);

  log_i(" WiFi connecting");

  // Wait some time to connect to wifi
  for (int i = 0; i < 30 && WiFi.status() != WL_CONNECTED; i++) {
    log_i(".");
    delay(1000);
  }

  log_i("\n Connected!");
}

void initDisplays(void) {
  auto cfg = M5.config();

  cfg.external_display.module_rca = true;  // default=true. use ModuleRCA VideoOutput

  // setting for Module RCA.
  cfg.module_rca.logical_width  = 290;
  cfg.module_rca.logical_height = 210;
  cfg.module_rca.output_width   = 320;
  cfg.module_rca.output_height  = 240;
  cfg.module_rca.signal_type    = M5ModuleRCA::signal_type_t::NTSC_J;      //  NTSC / NTSC_J / PAL_M / PAL_N
  cfg.module_rca.use_psram      = M5ModuleRCA::use_psram_t::psram_no_use;  // psram_no_use / psram_half_use
  cfg.module_rca.pin_dac        = GPIO_NUM_26;
  cfg.module_rca.output_level   = 128;

  // begin M5Unified.
  M5.begin(cfg);

  // Get the number of available displays
  int display_count = M5.getDisplayCount();

  // Displays settings
  for (int i = 0; i < display_count; ++i) {
    M5.Displays(i).setFont(&fonts::efontJA_24);
    M5.Displays(i).setTextSize(1);
    M5.Displays(i).setTextColor(TFT_WHITE, TFT_BLACK);
    M5.Displays(i).printf("No.%d\n", i);
  }

  canvas.setFont(&fonts::efontJA_24);
  canvas.setTextSize(1);

  // If an external display is to be used as the main display, it can be listed in order of priority.
  M5.setPrimaryDisplayType({m5::board_t::board_M5ModuleRCA});

  // The primary display can be used with M5.Display.
  // M5.Display.print("primary display\n");
}

void getdata(void) {
  data[0].name  = "為替ドル円";
  data[0].value = getMonyOrder("USDJPY=X");
  data[1].name  = "日経平均株価";
  data[1].value = getStockPrice("^N225");
  data[2].name  = "4442.T";
  data[2].value = getStockPrice("4442.T");
}

void showData(void) {
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 3; j++) {
      canvas.createSprite(320, 24);
      canvas.setCursor(0, 0);
      canvas.printf("%s %5.2f円", data[j].name.c_str(), data[j].value);
      canvas.drawLine(0, 23, 319, 23, TFT_GREEN);
      canvas.pushSprite(&M5.Displays(i), 0, 30 * j, TFT_BLACK);
      canvas.deleteSprite();
    }
  }
}

void setup(void) {
  initDisplays();

  M5.delay(5000);

  initWifi();

  timer.attach(60, secondTimer);

  secondTimer();
}

void loop(void) {
  if (flag) {
    flag = false;
    getdata();

    for (int i = 0; i < 2; i++) {
      M5.Displays(i).fillScreen(TFT_BLACK);
    }
  }
  showData();
  M5.delay(1);
}
