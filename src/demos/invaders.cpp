// 侵略者の隊列を撃つゲーム。砲台は一番近い列の下へ行って撃ち、落ちてくる弾はよける
#include "demo.h"

namespace
{
const char *const ALIEN_A[2][8] = {
    {"...XX...", "..XXXX..", ".XXXXXX.", "XX.XX.XX", "XXXXXXXX", "..X..X..", ".X.XX.X.", "X.X..X.X"},
    {"...XX...", "..XXXX..", ".XXXXXX.", "XX.XX.XX", "XXXXXXXX", ".X.XX.X.", "X......X", ".X....X."},
};
const char *const ALIEN_B[2][8] = {
    {"..X....X..", "...X..X...", "..XXXXXX..", ".XX.XX.XX.", "XXXXXXXXXX", "X.XXXXXX.X", "X.X....X.X", "..XX..XX.."},
    {"..X....X..", "X..X..X..X", "X.XXXXXX.X", "XXX.XX.XXX", "XXXXXXXXXX", ".XXXXXXXX.", "..X....X..", ".X......X."},
};
const char *const ALIEN_C[2][8] = {
    {"...XXXX...", ".XXXXXXXX.", "XXXXXXXXXX", "XXX.XX.XXX", "XXXXXXXXXX", "..XX..XX..", ".XX.XX.XX.", "XX......XX"},
    {"...XXXX...", ".XXXXXXXX.", "XXXXXXXXXX", "XXX.XX.XXX", "XXXXXXXXXX", "...XXXX...", "..XX..XX..", "...X..X..."},
};
const char *const BOOM[7] = {"X..X..X..X", ".X..X..X..", "..X....X..", "XX......XX", "..X....X..", ".X..X..X..", "X..X..X..X"};
const char *const CANNON[6] = {".....X.....", "....XXX....", ".XXXXXXXXX.", "XXXXXXXXXXX", "XXXXXXXXXXX", "XXXXXXXXXXX"};
const char *const UFO[7] = {".....XXXXXX.....", "...XXXXXXXXXX...", "..XXXXXXXXXXXX..", ".XX.XX.XX.XX.XX.",
                            "XXXXXXXXXXXXXXXX", "..XXX..XX..XXX..", "...X........X..."};
const char *const SHIELD[10] = {"....XXXXXXXXXX....", "...XXXXXXXXXXXX...", "..XXXXXXXXXXXXXX..", ".XXXXXXXXXXXXXXXX.",
                                "XXXXXXXXXXXXXXXXXX", "XXXXXXXXXXXXXXXXXX", "XXXXXXXXXXXXXXXXXX", "XXXXX........XXXXX",
                                "XXXX..........XXXX", "XXXX..........XXXX"};

const int COLS = 6, ROWS = 5, DX = 14, DY = 13;
const int PY = 210, SHY = 186, SH_W = 18, SH_H = 10;
const int SHX[3] = {9, 47, 85};
const uint32_t BG = 0x020408;
const uint32_t ROW_C[ROWS] = {0xFF60D0, 0x40E0FF, 0x40E0FF, 0x70FF80, 0x70FF80};

class Invaders : public Demo
{
  bool alive[ROWS][COLS];
  uint8_t boom[ROWS][COLS];
  int fx, fy, dir, marchT, anim, wave;
  float px;
  int bx, by; // 自分の弾 (by < 0 はなし)
  struct Bomb
  {
    int x, y;
    bool on;
  } bombs[3];
  bool shield[3][SH_H][SH_W];
  int ufoX, ufoT, ufoBoom;
  int score, lives, deadT, overT;
  Particles fx_;

  int aw(int r) const { return r == 0 ? 8 : 10; }
  int ax(int r, int c) const { return fx + c * DX + (r == 0 ? 1 : 0); }
  int ay(int r) const { return fy + r * DY; }
  int aliveN() const
  {
    int n = 0;
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        n += alive[r][c];
    return n;
  }

