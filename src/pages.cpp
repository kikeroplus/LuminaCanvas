// 左側 (デモなしでは全幅) のページ: 現在の天気・24 時間・週間・日の出と潮位・月・天気概況・ニュース・地震・入手元・操作
#include "pages.h"

int pageW = INFO_W;

const char *PAGE_NAMES[PG_N] = {"現在の天気", "24時間の予報", "週間予報", "日の出と潮位", "月の満ち欠け", "天気概況", "ニュース", "地震情報", "情報の入手元", "スイッチの操作"};

#define FJ12 (&fonts::lgfxJapanGothicP_12)
#define FJ16 (&fonts::lgfxJapanGothicP_16)
#define FJ20 (&fonts::lgfxJapanGothicP_20)
#define FE14 (&fonts::efontJA_14)

#define PAGE_MS       15000 // 天気の 3 ページを見せる時間
#define OV_SCREEN_MS  9000  // 概況の 1 画面
#define OV_LINES_PER  8
#define NEWS_ITEM_MS  10000 // ニュース 1 本
#define NEWS_PER_VISIT 5    // 1 回にニュースを何本見せたら次のページへ行くか
#define MANUAL_MS     30000 // 入手元と操作のページを開いてから最初のページへ戻るまで

static void txt(LGFX_Sprite &g, const lgfx::IFont *f, uint32_t c, int x, int y, const char *s, uint8_t datum = TL_DATUM)
{
  g.setFont(f);
  g.setTextColor(c);
  g.setTextDatum(datum);
  g.drawString(s, x, y);
}

static void noData(LGFX_Sprite &g, const char *msg)
{
  txt(g, FJ16, C_DIM, pageW / 2, (CONTENT_Y + CONTENT_B) / 2, msg, MC_DATUM);
}

static void title(LGFX_Sprite &g, const char *s) { txt(g, FJ16, C_AMBER, 6, CONTENT_Y + 2, s); }

// ---- 現在の天気 ----
// 値 (16px) と単位 (12px) を右寄せで並べる
static void valueUnit(LGFX_Sprite &g, int xr, int y, const char *v, const char *unit, uint32_t c)
{
  g.setFont(FJ12);
  int uw = *unit ? g.textWidth(unit) + 1 : 0;
  if (*unit)
    txt(g, FJ12, C_DIM, xr, y + 3, unit, TR_DATUM);
  txt(g, FJ16, c, xr - uw, y, v, TR_DATUM);
}

static void cell(LGFX_Sprite &g, int x, int y, const char *label, const char *v, const char *unit, uint32_t c, int w)
{
  g.fillRoundRect(x, y, w, 21, 4, C_PANEL);
  txt(g, FJ12, C_DIM, x + 5, y + 5, label);
  valueUnit(g, x + w - 4, y + 3, v, unit, c);
}

static void drawNow(LGFX_Sprite &g)
{
  Weather w;
  xSemaphoreTake(dataLock, portMAX_DELAY);
  w = wx;
  xSemaphoreGive(dataLock);
  if (!w.ok)
    return noData(g, "天気を取得中…");

  char b[32];
  txt(g, FJ16, C_AMBER, 6, CONTENT_Y + 2, WX_PLACE);
  snprintf(b, sizeof(b), "%s 観測", w.obsTime);
  txt(g, FJ12, C_DIM, pageW - 6, CONTENT_Y + 5, b, TR_DATUM);

  drawWxIcon(g, 48, 88, 70, w.code, w.isDay, C_BG);
  snprintf(b, sizeof(b), "%.1f", w.temp);
  txt(g, &fonts::DejaVu40, C_TEXT, 182, 56, b, TR_DATUM);
  txt(g, FJ16, C_DIM, 184, 60, "°C");
  if (w.dayN > 0)
  {
    snprintf(b, sizeof(b), "最低 %d°", (int)lroundf(w.days[0].tmin));
    txt(g, FJ12, 0x80C0FF, INFO_W - 6, 102, b, TR_DATUM);
    g.setFont(FJ12);
    int lw = g.textWidth(b);
    snprintf(b, sizeof(b), "最高 %d°", (int)lroundf(w.days[0].tmax));
    txt(g, FJ12, 0xFF9060, INFO_W - 14 - lw, 102, b, TR_DATUM);
  }
  txt(g, FJ20, C_TEXT, INFO_W / 2, 128, wxLabel(w.code), TC_DATUM);

  // 広いときは右にこれからの 5 時間を並べ、下の枠は 3 列にする
  if (pageWide())
  {
    g.drawFastVLine(INFO_W + 2, 54, 96, C_LINE);
    for (int i = 1; i <= 5 && i < w.hourN; i++)
    {
      const HourWx &h = w.hours[i];
      int y = 34 + i * 20;
      snprintf(b, sizeof(b), "%d時", h.hour);
      txt(g, FJ12, C_DIM, INFO_W + 32, y + 3, b, TR_DATUM);
      drawWxIcon(g, INFO_W + 48, y + 9, 18, h.code, h.day, C_BG);
      snprintf(b, sizeof(b), "%d°", (int)lroundf(h.temp));
      txt(g, FJ16, C_TEXT, INFO_W + 84, y, b, TR_DATUM);
      if (h.pop > 0)
      {
        snprintf(b, sizeof(b), "%d", h.pop);
        txt(g, FJ12, h.pop >= 50 ? C_BLUE : 0x80C0FF, pageW - 6, y + 3, b, TR_DATUM);
      }
    }
  }
  const int CW = pageWide() ? 102 : 99, COLS = pageWide() ? 3 : 2;
  int k = 0;
  auto at = [&](int &x, int &y) {
    x = 4 + k % COLS * (CW + 2);
    y = (pageWide() ? 164 : 156) + k / COLS * (pageWide() ? 26 : 23);
    k++;
  };
  int x, y;
  snprintf(b, sizeof(b), "%.1f", w.feels);
  at(x, y), cell(g, x, y, "体感", b, "°", C_TEXT, CW);
  snprintf(b, sizeof(b), "%d", w.hum);
  at(x, y), cell(g, x, y, "湿度", b, "%", C_TEXT, CW);
  snprintf(b, sizeof(b), "%.1f", w.wind);
  at(x, y), cell(g, x, y, windName(w.windDir), b, "m/s", C_TEXT, CW); // 「北北西 4.0」を値にすると見出しにかかるので、風向きを見出しにする
  snprintf(b, sizeof(b), "%d", w.dayN > 0 ? w.days[0].pop : 0);
  at(x, y), cell(g, x, y, "降水確率", b, "%", C_BLUE, CW);
  at(x, y), cell(g, x, y, "日の出", w.sunrise, "", C_TEXT, CW);
  at(x, y), cell(g, x, y, "日の入", w.sunset, "", C_TEXT, CW);
}

