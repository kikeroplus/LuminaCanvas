// 通信: WiFi・時刻合わせと、天気・天気概況・ニュースの取得。
// HTTPS のやりとりは 1〜2 秒かかるので、画面を止めないように別のタスク (コア 0) で行う。
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "common.h"
#include "secrets.h"

Weather wx;
Overview ov;
News news;
NetState net;
SemaphoreHandle_t dataLock;

static Article *newsSpare; // 取得中に書き込む裏の配列
static volatile bool refreshAll = false;

void netRefreshAll() { refreshAll = true; }

static void setErr(const char *what, int code)
{
  snprintf(net.err, sizeof(net.err), "%s %d", what, code);
  Serial.printf("net: %s failed (%d)\n", what, code);
}

// Stream の find などはデータを待つあいだ millis() を回すだけで CPU を譲らない。
// コア 0 の IDLE が動けずタスクウォッチドッグで再起動するので、来ていなければ 1 tick 休む
class YieldStream : public Stream
{
  Stream &s;

public:
  YieldStream(Stream &s) : s(s) {}
  int available() override { return s.available(); }
  int read() override
  {
    int c = s.read();
    if (c < 0)
      vTaskDelay(1);
    return c;
  }
  int peek() override
  {
    int c = s.peek();
    if (c < 0)
      vTaskDelay(1);
    return c;
  }
  size_t write(uint8_t) override { return 0; }
};

// "2026-10-22T14:00" の HH:MM
static void hhmm(char *out, const char *iso)
{
  const char *t = strchr(iso, 'T');
  strlcpy(out, t ? t + 1 : "--:--", 6);
}

// ---- 天気 (Open-Meteo、鍵なしで使える) ----
static bool fetchWeather()
{
  static const char *URL =
      "http://api.open-meteo.com/v1/forecast?latitude=" WX_LAT "&longitude=" WX_LON
      "&current=temperature_2m,relative_humidity_2m,apparent_temperature,is_day,weather_code,precipitation,wind_speed_10m,wind_direction_10m"
      "&hourly=temperature_2m,precipitation_probability,weather_code,is_day"
      "&daily=weather_code,temperature_2m_max,temperature_2m_min,precipitation_probability_max,sunrise,sunset"
      "&timezone=Asia%2FTokyo&forecast_days=7&forecast_hours=24&wind_speed_unit=ms";
  HTTPClient http;
  http.useHTTP10(true); // chunked にさせず、そのまま JSON を読む
  http.setTimeout(8000);
  if (!http.begin(URL))
    return false;
  int code = http.GET();
  if (code != 200)
  {
    setErr("weather", code);
    http.end();
    return false;
  }
  JsonDocument doc;
  YieldStream body(http.getStream());
  DeserializationError e = deserializeJson(doc, body);
  http.end();
  if (e)
  {
    setErr("weather json", (int)e.code());
    return false;
  }

  static Weather w; // 大きいのでスタックに置かない
  memset(&w, 0, sizeof(w));
  JsonObject cur = doc["current"];
  hhmm(w.obsTime, cur["time"] | "");
  w.temp = cur["temperature_2m"] | NAN;
  w.feels = cur["apparent_temperature"] | NAN;
  w.hum = cur["relative_humidity_2m"] | -1;
  w.code = cur["weather_code"] | -1;
  w.isDay = (cur["is_day"] | 1) != 0;
  w.precip = cur["precipitation"] | 0.0f;
  w.wind = cur["wind_speed_10m"] | NAN;
  w.windDir = cur["wind_direction_10m"] | -1;

  JsonObject h = doc["hourly"];
  JsonArray ht = h["time"];
  for (int i = 0; i < (int)ht.size() && i < 24; i++)
  {
    const char *t = ht[i] | "";
    HourWx &o = w.hours[i];
    o.hour = strlen(t) >= 13 ? atoi(t + 11) : 0;
    o.temp = h["temperature_2m"][i] | NAN;
    o.pop = h["precipitation_probability"][i] | -1;
    o.code = h["weather_code"][i] | 0;
    o.day = (h["is_day"][i] | 1) != 0;
    w.hourN = i + 1;
  }

  JsonObject d = doc["daily"];
  JsonArray dt = d["time"];
  for (int i = 0; i < (int)dt.size() && i < 7; i++)
  {
    const char *t = dt[i] | "2000-01-01";
    struct tm tm = {};
    tm.tm_year = atoi(t) - 1900;
    tm.tm_mon = atoi(t + 5) - 1;
    tm.tm_mday = atoi(t + 8);
    tm.tm_hour = 12;
    mktime(&tm); // 曜日を求める
    DayWx &o = w.days[i];
    o.mon = tm.tm_mon + 1;
    o.mday = tm.tm_mday;
    o.wday = tm.tm_wday;
    o.code = d["weather_code"][i] | 0;
    o.tmax = d["temperature_2m_max"][i] | NAN;
    o.tmin = d["temperature_2m_min"][i] | NAN;
    o.pop = d["precipitation_probability_max"][i] | -1;
    w.dayN = i + 1;
  }
  hhmm(w.sunrise, d["sunrise"][0] | "");
  hhmm(w.sunset, d["sunset"][0] | "");
  w.at = time(nullptr);
  w.ok = true;

  xSemaphoreTake(dataLock, portMAX_DELAY);
  wx = w;
  xSemaphoreGive(dataLock);
  Serial.printf("net: weather ok %.1fC code %d\n", w.temp, w.code);
  return true;
}

