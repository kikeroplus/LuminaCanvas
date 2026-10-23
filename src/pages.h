#pragma once
#include "common.h"

// 左側のページ。幅は pageW (デモを流しているときは INFO_W、デモを「なし」にすると SCREEN_W)。
// 上の帯 (時計) と下の帯 (ページ名) は main.cpp が描く
#define CONTENT_Y 31
#define CONTENT_B 224 // 中身を描いてよい下の端

enum PageId
{
  PG_NOW,      // 現在の天気
  PG_HOURLY,   // 24 時間の予報
  PG_WEEK,     // 週間予報
  PG_SUN,      // 日の出・日の入りまでの時間と潮位
  PG_MOON,     // 月の出・月の入りと満ち欠け
  PG_OVERVIEW, // 気象庁の天気概況
  PG_NEWS,     // ニュース
  PG_QUAKE,    // 地震情報
  PG_SOURCES,  // 情報の入手元
  PG_HELP,     // スイッチの操作
  PG_N
};
#define PG_AUTO_N PG_SOURCES // 自動切り替えで回るのはここより前だけ (入手元と操作は SW1/SW2 で開く)
extern const char *PAGE_NAMES[PG_N];
extern int pageW;
inline bool pageWide() { return pageW > INFO_W; }

void pageEnter(int page);
// このページを見せ始めてからの時間 ms に対して、自動で次へ進むまでの進み具合 (1 以上で次へ)
float pageProgress(int page, uint32_t ms);
void drawPage(LGFX_Sprite &g, int page, uint32_t ms);
// 下の帯の右に出す「3/24」など (なければ空)
void pageSubInfo(int page, uint32_t ms, char *out, size_t cap);
void pageNextItem(int page); // SW4: ニュースは次の記事、概況は次の画面

// スイッチ sw (0〜3) の長押しで切り替わるもののいまの値 (「明るい」など。なければ空)。main.cpp が書く
const char *switchState(int sw);