// ---- 24 時間の予報 ----
static void drawHourly(LGFX_Sprite &g)
{
  Weather w;
  xSemaphoreTake(dataLock, portMAX_DELAY);
  w = wx;
  xSemaphoreGive(dataLock);
  if (!w.ok || w.hourN < 2)
    return noData(g, "天気を取得中…");
  title(g, "24時間の予報");

  int n = w.hourN;
  auto X = [&](int i) { return 16 + i * (pageW - 32) / (n - 1); };
  float lo = 99, hi = -99;
  for (int i = 0; i < n; i++)
  {
    lo = min(lo, w.hours[i].temp);
    hi = max(hi, w.hours[i].temp);
  }
  char b[16];
  snprintf(b, sizeof(b), "%d〜%d°", (int)lroundf(lo), (int)lroundf(hi));
  txt(g, FJ12, C_DIM, pageW - 6, CONTENT_Y + 5, b, TR_DATUM);
  if (hi - lo < 2)
  {
    lo -= 1;
    hi += 1;
  }
  const int TT = 84, TB = 134; // 気温のグラフの上下
  auto Y = [&](float t) { return (int)(TB - (t - lo) / (hi - lo) * (TB - TT)); };

  for (int i = 0; i < n; i += 3)
  {
    drawWxIcon(g, X(i), 60, pageWide() ? 28 : 22, w.hours[i].code, w.hours[i].day, C_BG);
    g.drawFastVLine(X(i), TT - 4, 192 - TT + 4, C_PANEL);
  }
  for (int i = 1; i < n; i++)
    g.drawWideLine(X(i - 1), Y(w.hours[i - 1].temp), X(i), Y(w.hours[i].temp), 1.0f, C_AMBER);
  g.setFont(&fonts::DejaVu9);
  for (int i = 0; i < n; i += 3)
  {
    int y = Y(w.hours[i].temp);
    g.fillSmoothCircle(X(i), y, 2, C_AMBER);
    snprintf(b, sizeof(b), "%d", (int)lroundf(w.hours[i].temp));
    txt(g, &fonts::DejaVu9, C_TEXT, X(i), y - 12, b, TC_DATUM);
  }

  // 降水確率の棒
  const int PB = 190, PH = 36;
  txt(g, FJ12, C_DIM, 4, 140, "降水確率");
  g.drawFastHLine(8, PB, pageW - 16, C_LINE);
  for (int i = 0; i < n; i++)
  {
    int p = max(0, (int)w.hours[i].pop);
    int h = p * PH / 100;
    if (h > 0)
      g.fillRect(X(i) - 2, PB - h, 5, h, mix(C_BLUE, C_BG, 0.25f));
    if (i % 3 == 0 && p > 0)
    {
      snprintf(b, sizeof(b), "%d", p);
      txt(g, &fonts::DejaVu9, 0x80C0FF, X(i), PB - h - 11, b, TC_DATUM);
    }
  }
  for (int i = 0; i < n; i += 3)
  {
    snprintf(b, sizeof(b), "%d", w.hours[i].hour);
    txt(g, &fonts::DejaVu9, C_DIM, X(i), PB + 4, b, TC_DATUM);
  }
  txt(g, FJ12, C_DIM, pageW - 4, PB + 18, "時", TR_DATUM);
}

// ---- 週間予報 ----
static void drawWeek(LGFX_Sprite &g)
{
  Weather w;
  xSemaphoreTake(dataLock, portMAX_DELAY);
  w = wx;
  xSemaphoreGive(dataLock);
  if (!w.ok || w.dayN == 0)
    return noData(g, "天気を取得中…");

  float lo = 99, hi = -99;
  for (int i = 0; i < w.dayN; i++)
  {
    lo = min(lo, w.days[i].tmin);
    hi = max(hi, w.days[i].tmax);
  }
  if (hi - lo < 1)
    hi = lo + 1;

  char b[16];
  // 広いときは日付を月から出し、天気の名前を足して、最低〜最高の棒を長くする
  const bool wide = pageWide();
  const int RH = 27, IX = wide ? 92 : 71, PX = wide ? 214 : 120, BX = wide ? 248 : 156, BW = wide ? 30 : 16;
  for (int i = 0; i < w.dayN; i++)
  {
    const DayWx &d = w.days[i];
    int y = CONTENT_Y + 2 + i * RH;
    if (i > 0)
      g.drawFastHLine(4, y - 1, pageW - 8, C_LINE);
    uint32_t wc = d.wday == 0 ? 0xFF7070 : d.wday == 6 ? 0x70A8FF : C_TEXT;
    txt(g, FJ16, i == 0 ? C_AMBER : wc, 6, y + 5, i == 0 ? "今日" : WDAY_JA[d.wday]);
    if (wide)
      snprintf(b, sizeof(b), "%d/%d", d.mon, d.mday);
    else
      snprintf(b, sizeof(b), "%d", d.mday); // 月まで出すと隣のアイコンにかかる
    txt(g, FJ12, C_DIM, IX - 15, y + 8, b, TR_DATUM);
    drawWxIcon(g, IX, y + 13, 24, d.code, true, C_BG);
    if (wide)
    {
      static char name[1][LINE_CAP];
      g.setFont(FJ12);
      wrapText(g, wxLabel(d.code), PX - 32 - (IX + 16), name, 1);
      txt(g, FJ12, C_TEXT, IX + 16, y + 8, name[0]);
    }
    if (d.pop >= 0)
    {
      snprintf(b, sizeof(b), "%d%%", d.pop);
      txt(g, FJ12, d.pop >= 50 ? C_BLUE : C_DIM, PX, y + 8, b, TR_DATUM);
    }
    snprintf(b, sizeof(b), "%d°", (int)lroundf(d.tmin));
    txt(g, FJ16, 0x80C0FF, BX - 4, y + 5, b, TR_DATUM);
    snprintf(b, sizeof(b), "%d°", (int)lroundf(d.tmax));
    txt(g, FJ16, 0xFF9060, BX + BW + 4, y + 5, b);
    // 週の中での最低〜最高の幅
    g.fillRoundRect(BX, y + 11, BW, 5, 2, C_PANEL);
    int x0 = BX + (d.tmin - lo) / (hi - lo) * BW, x1 = BX + (d.tmax - lo) / (hi - lo) * BW;
    g.fillRoundRect(x0, y + 11, max(4, x1 - x0), 5, 2, mix(0x80C0FF, 0xFF9060, (d.tmax - lo) / (hi - lo)));
  }
}

