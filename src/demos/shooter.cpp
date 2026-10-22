// 縦スクロールのシューティング。自機は弾の通り道を先読みしてよけながら、一番近い敵の下へ回る
#include "demo.h"

namespace
{
const char *const SHIP[12] = {".....X.....", "....XXX....", "....XWX....", "...XXWXX...", "...XXXXX...", "..XXXXXXX..",
                              ".XXRXXXRXX.", "XXXXXXXXXXX", "XX.XXXXX.XX", "X...XXX...X", ".....F.....", "....FFF...."};
const PalEntry SHIP_PAL[] = {{'X', 0xC8D8F0}, {'W', 0x40E0FF}, {'R', 0xFF5050}, {'F', 0xFFA030}};
const char *const DART[7] = {"X.......X", "XX.....XX", ".XXXXXXX.", "..XXRXX..", "..XXXXX..", "...XXX...", "....X...."};
const char *const SAUCER[5] = {"..XXXXX..", ".XXXXXXX.", "XXRXRXRXX", ".XXXXXXX.", "..X...X.."};

const int PY = 200;
const uint32_t BG = 0x00020A;

struct Star
{
  float x, y, v;
};
struct Enemy
{
  float x, y, bx, ph;
  int t, type, path, dir;
  bool on;
};
struct Shot
{
  float x, y, vx, vy;
  bool on;
};

class Shooter : public Demo
{
  Star stars[36];
  Enemy en[12];
  Shot mine[14], theirs[28];
  float px, pTarget;
  int fireT, waveT, bossT, invT, deadT, lives, score, frame, overT;
  struct
  {
    bool on;
    float x, y;
    int hp, t, hitT;
  } boss;
  Particles fx;

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
    float dx = px - x, dy = PY + 6 - y, d = sqrtf(dx * dx + dy * dy) + 0.01f;
    enemyShot(x, y, dx / d * speed, dy / d * speed);
  }

  void spawnWave()
  {
    int path = irand(0, 2), type = irand(0, 1), dir = irand(0, 1) ? 1 : -1;
    float bx = frand(30, GAME_W - 30);
    int n = 5;
    for (Enemy &e : en)
    {
      if (e.on || n == 0)
        continue;
      n--;
      e.on = true;
      e.type = type;
      e.path = path;
      e.dir = dir;
      e.t = -n * (path == 2 ? 0 : 14); // 時間差で 1 機ずつ出てくる
      e.bx = path == 2 ? 14 + n * 21 : bx;
      e.ph = n * 0.6f;
      e.x = -50;
      e.y = -50;
    }
  }

  void movePath(Enemy &e)
  {
    float t = e.t;
    switch (e.path)
    {
    case 0: // くねくね下りる
      e.x = e.bx + sinf(t * 0.05f + e.ph) * 30;
      e.y = -10 + t * 0.9f;
      break;
    case 1: // 横から弧を描いて横切る
      e.x = (e.dir > 0 ? -10 : GAME_W + 10) + e.dir * t * 1.0f;
      e.y = 16 + 70 * sinf(t * 0.022f);
      break;
    default: // 横一列で降りて止まり、また下りる
      e.x = e.bx;
      e.y = t < 60 ? -10 + t * 1.2f : t < 140 ? 62 : 62 + (t - 140) * 1.6f;
      break;
    }
  }

  void killPlayer()
  {
    fx.burst(px, PY + 6, 0xFFC040, 30, 2.0f, 30);
    fx.burst(px, PY + 6, 0xFFFFFF, 12, 1.0f, 20);
    deadT = 60;
    if (--lives < 0)
      overT = 1;
  }

