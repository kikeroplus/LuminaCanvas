// 落ち物パズル。置き場所は盤面の点数 (高さ・穴・でこぼこ・消えた行) で選ぶ
#include "demo.h"

namespace
{
// 4x4 の中のブロックを 16 ビットで表す (左上が最上位ビット)。回転 4 つずつ
const uint16_t SHAPES[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, // I
    {0x44C0, 0x8E00, 0x6440, 0x0E20}, // J
    {0x4460, 0x0E80, 0xC440, 0x2E00}, // L
    {0xCC00, 0xCC00, 0xCC00, 0xCC00}, // O
    {0x06C0, 0x8C40, 0x6C00, 0x4620}, // S
    {0x0E40, 0x4C40, 0x4E00, 0x4640}, // T
    {0x0C60, 0x4C80, 0xC600, 0x2640}, // Z
};
const uint32_t COLORS[8] = {0, 0x30D8F0, 0x3870F0, 0xF09030, 0xF0D030, 0x40D060, 0xB050E0, 0xF04050};
const int W = 10, H = 20, CS = 10, OX = 6, OY = 22;
const uint32_t BG = 0x05080E;

template <class F>
void cells(int p, int r, int px, int py, F fn)
{
  uint16_t m = SHAPES[p][r];
  int row = 0, col = 0;
  for (uint16_t bit = 0x8000; bit; bit >>= 1)
  {
    if (m & bit)
      fn(px + col, py + row);
    if (++col == 4)
    {
      col = 0;
      row++;
    }
  }
}

class Tetris : public Demo
{
  uint8_t b[H][W];
  int cur, rot, x, y, next;
  int tRot, tX; // 目標の向きと位置
  int tick, wait;
  int clearRows[4], clearN, clearT;
  int score, lines;
  int overT; // 0 でなければ終わりの演出中

  bool fits(const uint8_t (&bb)[H][W], int p, int r, int px, int py) const
  {
    bool ok = true;
    cells(p, r, px, py, [&](int cx, int cy) {
      if (cx < 0 || cx >= W || cy >= H || (cy >= 0 && bb[cy][cx]))
        ok = false;
    });
    return ok;
  }
  bool fits(int p, int r, int px, int py) const { return fits(b, p, r, px, py); }

  static float evaluate(const uint8_t (&bb)[H][W], int cleared)
  {
    int hs[W], agg = 0, holes = 0, bump = 0;
    for (int cx = 0; cx < W; cx++)
    {
      int h = 0;
      for (int cy = 0; cy < H; cy++)
        if (bb[cy][cx])
        {
          h = H - cy;
          break;
        }
      hs[cx] = h;
      agg += h;
      for (int cy = H - h; cy < H; cy++)
        if (!bb[cy][cx])
          holes++;
    }
    for (int cx = 0; cx + 1 < W; cx++)
      bump += abs(hs[cx] - hs[cx + 1]);
    return -0.51f * agg + 0.76f * cleared - 0.36f * holes - 0.18f * bump;
  }

  void plan()
  {
    float best = -1e9f;
    tRot = 0;
    tX = x;
    static uint8_t tmp[H][W];
    for (int r = 0; r < 4; r++)
      for (int px = -2; px < W; px++)
      {
        if (!fits(cur, r, px, 0))
          continue;
        int py = 0;
        while (fits(cur, r, px, py + 1))
          py++;
        memcpy(tmp, b, sizeof(b));
        cells(cur, r, px, py, [&](int cx, int cy) {
          if (cy >= 0)
            tmp[cy][cx] = 1;
        });
        int cleared = 0;
        for (int cy = 0; cy < H; cy++)
        {
          bool full = true;
          for (int cx = 0; cx < W && full; cx++)
            full = tmp[cy][cx];
          if (full)
          {
            cleared++;
            memmove(tmp[1], tmp[0], cy * W);
            memset(tmp[0], 0, W);
          }
        }
        float s = evaluate(tmp, cleared) + frand(0, 0.25f); // 少しだけ迷わせて毎回同じ展開にしない
        if (s > best)
        {
          best = s;
          tRot = r;
          tX = px;
        }
      }
  }

  void spawn()
  {
    cur = next;
    next = irand(0, 6);
    rot = 0;
    x = 3;
    y = 0;
    tick = 0;
    wait = 6;
    if (!fits(cur, rot, x, y))
    {
      overT = 1;
      return;
    }
    plan();
  }

