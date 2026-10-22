#pragma once
#include <Arduino.h>
#include "lgfx_panel.h"

// ---- 色 (InfoPanel と同じ暗い紺の地。色は 0xRRGGBB の uint32_t で渡す) ----
const uint32_t C_BG = 0x04070C, C_PANEL = 0x0A111C, C_LINE = 0x1C3448,
               C_DIM = 0x5A7084, C_TEXT = 0xD8E6F0, C_CYAN = 0x00D0FF, C_AMBER = 0xFFB040,
               C_GREEN = 0x30E080, C_YELLOW = 0xF0C020, C_RED = 0xFF4050, C_BLUE = 0x4090FF;

uint32_t mix(uint32_t a, uint32_t b, float t); // a と b を t (0〜1) で混ぜる

// ---- 天気 (Open-Meteo) ----
struct HourWx
{
  int8_t hour;
  float temp;
  int8_t pop; // 降水確率 %
  uint8_t code;
  bool day;
};
struct DayWx
{
  uint8_t mon, mday, wday; // wday: 0=日曜
  uint8_t code;
  float tmax, tmin;
  int8_t pop;
};
struct Weather
{
  bool ok;
  time_t at; // 取得した時刻
  char obsTime[6]; // 観測 (current) の時刻 HH:MM
  float temp, feels, wind, precip;
  int hum, windDir, code;
  bool isDay;
  char sunrise[6], sunset[6];
  HourWx hours[24];
  int hourN;
  DayWx days[7];
  int dayN;
};

// ---- 天気概況 (気象庁) ----
struct Overview
{
  bool ok;
  time_t at;
  char office[32];   // 岐阜地方気象台
  char report[12];   // 発表 "10/22 10:38"
  char text[1600];
};

// ---- ニュース (NHK の RSS) ----
struct Article
{
  char cat[20];
  char title[200];
  char desc[480];
  char when[12]; // "10/22 12:22"
};
#define NEWS_MAX 40
struct News
{
  Article *items; // PSRAM。取得のたびに裏の配列と入れ替える
  int n;
  time_t at;
};

extern Weather wx;
extern Overview ov;
extern News news;
extern SemaphoreHandle_t dataLock; // 上の 3 つを読み書きするときに取る

// 通信の様子 (画面の隅に出す)
struct NetState
{
  bool wifi;
  bool busy;        // 取得中
  char err[48];     // 最後の失敗
};
extern NetState net;

void netBegin();
void netRefreshAll(); // 次のすきに全部取り直す

// ---- 天気コード (WMO) ----
enum WxIcon
{
  WI_CLEAR,
  WI_PARTLY,
  WI_CLOUD,
  WI_FOG,
  WI_DRIZZLE,
  WI_RAIN,
  WI_HEAVY,
  WI_SNOW,
  WI_SLEET,
  WI_THUNDER,
};
const char *wxLabel(int code);
WxIcon wxIcon(int code);
void drawWxIcon(LGFX_Sprite &g, int cx, int cy, int size, int code, bool day, uint32_t bg);
const char *windName(int deg);

// ---- 文字 (UTF-8 の日本語を折り返す) ----
#define LINE_CAP 120
// s を幅 maxW で折り返して out に入れる。入りきらなければ最後の行の末尾を「…」にする。行数を返す
int wrapText(LGFX_Sprite &g, const char *s, int maxW, char (*out)[LINE_CAP], int maxLines);

extern const char *WDAY_JA[7];
