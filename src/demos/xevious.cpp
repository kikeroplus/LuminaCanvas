// ゼビウス風: 森や川の上を縦に流れながら、空の敵はザッパー、地上の敵は照準を合わせてブラスター (爆弾) で倒す。
// 自機は弾の通り道を先読みしてよけつつ、照準を地上の敵に重ねにいく。ときどき来る母艦は前に出て中心の核を狙う
#include "demo.h"

namespace
{
const char *const SOL[14] = {"......X......", ".....XWX.....", ".....XWX.....", "....XXWXX....", "....XWRWX....",
                             "...XXWRWXX...", "..XXXWWWXXX..", ".XXXXXXXXXXX.", "XXXXXWXWXXXXX", "XXXX.WXW.XXXX",
                             "XXX..XXX..XXX", "XX...X.X...XX", "X....F.F....X", ".....F.F....."};
const PalEntry SOL_PAL[] = {{'X', 0xE0E0E0}, {'W', 0x8890A8}, {'R', 0xE83030}, {'F', 0xFFA030}};
const char *const TORKAN[6] = {"...XXXXX...", ".XXXOOOXXX.", "XXXOOROOXXX", "XX.XXXXX.XX", "X...XXX...X", "....X.X...."};
const PalEntry TORKAN_PAL[] = {{'X', 0xB8A890}, {'O', 0x6A5040}, {'R', 0xFF4040}};

const int T = 8;                 // 地面の 1 マス
const int COLS = GAME_W / T;     // 14
const int ROWS = GAME_H / T + 2; // 画面より 1 行ずつ多め
const float SPEED = 0.6f;        // 地面が流れる速さ (px/フレーム)
const int PY0 = 196;             // 自機のふだんの高さ
const int SIGHT = 48;            // 照準は自機のこれだけ前
const int BOMB_T = 14;           // 爆弾が落ちるまでのフレーム数
const int BOSS_Y = 84;           // 母艦が止まる高さ (自機が前に出すぎないように低め)

enum Tile : uint8_t
{
  GRASS,
  FOREST,
  SAND,
  WATER,
  ROAD_V,
  ROAD_H,
  BRIDGE,
};

struct Air
{
  float x, y, vx, vy, spin, turnY; // turnY: トーロイドが切り返す高さ
  int type, t; // type 0 = トーロイド (輪)、1 = トーカン (止まって撃つ)
  bool on, turned, fired;
};
struct Ground
{
  float x, y;
  int type, t; // type 0 = バーラ (ピラミッド)、1 = ログラム (撃ってくる球)
  bool on;
};
struct Crater
{
  float x, y;
  bool on;
};
struct Shot
{
  float x, y, vx, vy;
  bool on;
};

class Xevious : public Demo
{
  uint8_t map[ROWS][COLS];
  int rowId[ROWS]; // 行の通し番号 (模様をずらさないため)
  float scrollY;   // 0〜T
  int rowCount;
  uint32_t seed;
  // 地形を作る途中の状態
  bool riverOn, roadOn;
  float riverC, riverW, riverDrift;
  int riverLen, roadLen, roadCol, groupT;

  Air air[10];
  Ground gnd[16];
  Crater craters[10];
  Shot mine[10], theirs[24];
  struct
  {
    bool on;
    int t;
    float x;
  } bomb;
  struct
  {
    bool on, dead;
    float y;
    int t, hp, hitT;
  } boss;
  float px, py;
  int fireT, waveT, bossT, invT, deadT, lives, score, frame, overT;
  Particles fx;

  // ---- 地形 ----
  float hash2(int x, int y) const
  {
    uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u + seed;
    h = (h ^ (h >> 13)) * 1274126177u;
    return ((h ^ (h >> 16)) & 0xFFFF) / 65535.0f;
  }
  float noise(float x, float y) const
  {
    int ix = floorf(x), iy = floorf(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx);
    fy = fy * fy * (3 - 2 * fy);
    float a = hash2(ix, iy), b = hash2(ix + 1, iy), c = hash2(ix, iy + 1), d = hash2(ix + 1, iy + 1);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
  }

