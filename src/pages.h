#pragma once
#include "common.h"

// 左側 (幅 INFO_W) のページ。上の帯 (時計) と下の帯 (ページ名) は main.cpp が描く
#define CONTENT_Y 31
#define CONTENT_B 224 // 中身を描いてよい下の端

enum PageId
{
  PG_NOW,      // 現在の天気
  PG_HOURLY,   // 24 時間の予報
  PG_WEEK,     // 週間予報
  PG_OVERVIEW, // 気象庁の天気概況
  PG_NEWS,     // ニュース
  PG_N
};
extern const char *PAGE_NAMES[PG_N];

void pageEnter(int page);
// このページを見せ始めてからの時間 ms に対して、自動で次へ進むまでの進み具合 (1 以上で次へ)
float pageProgress(int page, uint32_t ms);
void drawPage(LGFX_Sprite &g, int page, uint32_t ms);
// 下の帯の右に出す「3/24」など (なければ空)
void pageSubInfo(int page, uint32_t ms, char *out, size_t cap);
void pageNextItem(int page); // SW4: ニュースは次の記事、概況は次の画面