// ---- 日の出・日の入りまでの時間と潮位 ----
// 秒を「1時間37分」に (分は切り上げ)
static void fmtDur(char *b, size_t cap, long sec)
{
  long m = (max(0L, sec) + 59) / 60;
  if (m >= 60)
    snprintf(b, cap, "%ld時間%02ld分", m / 60, m % 60);
  else
    snprintf(b, cap, "%ld分", m);
}

static void hm(char *b, size_t cap, time_t t)
{
  struct tm tm;
  localtime_r(&t, &tm);
  snprintf(b, cap, "%d:%02d", tm.tm_hour, tm.tm_min);
}

static void drawSunPart(LGFX_Sprite &g, const Weather &w, time_t now)
{
  if (!w.ok || !w.riseT[0] || !w.riseT[1])
  {
    txt(g, FJ12, C_DIM, 6, CONTENT_Y + 30, "日の出・日の入りを取得中…");
    return;
  }
  time_t r0 = w.riseT[0], s0 = w.setT[0], r1 = w.riseT[1];
  bool day = now >= r0 && now < s0;
  time_t target = day ? s0 : now < r0 ? r0 : r1;
  char b[32];
  txt(g, FJ12, C_DIM, 6, CONTENT_Y + 3, day ? "日の入りまで" : "日の出まで");
  fmtDur(b, sizeof(b), target - now);
  txt(g, FJ20, day ? C_AMBER : 0x80C0FF, 6, CONTENT_Y + 19, b);
  long len = s0 - r0;
  snprintf(b, sizeof(b), "昼 %ld時間%02ld分", len / 3600, len / 60 % 60); // 長く書くと右の弧の時刻にかかる
  txt(g, FJ12, C_DIM, 6, CONTENT_Y + 45, b);

  // 右に太陽の通り道。昼は上の弧 (左が日の出、右が日の入り)、夜は下の浅い弧を右から左へ戻る
  const int cx = pageWide() ? pageW - 76 : 160, hy = CONTENT_Y + 40, rx = pageWide() ? 60 : 36, ry = 30, ryN = 10;
  for (int i = 0; i <= 36; i++)
  {
    float a = PI * i / 36;
    g.drawPixel(cx - rx * cosf(a), hy - ry * sinf(a), C_LINE);
    if (i % 2 == 0)
      g.drawPixel(cx - rx * cosf(a), hy + ryN * sinf(a), C_PANEL);
  }
  g.drawFastHLine(cx - rx - 6, hy, rx * 2 + 13, C_DIM);
  int sx, sy;
  if (day)
  {
    float f = (float)(now - r0) / (s0 - r0);
    sx = cx - rx * cosf(PI * f);
    sy = hy - ry * sinf(PI * f);
    g.fillSmoothCircle(sx, sy, 7, mix(C_AMBER, C_BG, 0.7f));
    g.fillSmoothCircle(sx, sy, 4, C_AMBER);
  }
  else
  {
    time_t from = now < r0 ? s0 - 86400 : s0, to = now < r0 ? r0 : r1; // 夜明け前は昨日の日の入りを 24 時間前で見積もる
    float f = constrain((float)(now - from) / (to - from), 0.0f, 1.0f);
    sx = cx + rx * cosf(PI * f);
    sy = hy + ryN * sinf(PI * f);
    g.fillSmoothCircle(sx, sy, 3, (uint32_t)0x506478);
  }
  hm(b, sizeof(b), day || now < r0 ? r0 : r1);
  txt(g, FJ12, C_DIM, cx - rx, hy + 14, b, TC_DATUM);
  hm(b, sizeof(b), s0);
  txt(g, FJ12, C_DIM, pageW - 4, hy + 14, b, TR_DATUM);
}

static void drawTidePart(LGFX_Sprite &g, const Tide &t, time_t now)
{
  const int Y0 = CONTENT_Y + 72;
  g.drawFastHLine(4, Y0 - 3, pageW - 8, C_LINE);
  txt(g, FJ16, C_AMBER, 6, Y0, TIDE_PLACE "の潮位");
  struct tm tm;
  localtime_r(&now, &tm);
  if (!t.ok || t.mday != tm.tm_mday)
  {
    txt(g, FJ12, C_DIM, pageW / 2, Y0 + 50, "潮位を取得中…", MC_DATUM);
    return;
  }
  char b[32];
  snprintf(b, sizeof(b), "%s 月齢%.1f", t.title, t.moonAge);
  txt(g, FJ12, C_DIM, pageW - 6, Y0 + 3, b, TR_DATUM);

  // 今日の 0〜24 時の潮位の線。満潮・干潮には時刻を添える
  const int GX0 = 8, GX1 = pageW - 8, GT = Y0 + 34, GB = Y0 + 76;
  float lo = 1e9, hi = -1e9;
  for (float v : t.cm)
  {
    lo = min(lo, v);
    hi = max(hi, v);
  }
  if (hi - lo < 10)
    hi = lo + 10;
  auto Y = [&](float v) { return (int)(GB - (v - lo) / (hi - lo) * (GB - GT)); };
  time_t day0 = now - (tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec);
  auto X = [&](time_t at) { return GX0 + (int)((at - day0) * (GX1 - GX0) / 86400); };
  auto cmAt = [&](float pos) { // pos: 0〜TIDE_PTS-1
    int i = constrain((int)pos, 0, TIDE_PTS - 2);
    float f = pos - i;
    return t.cm[i] + (t.cm[i + 1] - t.cm[i]) * f;
  };
  for (int h = 6; h < 24; h += 6)
    g.drawFastVLine(GX0 + (GX1 - GX0) * h / 24, GT - 4, GB - GT + 8, C_PANEL);
  int nowX = X(now);
  for (int x = GX0; x <= GX1; x++)
  {
    float v = cmAt((float)(x - GX0) * (TIDE_PTS - 1) / (GX1 - GX0));
    int y = Y(v);
    g.drawFastVLine(x, y, GB + 6 - y, x < nowX ? (uint32_t)0x0C2238 : (uint32_t)0x123A5C); // 過ぎた時間は暗く
    g.drawPixel(x, y, (uint32_t)0x60B0FF);
  }
  for (int i = 0; i < t.evN; i++)
  {
    const TideEv &e = t.ev[i];
    if (e.t < day0 || e.t >= day0 + 86400)
      continue;
    int x = X(e.t), y = Y(e.cm);
    hm(b, sizeof(b), e.t);
    txt(g, &fonts::DejaVu9, e.high ? C_TEXT : C_DIM, constrain(x, GX0 + 12, GX1 - 12), e.high ? y - 11 : y + 4, b, TC_DATUM);
  }
  float curPos = (float)(now - day0) * (TIDE_PTS - 1) / 86400;
  float cur = cmAt(curPos);
  g.drawFastVLine(nowX, GT - 6, GB - GT + 12, C_AMBER);
  g.fillSmoothCircle(nowX, Y(cur), 3, C_AMBER);

  // いまの潮位と、次の満潮か干潮まで
  const int RY = GB + 12;
  snprintf(b, sizeof(b), "%d", (int)lroundf(cur));
  txt(g, FJ16, C_TEXT, 6, RY, b);
  g.setFont(FJ16);
  int x = 6 + g.textWidth(b) + 2;
  bool rising = cmAt(curPos + 0.5f) > cur;
  txt(g, FJ12, C_DIM, x, RY + 3, rising ? "cm 上げ" : "cm 下げ");
  for (int i = 0; i < t.evN; i++)
    if (t.ev[i].t > now)
    {
      char d[20];
      fmtDur(d, sizeof(d), t.ev[i].t - now);
      snprintf(b, sizeof(b), "%sまで %s", t.ev[i].high ? "満潮" : "干潮", d);
      txt(g, FJ12, t.ev[i].high ? 0x80C0FF : C_DIM, pageW - 6, RY + 3, b, TR_DATUM);
      break;
    }
}