  void genRow(uint8_t *row, int n)
  {
    for (int c = 0; c < COLS; c++)
    {
      float v = noise(c * 0.3f, n * 0.1f);
      row[c] = v > 0.64f ? FOREST : v < 0.28f ? SAND : GRASS;
    }
    // 川: くねりながら流れ、両岸は砂
    if (riverOn)
    {
      riverDrift = constrain(riverDrift + frand(-0.06f, 0.06f), -0.35f, 0.35f);
      riverC += riverDrift;
      if (riverC < 2 || riverC > COLS - 3)
        riverDrift = -riverDrift;
      riverC = constrain(riverC, 2.0f, COLS - 3.0f);
      riverW = constrain(riverW + frand(-0.15f, 0.15f), 1.6f, 4.0f);
      for (int c = 0; c < COLS; c++)
      {
        float d = fabsf(c + 0.5f - riverC);
        if (d < riverW / 2)
          row[c] = WATER;
        else if (d < riverW / 2 + 1)
          row[c] = SAND;
      }
      if (--riverLen <= 0)
        riverOn = false;
    }
    else if (irand(0, 120) == 0)
    {
      riverOn = true;
      riverC = frand(3, COLS - 4);
      riverW = frand(2, 3.5f);
      riverDrift = 0;
      riverLen = irand(40, 110);
    }
    // 道: 縦にまっすぐ。ときどき横切る道も
    if (roadOn)
    {
      row[roadCol] = row[roadCol] == WATER ? BRIDGE : ROAD_V;
      if (--roadLen <= 0)
        roadOn = false;
    }
    else if (irand(0, 90) == 0)
    {
      roadOn = true;
      roadCol = irand(1, COLS - 2);
      roadLen = irand(30, 80);
    }
    if (irand(0, 70) == 0)
      for (int c = 0; c < COLS; c++)
        row[c] = row[c] == WATER ? BRIDGE : row[c] == ROAD_V ? ROAD_V : ROAD_H;
  }

  void spawnGround(int col, float y, int type)
  {
    for (Ground &o : gnd)
      if (!o.on)
      {
        o = {col * T + T / 2.0f, y, type, irand(0, 60), true};
        return;
      }
  }

  void newRow()
  {
    memmove(&map[1][0], &map[0][0], sizeof(map[0]) * (ROWS - 1));
    memmove(&rowId[1], &rowId[0], sizeof(rowId[0]) * (ROWS - 1));
    rowId[0] = rowCount++;
    genRow(map[0], rowId[0]);
    // 地上の敵は水と道を避けて置く。バーラはまとめて (四角や横一列)
    float y = scrollY - T + T / 2.0f;
    auto land = [&](int c) { return map[0][c] == GRASS || map[0][c] == SAND || map[0][c] == FOREST; };
    if (boss.on)
      return;
    if (--groupT <= 0)
    {
      groupT = irand(14, 30);
      int c = irand(1, COLS - 4);
      if (land(c) && land(c + 2))
      {
        if (irand(0, 1))
          for (int k = 0; k < 4; k++)
            spawnGround(c + (k % 2) * 2, y - (k / 2) * 16, 0);
        else
          for (int k = 0; k < 3; k++)
            spawnGround(min(c + k * 2, COLS - 1), y, 0);
      }
    }
    else if (irand(0, 22) == 0)
    {
      int c = irand(1, COLS - 2);
      if (land(c))
        spawnGround(c, y, 1);
    }
  }

