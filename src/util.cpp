// 色・天気コード・天気の絵・日本語の折り返し
#include "common.h"

const char *WDAY_JA[7] = {"日", "月", "火", "水", "木", "金", "土"};

uint32_t mix(uint32_t a, uint32_t b, float t)
{
  t = constrain(t, 0.0f, 1.0f);
  uint32_t out = 0;
  for (int sh = 0; sh <= 16; sh += 8)
  {
    int ca = (a >> sh) & 0xFF, cb = (b >> sh) & 0xFF;
    out |= (uint32_t)(ca + (cb - ca) * t) << sh;
  }
  return out;
}

// ---- 天気コード (WMO) ----
const char *wxLabel(int code)
{
  switch (code)
  {
  case 0: return "快晴";
  case 1: return "晴れ";
  case 2: return "晴れ時々曇り";
  case 3: return "曇り";
  case 45: case 48: return "霧";
  case 51: case 53: case 55: return "霧雨";
  case 56: case 57: return "着氷性の霧雨";
  case 61: return "小雨";
  case 63: return "雨";
  case 65: return "大雨";
  case 66: case 67: return "着氷性の雨";
  case 71: return "小雪";
  case 73: return "雪";
  case 75: return "大雪";
  case 77: return "霧雪";
  case 80: return "にわか雨";
  case 81: return "強いにわか雨";
  case 82: return "激しいにわか雨";
  case 85: case 86: return "にわか雪";
  case 95: return "雷雨";
  case 96: case 99: return "ひょうを伴う雷雨";
  default: return "--";
  }
}

WxIcon wxIcon(int code)
{
  if (code <= 1) return WI_CLEAR;
  if (code == 2) return WI_PARTLY;
  if (code == 3) return WI_CLOUD;
  if (code == 45 || code == 48) return WI_FOG;
  if (code >= 51 && code <= 55) return WI_DRIZZLE;
  if (code == 56 || code == 57 || code == 66 || code == 67) return WI_SLEET;
  if (code == 65 || code == 82) return WI_HEAVY;
  if ((code >= 61 && code <= 63) || code == 80 || code == 81) return WI_RAIN;
  if ((code >= 71 && code <= 77) || code == 85 || code == 86) return WI_SNOW;
  if (code >= 95) return WI_THUNDER;
  return WI_CLOUD;
}

const char *windName(int deg)
{
  static const char *N[] = {"北", "北北東", "北東", "東北東", "東", "東南東", "南東", "南南東",
                            "南", "南南西", "南西", "西南西", "西", "西北西", "北西", "北北西"};
  if (deg < 0)
    return "--";
  return N[((deg * 16 + 180) / 360) % 16];
}

// ---- 天気の絵 (図形で描く。size は外枠の一辺) ----
static void sun(LGFX_Sprite &g, int cx, int cy, int r)
{
  for (int i = 0; i < 8; i++)
  {
    float a = i * PI / 4;
    int x0 = cx + cosf(a) * r * 1.45f, y0 = cy + sinf(a) * r * 1.45f;
    int x1 = cx + cosf(a) * r * 1.95f, y1 = cy + sinf(a) * r * 1.95f;
    g.drawWideLine(x0, y0, x1, y1, max(0.6f, r * 0.12f), (uint32_t)0xFFC030);
  }
  g.fillSmoothCircle(cx, cy, r, (uint32_t)0xFFC030);
}

static void moon(LGFX_Sprite &g, int cx, int cy, int r, uint32_t bg)
{
  g.fillSmoothCircle(cx, cy, r, (uint32_t)0xF0E6A0);
  g.fillSmoothCircle(cx + r * 0.55f, cy - r * 0.4f, r * 0.85f, bg);
}

static void cloud(LGFX_Sprite &g, int cx, int cy, int w, uint32_t c)
{
  // 横幅 w の雲 (下が平ら)
  int r1 = w * 0.22f, r2 = w * 0.30f, r3 = w * 0.20f;
  int base = cy + w * 0.12f;
  g.fillSmoothCircle(cx - w * 0.26f, base - r1, r1, c);
  g.fillSmoothCircle(cx + w * 0.02f, base - r2 * 1.1f, r2, c);
  g.fillSmoothCircle(cx + w * 0.30f, base - r3, r3, c);
  g.fillSmoothRoundRect(cx - w * 0.48f, base - r1, w * 0.96f, r1 + 1, r1 / 2 + 1, c);
}