static void drawSun(LGFX_Sprite &g)
{
  static Weather w; // 大きいのでスタックに置かない
  static Tide t;
  xSemaphoreTake(dataLock, portMAX_DELAY);
  w = wx;
  t = tide;
  xSemaphoreGive(dataLock);
  time_t now = time(nullptr);
  drawSunPart(g, w, now);
  drawTidePart(g, t, now);
}

// ---- 月の出・月の入りと満ち欠け ----
static const char *moonName(float age)
{
  // 朔望月を 8 つに分けた呼び名 (新月・上弦・満月・下弦を真ん中に置く)
  static const char *N[] = {"新月", "三日月", "上弦の月", "十三夜", "満月", "寝待月", "下弦の月", "有明月"};
  return N[(int)floorf(age / (29.530589f / 8) + 0.5f) % 8];
}

// 欠けた月の絵。f は新月 0 → 満月 0.5 → 新月 1 (北半球で見た形: 満ちていくときは右が光る)
// 月の海 (暗い模様) の位置。地球から北を上にして見た向きで、x は右、y は上、月の半径を 1 とする。
// 実際の配置に近づけてあるので、豊かの海・神酒の海が耳、静かの海が頭の「餅をつくウサギ」に見える
struct Mare
{
  float x, y, rx, ry, k; // k: 濃さ
};
static const Mare MARIA[] = {
    {-0.66f, 0.34f, 0.24f, 0.30f, 0.75f}, {-0.60f, 0.02f, 0.28f, 0.32f, 0.8f}, // 嵐の大洋
    {-0.66f, -0.30f, 0.20f, 0.20f, 0.7f}, {-0.42f, 0.18f, 0.18f, 0.20f, 0.7f},
    {-0.28f, 0.50f, 0.30f, 0.25f, 1.0f},                                      // 雨の海
    {0.16f, 0.44f, 0.18f, 0.17f, 1.0f},                                       // 晴れの海
    {0.02f, 0.25f, 0.10f, 0.08f, 0.85f},                                      // 蒸気の海
    {0.38f, 0.16f, 0.21f, 0.17f, 1.0f},   {0.24f, 0.04f, 0.14f, 0.11f, 0.95f}, // 静かの海
    {0.30f, 0.28f, 0.10f, 0.08f, 0.95f},
    {0.76f, 0.32f, 0.13f, 0.10f, 1.0f},                                       // 危難の海
    {0.60f, -0.08f, 0.11f, 0.16f, 0.9f},  {0.66f, -0.26f, 0.09f, 0.11f, 0.85f}, // 豊かの海
    {0.52f, 0.02f, 0.08f, 0.08f, 0.9f},
    {0.40f, -0.28f, 0.10f, 0.10f, 0.95f}, {0.34f, -0.14f, 0.06f, 0.07f, 0.85f}, // 神酒の海
    {-0.18f, -0.36f, 0.20f, 0.15f, 0.8f},                                     // 雲の海
    {-0.10f, -0.14f, 0.10f, 0.09f, 0.75f},                                    // 知られた海
    {-0.55f, -0.44f, 0.11f, 0.10f, 0.9f},                                     // 湿りの海
    {0.00f, 0.77f, 0.44f, 0.06f, 0.65f},  {-0.40f, 0.73f, 0.10f, 0.07f, 0.6f}, // 氷の海
    {0.10f, 0.06f, 0.07f, 0.06f, 0.8f},                                       // 中央の入り江
};

// 半径 r の月の、各点の海の濃さ (0〜255)。重いので最初の 1 回だけ作る
static uint8_t *moonShade(int r)
{
  static uint8_t *tab = nullptr;
  static int tabR = 0;
  if (tab && tabR == r)
    return tab;
  free(tab);
  int n = 2 * r + 1;
  tab = (uint8_t *)ps_malloc(n * n);
  tabR = r;
  auto wave = [](float x, float y) { // 海の縁を不規則にするゆらぎ
    return sinf(x * 7.1f + y * 3.3f) * 0.5f + sinf(-x * 4.7f + y * 8.9f) * 0.35f + sinf(x * 13.3f - y * 11.1f) * 0.15f;
  };
  if (!tab)
    return nullptr;
  for (int py = -r; py <= r; py++)
    for (int px = -r; px <= r; px++)
    {
      float x = (float)px / r, y = (float)-py / r;
      float wx = x + 0.035f * wave(x, y), wy = y + 0.035f * wave(y + 1.7f, x - 0.6f);
      float s = 0;
      for (const Mare &m : MARIA)
      {
        float q = sq((wx - m.x) / m.rx) + sq((wy - m.y) / m.ry);
        if (q < 6)
          s += m.k * expf(-q * 1.35f);
      }
      float d = constrain((s - 0.15f) * 1.9f, 0.0f, 1.0f);
      float grain = fmodf(fabsf(sinf(px * 12.9898f + py * 78.233f) * 43758.5453f), 1.0f); // ざらつき
      d = constrain(d + (grain - 0.5f) * 0.10f, 0.0f, 1.0f);
      tab[(py + r) * n + px + r] = d * 255;
    }
  return tab;
}