// ---- 天気概況 (気象庁の JSON) ----
static bool fetchOverview()
{
  WiFiClientSecure tls;
  tls.setInsecure(); // 証明書は確かめない (表示するだけなので)
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(8000);
  if (!http.begin(tls, "https://www.jma.go.jp/bosai/forecast/data/overview_forecast/" JMA_AREA ".json"))
    return false;
  int code = http.GET();
  if (code != 200)
  {
    setErr("jma", code);
    http.end();
    return false;
  }
  JsonDocument doc;
  YieldStream body(http.getStream());
  DeserializationError e = deserializeJson(doc, body);
  http.end();
  if (e)
  {
    setErr("jma json", (int)e.code());
    return false;
  }

  static Overview o;
  memset(&o, 0, sizeof(o));
  strlcpy(o.office, doc["publishingOffice"] | "", sizeof(o.office));
  const char *rt = doc["reportDatetime"] | ""; // 2026-10-22T10:38:00+09:00
  if (strlen(rt) >= 16)
    snprintf(o.report, sizeof(o.report), "%d/%d %.5s", atoi(rt + 5), atoi(rt + 8), rt + 11);
  const char *head = doc["headlineText"] | "";
  if (*head)
    snprintf(o.text, sizeof(o.text), "【%s】\n%s", head, doc["text"] | "");
  else
    strlcpy(o.text, doc["text"] | "", sizeof(o.text));
  o.at = time(nullptr);
  o.ok = true;

  xSemaphoreTake(dataLock, portMAX_DELAY);
  ov = o;
  xSemaphoreGive(dataLock);
  Serial.printf("net: overview ok (%d bytes)\n", strlen(o.text));
  return true;
}

// ---- ニュース (NHK の RSS) ----
struct NewsCat
{
  const char *name;
  const char *file;
  int take; // 先頭から何本取るか
};
static const NewsCat CATS[] = {
    {"主要", "cat0", 8},
    {"社会", "cat1", 4},
    {"経済", "cat5", 4},
    {"国際", "cat6", 4},
    {"科学・文化", "cat3", 3},
    {"暮らし", "cat2", 3},
    {"スポーツ", "cat7", 4},
};

// &amp; などの文字参照を戻す (その場で書き換える)
static void unescape(char *s)
{
  static const struct
  {
    const char *ent, *ch;
  } ENT[] = {{"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&apos;", "'"}, {"&#39;", "'"}};
  char *w = s;
  for (char *r = s; *r;)
  {
    bool hit = false;
    if (*r == '&')
      for (auto &e : ENT)
      {
        size_t n = strlen(e.ent);
        if (!strncmp(r, e.ent, n))
        {
          *w++ = *e.ch;
          r += n;
          hit = true;
          break;
        }
      }
    if (!hit)
      *w++ = *r++;
  }
  *w = 0;
}

// "Thu, 22 Oct 2026 12:22:00 +0900" → "10/22 12:22"
static void rssDate(char *out, size_t cap, const char *s)
{
  static const char *MON = "JanFebMarAprMayJunJulAugSepOctNovDec";
  int day = 0, hh = 0, mm = 0;
  char mon[4] = "";
  if (sscanf(s, "%*[^,], %d %3s %*d %d:%d", &day, mon, &hh, &mm) == 4)
  {
    const char *p = strstr(MON, mon);
    snprintf(out, cap, "%d/%d %02d:%02d", p ? (int)(p - MON) / 3 + 1 : 0, day, hh, mm);
  }
  else
    strlcpy(out, "", cap);
}

