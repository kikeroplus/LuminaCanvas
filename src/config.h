#pragma once
// ---- 配線 (InfoPanel と同じ) ----
#define TFT_SCK  12 // SCL
#define TFT_MOSI 11 // SDA
#define TFT_CS   10
#define TFT_DC   9
#define TFT_RST  8
#define TFT_BL   7
#define TFT_ROTATION 3        // 横置き 320x240。上下が逆なら 1
#define TFT_SPI_FREQ 40000000 // 配線が短く安定していれば 80000000 にすると描画が速くなる

#define SW1_PIN    13
#define SW2_PIN    14
#define SW3_PIN    1
#define SW4_PIN    2
#define BUZZER_PIN 6

// ---- WiFi (起動時に WiFiManager の設定ポータルで設定する。include/WiFiSetup.h は WorkingTimer と同じファイル) ----
#define DEVICE_BASE_NAME "LuminaCanvas" // 後ろに MAC の末尾 4 桁を付けて AP 名・ホスト名に使う (例 LuminaCanvas-3FA2)
#define WIFI_PORTAL_SEC  180            // 設定モードの上限。過ぎたらオフラインで動く (保存済みの WiFi へのつなぎ直しは続ける)

// ---- 場所 (岐阜市) ----
#define WX_PLACE "岐阜市"
#define WX_LAT   "35.4232"
#define WX_LON   "136.7606"
#define JMA_AREA "210000" // 気象庁の府県コード (岐阜県)

// ---- 潮位 (tide736.net。海上保安庁の推算) ----
#define TIDE_PLACE "四日市港"
#define TIDE_PC    "24" // 府県 (三重県)
#define TIDE_HC    "1"  // 港 (四日市)

// ---- 取得の間隔 ----
#define WX_EVERY_MS   (15UL * 60 * 1000) // 天気 (Open-Meteo)
#define OV_EVERY_MS   (60UL * 60 * 1000) // 天気概況 (気象庁、1 日 3 回ほど更新される)
#define NEWS_EVERY_MS (20UL * 60 * 1000) // ニュース (NHK の RSS)
#define TIDE_EVERY_MS (6UL * 60 * 60 * 1000) // 潮位 (推算なので日付が替わったときにも取り直す)
#define EQ_EVERY_MS   (3UL * 60 * 1000)  // 地震 (気象庁)
#define RETRY_MS      (60UL * 1000)      // 失敗したときに次に試すまで

// ---- 画面の割り付け ----
#define SCREEN_W 320
#define SCREEN_H 240
#define INFO_W   208 // 左: 天気・ニュースのページ
#define GAME_X   INFO_W
#define GAME_W   (SCREEN_W - INFO_W) // 右: ゲームのデモ (112)
#define GAME_LABEL_H 14              // デモの名前を出す帯
#define GAME_H   (SCREEN_H - GAME_LABEL_H) // 226
