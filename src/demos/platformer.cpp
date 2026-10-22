// 横スクロールのアクション。地形はその場で作り続け、主人公は穴・土管・敵の手前で跳ぶ
#include "demo.h"

namespace
{
const int TS = 8;            // タイルの一辺
const int GROUND_ROW = 24;   // 地面の一番上の行 (y=192)
const int ROWS_N = 28;       // 224px
const int RING = 64;         // 覚えておく列の数
const float GRAV = 0.32f, JUMP_V = -5.0f, RUN_V = 1.3f;
const uint32_t SKY = 0x6890F0;

// 主人公 (8x12)。R 帽子と服、S 肌、B つなぎ、K 髪と靴
const char *const HERO_RUN1[12] = {"..RRRR..", ".RRRRRRR", ".KKSSKS.", ".KSSSSSS", "..SSSKK.", "..SSSS..",
                                   ".RRBRR..", "RRRBBRRR", "SSBBBBSS", "..BBBB..", ".BB..BB.", "KK....KK"};
const char *const HERO_RUN2[12] = {"..RRRR..", ".RRRRRRR", ".KKSSKS.", ".KSSSSSS", "..SSSKK.", "..SSSS..",
                                   "..RBRR..", ".RRBBRR.", ".SBBBBS.", "..BBBB..", "..BBB...", "..KKK..."};
const char *const HERO_JUMP[12] = {"..RRRR.S", ".RRRRRRR", ".KKSSKS.", ".KSSSSSS", "..SSSKK.", "..SSSS..",
                                   ".RRBRRR.", "RRRBBR..", "SSBBBB..", "..BBBB..", ".BBB.BB.", "KK....KK"};
const PalEntry HERO_PAL[] = {{'R', 0xE03828}, {'S', 0xFFC890}, {'B', 0x2850D0}, {'K', 0x603010}};

// 歩く敵 (8x8)
const char *const WALK1[8] = {"..KKKK..", ".KKKKKK.", "KWDKKDWK", "KKKKKKKK", ".KKKKKK.", "..TTTT..", ".TT..TT.", "DD....DD"};
const char *const WALK2[8] = {"..KKKK..", ".KKKKKK.", "KWDKKDWK", "KKKKKKKK", ".KKKKKK.", "..TTTT..", ".TTTTTT.", "..DD.DD."};
const char *const FLAT[4] = {"KKKKKKKK", "KWDKKDWK", "TTTTTTTT", "DD....DD"};
const PalEntry WALK_PAL[] = {{'K', 0xA04818}, {'W', 0xFFFFFF}, {'D', 0x101010}, {'T', 0xF0C090}};

struct Col
{
  bool ground;
  uint8_t pipe;  // 0 なし、1 左半分、2 右半分
  uint8_t pipeH; // 土管の高さ (タイル)
  uint8_t stair; // 階段の高さ (タイル)
  uint8_t blockRow, block; // block: 0 なし、1 れんが、2 はてな、3 使用済み
  uint8_t coinRow;         // 0 なし
};

struct Enemy
{
  float x, y, vx, vy;
  bool on;
  int flatT; // 踏まれてからの残り
};

class Platformer : public Demo
{
  Col cols[RING];
  int genCol;
  Enemy en[5];
  float hx, hy, hvy;
  bool onGround, dying;
  int deadT, animT, camX;
  int score, coins;
  int bumpCol, bumpT;   // 下から叩かれたブロック
  int popX, popY, popT; // 飛び出したコイン
  Particles fx;

  Col &col(int c) { return cols[((c % RING) + RING) % RING]; }

  bool solidTile(int c, int r)
  {
    if (r < 0)
      return false;
    if (r >= ROWS_N)
      return col(c).ground;
    Col &k = col(c);
    if (r >= GROUND_ROW)
      return k.ground;
    if (k.pipe && r >= GROUND_ROW - k.pipeH)
      return true;
    if (k.stair && r >= GROUND_ROW - k.stair)
      return true;
    return k.block && r == k.blockRow;
  }
  bool solidAt(float x, float y) { return solidTile((int)floorf(x / TS), (int)floorf(y / TS)); }

  void flat(int n, bool enemy)
  {
    for (int i = 0; i < n; i++)
      col(genCol++) = {true, 0, 0, 0, 0, 0, 0};
    if (enemy)
      for (Enemy &e : en)
        if (!e.on)
        {
          e = {(float)(genCol - n / 2) * TS, (float)(GROUND_ROW * TS - 8), -0.5f, 0, true, 0};
          break;
        }
  }

