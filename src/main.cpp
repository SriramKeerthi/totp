#include <ArduinoOTA.h>
#include <M5StickCPlus.h>
#include <NTPClient.h>
#include <WiFiManager.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <TOTP.h>

void wifiSetup();
void ntpSync();

static const String AP_NAME_PREFIX = "ESP32-AP-";
static const String AP_PASSWORD = "whoknows";

// The shared secret is MyLegoDoor
uint8_t hmacKey[] = {0x45, 0xe3, 0x06, 0x1d, 0xab, 0x16, 0xc7, 0xa5, 0x74, 0x09};

WiFiUDP ntpUDP;
char *ntpServer = "pool.ntp.org";

TOTP totp = TOTP(hmacKey, 10);
char code[7];

void setup() {
    M5.begin();
    M5.Lcd.begin();
    M5.Lcd.setRotation(1);
    M5.Lcd.setTextWrap(false);
    M5.Lcd.setTextSize(2);

    wifiSetup();

    M5.Lcd.fillScreen(BLACK);

    if (WiFi.isConnected()) {
        M5.Lcd.println(WiFi.localIP());
        ntpSync();
        ArduinoOTA.begin();
    }

    sleep(5);
    M5.Lcd.fillScreen(BLACK);
}

static void wifiApCallback(WiFiManager* wm) {
    M5.Lcd.qrcode( "WIFI:T:WPA;S:" + wm->getConfigPortalSSID() + ";P:" + AP_PASSWORD + ";;", 55, 4, 130, 3);
}

String validChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

uint8_t *parseHmac(String secretKey, int numBytes)
{
    Serial.print("HMAC: ");
    uint8_t *hmac = new uint8_t[numBytes];

    uint16_t bitBuffer;
    int currentCharIndex;
    int bitsInBuffer;

    if (secretKey.length() < 3)
    {
        hmac[0] = (uint8_t)(((uint8_t)validChars.indexOf(secretKey[0])) << 3 | (uint8_t)validChars.indexOf(secretKey[1]));
        return hmac;
    }

    bitBuffer = (uint16_t)(((uint8_t)validChars.indexOf(secretKey[0])) << 5 | (uint8_t)validChars.indexOf(secretKey[1]));
    bitsInBuffer = 10;
    currentCharIndex = 2;
    for (int i = 0; i < numBytes; i++)
    {
        int trailing = bitsInBuffer - 8;
        hmac[i] = (uint8_t)(bitBuffer >> trailing);
        Serial.printf("0x%02x, ", hmac[i]);
        bitBuffer <<= 16 - trailing;
        bitBuffer >>= 16 - trailing;
        bitsInBuffer -= 8;
        while (bitsInBuffer < 8 && currentCharIndex < secretKey.length())
        {
            bitBuffer <<= 5;
            bitBuffer |= (uint8_t)validChars.indexOf(secretKey[currentCharIndex++]);
            bitsInBuffer += 5;
        }
    }
    Serial.println();
    return hmac;
}

void parseOtpUrl(String otpUrl)
{
    int secretIndex = otpUrl.indexOf("secret=");
    if (secretIndex < 0)
        return;
    int andIndex = otpUrl.indexOf('&', secretIndex);
    String secret;
    if (andIndex < 0)
        secret = otpUrl.substring(secretIndex + 7);
    else
        secret = otpUrl.substring(secretIndex + 7, andIndex);
    Serial.print("Secret: ");
    Serial.println(secret);
    int numBytes = secret.length() * 5 / 8;
    totp = TOTP(parseHmac(secret, numBytes), numBytes);
}

void wifiSetup() {
    WiFiManager wifiManager;
    wifiManager.setAPCallback(wifiApCallback);
    String mac = WiFi.macAddress();
    int idx;
    while ((idx = mac.indexOf(':')) >= 0) {
        mac.remove(idx, 1);
    }
    String apName = AP_NAME_PREFIX + mac;
    wifiManager.autoConnect(apName.c_str(), AP_PASSWORD.c_str());
}

void ntpSync() {
    // Set ntp time to local
    configTime(3600, 0, ntpServer);

    // Get local time
    struct tm timeInfo;
    if (getLocalTime(&timeInfo))
    {
        // Set RTC time
        RTC_TimeTypeDef TimeStruct;
        TimeStruct.Hours = timeInfo.tm_hour;
        TimeStruct.Minutes = timeInfo.tm_min;
        TimeStruct.Seconds = timeInfo.tm_sec;
        M5.Rtc.SetTime(&TimeStruct);

        RTC_DateTypeDef DateStruct;
        DateStruct.WeekDay = timeInfo.tm_wday;
        DateStruct.Month = timeInfo.tm_mon + 1;
        DateStruct.Date = timeInfo.tm_mday;
        DateStruct.Year = timeInfo.tm_year + 1900;
        M5.Rtc.SetData(&DateStruct);
    }
}

long getTimestamp()
{
    tm t;
    getLocalTime(&t);
    return mktime(&t);
}

int i = 0;
void loop() {
    // return;
    ArduinoOTA.handle();
    M5.update();
    if (M5.BtnA.isPressed() && M5.BtnB.isPressed()) {
        WiFiManager wifiManager;
        wifiManager.resetSettings();
        wifiManager.reboot();
    }
    // M5.Lcd.fillScreen(BLACK);

    M5.Lcd.setTextSize(2);
    M5.Lcd.setCursor(0,0);
    M5.Lcd.println(getTimestamp());

    M5.Lcd.setTextSize(4);

    String otpUrl = Serial.readString();
    if (otpUrl.startsWith("otpauth://totp/"))
    {
        parseOtpUrl(otpUrl);
    }
    char *newCode = totp.getCode(getTimestamp());
    if (strcmp(code, newCode) != 0)
    {
        Serial.println(getTimestamp());
        strcpy(code, newCode);
        Serial.println(code);
        M5.Lcd.println(code);
    }
    delay(100);
}
// otpauth://totp/Token?secret=IXRQMHNLC3D2K5AJIXRQMHNLC3D2K5AJ

