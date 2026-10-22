#pragma once
// 右側の小窓 (GAME_W x GAME_H) で自動で遊ぶゲームのデモ。1 秒に 30 回 step() と draw() が呼ばれる
#include "../common.h"

class Demo
{
public:
  virtual ~Demo() {}
  virtual const char *name() const = 0;
  virtual void reset() = 0;
  virtual void step() = 0;
  virtual void draw(LGFX_Sprite &g) = 0;
};

Demo *makeTetris();
Demo *makeInvaders();
Demo *makeBreakout();
Demo *makePlatformer();
Demo *makeShooter();
Demo *makeSnake();

// ---- 共通の道具 ----
inline float frand(float a, float b) { return a + (b - a) * (esp_random() / 4294967295.0f); }
inline int irand(int a, int b) { return a + (int)(esp_random() % (uint32_t)(b - a + 1)); } // a 以上 b 以下

// 文字で描いた点の絵を描く。'.' は透明、それ以外は pal の中の同じ文字の色
struct PalEntry
{
  char ch;
  uint32_t rgb;
};
inline void blit(LGFX_Sprite &g, int x, int y, const char *const *rows, int h, const PalEntry *pal, int palN, bool flip = false)
{
  for (int r = 0; r < h; r++)
  {
    int w = strlen(rows[r]);
    for (int c = 0; c < w; c++)
    {
      char ch = rows[r][flip ? w - 1 - c : c];
      if (ch == '.')
        continue;
      for (int i = 0; i < palN; i++)
        if (pal[i].ch == ch)
        {
          g.drawPixel(x + c, y + r, pal[i].rgb);
          break;
        }
    }
  }
}
// 1 色だけの絵
inline void blit(LGFX_Sprite &g, int x, int y, const char *const *rows, int h, uint32_t rgb)
{
  PalEntry p = {'X', rgb};
  blit(g, x, y, rows, h, &p, 1);
}

// 爆発などの火花
class Particles
{
  struct P
  {
    float x, y, vx, vy;
    uint8_t life, max;
    uint32_t rgb;
  } p[80];

public:
  void clear()
  {
    for (P &q : p)
      q.life = 0;
  }
  void burst(float x, float y, uint32_t rgb, int n, float speed = 1.5f, int life = 18)
  {
    for (P &q : p)
    {
      if (n <= 0)
        break;
      if (q.life)
        continue;
      float a = frand(0, 2 * PI), s = frand(0.3f, 1.0f) * speed;
      q = {x, y, cosf(a) * s, sinf(a) * s, (uint8_t)irand(life / 2, life), (uint8_t)life, rgb};
      n--;
    }
  }
  void step(float gravity = 0)
  {
    for (P &q : p)
      if (q.life)
      {
        q.x += q.vx;
        q.y += q.vy;
        q.vy += gravity;
        q.life--;
      }
  }
  void draw(LGFX_Sprite &g, uint32_t bg)
  {
    for (P &q : p)
      if (q.life)
        g.fillRect((int)q.x, (int)q.y, 2, 2, mix(bg, q.rgb, (float)q.life / q.max * 1.5f));
  }
};

// 上の得点などに使う小さい文字 (6x8)
inline void hud(LGFX_Sprite &g, int x, int y, uint32_t c, const char *s, uint8_t datum = TL_DATUM)
{
  g.setFont(&fonts::Font0);
  g.setTextSize(1);
  g.setTextColor(c);
  g.setTextDatum(datum);
  g.drawString(s, x, y);
}