  void genSegment()
  {
    int kind = genCol < 24 ? 0 : irand(0, 6);
    switch (kind)
    {
    case 0:
      flat(irand(4, 8), genCol > 24 && irand(0, 1));
      break;
    case 1: // 穴
      flat(2, false);
      for (int i = irand(2, 3); i > 0; i--)
        col(genCol++) = {false, 0, 0, 0, 0, 0, 0};
      flat(3, false);
      break;
    case 2: // 土管
    {
      flat(3, false);
      uint8_t h = irand(2, 4);
      col(genCol++) = {true, 1, h, 0, 0, 0, 0};
      col(genCol++) = {true, 2, h, 0, 0, 0, 0};
      flat(3, irand(0, 2) == 0);
      break;
    }
    case 3: // 空中のブロック
    {
      int start = genCol;
      flat(8, irand(0, 1));
      int row = GROUND_ROW - 5;
      for (int i = 2; i < 7; i++)
      {
        col(start + i).blockRow = row;
        col(start + i).block = (i % 2 == 0) ? 1 : 2;
      }
      break;
    }
    case 4: // 階段 (上って下りる)
    {
      int h = irand(2, 4);
      for (int i = 1; i <= h; i++)
        col(genCol++) = {true, 0, 0, (uint8_t)i, 0, 0, 0};
      for (int i = h; i >= 1; i--)
        col(genCol++) = {true, 0, 0, (uint8_t)i, 0, 0, 0};
      flat(3, false);
      break;
    }
    case 5: // コインの列
    {
      int start = genCol;
      flat(7, false);
      for (int i = 1; i < 6; i++)
        col(start + i).coinRow = GROUND_ROW - 4 - (i == 1 || i == 5 ? 0 : 1);
      break;
    }
    default: // はてなブロック 1 つ
    {
      int start = genCol;
      flat(5, irand(0, 1));
      col(start + 2).blockRow = GROUND_ROW - 5;
      col(start + 2).block = 2;
      break;
    }
    }
  }

  void respawn()
  {
    int c = camX / TS + 3;
    while (!col(c).ground || col(c).pipe || col(c).stair)
      c++;
    hx = c * TS;
    hy = GROUND_ROW * TS - 12 - 20;
    hvy = 0;
    dying = false;
    deadT = 0;
  }

  void die()
  {
    dying = true;
    hvy = -4.5f;
    deadT = 0;
  }

  void bump(int c, int r)
  {
    Col &k = col(c);
    if (!k.block || k.blockRow != r)
      return;
    bumpCol = c;
    bumpT = 8;
    if (k.block == 2)
    {
      k.block = 3;
      coins++;
      score += 200;
      popX = c * TS + 4;
      popY = r * TS - 4;
      popT = 18;
    }
    for (Enemy &e : en) // 上に乗っている敵ははじく
      if (e.on && !e.flatT && fabsf(e.x + 4 - (c * TS + 4)) < 8 && fabsf(e.y + 8 - r * TS) < 3)
      {
        e.flatT = 20;
        score += 100;
      }
  }

  void heroPhysics()
  {
    hvy = min(hvy + GRAV, 5.0f);
    // 横: 壁に当たったら止まる (跳ぶのは考える側の役目)
    float nx = hx + RUN_V;
    if (!(solidAt(nx + 7, hy + 1) || solidAt(nx + 7, hy + 6) || solidAt(nx + 7, hy + 11)))
      hx = nx;
    // 縦
    float ny = hy + hvy;
    onGround = false;
    if (hvy > 0)
    {
      if (solidAt(hx + 1, ny + 11) || solidAt(hx + 6, ny + 11))
      {
        ny = floorf((ny + 11) / TS) * TS - 12;
        hvy = 0;
        onGround = true;
      }
    }
    else if (solidAt(hx + 1, ny) || solidAt(hx + 6, ny))
    {
      int r = (int)floorf(ny / TS);
      bump((int)floorf((hx + 4) / TS), r);
      ny = (r + 1) * TS;
      hvy = 0.5f;
    }
    hy = ny;
  }

  void think()
  {
    if (!onGround)
      return;
    bool jump = false;
    // 前の壁 (土管・階段)
    for (int d = 2; d <= 14 && !jump; d += 2)
      if (solidAt(hx + 8 + d, hy + 10) || solidAt(hx + 8 + d, hy + 2))
        jump = true;
    // 足元の先が穴
    int fc = (int)floorf((hx + 10) / TS);
    if (!col(fc).ground)
      jump = true;
    // 敵
    for (Enemy &e : en)
      if (e.on && !e.flatT && e.x - (hx + 8) > 2 && e.x - (hx + 8) < 20 && fabsf(e.y - (hy + 4)) < 10)
        jump = true;
    // はてなブロックが少し先の頭上にあれば叩きに行く
    int bc = (int)floorf((hx + 4) / TS) + 1;
    if (col(bc).block == 2 && irand(0, 2) == 0)
      jump = true;
    if (jump)
      hvy = JUMP_V;
  }

public:
  const char *name() const override { return "RUNNER"; }