  void drawTile(LGFX_Sprite &g, int x, int y, uint8_t t, int c, int r)
  {
    uint32_t h = (uint32_t)(hash2(c, r) * 65535);
    switch (t)
    {
    case GRASS:
      g.fillRect(x, y, T, T, (uint32_t)0x3A6B30);
      g.drawPixel(x + (h & 7), y + ((h >> 3) & 7), (uint32_t)0x2C5826);
      g.drawPixel(x + ((h >> 6) & 7), y + ((h >> 9) & 7), (uint32_t)0x4A7E3C);
      break;
    case FOREST:
      g.fillRect(x, y, T, T, (uint32_t)0x1E4620);
      g.fillCircle(x + 2 + (h & 3), y + 2 + ((h >> 2) & 1), 2, (uint32_t)0x2D6630);
      g.fillCircle(x + 3 + ((h >> 4) & 3), y + 5 + ((h >> 6) & 1), 2, (uint32_t)0x285C2A);
      g.drawPixel(x + 2 + (h & 3), y + 1 + ((h >> 2) & 1), (uint32_t)0x4A8A40);
      break;
    case SAND:
      g.fillRect(x, y, T, T, (uint32_t)0x9C8A60);
      g.drawPixel(x + (h & 7), y + ((h >> 3) & 7), (uint32_t)0x857550);
      break;
    case WATER:
      g.fillRect(x, y, T, T, (uint32_t)0x24508A);
      if ((r * 3 + c + frame / 10) % 4 == 0) // さざ波
        g.drawFastHLine(x + (h & 3), y + ((h >> 2) & 7), 4, (uint32_t)0x4A78B8);
      break;
    case ROAD_V:
      g.fillRect(x, y, T, T, (uint32_t)0x5E5E5A);
      g.drawFastVLine(x + 3, y + 1, 3, (uint32_t)0x9A9A8A);
      break;
    case ROAD_H:
      g.fillRect(x, y, T, T, (uint32_t)0x5E5E5A);
      g.drawFastHLine(x + 1, y + 3, 3, (uint32_t)0x9A9A8A);
      break;
    case BRIDGE:
      g.fillRect(x, y, T, T, (uint32_t)0x7A6A50);
      g.drawFastVLine(x, y, T, (uint32_t)0x4A3E2E);
      g.drawFastVLine(x + T - 1, y, T, (uint32_t)0x4A3E2E);
      break;
    }
  }

  // ---- 敵と弾 ----
  void enemyShot(float x, float y, float vx, float vy)
  {
    for (Shot &s : theirs)
      if (!s.on)
      {
        s = {x, y, vx, vy, true};
        return;
      }
  }
  void aimed(float x, float y, float speed)
  {
    float dx = px - x, dy = py + 7 - y, d = sqrtf(dx * dx + dy * dy) + 0.01f;
    enemyShot(x, y, dx / d * speed, dy / d * speed);
  }

  void spawnWave()
  {
    if (irand(0, 2) == 0) // トーカン 1〜2 機
    {
      for (int n = irand(1, 2); n > 0; n--)
        for (Air &a : air)
          if (!a.on)
          {
            a = {frand(16, GAME_W - 16), -10, 0, 2.2f, 0, 0, 1, 0, true, false, false};
            break;
          }
      return;
    }
    // トーロイドの編隊。少しずつずれて降りてくる
    float x0 = frand(20, GAME_W - 20);
    for (int n = 0, k = irand(3, 5); n < k; n++)
      for (Air &a : air)
        if (!a.on)
        {
          a = {x0 + (n - k / 2) * 12.0f, -10.0f - n * 14, 0, 1.8f, n * 0.7f, py - frand(70, 120), 0, 0, true, false, false};
          break;
        }
  }

  void stepAir(Air &a)
  {
    a.t++;
    a.spin += 0.25f;
    if (a.type == 0)
    {
      // 自機の少し上まで来たら、横へ切り返して上へ逃げる
      if (!a.turned && a.y > a.turnY)
      {
        a.turned = true;
        a.vx = a.x < px ? -0.4f : 0.4f;
        if (irand(0, 2) == 0)
          aimed(a.x, a.y, 1.8f);
      }
      if (a.turned)
      {
        a.vx += a.vx < 0 ? -0.1f : 0.1f;
        a.vy = max(a.vy - 0.12f, -2.0f);
      }
    }
    else
    {
      // 降りてきて止まり、撃ってから上へ戻る
      if (a.vy > 0 && a.y > 50)
        a.vy = max(0.0f, a.vy - 0.12f);
      if (a.vy == 0 && !a.fired && a.t > 40)
      {
        a.fired = true;
        aimed(a.x, a.y + 3, 2.0f);
        aimed(a.x, a.y + 3, 1.4f);
      }
      if (a.fired && a.t > 70)
        a.vy = max(a.vy - 0.1f, -2.2f);
    }
    a.x += a.vx;
    a.y += a.vy;
    if (a.y < -30 || a.y > GAME_H + 20 || a.x < -20 || a.x > GAME_W + 20)
      a.on = false;
  }

