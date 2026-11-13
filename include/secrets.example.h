#pragma once
// Copy to include/secrets.h (gitignored). Wi-Fi can also be set later over USB serial:
//   python3 tools/serial.py wifi_set "<ssid>" "<password>"
#define WIFI_SSID ""
#define WIFI_PASS ""
#define OTA_PASSWORD ""   // set 8+ random characters to enable updates over Wi-Fi (pio run -e ota)
