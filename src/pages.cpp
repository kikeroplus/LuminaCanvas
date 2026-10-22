// 左側のページ: 現在の天気・24 時間・週間・天気概況・ニュース
#include "pages.h"

const char *PAGE_NAMES[PG_N] = {"現在の天気", "24時間の予報", "週間予報", "天気概況", "ニュース"};

#define FJ12 (&fonts::lgfxJapanGothicP_12)
#define FJ16 (&fonts::lgfxJapanGothicP_16)
#define FJ20 (&fonts::lgfxJapanGothicP_20)
#define FE14 (&fonts::efontJA_14)

#define PAGE_MS       15000 // 天気の 3 ページを見せる時間
#define OV_SCREEN_MS  9000  // 概況の 1 画面
#define OV_LINES_PER  8
#define NEWS_ITEM_MS  10000 // ニュース 1 本
#define NEWS_PER_VISIT 5    // 1 回にニュースを何本見せたら次のページへ行くか

static void txt(LGFX_Sprite &g, const lgfx::IFont *f, uint32_t c, int x, int y, const char *s, uint8_t datum = TL_DATUM)
{
  g.setFont(f);
  g.setTextColor(c);
  g.setTextDatum(datum);
  g.drawString(s, x, y);
}

static void noData(LGFX_Sprite &g, const char *msg)
{
  txt(g, FJ16, C_DIM, INFO_W / 2, (CONTENT_Y + CONTENT_B) / 2, msg, MC_DATUM);
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

static void cell(LGFX_Sprite &g, int x, int y, const char *label, const char *v, const char *unit, uint32_t c = C_TEXT)
{
  g.fillRoundRect(x, y, 99, 21, 4, C_PANEL);
  txt(g, FJ12, C_DIM, x + 5, y + 5, label);
  valueUnit(g, x + 95, y + 3, v, unit, c);
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
  txt(g, FJ12, C_DIM, INFO_W - 6, CONTENT_Y + 5, b, TR_DATUM);

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

  int y = 156;
  snprintf(b, sizeof(b), "%.1f", w.feels);
  cell(g, 4, y, "体感", b, "°");
  snprintf(b, sizeof(b), "%d", w.hum);
  cell(g, 105, y, "湿度", b, "%");
  y += 23;
  snprintf(b, sizeof(b), "%s %.1f", windName(w.windDir), w.wind);
  cell(g, 4, y, "風", b, "m/s");
  snprintf(b, sizeof(b), "%d", w.dayN > 0 ? w.days[0].pop : 0);
  cell(g, 105, y, "降水確率", b, "%", C_BLUE);
  y += 23;
  cell(g, 4, y, "日の出", w.sunrise, "");
  cell(g, 105, y, "日の入", w.sunset, "");
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
  auto X = [&](int i) { return 16 + i * (INFO_W - 32) / (n - 1); };
  float lo = 99, hi = -99;
  for (int i = 0; i < n; i++)
  {
    lo = min(lo, w.hours[i].temp);
    hi = max(hi, w.hours[i].temp);
  }
  char b[16];
  snprintf(b, sizeof(b), "%d〜%d°", (int)lroundf(lo), (int)lroundf(hi));
  txt(g, FJ12, C_DIM, INFO_W - 6, CONTENT_Y + 5, b, TR_DATUM);
  if (hi - lo < 2)
  {
    lo -= 1;
    hi += 1;
  }
  const int TT = 84, TB = 134; // 気温のグラフの上下
  auto Y = [&](float t) { return (int)(TB - (t - lo) / (hi - lo) * (TB - TT)); };

  for (int i = 0; i < n; i += 3)
  {
    drawWxIcon(g, X(i), 60, 22, w.hours[i].code, w.hours[i].day, C_BG);
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
  g.drawFastHLine(8, PB, INFO_W - 16, C_LINE);
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
  txt(g, FJ12, C_DIM, INFO_W - 4, PB + 18, "時", TR_DATUM);
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
  const int RH = 27, BX = 150, BW = 24;
  for (int i = 0; i < w.dayN; i++)
  {
    const DayWx &d = w.days[i];
    int y = CONTENT_Y + 2 + i * RH;
    if (i > 0)
      g.drawFastHLine(4, y - 1, INFO_W - 8, C_LINE);
    uint32_t wc = d.wday == 0 ? 0xFF7070 : d.wday == 6 ? 0x70A8FF : C_TEXT;
    txt(g, FJ16, i == 0 ? C_AMBER : wc, 6, y + 5, i == 0 ? "今日" : WDAY_JA[d.wday]);
    snprintf(b, sizeof(b), "%d/%d", d.mon, d.mday);
    txt(g, FJ12, C_DIM, 40, y + 8, b);
    drawWxIcon(g, 84, y + 13, 24, d.code, true, C_BG);
    if (d.pop >= 0)
    {
      snprintf(b, sizeof(b), "%d%%", d.pop);
      txt(g, FJ12, d.pop >= 50 ? C_BLUE : C_DIM, 122, y + 8, b, TR_DATUM);
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

// ---- 天気概況 (気象庁) ----
static char ovLines[40][LINE_CAP];
static int ovLineN = 0;
static time_t ovWrapped = -1;
static int ovSkip = 0;

static int ovScreens() { return max(1, (ovLineN + OV_LINES_PER - 1) / OV_LINES_PER); }
static int ovScreen(uint32_t ms) { return min(ovScreens() - 1, (int)(ms / OV_SCREEN_MS) + ovSkip); }

static void drawOverview(LGFX_Sprite &g, uint32_t ms)
{
  xSemaphoreTake(dataLock, portMAX_DELAY);
  bool ok = ov.ok;
  if (ok && ov.at != ovWrapped)
  {
    // 段落の間の空行は詰める (段落の頭の全角空白で区切りが分かる)
    static char buf[sizeof(ov.text)];
    char *w = buf;
    for (const char *r = ov.text; *r; r++)
      if (!(*r == '\n' && w > buf && w[-1] == '\n'))
        *w++ = *r;
    *w = 0;
    g.setFont(FJ16);
    ovLineN = wrapText(g, buf, INFO_W - 12, ovLines, 40);
    ovWrapped = ov.at;
  }
  char office[32], report[12];
  strlcpy(office, ov.office, sizeof(office));
  strlcpy(report, ov.report, sizeof(report));
  xSemaphoreGive(dataLock);
  if (!ok)
    return noData(g, "天気概況を取得中…");

  title(g, "天気概況");
  char b[64];
  snprintf(b, sizeof(b), "%s %s発表", office, report);
  txt(g, FJ12, C_DIM, INFO_W - 6, CONTENT_Y + 5, b, TR_DATUM);
  g.drawFastHLine(4, CONTENT_Y + 22, INFO_W - 8, C_LINE);
  int first = ovScreen(ms) * OV_LINES_PER;
  for (int i = 0; i < OV_LINES_PER && first + i < ovLineN; i++)
    txt(g, FJ16, C_TEXT, 6, CONTENT_Y + 28 + i * 20, ovLines[first + i]);
}

// ---- ニュース ----
static int newsIdx = 0;       // 次に見せる記事 (ページを離れても続きから)
static int newsShown = 0;     // 今回見せた本数
static uint32_t newsItemAt = 0, newsLastMs = 0;
static char nwTitle[4][LINE_CAP], nwDesc[8][LINE_CAP];
static int nwTitleN = 0, nwDescN = 0, nwKey = -1;
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
  bool rewrap = idx != nwKey || news.at != nwAt;
  nwKey = idx;
  nwAt = news.at;
  xSemaphoreGive(dataLock);

  const int TY = CONTENT_Y + 22, TLH = 23, DLH = 17;
  if (rewrap)
  {
    g.setFont(FJ20);
    nwTitleN = wrapText(g, a.title, INFO_W - 10, nwTitle, 4);
    int dy = TY + nwTitleN * TLH + 10;
    g.setFont(FE14);
    nwDescN = wrapText(g, a.desc, INFO_W - 10, nwDesc, min(8, (CONTENT_B - dy) / DLH));
  }

  g.setFont(FJ12);
  int cw = g.textWidth(a.cat) + 10;
  g.fillRoundRect(4, CONTENT_Y + 2, cw, 16, 4, catColor(a.cat));
  txt(g, FJ12, C_BG, 9, CONTENT_Y + 4, a.cat);
  txt(g, FJ12, C_DIM, cw + 10, CONTENT_Y + 4, a.when);
  txt(g, FJ12, C_DIM, INFO_W - 6, CONTENT_Y + 4, "NHK", TR_DATUM);

  for (int i = 0; i < nwTitleN; i++)
    txt(g, FJ20, C_TEXT, 5, TY + i * TLH, nwTitle[i]);
  int dy = TY + nwTitleN * TLH + 3;
  g.drawFastHLine(4, dy, INFO_W - 8, C_LINE);
  dy += 7;
  for (int i = 0; i < nwDescN; i++)
    txt(g, FE14, 0xA8B8C8, 5, dy + i * DLH, nwDesc[i]);
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
  case PG_OVERVIEW: drawOverview(g, ms); break;
  case PG_NEWS: drawNews(g); break;
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