  void lock()
  {
    cells(cur, rot, x, y, [&](int cx, int cy) {
      if (cy >= 0)
        b[cy][cx] = cur + 1;
    });
    score += 10;
    clearN = 0;
    for (int cy = 0; cy < H; cy++)
    {
      bool full = true;
      for (int cx = 0; cx < W && full; cx++)
        full = b[cy][cx];
      if (full && clearN < 4)
        clearRows[clearN++] = cy;
    }
    if (clearN)
    {
      static const int PTS[5] = {0, 100, 300, 500, 800};
      score += PTS[clearN];
      lines += clearN;
      clearT = 14;
    }
    else
      spawn();
  }

  void cell(LGFX_Sprite &g, int cx, int cy, uint32_t c)
  {
    int px = OX + cx * CS, py = OY + cy * CS;
    g.fillRect(px, py, CS - 1, CS - 1, c);
    g.drawFastHLine(px, py, CS - 1, mix(c, 0xFFFFFF, 0.45f));
    g.drawFastVLine(px, py, CS - 1, mix(c, 0xFFFFFF, 0.25f));
    g.drawFastHLine(px, py + CS - 2, CS - 1, mix(c, 0x000000, 0.35f));
  }

public:
  const char *name() const override { return "TETRIS"; }

  void reset() override
  {
    memset(b, 0, sizeof(b));
    score = lines = 0;
    overT = clearT = 0;
    next = irand(0, 6);
    spawn();
  }

  void step() override
  {
    if (overT)
    {
      if (++overT > 75)
        reset();
      return;
    }
    if (clearT)
    {
      if (--clearT == 0)
      {
        for (int i = 0; i < clearN; i++)
        {
          int cy = clearRows[i];
          memmove(b[1], b[0], cy * W);
          memset(b[0], 0, W);
        }
        spawn();
      }
      return;
    }
    if (wait)
    {
      wait--;
      return;
    }
    tick++;
    if (rot != tRot || x != tX)
    {
      if (tick % 3 == 0)
      {
        if (rot != tRot)
        {
          int nr = (rot + 1) % 4;
          if (fits(cur, nr, x, y))
            rot = nr;
          else
            tRot = rot; // 回せないなら今の向きで妥協
        }
        else
        {
          int nx = x + (tX > x ? 1 : -1);
          if (fits(cur, rot, nx, y))
            x = nx;
          else
            tX = x;
        }
      }
      if (tick % 12 == 0)
      {
        if (fits(cur, rot, x, y + 1))
          y++;
        else
          lock();
      }
    }
    else if (fits(cur, rot, x, y + 1))
      y++;
    else
      lock();
  }

  void draw(LGFX_Sprite &g) override
  {
    g.fillScreen(BG);
    char s[16];
    hud(g, OX, 2, C_DIM, "SCORE");
    snprintf(s, sizeof(s), "%d", score);
    hud(g, OX, 11, C_TEXT, s);
    hud(g, 52, 2, C_DIM, "LINES");
    snprintf(s, sizeof(s), "%d", lines);
    hud(g, 52, 11, C_TEXT, s);
    cells(next, 0, 0, 0, [&](int cx, int cy) { g.fillRect(90 + cx * 4, 3 + cy * 4, 3, 3, COLORS[next + 1]); });

    g.drawRect(OX - 2, OY - 2, W * CS + 3, H * CS + 3, C_LINE);
    for (int cy = 0; cy < H; cy++)
      for (int cx = 0; cx < W; cx++)
        g.drawPixel(OX + cx * CS + 4, OY + cy * CS + 4, 0x10202C);

    for (int cy = 0; cy < H; cy++)
    {
      bool flash = false;
      for (int i = 0; i < clearN && clearT; i++)
        flash |= clearRows[i] == cy;
      for (int cx = 0; cx < W; cx++)
      {
        if (!b[cy][cx])
          continue;
        uint32_t c = COLORS[b[cy][cx]];
        if (overT && cy >= H - overT / 3)
          c = 0x404850;
        if (flash)
          c = (clearT / 2) % 2 ? 0xFFFFFF : mix(c, 0xFFFFFF, 0.5f);
        cell(g, cx, cy, c);
      }
    }

    if (!overT && !clearT)
    {
      int gy = y;
      while (fits(cur, rot, x, gy + 1))
        gy++;
      cells(cur, rot, x, gy, [&](int cx, int cy) {
        if (cy >= 0)
          g.drawRect(OX + cx * CS, OY + cy * CS, CS - 1, CS - 1, mix(BG, COLORS[cur + 1], 0.35f));
      });
      cells(cur, rot, x, y, [&](int cx, int cy) {
        if (cy >= 0)
          cell(g, cx, cy, COLORS[cur + 1]);
      });
    }
    if (overT > 20)
    {
      g.fillRect(OX, 100, W * CS, 20, BG);
      hud(g, GAME_W / 2, 106, C_RED, "GAME OVER", TC_DATUM);
    }
  }
};
} // namespace

Demo *makeTetris() { return new Tetris(); }