// 欠けた月の絵。f は新月 0 → 満月 0.5 → 新月 1 (北半球で見た形: 満ちていくときは右が光る)。
// 光っている側に海の模様を描き、暗い側にも地球の照り返しでうっすら見せる
static void drawMoonDisc(LGFX_Sprite &g, int cx, int cy, int r, float f)
{
  const uint32_t LIT = 0xF4EBC4, MARE = 0x8E8A76, DARK = 0x1A2430, DARK_MARE = 0x111922;
  uint8_t *shade = moonShade(r);
  int n = 2 * r + 1;
  float c = cosf(2 * PI * f);
  for (int dy = -r; dy <= r; dy++)
  {
    float w = sqrtf((float)(r * r - dy * dy));
    float xt = w * c; // 明るいところと暗いところの境
    float l0 = f < 0.5f ? xt : -w, l1 = f < 0.5f ? w : -xt; // 光っている x の範囲
    for (int dx = -(int)w; dx <= (int)w; dx++)
    {
      float d = shade ? shade[(dy + r) * n + dx + r] / 255.0f : 0;
      uint32_t col;
      if (dx >= l0 && dx <= l1)
      {
        float limb = 1 - 0.22f * sq((float)(dx * dx + dy * dy) / (r * r)); // 縁ほど少し暗く
        col = mix(0, mix(LIT, MARE, d), limb);
      }
      else
        col = mix(DARK, DARK_MARE, d);
      g.drawPixel(cx + dx, cy + dy, col);
    }
  }
  // ティコ (南の明るいクレーター)
  int tx = cx - r * 15 / 100, ty = cy + r * 70 / 100;
  float tw = sqrtf((float)(r * r - (ty - cy) * (ty - cy)));
  if (tx - cx >= (f < 0.5f ? tw * c : -tw) && tx - cx <= (f < 0.5f ? tw : -tw * c))
  {
    g.fillRect(tx - 1, ty, 3, 1, (uint32_t)0xFFFCEB);
    g.fillRect(tx, ty - 1, 1, 3, (uint32_t)0xFFFCEB);
  }
  g.drawCircle(cx, cy, r, (uint32_t)0x2A3848);
}

// 時刻を「14:53」に。今日でなければ頭に「昨」「翌」を付ける
static void hmDay(char *b, size_t cap, time_t t, time_t now)
{
  struct tm a, n;
  localtime_r(&t, &a);
  localtime_r(&now, &n);
  int dd = (a.tm_year - n.tm_year) * 400 + a.tm_yday - n.tm_yday;
  snprintf(b, cap, "%s%d:%02d", dd < 0 ? "昨" : dd > 0 ? "翌" : "", a.tm_hour, a.tm_min);
}

// 今日から数えて何日後か (暦の日付で)
static int daysFromToday(time_t t, time_t now)
{
  struct tm a, n;
  localtime_r(&t, &a);
  localtime_r(&now, &n);
  a.tm_hour = n.tm_hour = 12;
  a.tm_min = n.tm_min = a.tm_sec = n.tm_sec = 0;
  return (int)lround(difftime(mktime(&a), mktime(&n)) / 86400);
}

// いまの月の形 (新月 0 → 満月 0.5 → 新月 1) と月齢 (日)。満ち欠けは計算で出す (取得しなくても出せる)
static float moonPhase(time_t now, float *age)
{
  time_t nextNew = nextMoonPhase(now, false), prevNew = nextMoonPhase(now - 31 * 86400, false);
  for (time_t p; (p = nextMoonPhase(prevNew, false)) && p <= now;)
    prevNew = p;
  // 形は新月→満月→新月の実際の時刻の間で割り振る (平均の周期で割ると、月の速さのむらで数 % ずれる)
  time_t full = nextMoonPhase(prevNew, true);
  *age = (now - prevNew) / 86400.0f;
  return now < full ? 0.5f * (now - prevNew) / (full - prevNew) : 0.5f + 0.5f * (now - full) / (nextNew - full);
}