  void think()
  {
    // 狙う場所: ボス、いなければ一番下の敵
    float target = GAME_W / 2;
    if (boss.on)
      target = boss.x;
    else
    {
      float lowest = -1;
      for (Enemy &e : en)
        if (e.on && e.y > 0 && e.y < PY - 30 && e.y > lowest)
        {
          lowest = e.y;
          target = e.x;
        }
    }
    // 候補の位置ごとに、弾がそこを通るかを見て危なさを点数にする
    float best = 1e9f, bestX = px;
    for (int k = -3; k <= 3; k++)
    {
      float cx = constrain(px + k * 8, 8.0f, GAME_W - 8.0f);
      float cost = fabsf(cx - target) * 0.04f + abs(k) * 0.05f;
      for (Shot &s : theirs)
        if (s.on && s.vy > 0 && s.y < PY + 12)
        {
          float t = (PY + 4 - s.y) / s.vy;
          if (t > 40)
            continue;
          float x = s.x + s.vx * t;
          if (fabsf(x - cx) < 8)
            cost += 12.0f / (1 + t * 0.15f);
        }
      for (Enemy &e : en)
        if (e.on && e.y > PY - 40 && fabsf(e.x - cx) < 12)
          cost += 6;
      if (cost < best)
      {
        best = cost;
        bestX = cx;
      }
    }
    pTarget = bestX;
    px += constrain(pTarget - px, -2.2f, 2.2f);
  }

public:
  const char *name() const override { return "SHOOTER"; }

  void reset() override
  {
    for (Star &s : stars)
      s = {frand(0, GAME_W), frand(0, GAME_H), (float)irand(1, 3) * 0.6f};
    for (Enemy &e : en)
      e.on = false;
    for (Shot &s : mine)
      s.on = false;
    for (Shot &s : theirs)
      s.on = false;
    boss.on = false;
    px = pTarget = GAME_W / 2;
    fireT = 0;
    waveT = 40;
    bossT = 750;
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
    for (Star &s : stars)
    {
      s.y += s.v;
      if (s.y > GAME_H)
        s = {frand(0, GAME_W), 0, s.v};
    }
    if (overT)
    {
      if (++overT > 90)
        reset();
      return;
    }

    // 出てくる敵
    if (!boss.on && --bossT <= 0)
    {
      boss = {true, GAME_W / 2.0f, -24, 60, 0, 0};
      bossT = 900;
    }
    if (!boss.on && --waveT <= 0)
    {
      spawnWave();
      waveT = irand(90, 130);
    }

    for (Enemy &e : en)
    {
      if (!e.on)
        continue;
      e.t++;
      if (e.t < 0)
        continue;
      movePath(e);
      if (e.t > 30 && (e.y > GAME_H + 10 || e.x < -20 || e.x > GAME_W + 20))
        e.on = false;
      else if (e.y > 10 && e.y < PY - 40 && irand(0, 150) == 0)
        aimed(e.x, e.y + 4, 1.6f);
    }

    if (boss.on)
    {
      boss.t++;
      if (boss.hitT)
        boss.hitT--;
      if (boss.y < 34)
        boss.y += 0.6f;
      else
      {
        boss.x = GAME_W / 2 + sinf(boss.t * 0.02f) * 32;
        if (boss.t % 60 == 0) // 扇形に 5 発
          for (int i = -2; i <= 2; i++)
            enemyShot(boss.x, boss.y + 10, sinf(i * 0.3f) * 1.4f, cosf(i * 0.3f) * 1.4f);
        if (boss.t % 60 == 30)
          aimed(boss.x, boss.y + 10, 2.0f);
      }
    }

    // 弾
    for (Shot &s : theirs)
      if (s.on)
      {
        s.x += s.vx;
        s.y += s.vy;
        if (s.y > GAME_H || s.x < -4 || s.x > GAME_W + 4)
          s.on = false;
        else if (!deadT && !invT && fabsf(s.x - px) < 3.5f && s.y > PY + 1 && s.y < PY + 10)
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
      if (s.y < 10)
      {
        s.on = false;
        continue;
      }
      for (Enemy &e : en)
        if (e.on && e.t >= 0 && fabsf(s.x - e.x) < 5 && fabsf(s.y - e.y) < 5)
        {
          e.on = false;
          s.on = false;
          score += 100;
          fx.burst(e.x, e.y, e.type ? 0x60FF90 : 0xFF70C0, 12, 1.6f, 16);
          break;
        }
      if (s.on && boss.on && fabsf(s.x - boss.x) < 15 && fabsf(s.y - boss.y) < 9)
      {
        s.on = false;
        boss.hitT = 3;
        if (--boss.hp <= 0)
        {
          boss.on = false;
          score += 5000;
          for (int i = 0; i < 5; i++)
            fx.burst(boss.x + frand(-12, 12), boss.y + frand(-6, 6), i % 2 ? 0xFFFFFF : 0xFFA030, 14, 2.2f, 30);
          waveT = 60;
        }
      }
    }

    // 自機
    if (deadT)
    {
      if (--deadT == 0)
      {
        px = GAME_W / 2;
        invT = 90;
      }
      return;
    }
    if (invT)
      invT--;
    think();
    if (++fireT >= 5)
    {
      fireT = 0;
      for (int side = -1; side <= 1; side += 2)
        for (Shot &s : mine)
          if (!s.on)
          {
            s = {px + side * 3, PY, 0, -6, true};
            break;
          }
    }
    for (Enemy &e : en) // 体当たり
      if (e.on && !invT && fabsf(e.x - px) < 7 && fabsf(e.y - (PY + 6)) < 7)
      {
        e.on = false;
        killPlayer();
        break;
      }
  }