  void newWave()
  {
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
      {
        alive[r][c] = true;
        boom[r][c] = 0;
      }
    fx = 8;
    fy = 26 + min(wave, 4) * 6;
    dir = 1;
    marchT = 0;
    by = -1;
    for (Bomb &b : bombs)
      b.on = false;
    wave++;
  }

  // 盾に当たったら周りを少し崩して true
  bool hitShield(int x, int y)
  {
    for (int s = 0; s < 3; s++)
    {
      int lx = x - SHX[s], ly = y - SHY;
      if (lx < 0 || lx >= SH_W || ly < 0 || ly >= SH_H || !shield[s][ly][lx])
        continue;
      for (int dy = -2; dy <= 2; dy++)
        for (int dx = -2; dx <= 2; dx++)
        {
          int xx = lx + dx, yy = ly + dy;
          if (xx >= 0 && xx < SH_W && yy >= 0 && yy < SH_H && irand(0, 3))
            shield[s][yy][xx] = false;
        }
      return true;
    }
    return false;
  }

public:
  const char *name() const override { return "INVADERS"; }

  void reset() override
  {
    wave = 0;
    newWave();
    for (int s = 0; s < 3; s++)
      for (int y = 0; y < SH_H; y++)
        for (int x = 0; x < SH_W; x++)
          shield[s][y][x] = SHIELD[y][x] == 'X';
    px = GAME_W / 2;
    ufoX = -100;
    ufoT = irand(200, 500);
    ufoBoom = 0;
    score = 0;
    lives = 2;
    deadT = overT = 0;
    fx_.clear();
  }

  void step() override
  {
    fx_.step();
    if (overT)
    {
      if (++overT > 90)
        reset();
      return;
    }
    // 隊列の行進 (減るほど速い)
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (boom[r][c])
          boom[r][c]--;
    int n = aliveN();
    if (n == 0)
    {
      newWave();
      return;
    }
    if (++marchT >= 2 + n * 10 / 30)
    {
      marchT = 0;
      anim ^= 1;
      int minC = COLS, maxC = -1, maxR = -1;
      for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
          if (alive[r][c])
          {
            minC = min(minC, c);
            maxC = max(maxC, c);
            maxR = max(maxR, r);
          }
      int left = fx + minC * DX, right = fx + maxC * DX + 10;
      if (right + 2 * dir > GAME_W - 2 || left + 2 * dir < 2)
      {
        fy += 5;
        dir = -dir;
      }
      else
        fx += 2 * dir;
      if (ay(maxR) + 8 >= SHY)
        overT = 1; // 下まで来られた
    }

    // UFO
    if (ufoX > -100)
    {
      ufoX += 1;
      if (ufoX > GAME_W)
        ufoX = -100;
    }
    else if (--ufoT <= 0)
    {
      ufoX = -16;
      ufoT = irand(400, 800);
    }
    if (ufoBoom)
      ufoBoom--;

    // 敵の弾
    if (irand(0, 99) < 4)
      for (Bomb &b : bombs)
        if (!b.on)
        {
          int c = irand(0, COLS - 1);
          for (int r = ROWS - 1; r >= 0; r--)
            if (alive[r][c])
            {
              b = {ax(r, c) + aw(r) / 2, ay(r) + 8, true};
              break;
            }
          break;
        }
    for (Bomb &b : bombs)
    {
      if (!b.on)
        continue;
      b.y += 2;
      if (b.y > GAME_H || hitShield(b.x, b.y + 3))
        b.on = false;
      else if (!deadT && b.y + 4 >= PY && b.y <= PY + 6 && fabsf(b.x - px) <= 5)
      {
        b.on = false;
        deadT = 70;
        fx_.burst(px, PY + 3, 0x60FF60, 24, 1.6f, 26);
        if (--lives < 0)
          overT = 1;
      }
    }

    // 自分
    if (deadT)
    {
      deadT--;
      return;
    }
    float target = px;
    int bestD = 999;
    for (int c = 0; c < COLS; c++)
      for (int r = ROWS - 1; r >= 0; r--)
        if (alive[r][c])
        {
          int cx = ax(r, c) + aw(r) / 2 + dir * 4; // 行進の先を狙う
          if (abs(cx - (int)px) < bestD)
          {
            bestD = abs(cx - (int)px);
            target = cx;
          }
          break;
        }
    if (ufoX > 0 && ufoX < GAME_W - 30)
      target = ufoX + 14;
    for (Bomb &b : bombs) // 頭の上の弾からは逃げる
      if (b.on && b.y > PY - 50 && fabsf(b.x - px) < 9)
        target = px + (b.x < px ? 14 : -14);
    target = constrain(target, 8, GAME_W - 8);
    px += constrain(target - px, -1.5f, 1.5f);

    bool lined = fabsf(target - px) < 3;
    if (by < 0 && lined)
    {
      bx = px;
      by = PY - 4;
    }
    if (by >= 0)
    {
      by -= 5;
      if (by < 10 || hitShield(bx, by))
        by = -1;
      else if (ufoX > -16 && by < 22 && bx >= ufoX && bx < ufoX + 16)
      {
        score += 100;
        ufoBoom = 30;
        fx_.burst(ufoX + 8, 17, 0xFF4040, 20, 1.8f);
        ufoX = -100;
        by = -1;
      }
      else
        for (int r = 0; r < ROWS && by >= 0; r++)
          for (int c = 0; c < COLS; c++)
            if (alive[r][c] && bx >= ax(r, c) && bx < ax(r, c) + aw(r) && by >= ay(r) && by < ay(r) + 8)
            {
              alive[r][c] = false;
              boom[r][c] = 8;
              score += (ROWS - r) * 10;
              fx_.burst(ax(r, c) + 5, ay(r) + 4, ROW_C[r], 6, 1.2f, 12);
              by = -1;
              break;
            }
    }
  }