static void drawMoon(LGFX_Sprite &g)
{
  static Tide t;
  xSemaphoreTake(dataLock, portMAX_DELAY);
  t = tide;
  xSemaphoreGive(dataLock);
  time_t now = time(nullptr);
  if (now < 1700000000)
    return noData(g, "時刻を合わせています…");

  float age, f = moonPhase(now, &age);
  drawMoonDisc(g, 52, CONTENT_Y + 46, 40, f);

  char b[40];
  const int RX = 104;
  txt(g, FJ12, C_DIM, RX, CONTENT_Y + 4, "月齢");
  snprintf(b, sizeof(b), "%.1f", age);
  txt(g, &fonts::DejaVu24, C_TEXT, RX, CONTENT_Y + 19, b);
  txt(g, FJ20, (uint32_t)0xF4EBC4, RX, CONTENT_Y + 49, moonName(age));
  snprintf(b, sizeof(b), "輝いている面 %d%%", (int)lroundf((1 - cosf(2 * PI * f)) / 2 * 100));
  txt(g, FJ12, C_DIM, RX, CONTENT_Y + 75, b);

  // 月の出・南中・月の入り。出ている間はその月、沈んでいれば次に出る月。広いときは字を大きくして間を空ける
  const bool wide = pageWide();
  const lgfx::IFont *FV = wide ? FJ20 : FJ16;
  int Y1 = CONTENT_Y + (wide ? 100 : 95);
  g.drawFastHLine(4, Y1 - 3, pageW - 8, C_LINE);
  const MoonPass *p = nullptr;
  bool up = false;
  if (t.ok)
    for (int i = 0; i < 3 && !p; i++)
    {
      const MoonPass &m = t.moon[i];
      if (m.rise && m.set && m.rise <= now && now < m.set)
      {
        p = &m;
        up = true;
      }
      else if (m.rise > now)
        p = &m;
    }
  if (!p)
  {
    txt(g, FJ12, C_DIM, pageW / 2, Y1 + 30, "月の出・月の入りを取得中…", MC_DATUM);
  }
  else
  {
    const char *L[] = {"月の出", "南中", "月の入り"};
    time_t T[] = {p->rise, p->mid, p->set};
    for (int i = 0; i < 3; i++)
    {
      int x = wide ? pageW / 6 + i * pageW / 3 : 36 + i * 68;
      txt(g, FJ12, C_DIM, x, Y1 + 1, L[i], TC_DATUM);
      if (T[i])
        hmDay(b, sizeof(b), T[i], now);
      else
        strcpy(b, "--");
      txt(g, FV, T[i] && T[i] <= now ? C_DIM : C_TEXT, x, Y1 + 15, b, TC_DATUM); // 過ぎたものは暗く
    }
    int Y2 = Y1 + (wide ? 48 : 38);
    g.drawFastHLine(4, Y2 - 3, pageW - 8, C_LINE);
    txt(g, FJ12, C_DIM, 6, Y2 + 3, up ? "月の入りまで" : "月の出まで");
    fmtDur(b, sizeof(b), (up ? p->set : p->rise) - now);
    txt(g, FV, (uint32_t)0xF4EBC4, pageW - 6, Y2, b, TR_DATUM);
  }

  // 次の満月・新月 (近いほうを先に)。広いときは右上に並べる
  time_t nextFull = nextMoonPhase(now, true), nextNew = nextMoonPhase(now, false);
  struct
  {
    const char *name;
    time_t at;
  } ev[2] = {{"満月", nextFull}, {"新月", nextNew}};
  if (nextNew < nextFull)
    std::swap(ev[0], ev[1]);
  if (wide)
    g.drawFastVLine(INFO_W + 2, CONTENT_Y + 4, 82, C_LINE);
  for (int i = 0; i < 2; i++)
  {
    struct tm tm;
    localtime_r(&ev[i].at, &tm);
    int d = daysFromToday(ev[i].at, now);
    char dd[16];
    if (d == 0)
      strcpy(dd, "今日");
    else
      snprintf(dd, sizeof(dd), "あと%d日", d);
    if (wide)
    {
      // 1 行目に名前とあと何日、2 行目に日付と時刻を両端に分ける (続けて書くと幅に入らない)
      int x = INFO_W + 10, y = CONTENT_Y + 6 + i * 42;
      txt(g, FJ16, ev[i].at == nextFull ? (uint32_t)0xF4EBC4 : C_TEXT, x, y, ev[i].name);
      txt(g, FJ12, C_DIM, pageW - 6, y + 3, dd, TR_DATUM);
      snprintf(b, sizeof(b), "%d/%d(%s)", tm.tm_mon + 1, tm.tm_mday, WDAY_JA[tm.tm_wday]);
      txt(g, FJ12, C_TEXT, x, y + 20, b);
      snprintf(b, sizeof(b), "%d:%02d", tm.tm_hour, tm.tm_min);
      txt(g, FJ12, C_TEXT, pageW - 6, y + 20, b, TR_DATUM);
      continue;
    }
    snprintf(b, sizeof(b), "%d/%d(%s) %d:%02d", tm.tm_mon + 1, tm.tm_mday, WDAY_JA[tm.tm_wday], tm.tm_hour, tm.tm_min);
    int y = CONTENT_Y + 158 + i * 17;
    txt(g, FJ12, C_DIM, 6, y, ev[i].name);
    txt(g, FJ12, C_TEXT, 36, y, b);
    txt(g, FJ12, C_DIM, pageW - 6, y, dd, TR_DATUM);
  }
}

// ---- 天気概況 (気象庁) ----
static char ovLines[40][LINE_CAP];
static int ovLineN = 0;
static time_t ovWrapped = -1;
static int ovWrapW = 0;
static int ovSkip = 0;

static int ovScreens() { return max(1, (ovLineN + OV_LINES_PER - 1) / OV_LINES_PER); }
static int ovScreen(uint32_t ms) { return min(ovScreens() - 1, (int)(ms / OV_SCREEN_MS) + ovSkip); }

static void drawOverview(LGFX_Sprite &g, uint32_t ms)
{
  xSemaphoreTake(dataLock, portMAX_DELAY);
  bool ok = ov.ok;
  if (ok && (ov.at != ovWrapped || pageW != ovWrapW))
  {
    // 段落の間の空行は詰める (段落の頭の全角空白で区切りが分かる)
    static char buf[sizeof(ov.text)];
    char *w = buf;
    for (const char *r = ov.text; *r; r++)
      if (!(*r == '\n' && w > buf && w[-1] == '\n'))
        *w++ = *r;
    *w = 0;
    g.setFont(FJ16);
    ovLineN = wrapText(g, buf, pageW - 12, ovLines, 40);
    ovWrapped = ov.at;
    ovWrapW = pageW;
  }
  char office[32], report[12];
  strlcpy(office, ov.office, sizeof(office));
  strlcpy(report, ov.report, sizeof(report));
  xSemaphoreGive(dataLock);
  if (!ok)
    return noData(g, "天気概況を取得中…");

  title(g, "天気概況");
  g.setFont(FJ16);
  int room = pageW - 6 - (6 + g.textWidth("天気概況") + 8); // 見出しの右に空いている幅
  char b[64];
  snprintf(b, sizeof(b), "%s %s発表", office, report);
  g.setFont(FJ12);
  if (g.textWidth(b) > room) // 入らなければ気象台の名前は省く
    snprintf(b, sizeof(b), "%s発表", report);
  txt(g, FJ12, C_DIM, pageW - 6, CONTENT_Y + 5, b, TR_DATUM);
  g.drawFastHLine(4, CONTENT_Y + 22, pageW - 8, C_LINE);
  int first = ovScreen(ms) * OV_LINES_PER;
  for (int i = 0; i < OV_LINES_PER && first + i < ovLineN; i++)
    txt(g, FJ16, C_TEXT, 6, CONTENT_Y + 28 + i * 20, ovLines[first + i]);
}

// ---- ニュース ----
static int newsIdx = 0;       // 次に見せる記事 (ページを離れても続きから)
static int newsShown = 0;     // 今回見せた本数
static uint32_t newsItemAt = 0, newsLastMs = 0;
static char nwTitle[4][LINE_CAP], nwDesc[8][LINE_CAP];
static int nwTitleN = 0, nwDescN = 0, nwKey = -1, nwW = 0;
static time_t nwAt = -1;

static uint32_t catColor(const char *cat)
{
  static const struct
  {
    const char *name;
    uint32_t c;
  } C[] = {{"主要", 0xFF6060}, {"社会", 0xFFB040}, {"経済", 0x40D090}, {"国際", 0x40A0FF},
           {"科学・文化", 0xB080FF}, {"暮らし", 0xFF80C0}, {"スポーツ", 0x30D0D0}};
  for (auto &e : C)
    if (!strcmp(cat, e.name))
      return e.c;
  return C_DIM;
}

