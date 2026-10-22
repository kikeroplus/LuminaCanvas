// ブロック崩し。パドルは球の落ちる場所を先読みして待ち、当てる位置を少しずつ変えて角度をつける
#include "demo.h"

namespace
{
const int COLS = 9, ROWS = 8, BW = 12, BH = 6, BX0 = 2, BY0 = 30;
const int PADDLE_Y = 212, PW = 20, WALL = 2;
const uint32_t BG = 0x04060C;
const uint32_t ROW_C[ROWS] = {0xFF4060, 0xFF7040, 0xFFA030, 0xF0D030, 0x60E060, 0x30D0C0, 0x4090FF, 0xA070FF};

class Breakout : public Demo
{
  uint8_t bricks[ROWS][COLS];
  float bx, by, vx, vy, speed;
  float pxp, aimOff; // パドルの中心、当てたい位置のずれ
  int score, level, serveT, clearT;
  Particles fx;

  int brickAt(float x, float y, int *r, int *c) const
  {
    int cc = (int)((x - BX0) / BW), rr = (int)((y - BY0) / BH);
    if (x < BX0 || y < BY0 || cc < 0 || cc >= COLS || rr < 0 || rr >= ROWS || !bricks[rr][cc])
      return 0;
    *r = rr;
    *c = cc;
    return 1;
  }

  void breakBrick(int r, int c)
  {
    bricks[r][c] = 0;
    score += (ROWS - r) * 10;
    fx.burst(BX0 + c * BW + BW / 2, BY0 + r * BH + BH / 2, ROW_C[r], 8, 1.4f, 16);
  }

  // 球がパドルの高さに来たときの x を、壁での跳ね返りも入れて求める
  float predictX() const
  {
    if (vy <= 0)
      return GAME_W / 2;
    float t = (PADDLE_Y - by) / vy;
    float x = bx + vx * t;
    float lo = WALL + 2, hi = GAME_W - WALL - 2, span = hi - lo;
    float m = fmodf(x - lo, 2 * span);
    if (m < 0)
      m += 2 * span;
    return lo + (m < span ? m : 2 * span - m);
  }

  void serve()
  {
    bx = pxp;
    by = PADDLE_Y - 4;
    float a = frand(-0.6f, 0.6f);
    vx = sinf(a) * speed;
    vy = -cosf(a) * speed;
    serveT = 30;
  }

  void newLevel()
  {
    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        bricks[r][c] = 1;
    speed = min(2.2f + level * 0.25f, 3.6f);
    level++;
    serve();
  }

public:
  const char *name() const override { return "BREAKOUT"; }

  void reset() override
  {
    score = 0;
    level = 0;
    pxp = GAME_W / 2;
    aimOff = 0;
    clearT = 0;
    fx.clear();
    newLevel();
  }

  void step() override
  {
    fx.step(0.05f);
    if (clearT)
    {
      if (--clearT == 0)
        newLevel();
      return;
    }
    if (serveT)
    {
      serveT--;
      bx = pxp;
      return;
    }

    // パドル: 落ちてくる球の行き先へ
    float want = vy > 0 ? predictX() - aimOff : GAME_W / 2 + (bx - GAME_W / 2) * 0.5f;
    want = constrain(want, WALL + PW / 2, GAME_W - WALL - PW / 2);
    pxp += constrain(want - pxp, -3.0f, 3.0f);

    // x と y を別々に動かして、どちらの向きでぶつかったかを分ける
    int r, c;
    bx += vx;
    if (bx < WALL + 1 || bx > GAME_W - WALL - 2)
    {
      vx = -vx;
      bx += vx;
    }
    else if (brickAt(bx, by, &r, &c))
    {
      breakBrick(r, c);
      vx = -vx;
      bx += vx;
    }
    by += vy;
    if (by < 14)
    {
      vy = -vy;
      by += vy;
    }
    else if (brickAt(bx, by, &r, &c))
    {
      breakBrick(r, c);
      vy = -vy;
      by += vy;
    }
    else if (vy > 0 && by >= PADDLE_Y - 1 && by <= PADDLE_Y + 3 && fabsf(bx - pxp) <= PW / 2 + 1)
    {
      float k = (bx - pxp) / (PW / 2.0f); // -1〜1: 端に当たるほど斜めに返す
      float a = constrain(k, -1.0f, 1.0f) * 1.05f;
      vx = sinf(a) * speed;
      vy = -cosf(a) * speed;
      by = PADDLE_Y - 2;
      aimOff = frand(-7, 7);
    }
    else if (by > GAME_H)
      serve(); // 落とした

    bool any = false;
    for (int rr = 0; rr < ROWS && !any; rr++)
      for (int cc = 0; cc < COLS && !any; cc++)
        any = bricks[rr][cc];
    if (!any)
      clearT = 60;
  }

  void draw(LGFX_Sprite &g) override
  {
    g.fillScreen(BG);
    char s[16];
    snprintf(s, sizeof(s), "%05d", score);
    hud(g, 4, 2, C_TEXT, s);
    snprintf(s, sizeof(s), "LV%d", level);
    hud(g, GAME_W - 4, 2, C_DIM, s, TR_DATUM);
    g.fillRect(0, 12, GAME_W, 2, 0x203040);
    g.fillRect(0, 12, WALL, GAME_H, 0x203040);
    g.fillRect(GAME_W - WALL, 12, WALL, GAME_H, 0x203040);

    for (int r = 0; r < ROWS; r++)
      for (int c = 0; c < COLS; c++)
        if (bricks[r][c])
        {
          int x = BX0 + c * BW, y = BY0 + r * BH;
          g.fillRect(x, y, BW - 1, BH - 1, ROW_C[r]);
          g.drawFastHLine(x, y, BW - 1, mix(ROW_C[r], 0xFFFFFF, 0.4f));
        }
    fx.draw(g, BG);
    g.fillRoundRect((int)pxp - PW / 2, PADDLE_Y, PW, 4, 2, 0xD8E6F0);
    g.fillSmoothCircle((int)bx, (int)by, 2, 0xFFFFFF);
    if (clearT)
      hud(g, GAME_W / 2, 120, C_YELLOW, "CLEAR!", TC_DATUM);
  }
};
} // namespace

Demo *makeBreakout() { return new Breakout(); }