  void killPlayer()
  {
    fx.burst(px, py + 7, 0xFFC040, 30, 2.0f, 30);
    fx.burst(px, py + 7, 0xFFFFFF, 12, 1.0f, 20);
    deadT = 60;
    bomb.on = false;
    if (--lives < 0)
      overT = 1;
  }

  void bombLand()
  {
    float x = bomb.x, y = py - SIGHT;
    fx.burst(x, y, 0xFFB040, 10, 1.4f, 14);
    fx.burst(x, y, 0x806040, 6, 0.8f, 18);
    for (Ground &o : gnd)
      if (o.on && fabsf(o.x - x) < 7 && fabsf(o.y - y) < 7)
      {
        o.on = false;
        score += o.type ? 1000 : 300;
        fx.burst(o.x, o.y, 0xFFFFFF, 10, 1.6f, 16);
        for (Crater &c : craters)
          if (!c.on)
          {
            c = {o.x, o.y, true};
            break;
          }
      }
    if (boss.on && !boss.dead && fabsf(x - GAME_W / 2) < 8 && fabsf(boss.y - y) < 8)
    {
      boss.hitT = 6;
      if (--boss.hp <= 0)
      {
        boss.dead = true;
        score += 20000;
        for (int i = 0; i < 8; i++)
          fx.burst(GAME_W / 2 + frand(-24, 24), boss.y + frand(-24, 24), i % 2 ? 0xFFFFFF : 0xFFA030, 12, 2.2f, 34);
      }
    }
  }

  // ---- 自機の考え ----
  void think()
  {
    float targetX = GAME_W / 2, targetY = PY0;
    bool bossAim = boss.on && !boss.dead && boss.t > 0;
    if (bossAim)
    {
      // 母艦: 照準が核に重なるところまで前に出る
      targetX = GAME_W / 2;
      targetY = boss.y + SIGHT;
    }
    else
    {
      // 地上の敵で、照準にいちばん早く来るもの。いなければ一番下の空の敵
      float best = -1e9f;
      bool found = false;
      for (Ground &o : gnd)
        if (o.on && o.y < py - SIGHT - SPEED * BOMB_T + 4 && o.y > py - SIGHT - 130 && o.y > best)
        {
          best = o.y;
          targetX = o.x;
          found = true;
        }
      if (!found)
      {
        float lowest = -1;
        for (Air &a : air)
          if (a.on && a.y > 0 && a.y < py - 20 && a.y > lowest)
          {
            lowest = a.y;
            targetX = a.x;
          }
      }
    }
    py += constrain(targetY - py, -1.5f, 1.5f);

    // 候補の位置ごとに、弾がそこを通るかを見て危なさを点数にする
    float best = 1e9f, bestX = px;
    for (int k = -3; k <= 3; k++)
    {
      float cx = constrain(px + k * 8, 8.0f, GAME_W - 8.0f);
      float cost = fabsf(cx - targetX) * 0.04f + abs(k) * 0.05f;
      for (Shot &s : theirs)
        if (s.on && s.vy > 0 && s.y < py + 14)
        {
          float t = (py + 6 - s.y) / s.vy;
          if (t > 40)
            continue;
          float x = s.x + s.vx * t;
          if (fabsf(x - cx) < 8)
            cost += 12.0f / (1 + t * 0.15f);
        }
      for (Air &a : air)
        if (a.on && fabsf(a.y - py) < 40 && fabsf(a.x - cx) < 12)
          cost += 6;
      if (cost < best)
      {
        best = cost;
        bestX = cx;
      }
    }
    px += constrain(bestX - px, -2.0f, 2.0f);

    // 爆弾: 落ちたときに照準の下に来る地上の敵 (か母艦の核) があれば落とす
    if (!bomb.on)
    {
      float sy = py - SIGHT;
      bool drop = bossAim && fabsf(px - GAME_W / 2) < 6 && fabsf(boss.y - sy) < 6;
      for (Ground &o : gnd)
        if (o.on && fabsf(o.x - px) < 5 && fabsf(o.y + SPEED * BOMB_T - sy) < 5)
          drop = true;
      if (drop)
        bomb = {true, 0, px};
    }
  }