void drawWxIcon(LGFX_Sprite &g, int cx, int cy, int size, int code, bool day, uint32_t bg)
{
  WxIcon k = wxIcon(code);
  int s = size;
  const uint32_t CL = 0xD0DCE6, CD = 0x8090A0; // 白い雲、雨雲
  switch (k)
  {
  case WI_CLEAR:
    if (day)
      sun(g, cx, cy, s * 0.24f);
    else
      moon(g, cx, cy, s * 0.32f, bg);
    break;
  case WI_PARTLY:
    if (day)
      sun(g, cx - s * 0.14f, cy - s * 0.14f, s * 0.18f);
    else
      moon(g, cx - s * 0.12f, cy - s * 0.14f, s * 0.24f, bg);
    cloud(g, cx + s * 0.08f, cy + s * 0.14f, s * 0.72f, CL);
    break;
  case WI_CLOUD:
    cloud(g, cx - s * 0.12f, cy - s * 0.08f, s * 0.6f, CD);
    cloud(g, cx + s * 0.06f, cy + s * 0.1f, s * 0.8f, CL);
    break;
  case WI_FOG:
    cloud(g, cx, cy - s * 0.1f, s * 0.72f, CD);
    for (int i = 0; i < 3; i++)
      g.drawWideLine(cx - s * 0.36f + i * s * 0.04f, cy + s * (0.14f + i * 0.12f), cx + s * 0.36f - i * s * 0.04f,
                     cy + s * (0.14f + i * 0.12f), max(0.6f, s * 0.03f), (uint32_t)0xA0B0C0);
    break;
  case WI_DRIZZLE:
  case WI_RAIN:
  case WI_HEAVY:
  case WI_SLEET:
  {
    cloud(g, cx, cy - s * 0.14f, s * 0.84f, k == WI_DRIZZLE ? CL : CD);
    int drops = k == WI_HEAVY ? 5 : k == WI_DRIZZLE ? 3 : 4;
    float len = k == WI_DRIZZLE ? 0.08f : 0.16f;
    for (int i = 0; i < drops; i++)
    {
      float x = cx - s * 0.3f + i * s * 0.6f / (drops - 1);
      float y = cy + s * 0.12f + (i & 1) * s * 0.08f;
      if (k == WI_SLEET && (i & 1))
        g.fillSmoothCircle(x, y + s * 0.06f, max(1.0f, s * 0.035f), (uint32_t)0xFFFFFF);
      else
        g.drawWideLine(x, y, x - s * 0.05f, y + s * len, max(0.6f, s * 0.025f), (uint32_t)0x50A8FF);
    }
    break;
  }
  case WI_SNOW:
    cloud(g, cx, cy - s * 0.14f, s * 0.84f, CL);
    for (int i = 0; i < 4; i++)
    {
      float x = cx - s * 0.27f + i * s * 0.18f;
      float y = cy + s * 0.18f + (i & 1) * s * 0.1f;
      g.fillSmoothCircle(x, y, max(1.0f, s * 0.045f), (uint32_t)0xFFFFFF);
    }
    break;
  case WI_THUNDER:
  {
    cloud(g, cx, cy - s * 0.16f, s * 0.84f, CD);
    int x = cx, y = cy + s * 0.02f;
    g.fillTriangle(x + s * 0.06f, y, x - s * 0.12f, y + s * 0.2f, x + s * 0.02f, y + s * 0.2f, (uint32_t)0xFFD020);
    g.fillTriangle(x + s * 0.04f, y + s * 0.16f, x - s * 0.06f, y + s * 0.4f, x + s * 0.12f, y + s * 0.16f, (uint32_t)0xFFD020);
    break;
  }
  }
}

// ---- 日本語の折り返し ----
static int u8len(uint8_t c)
{
  if (c < 0x80) return 1;
  if ((c >> 5) == 6) return 2;
  if ((c >> 4) == 14) return 3;
  if ((c >> 3) == 30) return 4;
  return 1;
}

// 行の頭に来てはいけない文字 (句読点・閉じかっこ・長音など)
static bool noHead(const char *p, int n)
{
  static const char *LIST[] = {"、", "。", "，", "．", "・", "ー", "」", "』", "）", "】", "〉", "》", "”", "！", "？",
                               "ゃ", "ゅ", "ょ", "っ", "ャ", "ュ", "ョ", "ッ", "々", "…", ")", ",", ".", "!", "?"};
  for (const char *s : LIST)
    if ((int)strlen(s) == n && !strncmp(p, s, n))
      return true;
  return false;
}

int wrapText(LGFX_Sprite &g, const char *s, int maxW, char (*out)[LINE_CAP], int maxLines)
{
  int lines = 0, len = 0, w = 0;
  char *cur = out[0];
  cur[0] = 0;
  const char *p = s;
  while (*p && lines < maxLines)
  {
    if (*p == '\n')
    {
      p++;
      if (++lines >= maxLines)
        break;
      cur = out[lines];
      cur[0] = 0;
      len = w = 0;
      continue;
    }
    int n = u8len(*p);
    char ch[5] = {};
    memcpy(ch, p, n);
    int cw = g.textWidth(ch);
    // はみ出すなら改行。ただし行頭禁則の文字はぶら下げて今の行に入れる
    if (w + cw > maxW && len > 0 && !noHead(p, n))
    {
      if (++lines >= maxLines)
        break;
      cur = out[lines];
      cur[0] = 0;
      len = w = 0;
      if (*p == ' ')
      {
        p++;
        continue;
      }
    }
    if (len + n < LINE_CAP)
    {
      memcpy(cur + len, ch, n + 1);
      len += n;
      w += cw;
    }
    p += n;
  }
  if (lines < maxLines)
    lines++; // 最後の行を数える
  // 入りきらなかったら、最後の行の末尾を「…」に置き換える
  if (*p)
  {
    char *last = out[lines - 1];
    int l = strlen(last), ew = g.textWidth("…");
    while (l > 0 && g.textWidth(last) + ew > maxW)
    {
      do
        l--;
      while (l > 0 && ((uint8_t)last[l] & 0xC0) == 0x80);
      last[l] = 0;
    }
    strlcat(last, "…", LINE_CAP);
  }
  return lines;
}