  void draw(LGFX_Sprite &g) override
  {
    g.fillScreen(BG);
    char s[16];
    snprintf(s, sizeof(s), "%05d", score);
    hud(g, 4, 2, C_TEXT, s);
    for (int i = 0; i < lives; i++)
      g.fillRect(GAME_W - 10 - i * 9, 4, 7, 4, 0x60FF60);

    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
      {
        const char *const *spr = r == 0 ? ALIEN_A[anim] : r < 3 ? ALIEN_B[anim] : ALIEN_C[anim];
        if (alive[r][c])
          blit(g, ax(r, c), ay(r), spr, 8, ROW_C[r]);
        else if (boom[r][c])
          blit(g, fx + c * DX, ay(r), BOOM, 7, 0xFFFFFF);
      }
    if (ufoX > -16)
      blit(g, ufoX, 14, UFO, 7, 0xFF4040);
    if (ufoBoom)
      hud(g, 56, 14, 0xFF4040, "100", TC_DATUM);

    for (int sI = 0; sI < 3; sI++)
      for (int y = 0; y < SH_H; y++)
        for (int x = 0; x < SH_W; x++)
          if (shield[sI][y][x])
            g.drawPixel(SHX[sI] + x, SHY + y, 0x40E060);

    if (!deadT && !overT)
      blit(g, (int)px - 5, PY, CANNON, 6, 0x60FF60);
    if (by >= 0)
      g.fillRect(bx, by, 1, 4, 0xFFFFFF);
    for (Bomb &b : bombs)
      if (b.on)
        for (int i = 0; i < 4; i++)
          g.drawPixel(b.x + ((b.y / 2 + i) % 2), b.y + i, 0xFFE080);
    g.drawFastHLine(0, PY + 9, GAME_W, 0x40E060);
    fx_.draw(g, BG);
    if (overT > 10)
      hud(g, GAME_W / 2, 110, C_RED, "GAME OVER", TC_DATUM);
  }
};
} // namespace

Demo *makeInvaders() { return new Invaders(); }