  bool sightOnTarget() const
  {
    float sy = py - SIGHT;
    for (const Ground &o : gnd)
      if (o.on && fabsf(o.x - px) < 7 && fabsf(o.y - sy) < 7)
        return true;
    return boss.on && !boss.dead && fabsf(px - GAME_W / 2) < 8 && fabsf(boss.y - sy) < 8;
  }

  // ---- 描く ----
  void drawBoss(LGFX_Sprite &g)
  {
    int cx = GAME_W / 2, cy = boss.y, R = 30, cut = 12;
    uint32_t hull = boss.dead ? 0x4A4A50 : 0x8A8E98, dark = boss.dead ? 0x2A2A30 : 0x50545E;
    // 八角形 (四角の角を三角で落とす)
    g.fillRect(cx - R + cut, cy - R, (R - cut) * 2, R * 2, hull);
    g.fillRect(cx - R, cy - R + cut, R * 2, (R - cut) * 2, hull);
    g.fillTriangle(cx - R, cy - R + cut, cx - R + cut, cy - R, cx - R + cut, cy - R + cut, hull);
    g.fillTriangle(cx + R, cy - R + cut, cx + R - cut, cy - R, cx + R - cut, cy - R + cut, hull);
    g.fillTriangle(cx - R, cy + R - cut, cx - R + cut, cy + R, cx - R + cut, cy + R - cut, hull);
    g.fillTriangle(cx + R, cy + R - cut, cx + R - cut, cy + R, cx + R - cut, cy + R - cut, hull);
    g.drawFastHLine(cx - R + cut, cy - R, (R - cut) * 2, (uint32_t)0xC0C4CC);
    // 中の段と、十字の溝
    g.fillCircle(cx, cy, 16, dark);
    g.fillCircle(cx, cy, 13, hull);
    g.drawFastHLine(cx - R + 2, cy, R * 2 - 4, dark);
    g.drawFastVLine(cx, cy - R + 2, R * 2 - 4, dark);
    // 四隅の砲台
    for (int i = 0; i < 4; i++)
    {
      int tx = cx + (i % 2 ? 17 : -17), ty = cy + (i / 2 ? 17 : -17);
      g.fillCircle(tx, ty, 4, dark);
      g.fillCircle(tx, ty, 2, boss.dead ? dark : (boss.t / 20 + i) % 4 == 0 ? (uint32_t)0xFF6040 : (uint32_t)0xA0A4AC);
    }
    // 中心の核
    g.fillCircle(cx, cy, 7, (uint32_t)0x30202A);
    if (!boss.dead)
      g.fillCircle(cx, cy, 5, boss.hitT ? (uint32_t)0xFFFFFF : (frame / 6) % 2 ? (uint32_t)0xFF3040 : (uint32_t)0xC01830);
  }

  void drawGround(LGFX_Sprite &g, const Ground &o)
  {
    int x = o.x, y = o.y;
    if (o.type == 0)
    {
      // バーラ: 上から見たピラミッド。光の当たる面を明るく
      g.fillRect(x - 5, y - 5, 11, 11, (uint32_t)0x6E6E6E);
      g.fillTriangle(x - 5, y - 5, x + 5, y - 5, x, y, (uint32_t)0xB8B8B8);
      g.fillTriangle(x + 5, y - 5, x + 5, y + 5, x, y, (uint32_t)0x9A9A9A);
      g.fillTriangle(x - 5, y + 5, x + 5, y + 5, x, y, (uint32_t)0x505050);
      g.drawPixel(x, y, (uint32_t)0x303030);
    }
    else
    {
      // ログラム: 球の砲台。撃つ前に口が開く
      g.fillCircle(x, y, 5, (uint32_t)0x7A7A6A);
      g.fillCircle(x - 1, y - 1, 3, (uint32_t)0x9A9A88);
      int open = o.t % 90 > 70 ? 2 : 0;
      g.fillRect(x - 3, y - open / 2, 7, 1 + open, (uint32_t)0x202018);
    }
  }

public:
  const char *name() const override { return "XEVIOUS"; }