  void reset() override
  {
    genCol = 0;
    camX = 0;
    for (Enemy &e : en)
      e.on = false;
    while (genCol < 24)
      genSegment();
    score = coins = 0;
    bumpT = popT = 0;
    animT = 0;
    fx.clear();
    hx = 24;
    hy = GROUND_ROW * TS - 12;
    hvy = 0;
    dying = false;
    deadT = 0;
  }

  void step() override
  {
    animT++;
    fx.step(0.1f);
    while (genCol < camX / TS + 20)
      genSegment();
    if (bumpT)
      bumpT--;
    if (popT)
      popT--;

    // 敵
    for (Enemy &e : en)
    {
      if (!e.on)
        continue;
      if (e.x < camX - 16)
      {
        e.on = false;
        continue;
      }
      if (e.flatT)
      {
        if (--e.flatT == 0)
          e.on = false;
        continue;
      }
      if (e.x > camX + GAME_W + 8)
        continue; // 画面に入るまでは待つ
      float nx = e.x + e.vx;
      if (solidAt(nx + (e.vx > 0 ? 7 : 0), e.y + 4))
        e.vx = -e.vx;
      else
        e.x = nx;
      e.vy = min(e.vy + GRAV, 4.0f);
      float ny = e.y + e.vy;
      if (solidAt(e.x + 1, ny + 7) || solidAt(e.x + 6, ny + 7))
      {
        ny = floorf((ny + 7) / TS) * TS - 8;
        e.vy = 0;
      }
      e.y = ny;
      if (e.y > ROWS_N * TS + 16)
        e.on = false;
    }

    if (dying)
    {
      hvy += GRAV;
      hy += hvy;
      if (++deadT > 70)
        respawn();
      return;
    }
    think();
    heroPhysics();

    // 敵とぶつかる: 上からなら踏む、横ならやられる
    for (Enemy &e : en)
      if (e.on && !e.flatT && hx + 7 > e.x && hx < e.x + 8 && hy + 12 > e.y && hy < e.y + 8)
      {
        if (hvy > 0 && hy + 12 - e.y < 7)
        {
          e.flatT = 20;
          hvy = -3.5f;
          score += 100;
          fx.burst(e.x + 4 - camX, e.y + 4, 0xFFFFFF, 6, 1.0f, 10);
        }
        else
          die();
      }
    // コイン
    for (int c = (int)(hx / TS); c <= (int)((hx + 7) / TS); c++)
    {
      Col &k = col(c);
      if (k.coinRow && hy < (k.coinRow + 1) * TS && hy + 12 > k.coinRow * TS)
      {
        fx.burst(c * TS + 4 - camX, k.coinRow * TS + 4, 0xFFD040, 5, 1.0f, 10);
        k.coinRow = 0;
        coins++;
        score += 50;
      }
    }
    if (hy > ROWS_N * TS + 8)
    {
      dying = true;
      deadT = 30;
    }
    camX = max(camX, (int)hx - 40);
  }