// 次の <tag> の中身を読む。見つからなければ false
static bool readTag(Stream &s, const char *tag, char *out, size_t cap)
{
  char open[24];
  snprintf(open, sizeof(open), "<%s>", tag);
  if (!s.find(open))
    return false;
  String v = s.readStringUntil('<'); // NHK の RSS は CDATA を使わず、< は &lt; になっている
  strlcpy(out, v.c_str(), cap);
  unescape(out);
  return true;
}

static int fetchNewsCat(const NewsCat &c, Article *dst, int room)
{
  WiFiClientSecure tls;
  tls.setInsecure();
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(8000);
  char url[80];
  // www.nhk.or.jp/rss/news/catN.xml の転送先
  snprintf(url, sizeof(url), "https://news.web.nhk/n-data/conf/na/rss/%s.xml", c.file);
  if (!http.begin(tls, url))
    return 0;
  int code = http.GET();
  if (code != 200)
  {
    setErr(c.file, code);
    http.end();
    return 0;
  }
  YieldStream s(http.getStream());
  s.setTimeout(5000);
  int n = 0;
  char date[40];
  while (n < c.take && n < room && s.find("<item>"))
  {
    Article &a = dst[n];
    strlcpy(a.cat, c.name, sizeof(a.cat));
    if (!readTag(s, "title", a.title, sizeof(a.title)) ||
        !readTag(s, "description", a.desc, sizeof(a.desc)) ||
        !readTag(s, "pubDate", date, sizeof(date)))
      break;
    rssDate(a.when, sizeof(a.when), date);
    n++;
  }
  http.end(); // 残り (社会などは 100 本以上ある) は読まずに切る
  return n;
}

static bool fetchNews()
{
  int n = 0;
  for (const NewsCat &c : CATS)
  {
    static Article tmp[8]; // 5KB あるのでスタックに置かない
    int got = fetchNewsCat(c, tmp, 8);
    for (int i = 0; i < got && n < NEWS_MAX; i++)
    {
      bool dup = false; // 主要と各分野で同じ記事が出るので、見出しが同じものは飛ばす
      for (int j = 0; j < n && !dup; j++)
        dup = !strcmp(newsSpare[j].title, tmp[i].title);
      if (!dup)
        newsSpare[n++] = tmp[i];
    }
    vTaskDelay(pdMS_TO_TICKS(200));
  }
  if (n == 0)
    return false;

  xSemaphoreTake(dataLock, portMAX_DELAY);
  Article *old = news.items;
  news.items = newsSpare;
  news.n = n;
  news.at = time(nullptr);
  newsSpare = old;
  xSemaphoreGive(dataLock);
  Serial.printf("net: news ok (%d)\n", n);
  return true;
}

static void netTask(void *)
{
  uint32_t wxDue = 0, ovDue = 0, newsDue = 0; // 0 はすぐ
  for (;;)
  {
    net.wifi = WiFi.status() == WL_CONNECTED;
    if (refreshAll)
    {
      refreshAll = false;
      wxDue = ovDue = newsDue = 0;
    }
    uint32_t now = millis();
    if (net.wifi)
    {
      // 時刻が合うまでは待つ (天気の曜日やニュースの日付に使う)
      bool clockOk = time(nullptr) > 1700000000;
      if (clockOk && (!wxDue || (int32_t)(now - wxDue) >= 0))
      {
        net.busy = true;
        wxDue = millis() + (fetchWeather() ? WX_EVERY_MS : RETRY_MS);
      }
      else if (clockOk && (!ovDue || (int32_t)(now - ovDue) >= 0))
      {
        net.busy = true;
        ovDue = millis() + (fetchOverview() ? OV_EVERY_MS : RETRY_MS);
      }
      else if (clockOk && (!newsDue || (int32_t)(now - newsDue) >= 0))
      {
        net.busy = true;
        newsDue = millis() + (fetchNews() ? NEWS_EVERY_MS : RETRY_MS);
      }
      net.busy = false;
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}

void netBegin()
{
  dataLock = xSemaphoreCreateMutex();
  news.items = (Article *)ps_calloc(NEWS_MAX, sizeof(Article));
  newsSpare = (Article *)ps_calloc(NEWS_MAX, sizeof(Article));

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  configTzTime("JST-9", "ntp.nict.jp", "time.google.com", "pool.ntp.org");
  xTaskCreatePinnedToCore(netTask, "net", 16384, nullptr, 1, nullptr, 0);
}