  void reset() override
  {
    seed = esp_random();
    rowCount = 0;
    riverOn = roadOn = false;
    groupT = 10;
    for (Air &a : air)
      a.on = false;
    for (Ground &o : gnd)
      o.on = false;
    for (Crater &c : craters)
      c.on = false;
    for (Shot &s : mine)
      s.on = false;
    for (Shot &s : theirs)
      s.on = false;
    bomb.on = false;
    boss.on = false;
    scrollY = 0;
    for (int r = ROWS - 1; r >= 0; r--) // 下の行から作る (上が新しい)
    {
      rowId[r] = rowCount++;
      genRow(map[r], rowId[r]);
    }
    px = GAME_W / 2;
    py = PY0;
    fireT = 0;
    waveT = 60;
    bossT = 600; // 1 回のデモ (45 秒) のうちに母艦が来るように
    invT = 60;
    deadT = 0;
    lives = 3;
    score = 0;
    frame = 0;
    overT = 0;
    fx.clear();
  }

  void step() override
  {
    frame++;
    fx.step();
    // 地面と、その上のもの
    scrollY += SPEED;
    if (scrollY >= T)
    {
      scrollY -= T;
      newRow();
    }
    for (Ground &o : gnd)
      if (o.on)
      {
        o.y += SPEED;
        o.t++;
        if (o.y > GAME_H + 8)
          o.on = false;
        else if (o.type == 1 && o.t % 90 == 80 && o.y > 8 && o.y < py - 30 && !overT)
          aimed(o.x, o.y, 1.6f);
      }
    for (Crater &c : craters)
      if (c.on && (c.y += SPEED) > GAME_H + 8)
        c.on = false;

    if (overT)
    {
      if (++overT > 90)
        reset();
      return;
    }

    // 母艦: 地面と一緒に降りてきて止まり、しばらく撃ってから流れ去る
    if (!boss.on && --bossT <= 0)
    {
      boss = {true, false, -34, -1, 4, 0}; // 核に爆弾 4 発で落ちる
      bossT = 1800;
    }
    if (boss.on)
    {
      if (boss.hitT)
        boss.hitT--;
      if (boss.t < 0)
      {
        boss.y += SPEED;
        if (boss.y >= BOSS_Y)
          boss.t = 0;
      }
      else if (boss.dead || ++boss.t > 700)
      {
        boss.y += SPEED;
        if (boss.y > GAME_H + 40)
          boss.on = false;
      }
      else if (boss.t % 40 == 0)
      {
        int i = boss.t / 40 % 4;
        aimed(GAME_W / 2 + (i % 2 ? 17 : -17), boss.y + (i / 2 ? 17 : -17), 1.7f);
      }
    }
    if (!(boss.on && !boss.dead) && --waveT <= 0)
    {
      spawnWave();
      waveT = irand(70, 120);
    }
    for (Air &a : air)
      if (a.on)
        stepAir(a);

    // 弾
    for (Shot &s : theirs)
      if (s.on)
      {
        s.x += s.vx;
        s.y += s.vy;
        if (s.y > GAME_H || s.y < -4 || s.x < -4 || s.x > GAME_W + 4)
          s.on = false;
        else if (!deadT && !invT && fabsf(s.x - px) < 3.5f && s.y > py + 2 && s.y < py + 12)
        {
          s.on = false;
          killPlayer();
        }
      }
    for (Shot &s : mine)
    {
      if (!s.on)
        continue;
      s.y += s.vy;
      if (s.y < -4)
      {
        s.on = false;
        continue;
      }
      for (Air &a : air)
        if (a.on && fabsf(s.x - a.x) < 6 && fabsf(s.y - a.y) < 5)
        {
          a.on = false;
          s.on = false;
          score += a.type ? 50 : 30;
          fx.burst(a.x, a.y, a.type ? 0xFFB060 : 0xD0D0E0, 12, 1.6f, 16);
          break;
        }
    }
    if (bomb.on && ++bomb.t >= BOMB_T)
    {
      bomb.on = false;
      bombLand();
    }

    // 自機
    if (deadT)
    {
      if (--deadT == 0)
      {
        px = GAME_W / 2;
        py = PY0;
        invT = 90;
      }
      return;
    }
    if (invT)
      invT--;
    think();
    if (++fireT >= 6)
    {
      fireT = 0;
      for (int side = -1; side <= 1; side += 2)
        for (Shot &s : mine)
          if (!s.on)
          {
            s = {px + side * 3, py + 2, 0, -6, true};
            break;
          }
    }
    for (Air &a : air) // 体当たり
      if (a.on && !invT && fabsf(a.x - px) < 7 && fabsf(a.y - (py + 7)) < 7)
      {
        a.on = false;
        killPlayer();
        break;
      }
  }

