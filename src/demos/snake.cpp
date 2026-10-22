// へびゲーム。えさへの最短の道を探し、その先が狭すぎるときは広いほうへ逃げる
#include "demo.h"

namespace
{
const int CELL = 8, GW = GAME_W / CELL, GH = 26, OY = 14; // 14 x 26 マス
const int MAXLEN = GW * GH;
const uint32_t BG = 0x040A06;
const int DXS[4] = {1, 0, -1, 0}, DYS[4] = {0, 1, 0, -1};

class Snake : public Demo
{
  int8_t sx[MAXLEN], sy[MAXLEN]; // [0] が頭
  int len, grow, dir;
  int fx, fy;
  int moveT, deadT, score, best;
  Particles parts;

  bool blocked(int x, int y, bool tailFree) const
  {
    if (x < 0 || x >= GW || y < 0 || y >= GH)
      return true;
    int n = tailFree && !grow ? len - 1 : len; // しっぽは次の一歩でどくので通れる
    for (int i = 0; i < n; i++)
      if (sx[i] == x && sy[i] == y)
        return true;
    return false;
  }

  // (x,y) から行けるマスの数
  int area(int x, int y) const
  {
    static bool seen[GH][GW];
    static int8_t qx[MAXLEN], qy[MAXLEN];
    memset(seen, 0, sizeof(seen));
    for (int i = 0; i < len - 1; i++)
      seen[sy[i]][sx[i]] = true;
    if (x < 0 || x >= GW || y < 0 || y >= GH || seen[y][x])
      return 0;
    int h = 0, t = 0;
    qx[t] = x;
    qy[t++] = y;
    seen[y][x] = true;
    while (h < t)
    {
      int cx = qx[h], cy = qy[h++];
      for (int d = 0; d < 4; d++)
      {
        int nx = cx + DXS[d], ny = cy + DYS[d];
        if (nx >= 0 && nx < GW && ny >= 0 && ny < GH && !seen[ny][nx])
        {
          seen[ny][nx] = true;
          qx[t] = nx;
          qy[t++] = ny;
        }
      }
    }
    return t;
  }

  // えさまでの最短の道の最初の向き。なければ -1
  int pathToFood() const
  {
    static int8_t first[GH][GW];
    static int8_t qx[MAXLEN], qy[MAXLEN];
    memset(first, -1, sizeof(first));
    int h = 0, t = 0;
    for (int d = 0; d < 4; d++)
    {
      int nx = sx[0] + DXS[d], ny = sy[0] + DYS[d];
      if (!blocked(nx, ny, true) && first[ny][nx] < 0)
      {
        first[ny][nx] = d;
        qx[t] = nx;
        qy[t++] = ny;
      }
    }
    while (h < t)
    {
      int cx = qx[h], cy = qy[h++];
      if (cx == fx && cy == fy)
        return first[cy][cx];
      for (int d = 0; d < 4; d++)
      {
        int nx = cx + DXS[d], ny = cy + DYS[d];
        if (nx >= 0 && nx < GW && ny >= 0 && ny < GH && first[ny][nx] < 0 && !blocked(nx, ny, true))
        {
          first[ny][nx] = first[cy][cx];
          qx[t] = nx;
          qy[t++] = ny;
        }
      }
    }
    return -1;
  }

  void placeFood()
  {
    for (int tries = 0; tries < 500; tries++)
    {
      fx = irand(0, GW - 1);
      fy = irand(0, GH - 1);
      if (!blocked(fx, fy, false))
        return;
    }
  }

  int choose()
  {
    int d = pathToFood();
    if (d >= 0 && area(sx[0] + DXS[d], sy[0] + DYS[d]) >= len)
      return d;
    int bestD = -1, bestA = -1;
    for (int k = 0; k < 4; k++)
    {
      int nx = sx[0] + DXS[k], ny = sy[0] + DYS[k];
      if (blocked(nx, ny, true))
        continue;
      int a = area(nx, ny);
      if (a > bestA)
      {
        bestA = a;
        bestD = k;
      }
    }
    return bestD >= 0 ? bestD : (d >= 0 ? d : dir);
  }

public:
  Snake() : best(0) {}
  const char *name() const override { return "SNAKE"; }

  void reset() override
  {
    len = 4;
    grow = 0;
    dir = 0;
    for (int i = 0; i < len; i++)
    {
      sx[i] = 4 - i;
      sy[i] = GH / 2;
    }
    placeFood();
    moveT = 0;
    deadT = 0;
    score = 0;
    parts.clear();
  }

  void step() override
  {
    parts.step();
    if (deadT)
    {
      if (++deadT > 70)
        reset();
      return;
    }
    if (++moveT < 3)
      return;
    moveT = 0;
    dir = choose();
    int nx = sx[0] + DXS[dir], ny = sy[0] + DYS[dir];
    if (blocked(nx, ny, true))
    {
      deadT = 1;
      best = max(best, score);
      return;
    }
    if (grow)
      grow--;
    else
      len--;
    memmove(sx + 1, sx, len);
    memmove(sy + 1, sy, len);
    len++;
    sx[0] = nx;
    sy[0] = ny;
    if (nx == fx && ny == fy)
    {
      grow += 2;
      score += 10;
      parts.burst(fx * CELL + 4, OY + fy * CELL + 4, 0xFF5050, 10, 1.4f, 14);
      if (len + grow >= MAXLEN - 4)
        deadT = 1; // いっぱいになったらやり直し
      else
        placeFood();
    }
  }

  void draw(LGFX_Sprite &g) override
  {
    g.fillScreen(BG);
    char s[24];
    snprintf(s, sizeof(s), "LEN %d", len);
    hud(g, 4, 2, C_TEXT, s);
    snprintf(s, sizeof(s), "HI %d", max(best, score));
    hud(g, GAME_W - 4, 2, C_DIM, s, TR_DATUM);
    g.drawRect(0, OY - 1, GW * CELL, GH * CELL + 2, 0x163020);
    for (int y = 0; y < GH; y++)
      for (int x = 0; x < GW; x++)
        g.drawPixel(x * CELL + 4, OY + y * CELL + 4, 0x0E2014);

    g.fillSmoothCircle(fx * CELL + 4, OY + fy * CELL + 4, 3, 0xFF4848);
    g.drawFastVLine(fx * CELL + 4, OY + fy * CELL, 2, 0x60D060);

    bool show = !deadT || (deadT / 5) % 2 == 0;
    for (int i = len - 1; i >= 0 && show; i--)
    {
      uint32_t c = mix(0x60FF80, 0x1A6030, (float)i / len);
      if (deadT)
        c = mix(c, 0xFF3030, 0.6f);
      g.fillRoundRect(sx[i] * CELL, OY + sy[i] * CELL, CELL - 1, CELL - 1, 2, c);
    }
    if (show)
    {
      int hx = sx[0] * CELL + 3, hy = OY + sy[0] * CELL + 3;
      int ex = DYS[dir], ey = DXS[dir]; // 進む向きと直角に目を 2 つ
      g.fillRect(hx + DXS[dir] + ex * 2, hy + DYS[dir] + ey * 2, 2, 2, 0x000000);
      g.fillRect(hx + DXS[dir] - ex * 2, hy + DYS[dir] - ey * 2, 2, 2, 0x000000);
    }
    parts.draw(g, BG);
  }
};
} // namespace

Demo *makeSnake() { return new Snake(); }