  void draw(LGFX_Sprite &g) override
  {
    g.fillScreen(BG);
    for (Star &s : stars)
      g.drawPixel((int)s.x, (int)s.y, s.v > 1.5f ? 0xC0D0FF : s.v > 1.0f ? 0x6070A0 : 0x303850);

    for (Enemy &e : en)
      if (e.on && e.t >= 0)
      {
        if (e.type == 0)
          blit(g, (int)e.x - 4, (int)e.y - 3, DART, 7, (frame / 6) % 2 ? 0xFF70C0 : 0xFF50A0);
        else
          blit(g, (int)e.x - 4, (int)e.y - 2, SAUCER, 5, 0x60FF90);
      }

    if (boss.on)
    {
      int bx = boss.x, by = boss.y;
      uint32_t body = boss.hitT ? 0xFFFFFF : 0x8070C0;
      g.fillTriangle(bx - 16, by - 2, bx - 6, by - 8, bx - 6, by + 6, mix(body, 0x000000, 0.3f));
      g.fillTriangle(bx + 16, by - 2, bx + 6, by - 8, bx + 6, by + 6, mix(body, 0x000000, 0.3f));
      g.fillRoundRect(bx - 9, by - 9, 18, 18, 5, body);
      g.fillSmoothCircle(bx, by + 1, 4, (frame / 8) % 2 ? 0xFF4040 : 0xFFB040);
      g.fillRect(bx - 7, by + 9, 3, 3, 0x605090);
      g.fillRect(bx + 4, by + 9, 3, 3, 0x605090);
      g.fillRect(10, 12, GAME_W - 20, 3, 0x301020);
      g.fillRect(10, 12, (GAME_W - 20) * boss.hp / 60, 3, 0xFF4060);
    }

    for (Shot &s : mine)
      if (s.on)
        g.fillRect((int)s.x, (int)s.y, 1, 4, 0x80F0FF);
    for (Shot &s : theirs)
      if (s.on)
        g.fillSmoothCircle((int)s.x, (int)s.y, 2, 0xFF6090);

    if (!deadT && !overT && !(invT && (frame / 3) % 2))
    {
      PalEntry pal[4];
      memcpy(pal, SHIP_PAL, sizeof(pal));
      pal[3].rgb = (frame / 2) % 2 ? 0xFFA030 : 0xFFE060; // 噴射のゆらぎ
      blit(g, (int)px - 5, PY, SHIP, 12, pal, 4);
    }
    fx.draw(g, BG);

    char s[16];
    snprintf(s, sizeof(s), "%06d", score);
    hud(g, 4, 2, C_TEXT, s);
    for (int i = 0; i < lives; i++)
      g.fillTriangle(GAME_W - 8 - i * 9, 9, GAME_W - 5 - i * 9, 2, GAME_W - 2 - i * 9, 9, 0xC8D8F0);
    if (boss.on && boss.y < 34)
      hud(g, GAME_W / 2, 100, (frame / 8) % 2 ? C_RED : C_YELLOW, "WARNING", TC_DATUM);
    if (overT > 10)
      hud(g, GAME_W / 2, 110, C_RED, "GAME OVER", TC_DATUM);
  }
};
} // namespace

Demo *makeShooter() { return new Shooter(); }