static void drawNews(LGFX_Sprite &g)
{
  xSemaphoreTake(dataLock, portMAX_DELAY);
  int n = news.n;
  if (n == 0)
  {
    xSemaphoreGive(dataLock);
    return noData(g, "ニュースを取得中…");
  }
  int idx = newsIdx % n;
  static Article a;
  a = news.items[idx];
  bool rewrap = idx != nwKey || news.at != nwAt || pageW != nwW;
  nwW = pageW;
  nwKey = idx;
  nwAt = news.at;
  xSemaphoreGive(dataLock);

  const int TY = CONTENT_Y + 22, TLH = 23, DLH = 17;
  if (rewrap)
  {
    g.setFont(FJ20);
    nwTitleN = wrapText(g, a.title, pageW - 10, nwTitle, 4);
    int dy = TY + nwTitleN * TLH + 10;
    g.setFont(FE14);
    nwDescN = wrapText(g, a.desc, pageW - 10, nwDesc, min(8, (CONTENT_B - dy) / DLH));
  }

  g.setFont(FJ12);
  int cw = g.textWidth(a.cat) + 10;
  g.fillRoundRect(4, CONTENT_Y + 2, cw, 16, 4, catColor(a.cat));
  txt(g, FJ12, C_BG, 9, CONTENT_Y + 4, a.cat);
  txt(g, FJ12, C_DIM, cw + 10, CONTENT_Y + 4, a.when);
  txt(g, FJ12, C_DIM, pageW - 6, CONTENT_Y + 4, "NHK", TR_DATUM);

  for (int i = 0; i < nwTitleN; i++)
    txt(g, FJ20, C_TEXT, 5, TY + i * TLH, nwTitle[i]);
  int dy = TY + nwTitleN * TLH + 3;
  g.drawFastHLine(4, dy, pageW - 8, C_LINE);
  dy += 7;
  for (int i = 0; i < nwDescN; i++)
    txt(g, FE14, 0xA8B8C8, 5, dy + i * DLH, nwDesc[i]);
}

// ---- 地震情報 (気象庁) ----
// 最大震度の色 (気象庁の震度の色に近いもの) と、四角の中の文字
static uint32_t shindoColor(const char *m)
{
  switch (*m)
  {
  case '1': return 0x7090B0;
  case '2': return 0x3C8CE0;
  case '3': return 0x30B070;
  case '4': return 0xE8C020;
  case '5': return m[1] == '+' ? 0xF07010 : 0xF0A020;
  case '6': return m[1] == '+' ? 0xC02040 : 0xE83838;
  case '7': return 0xA02080;
  default: return C_PANEL;
  }
}

// 最大震度の四角 (左上 x, y、大きさ w x h)
static void shindoBox(LGFX_Sprite &g, int x, int y, int w, int h, const char *m)
{
  g.fillRoundRect(x, y, w, h, 4, shindoColor(m));
  uint32_t tc = *m == '3' || *m == '4' || (*m == '5' && m[1] != '+') ? C_BG : C_TEXT;
  int cx = x + w / 2, cy = y + h / 2;
  if (!*m)
    txt(g, FJ12, C_DIM, cx, cy, "-", MC_DATUM);
  else if (m[1] == '-' || m[1] == '+')
  {
    char b[8];
    snprintf(b, sizeof(b), "%c%s", m[0], m[1] == '+' ? "強" : "弱");
    txt(g, FJ12, tc, cx, cy, b, MC_DATUM);
  }
  else
    txt(g, FJ20, tc, cx, cy, m, MC_DATUM);
}

static void drawQuake(LGFX_Sprite &g)
{
  static Quakes qs;
  xSemaphoreTake(dataLock, portMAX_DELAY);
  qs = quakes;
  xSemaphoreGive(dataLock);
  if (!qs.ok)
    return noData(g, "地震情報を取得中…");

  const int RH = 32;
  char b[48];
  for (int i = 0; i < qs.n; i++)
  {
    const Quake &q = qs.q[i];
    int y = CONTENT_Y + 1 + i * RH;
    if (i > 0)
      g.drawFastHLine(4, y - 1, pageW - 8, C_LINE);
    shindoBox(g, 4, y + 2, 28, 27, q.maxi);

    // 広いときは規模を右に大きく出し、深さは「深さ」と書く
    const bool wide = pageWide();
    if (wide && *q.mag)
    {
      snprintf(b, sizeof(b), "M%s", q.mag);
      txt(g, FJ20, C_TEXT, pageW - 6, y + 6, b, TR_DATUM);
    }
    // 震源地 (長ければ「…」で切る)
    static char line[1][LINE_CAP];
    g.setFont(FJ16);
    wrapText(g, *q.place ? q.place : "震源を調査中", pageW - 42 - (wide ? 56 : 0), line, 1);
    txt(g, FJ16, *q.place ? C_TEXT : C_DIM, 38, y + 1, line[0]);
    // 時刻・規模・深さ
    int n = snprintf(b, sizeof(b), "%s", q.when);
    if (*q.mag && !wide)
      n += snprintf(b + n, sizeof(b) - n, " M%s", q.mag);
    if (q.depth == 0)
      snprintf(b + n, sizeof(b) - n, " ごく浅い");
    else if (q.depth > 0)
      snprintf(b + n, sizeof(b) - n, wide ? "  深さ %dkm" : " %dkm", q.depth); // 狭いときは「深さ」まで書くと幅に入らない
    txt(g, FJ12, C_DIM, 38, y + 18, b);
  }
}

// ---- 情報の入手元 ----
// 取得の間隔 (ms) を「15分ごと」「6時間ごと」に
static void fmtEvery(char *b, size_t cap, unsigned long ms)
{
  unsigned long m = ms / 60000;
  if (m >= 60 && m % 60 == 0)
    snprintf(b, cap, "%lu時間ごと", m / 60);
  else
    snprintf(b, cap, "%lu分ごと", m);
}

