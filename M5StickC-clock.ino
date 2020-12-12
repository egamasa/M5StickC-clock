#include <M5StickC.h>
#include <WiFi.h>
#include <time.h>

// Wi-Fi
const char* ssid     = "**********";
const char* password = "**********";
const int   timeout  = 5000;

// NTP
const char* ntpServer = "ntp.nict.jp";
const long  gmtOffset_sec = 9 * 3600;
const int   daylightOffset_sec = 0;

RTC_TimeTypeDef RTC_TimeStruct;  // Time
RTC_DateTypeDef RTC_DateStruct;  // Date

unsigned long setuptime;  // スリープ開始判定用（ミリ秒）
int sMin = 0;             // 画面書き換え判定用（分）

void printLocalTime() {
  static const char *wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
  M5.Rtc.GetTime(&RTC_TimeStruct);  // 時刻の取り出し
  M5.Rtc.GetData(&RTC_DateStruct);  // 日付の取り出し

  // 画面明るさ
  if (RTC_TimeStruct.Hours < 7 || 17 < RTC_TimeStruct.Hours) {
    M5.Axp.ScreenBreath( 7 );
  } else {
    M5.Axp.ScreenBreath( 9 );
  }

  if (sMin == RTC_TimeStruct.Minutes) {
    M5.Lcd.fillRect(132, 60, 20, 20, BLACK); // 秒の表示エリアだけ書き換え
  } else {
    M5.Lcd.fillScreen(BLACK); // 「分」が変わったら画面全体を書き換え
  }

  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setCursor(12, 5, 7);  //x,y,font 7:48ピクセル7セグ風フォント
  M5.Lcd.printf("%02d:%02d", RTC_TimeStruct.Hours, RTC_TimeStruct.Minutes); // 時分を表示

  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setCursor(15, 60, 2);  //x,y,font 1:Adafruit 8ピクセルASCIIフォント
  //  M5.Lcd.printf("Date:%04d.%02d.%02d %s\n", RTC_DateStruct.Year, RTC_DateStruct.Month, RTC_DateStruct.Date, wd[RTC_DateStruct.WeekDay]);
  M5.Lcd.printf("%04d/%02d/%02d %s\n", RTC_DateStruct.Year, RTC_DateStruct.Month, RTC_DateStruct.Date, wd[RTC_DateStruct.WeekDay]);

  M5.Lcd.setTextColor(GREEN);
  M5.Lcd.setCursor(132, 60);
  M5.Lcd.fillRect(132, 60, 20, 20, BLACK);
  M5.Lcd.printf("%02d\n", RTC_TimeStruct.Seconds); // 秒を表示

  sMin = RTC_TimeStruct.Minutes; // 「分」を保存
}

void setup() {
  M5.begin();
  M5.Lcd.setRotation(1);
  M5.Axp.ScreenBreath(7);
  M5.Lcd.fillScreen(BLACK);

  // 電源オン時は、WiFiでNTPサーバーから時刻を取得
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();  // スリープからの復帰理由を取得

  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0      :
      Serial.printf("外部割り込み(RTC_IO)で起動\n"); break;  // スリープ復帰時はWiFiで時刻取得しない

    default                         :
      Serial.printf("スリープ以外からの起動\n");  // 電源オン時は、Wi-FiでNTPサーバーから時刻取得

      // Connect to Wi-Fi
      M5.Lcd.setCursor(0, 0, 2);
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.setTextSize(1);
      M5.Lcd.print("SSID : ");
      M5.Lcd.println(ssid);

      // Wi-Fi接続試行
      M5.Lcd.print("Wi-Fi Connecting");
      WiFi.begin(ssid, password);

      // Wi-Fi接続確認（1秒毎）
      while (WiFi.status() != WL_CONNECTED) {
        delay(1000);
        M5.Lcd.print(".");

        if (timeout < millis()) {
          M5.Lcd.println("\nFailed.");
          break;
        }
      }

      if (WiFi.status() == WL_CONNECTED) {
        M5.Lcd.println("\nConnected.");

        // Set ntp time to local
        configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

        // Get local time
        struct tm timeInfo;
        if (getLocalTime(&timeInfo)) {
          M5.Lcd.print("NTP : ");
          M5.Lcd.println(ntpServer);

          // Set RTC time
          RTC_TimeTypeDef TimeStruct;
          TimeStruct.Hours   = timeInfo.tm_hour;
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

        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);

        M5.Lcd.println("\nDisconnected.");
      }

      break;
  }
  setuptime = millis();  // 起動からの経過時間（ミリ秒）を保存
  delay(1000);
}

void loop() {
  M5.update();
  printLocalTime();
  delay(980);  // 0.98秒待ち

  // GPIO37がLOWになったら(M5StickCのA(HOME)ボタンが押されたら)スリープから復帰
  //  pinMode(GPIO_NUM_37, INPUT_PULLUP);
  //  esp_sleep_enable_ext0_wakeup(GPIO_NUM_37, LOW);

  // Bボタン：スリープ開始
  if (M5.BtnB.wasPressed()) {
    M5.Axp.SetSleep();
    esp_deep_sleep_start();
  }

  // 表示開始から１分経過後にディープスリープスタート
  //  if(millis() > setuptime + 1000 * 60){
  //    M5.Axp.SetSleep(); //Display OFF
  //    esp_deep_sleep_start();
  //  }
}