  void draw(LGFX_Sprite &g) override
  {
    // 地面
    for (int r = 0; r < ROWS; r++)
    {
      int y = (r - 1) * T + (int)scrollY;
      if (y <= -T || y >= GAME_H)
        continue;
      for (int c = 0; c < COLS; c++)
        drawTile(g, c * T, y, map[r][c], c, rowId[r]);
    }
    for (Crater &c : craters)
      if (c.on)
      {
        g.fillEllipse((int)c.x, (int)c.y, 6, 5, (uint32_t)0x3A3020);
        g.fillEllipse((int)c.x, (int)c.y, 3, 2, (uint32_t)0x1E180E);
      }
    if (boss.on)
      drawBoss(g);
    for (Ground &o : gnd)
      if (o.on)
        drawGround(g, o);

    // 照準 (地上の敵に重なると赤く点滅)。爆弾は自機から照準へ小さくなりながら落ちる
    if (!deadT && !overT)
    {
      int sx = px, sy = py - SIGHT;
      uint32_t c = sightOnTarget() && (frame / 3) % 2 ? (uint32_t)0xFF3030 : (uint32_t)0xE0E0E0;
      g.drawRect(sx - 5, sy - 5, 11, 11, c);
      g.fillRect(sx - 1, sy - 1, 3, 3, c);
      g.drawFastHLine(sx - 8, sy, 3, c);
      g.drawFastHLine(sx + 6, sy, 3, c);
    }
    if (bomb.on)
    {
      float f = (float)bomb.t / BOMB_T;
      int by = py + 2 - (SIGHT + 2) * f;
      g.fillCircle((int)bomb.x, by, f < 0.5f ? 2 : 1, (uint32_t)0xFFE080);
    }

    // 空
    for (Air &a : air)
      if (a.on)
      {
        if (a.type == 0)
        {
          // トーロイド: 回る輪。幅を変えて回っているように見せる
          int rx = 1 + (int)(4.5f * fabsf(cosf(a.spin)));
          g.fillEllipse((int)a.x, (int)a.y, rx, 5, (uint32_t)0xC0C4D0);
          if (rx > 2)
            g.fillEllipse((int)a.x, (int)a.y, rx - 2, 2, (uint32_t)0x30343C);
        }
        else
          blit(g, (int)a.x - 5, (int)a.y - 3, TORKAN, 6, TORKAN_PAL, 3);
      }
    for (Shot &s : mine)
      if (s.on)
        g.fillRect((int)s.x, (int)s.y, 1, 4, (uint32_t)0xFFFFA0);
    for (Shot &s : theirs)
      if (s.on)
        g.fillCircle((int)s.x, (int)s.y, 2, (frame / 2) % 2 ? (uint32_t)0xFFFFFF : (uint32_t)0xFF5070);

    if (!deadT && !overT && !(invT && (frame / 3) % 2))
    {
      PalEntry pal[4];
      memcpy(pal, SOL_PAL, sizeof(pal));
      pal[3].rgb = (frame / 2) % 2 ? 0xFFA030 : 0xFFE060; // 噴射のゆらぎ
      blit(g, (int)px - 6, (int)py, SOL, 14, pal, 4);
    }
    fx.draw(g, 0x3A6B30);

    char s[16];
    snprintf(s, sizeof(s), "%06d", score);
    hud(g, 4, 2, C_TEXT, s);
    for (int i = 0; i < lives; i++)
      g.fillTriangle(GAME_W - 8 - i * 9, 9, GAME_W - 5 - i * 9, 2, GAME_W - 2 - i * 9, 9, (uint32_t)0xE0E0E0);
    if (boss.on && boss.t < 0 && boss.y > -20)
      hud(g, GAME_W / 2, 120, (frame / 8) % 2 ? C_RED : C_YELLOW, "WARNING", TC_DATUM);
    if (overT > 10)
      hud(g, GAME_W / 2, 110, C_RED, "GAME OVER", TC_DATUM);
  }
};
} // namespace

Demo *makeXevious() { return new Xevious(); }