  void drawTile(LGFX_Sprite &g, int c, int r, int sx, int sy)
  {
    Col &k = col(c);
    if (r >= GROUND_ROW)
    {
      g.fillRect(sx, sy, TS, TS, 0xC06830);
      g.drawFastHLine(sx, sy + (r == GROUND_ROW ? 0 : 7), TS, r == GROUND_ROW ? 0xF0A060 : 0x803818);
      g.drawFastVLine(sx + ((c + r) % 2 ? 2 : 6), sy + 1, 6, 0x803818);
      return;
    }
    if (k.pipe && r >= GROUND_ROW - k.pipeH)
    {
      bool top = r == GROUND_ROW - k.pipeH;
      int x = sx + (top ? (k.pipe == 1 ? -1 : 0) : 0), w = TS + (top ? 1 : 0);
      g.fillRect(x, sy, w, TS, 0x30B030);
      if (k.pipe == 1)
        g.drawFastVLine(x + 2, sy, TS, 0x90F070);
      else
        g.drawFastVLine(x + w - 2, sy, TS, 0x107010);
      if (top)
        g.drawFastHLine(x, sy + TS - 1, w, 0x107010);
      return;
    }
    if (k.stair && r >= GROUND_ROW - k.stair)
    {
      g.fillRect(sx, sy, TS, TS, 0xC88850);
      g.drawFastHLine(sx, sy, TS, 0xF0C090);
      g.drawFastVLine(sx, sy, TS, 0xF0C090);
      g.drawFastHLine(sx, sy + 7, TS, 0x704020);
      g.drawFastVLine(sx + 7, sy, TS, 0x704020);
      return;
    }
    if (k.block && r == k.blockRow)
    {
      if (bumpT && bumpCol == c)
        sy -= bumpT > 4 ? 8 - bumpT : bumpT; // 叩かれて少し跳ねる
      if (k.block == 1)
      {
        g.fillRect(sx, sy, TS, TS, 0xB85020);
        g.drawFastHLine(sx, sy + 3, TS, 0x401808);
        g.drawFastHLine(sx, sy + 7, TS, 0x401808);
        g.drawFastVLine(sx + 3, sy, 3, 0x401808);
        g.drawFastVLine(sx + 6, sy + 4, 3, 0x401808);
      }
      else if (k.block == 2)
      {
        uint32_t c2 = (animT / 8) % 4 == 3 ? 0xFFD870 : 0xF0B030;
        g.fillRect(sx, sy, TS, TS, c2);
        g.drawRect(sx, sy, TS, TS, 0x905010);
        hud(g, sx + 1, sy, 0x703808, "?");
      }
      else
      {
        g.fillRect(sx, sy, TS, TS, 0x8C5A38);
        g.drawRect(sx, sy, TS, TS, 0x502010);
      }
    }
  }

  void draw(LGFX_Sprite &g) override
  {
    g.fillScreen(SKY);
    // 遠くの雲と丘 (ゆっくり動く)
    for (int i = -1; i < 4; i++)
    {
      int cx = i * 56 - (camX / 3) % 56 + 20, cy = 30 + (i & 1) * 14;
      g.fillEllipse(cx, cy, 10, 5, 0xF0F6FF);
      g.fillEllipse(cx + 8, cy - 3, 8, 6, 0xF0F6FF);
      g.fillEllipse(cx + 15, cy, 8, 4, 0xF0F6FF);
    }
    for (int i = -1; i < 3; i++)
    {
      int hxp = i * 80 - (camX / 2) % 80 + 30;
      g.fillCircle(hxp, GROUND_ROW * TS + 2, 22, 0x48A848);
      g.fillCircle(hxp + 4, GROUND_ROW * TS - 14, 3, 0x307830);
    }

    int c0 = camX / TS;
    for (int c = c0; c <= c0 + GAME_W / TS + 1; c++)
    {
      int sx = c * TS - camX;
      for (int r = 0; r < ROWS_N; r++)
        if (solidTile(c, r))
          drawTile(g, c, r, sx, r * TS);
      Col &k = col(c);
      if (k.coinRow)
      {
        int w = 1 + (int)(1.5f + 1.5f * sinf(animT * 0.2f + c));
        g.fillEllipse(sx + 4, k.coinRow * TS + 4, w, 3, 0xFFD040);
      }
    }
    if (popT)
    {
      int y = popY - (18 - popT) * 2 + max(0, 12 - popT) * 2;
      g.fillEllipse(popX - camX, y, 2, 3, 0xFFD040);
    }

    for (Enemy &e : en)
      if (e.on && e.x < camX + GAME_W)
      {
        int sx = (int)e.x - camX, sy = (int)e.y;
        if (e.flatT)
          blit(g, sx, sy + 4, FLAT, 4, WALK_PAL, 4);
        else
          blit(g, sx, sy, (animT / 8) % 2 ? WALK1 : WALK2, 8, WALK_PAL, 4);
      }

    if (!dying || (deadT % 4) < 3)
    {
      const char *const *spr = !onGround ? HERO_JUMP : (animT / 5) % 2 ? HERO_RUN1 : HERO_RUN2;
      blit(g, (int)hx - camX, (int)hy, spr, 12, HERO_PAL, 4);
    }
    fx.draw(g, SKY);

    char s[24];
    snprintf(s, sizeof(s), "%06d", score);
    hud(g, 5, 3, 0x203050, s);
    hud(g, 4, 2, 0xFFFFFF, s);
    snprintf(s, sizeof(s), "@x%02d", coins % 100);
    hud(g, GAME_W - 3, 3, 0x203050, s, TR_DATUM);
    hud(g, GAME_W - 4, 2, 0xFFE060, s, TR_DATUM);
  }
};
} // namespace

Demo *makePlatformer() { return new Platformer(); }