static void drawSources(LGFX_Sprite &g)
{
  struct Src
  {
    const char *what, *from;
    unsigned long every; // 0 は取得しないもの
    time_t at;           // 最後に取れた時刻 (0 はまだ)
  };
  static char ovFrom[48];
  xSemaphoreTake(dataLock, portMAX_DELAY);
  snprintf(ovFrom, sizeof(ovFrom), "気象庁 (%s)", ov.ok && *ov.office ? ov.office : "天気概況");
  const Src S[] = {
      {"天気・予報・日の出", "Open-Meteo (open-meteo.com)", WX_EVERY_MS, wx.ok ? wx.at : 0},
      {"天気概況", ovFrom, OV_EVERY_MS, ov.ok ? ov.at : 0},
      {"ニュース", "NHK ニュースの RSS", NEWS_EVERY_MS, news.n > 0 ? news.at : 0},
      {"潮位・月の出入り", "tide736.net (" TIDE_PLACE "、海保の推算)", TIDE_EVERY_MS, tide.ok ? tide.at : 0},
      {"地震情報", "気象庁 (地震情報の一覧)", EQ_EVERY_MS, quakes.ok ? quakes.at : 0},
      {"月の満ち欠け", "本体で計算 (Meeus の式)", 0, 0},
      {"時刻", "NTP (ntp.nict.jp ほか)", 0, 0},
  };
  xSemaphoreGive(dataLock);

  const int RH = 27;
  char b[24];
  static char line[1][LINE_CAP];
  for (int i = 0; i < (int)(sizeof(S) / sizeof(S[0])); i++)
  {
    const Src &s = S[i];
    int y = CONTENT_Y + 1 + i * RH;
    if (i > 0)
      g.drawFastHLine(4, y - 1, pageW - 8, C_LINE);
    txt(g, FJ12, C_AMBER, 6, y + 1, s.what);
    // 右に取得の間隔と、最後に取れた時刻
    if (s.every)
    {
      int x = pageW - 6;
      if (s.at)
        hm(b, sizeof(b), s.at);
      else
        strcpy(b, "まだ");
      txt(g, FJ12, s.at ? C_TEXT : C_DIM, x, y + 1, b, TR_DATUM);
      g.setFont(FJ12);
      x -= g.textWidth(b) + 6;
      fmtEvery(b, sizeof(b), s.every);
      txt(g, FJ12, C_DIM, x, y + 1, b, TR_DATUM);
    }
    g.setFont(FJ12);
    wrapText(g, s.from, pageW - 18, line, 1); // 狭いときに入らなければ「…」で切る
    txt(g, FJ12, C_TEXT, 12, y + 14, line[0]);
  }
}

// ---- スイッチの操作 ----
static void drawHelp(LGFX_Sprite &g)
{
  // 狭いときは短い言葉、広いときは詳しく
  struct Sw
  {
    const char *press, *pressWide, *hold, *holdWide;
  };
  static const Sw S[4] = {
      {"次のページ", "次のページ", "明るさ", "明るさを変える"},
      {"前のページ", "前のページ", "音量", "音量を変える"},
      {"次のデモ", "次のデモ (SNAKE の次は「なし」)", "自動送り", "ページの自動切り替え"},
      {"次の記事・画面", "ニュースの次の記事 / 概況の次の画面", "取り直す", "天気とニュースを取り直す"},
  };
  const bool wide = pageWide();
  const int RH = 48, VX = 84;
  static char line[1][LINE_CAP];
  char b[4];
  for (int i = 0; i < 4; i++)
  {
    int y = CONTENT_Y + 1 + i * RH;
    if (i > 0)
      g.drawFastHLine(4, y - 1, pageW - 8, C_LINE);
    g.fillRoundRect(4, y + 3, 34, 41, 5, C_PANEL);
    txt(g, FJ12, C_DIM, 21, y + 6, "SW", TC_DATUM);
    snprintf(b, sizeof(b), "%d", i + 1);
    txt(g, FJ20, C_CYAN, 21, y + 20, b, TC_DATUM);

    txt(g, FJ12, C_DIM, 44, y + 7, "押す");
    g.setFont(FJ16);
    wrapText(g, wide ? S[i].pressWide : S[i].press, pageW - 6 - VX, line, 1);
    txt(g, FJ16, C_TEXT, VX, y + 4, line[0]);

    txt(g, FJ12, C_DIM, 44, y + 28, "長押し");
    txt(g, FJ16, C_TEXT, VX, y + 25, wide ? S[i].holdWide : S[i].hold);
    txt(g, FJ12, C_AMBER, pageW - 6, y + 28, switchState(i), TR_DATUM);
  }
}

// ---- ページの切り替え ----
void pageEnter(int page)
{
  ovSkip = 0;
  if (page == PG_NEWS)
  {
    newsShown = 0;
    newsItemAt = 0;
  }
}

float pageProgress(int page, uint32_t ms)
{
  switch (page)
  {
  case PG_OVERVIEW:
    return (float)(ms + ovSkip * OV_SCREEN_MS) / (ovScreens() * OV_SCREEN_MS);
  case PG_NEWS:
    newsLastMs = ms;
    if (news.n == 0)
      return ms / 8000.0f;
    if (ms - newsItemAt >= NEWS_ITEM_MS)
    {
      newsShown++;
      newsItemAt = ms;
      newsIdx++; // 5 本目が終わったときも進めておき、次に来たときは続きから
    }
    return (newsShown + (float)(ms - newsItemAt) / NEWS_ITEM_MS) / NEWS_PER_VISIT;
  case PG_SOURCES:
  case PG_HELP:
    return (float)ms / MANUAL_MS;
  default:
    return (float)ms / PAGE_MS;
  }
}

void drawPage(LGFX_Sprite &g, int page, uint32_t ms)
{
  switch (page)
  {
  case PG_NOW: drawNow(g); break;
  case PG_HOURLY: drawHourly(g); break;
  case PG_WEEK: drawWeek(g); break;
  case PG_SUN: drawSun(g); break;
  case PG_MOON: drawMoon(g); break;
  case PG_OVERVIEW: drawOverview(g, ms); break;
  case PG_NEWS: drawNews(g); break;
  case PG_QUAKE: drawQuake(g); break;
  case PG_SOURCES: drawSources(g); break;
  case PG_HELP: drawHelp(g); break;
  }
}

void pageSubInfo(int page, uint32_t ms, char *out, size_t cap)
{
  out[0] = 0;
  if (page == PG_OVERVIEW && ov.ok)
    snprintf(out, cap, "%d/%d", ovScreen(ms) + 1, ovScreens());
  else if (page == PG_NEWS && news.n > 0)
    snprintf(out, cap, "%d/%d", newsIdx % news.n + 1, news.n);
}

void pageNextItem(int page)
{
  if (page == PG_OVERVIEW)
    ovSkip = (ovSkip + 1) % ovScreens();
  else if (page == PG_NEWS)
  {
    newsIdx++;
    newsItemAt = newsLastMs; // 次の記事を最初から 10 秒見せる
  }
}
