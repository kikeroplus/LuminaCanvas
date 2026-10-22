// RPG 風: 毎回作る地図の上を勇者が歩き、敵に出会うと戦闘画面に切り替わってコマンドで戦う。
// 勇者は遠くの場所を目指して道を探して歩き、HP が減ってきたら一番近い町の宿屋で休む
#include "demo.h"

namespace
{
#define FJ12 (&fonts::lgfxJapanGothicP_12)

// ---- 絵 (すべてオリジナル) ----
const char *const HERO_A[13] = {"....HHHH....", "...HHHHHH...", "...HSSSSH...", "...SKSSKS...", "....SSSS....",
                                "..CBBBBBBC..", ".CCBBYYBBCC.", ".CSBBBBBBSC.", ".C.BBBBBB.C.", "...BB..BB...",
                                "...BB..BB...", "...NN..NN...", "............"};
const char *const HERO_B[13] = {"....HHHH....", "...HHHHHH...", "...HSSSSH...", "...SKSSKS...", "....SSSS....",
                                "..CBBBBBBC..", ".CCBBYYBBCC.", ".CSBBBBBBSC.", ".C.BBBBBB.C.", "..BB....BB..",
                                "..BB....BB..", "..NN....NN..", "............"};
const PalEntry HERO_PAL[] = {{'H', 0x7A4A20}, {'S', 0xF0C8A0}, {'K', 0x202020}, {'B', 0x3060D0},
                             {'Y', 0xF0D040}, {'C', 0xD03030}, {'N', 0x5A3A20}};

const char *const JELLY[12] = {"................", "......XXXX......", "....XXXXXXXX....", "...XXXXXXXXXX...",
                               "..XXXWWWWXXXXX..", "..XXWWKKWWXXXX..", ".XXXWWKKWWXXXXX.", ".XXXXWWWWXXXXXX.",
                               "XXXXXXXXXXXXXXXX", "XLXXXXXXXXXXXXLX", "XXLLXXXXXXXXLLXX", ".XXXXXXXXXXXXXX."};
const char *const BAT[10] = {"X..............X", "XX....X..X....XX", "XXX...XXXX...XXX", "XXXX.XXXXXX.XXXX",
                             "XXXXXXRXXRXXXXXX", ".XXXXXXXXXXXXXX.", "..XXXXXWWXXXXX..", "...XX.XXXX.XX...",
                             "....X..XX..X....", "................"};
const char *const GHOST[14] = {"....XXXXXX....", "..XXXXXXXXXX..", ".XXXXXXXXXXXX.", ".XXKKXXXXKKXX.",
                               "XXXKKXXXXKKXXX", "XXXXXXXXXXXXXX", "XXXXXKKKKXXXXX", "XXXXXKKKKXXXXX",
                               "XXXXXXXXXXXXXX", "XXXXXXXXXXXXXX", "XXXXXXXXXXXXXX", "XXXXXXXXXXXXXX",
                               "XX.XXX..XXX.XX", "X...X....X...X"};
const char *const SHROOM[12] = {"....RRRRRR....", "..RRWRRRRWRR..", ".RRRRRRRRRRRR.", "RRWWRRRRRRWWRR",
                                "RRWWRRRRRRWWRR", ".RRRRRRRRRRRR.", "...SSSSSSSS...", "...SKSSSSKS...",
                                "...SSSKKSSS...", "....SSSSSS....", "...SS....SS...", "..SSS....SSS.."};

struct Monster
{
  const char *name;
  const char *const *rows;
  int h;
  PalEntry pal[4];
  int palN, hp, atk, exp, minLv;
};
const Monster MONSTERS[] = {
    {"ゼリー", JELLY, 12, {{'X', 0x40C060}, {'W', 0xF0F0F0}, {'K', 0x202020}, {'L', 0x90F0A0}}, 4, 8, 3, 3, 1},
    {"コウモリ", BAT, 10, {{'X', 0x8050B0}, {'R', 0xFF3030}, {'W', 0xF0F0F0}}, 3, 11, 5, 5, 2},
    {"キノコ", SHROOM, 12, {{'R', 0xD03838}, {'W', 0xF8F0E0}, {'S', 0xE8D0A8}, {'K', 0x302018}}, 4, 18, 7, 9, 3},
    {"ゴースト", GHOST, 14, {{'X', 0xD8E0F0}, {'K', 0x302040}}, 2, 26, 10, 15, 5},
};
const int MON_N = sizeof(MONSTERS) / sizeof(MONSTERS[0]);

// 点の絵を scale 倍で描く
void blitScaled(LGFX_Sprite &g, int x, int y, const char *const *rows, int h, const PalEntry *pal, int palN, int scale,
                uint32_t solid = 0, bool useSolid = false)
{
  for (int r = 0; r < h; r++)
  {
    int w = strlen(rows[r]);
    for (int c = 0; c < w; c++)
    {
      char ch = rows[r][c];
      if (ch == '.')
        continue;
      for (int i = 0; i < palN; i++)
        if (pal[i].ch == ch)
        {
          g.fillRect(x + c * scale, y + r * scale, scale, scale, useSolid ? solid : pal[i].rgb);
          break;
        }
    }
  }
}

// ---- 地図 ----
const int MW = 40, MH = 40, TS = 16; // 地図の大きさ (マス) と 1 マスの大きさ
enum Tile : uint8_t
{
  SEA,
  SAND,
  GRASS,
  FOREST,
  MOUNT,
  TOWN,
};
bool passable(uint8_t t) { return t == SAND || t == GRASS || t == FOREST || t == TOWN; }

enum Mode
{
  FIELD,
  ENCOUNTER, // 画面が点滅して戦闘へ
  BATTLE,
  INN,       // 宿屋で休む (暗くなって明るくなる)
};

class Quest : public Demo
{
  uint8_t map[MH][MW];
  int16_t dist[MH][MW]; // 目的地からの歩数 (-1 は行けない)
  uint32_t seed;
  int townX[3], townY[3];

  // 勇者
  int tx, ty, dx, dy, prog; // マスの位置、歩いている向きと進み (0〜TS)
  int goalX, goalY, steps, calm; // calm: 戦闘のあとしばらく敵が出ない歩数
  int lv, hp, maxHp, mp, maxMp, exp;

  Mode mode;
  int modeT, frame;

  // 戦闘
  const Monster *mon;
  int monHp, monMaxHp, monBlink, shake, cmd, cmdT;
  bool battleOver, heroDown;
  struct Msg
  {
    char text[64];
    int kind; // 下の Act
    int val;
  } q[10];
  int qN, qT, shown; // qT: いまの文を出し始めてからのフレーム、shown: 出し終えた文字数
  char lines[2][64];
  enum Act
  {
    A_NONE,
    A_MON_HIT,  // 敵が点滅して HP が減る
    A_HERO_HIT, // 画面が揺れて HP が減る
    A_HEAL,
    A_EXP,
    A_LEVEL,
    A_END,  // 戦闘を終えてフィールドへ
    A_DOWN, // 力尽きて最初から
  };

  // ---- 地図を作る ----
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

  // (sx, sy) から歩ける所への歩数を dist に入れる
  void bfs(int sx, int sy)
  {
    static int16_t qx[MW * MH], qy[MW * MH];
    memset(dist, 0xFF, sizeof(dist));
    int head = 0, tail = 0;
    dist[sy][sx] = 0;
    qx[tail] = sx;
    qy[tail++] = sy;
    while (head < tail)
    {
      int x = qx[head], y = qy[head++];
      static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (auto &d : D)
      {
        int nx = x + d[0], ny = y + d[1];
        if (nx < 0 || ny < 0 || nx >= MW || ny >= MH || dist[ny][nx] >= 0 || !passable(map[ny][nx]))
          continue;
        dist[ny][nx] = dist[y][x] + 1;
        qx[tail] = nx;
        qy[tail++] = ny;
      }
    }
  }

  void makeMap()
  {
    for (int tries = 0; tries < 8; tries++)
    {
      seed = esp_random();
      for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
        {
          float e = noise(x * 0.16f, y * 0.16f) * 0.7f + noise(x * 0.4f, y * 0.4f) * 0.3f;
          float edge = min(min(x, MW - 1 - x), min(y, MH - 1 - y)) / 6.0f; // 端は海
          e -= (1 - min(1.0f, edge)) * 0.35f;
          map[y][x] = e < 0.30f ? SEA : e < 0.35f ? SAND : e < 0.58f ? GRASS : e < 0.70f ? FOREST : MOUNT;
        }
      // 真ん中に近い草原から歩ける広さを見て、町を離して 3 つ置く
      int sx = -1, sy = -1;
      for (int r = 0; r < 12 && sx < 0; r++)
        for (int y = MH / 2 - r; y <= MH / 2 + r && sx < 0; y++)
          for (int x = MW / 2 - r; x <= MW / 2 + r && sx < 0; x++)
            if (map[y][x] == GRASS)
            {
              sx = x;
              sy = y;
            }
      if (sx < 0)
        continue;
      bfs(sx, sy);
      int reach = 0;
      for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
          reach += dist[y][x] >= 0;
      if (reach < 300)
        continue;
      int n = 0;
      for (int k = 0; k < 400 && n < 3; k++)
      {
        int x = irand(2, MW - 3), y = irand(2, MH - 3);
        if (dist[y][x] < 0 || map[y][x] != GRASS)
          continue;
        bool far = true;
        for (int i = 0; i < n; i++)
          far &= abs(townX[i] - x) + abs(townY[i] - y) >= 12;
        if (!far)
          continue;
        map[y][x] = TOWN;
        townX[n] = x;
        townY[n++] = y;
      }
      if (n == 3)
        return;
    }
  }

  // ---- 勇者の考え ----
  void chooseGoal()
  {
    if (hp < maxHp * 4 / 10)
    {
      // 一番近い町へ
      bfs(tx, ty);
      int best = 9999;
      for (int i = 0; i < 3; i++)
        if (dist[townY[i]][townX[i]] >= 0 && dist[townY[i]][townX[i]] < best)
        {
          best = dist[townY[i]][townX[i]];
          goalX = townX[i];
          goalY = townY[i];
        }
    }
    else
    {
      // 少し遠くの、行ける場所
      bfs(tx, ty);
      for (int k = 0; k < 200; k++)
      {
        int x = irand(1, MW - 2), y = irand(1, MH - 2);
        if (dist[y][x] >= 8 && dist[y][x] <= 24)
        {
          goalX = x;
          goalY = y;
          break;
        }
      }
    }
    bfs(goalX, goalY); // 目的地からの歩数。勇者はこれが減るほうへ歩く
  }

  void nextStep()
  {
    if (tx == goalX && ty == goalY)
      chooseGoal();
    static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    int best = dist[ty][tx], bi = -1;
    int start = irand(0, 3); // 同じ歩数の道が何本かあるときは、まちまちに選ぶ
    for (int k = 0; k < 4; k++)
    {
      auto &d = D[(start + k) % 4];
      int nx = tx + d[0], ny = ty + d[1];
      if (nx < 0 || ny < 0 || nx >= MW || ny >= MH)
        continue;
      if (dist[ny][nx] >= 0 && (best < 0 || dist[ny][nx] < best))
      {
        best = dist[ny][nx];
        bi = (start + k) % 4;
      }
    }
    if (bi < 0)
    {
      chooseGoal();
      return;
    }
    dx = D[bi][0];
    dy = D[bi][1];
    prog = 0;
  }

  void arrive()
  {
    tx += dx;
    ty += dy;
    dx = dy = 0;
    steps++;
    if (map[ty][tx] == TOWN && hp < maxHp)
    {
      mode = INN;
      modeT = 0;
      return;
    }
    if (calm > 0)
      calm--;
    else if (map[ty][tx] != TOWN && irand(0, map[ty][tx] == FOREST ? 7 : 13) == 0)
    {
      mode = ENCOUNTER;
      modeT = 0;
    }
  }

  // ---- 戦闘 ----
  void say(int kind, int val, const char *fmt, ...)
  {
    if (qN >= 10)
      return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(q[qN].text, sizeof(q[qN].text), fmt, ap);
    va_end(ap);
    q[qN].kind = kind;
    q[qN].val = val;
    qN++;
  }

  void startBattle()
  {
    // レベルに合った敵 (たまに一つ上)
    int top = 0;
    for (int i = 0; i < MON_N; i++)
      if (MONSTERS[i].minLv <= lv)
        top = i;
    int i = irand(max(0, top - 1), min(MON_N - 1, top + (irand(0, 4) == 0)));
    mon = &MONSTERS[i];
    monHp = monMaxHp = mon->hp + irand(0, mon->hp / 4);
    monBlink = shake = 0;
    cmd = 0;
    cmdT = 0;
    battleOver = heroDown = false;
    qN = qT = shown = 0;
    lines[0][0] = lines[1][0] = 0;
    say(A_NONE, 0, "%sが あらわれた！", mon->name);
    mode = BATTLE;
    modeT = 0;
  }

  // 勇者の番: 何をするか決めて、そのあとの敵の番までの文を積む
  void heroTurn()
  {
    bool heal = hp < maxHp * 35 / 100 && mp >= 3;
    bool flee = !heal && hp < maxHp / 4 && irand(0, 1) == 0;
    cmd = heal ? 1 : flee ? 2 : 0;
    cmdT = 24; // カーソルを動かして見せる時間
    if (heal)
    {
      int v = min(maxHp - hp, 10 + lv * 3);
      say(A_NONE, 0, "ゆうしゃは ヒールを となえた！");
      say(A_HEAL, v, "HPが %dかいふくした！", v);
    }
    else if (flee)
    {
      say(A_NONE, 0, "ゆうしゃは にげだした！");
      if (irand(0, 2))
      {
        say(A_END, 0, "うまく にげきれた！");
        return;
      }
      say(A_NONE, 0, "しかし にげられなかった！");
    }
    else
    {
      int d = max(1, (int)lroundf((4 + lv * 2) * frand(0.8f, 1.2f)));
      say(A_NONE, 0, "ゆうしゃの こうげき！");
      if (irand(0, 9) == 0)
      {
        d *= 2;
        say(A_NONE, 0, "クリティカルヒット！");
      }
      say(A_MON_HIT, d, "%sに %dの ダメージ！", mon->name, d);
      if (monHp - d <= 0)
      {
        say(A_NONE, 0, "%sを たおした！", mon->name);
        say(A_EXP, mon->exp, "けいけんちを %d かくとく！", mon->exp);
        return;
      }
    }
    // 敵の番
    int d = max(1, (int)lroundf(mon->atk * frand(0.7f, 1.2f)) - lv / 2);
    say(A_NONE, 0, "%sの こうげき！", mon->name);
    if (irand(0, 7) == 0)
      say(A_NONE, 0, "ゆうしゃは みをかわした！");
    else
      say(A_HERO_HIT, d, "ゆうしゃは %dの ダメージを うけた！", d);
  }

  void doAct(const Msg &m)
  {
    switch (m.kind)
    {
    case A_MON_HIT:
      monHp = max(0, monHp - m.val);
      monBlink = 12;
      break;
    case A_HERO_HIT:
      hp = max(0, hp - m.val);
      shake = 10;
      if (hp == 0)
      {
        // 後ろに積んである文は捨てて、力尽きた文に替える
        qN = 1;
        say(A_DOWN, 0, "ゆうしゃは ちからつきた…");
      }
      break;
    case A_HEAL:
      hp = min(maxHp, hp + m.val);
      mp -= 3;
      break;
    case A_EXP:
      exp += m.val;
      if (exp >= lv * lv * 8)
      {
        lv++;
        maxHp = 20 + lv * 6;
        maxMp = 4 + lv * 2;
        say(A_LEVEL, lv, "レベルが %dに あがった！", lv);
      }
      say(A_END, 0, "");
      break;
    case A_END:
      battleOver = true;
      break;
    case A_DOWN:
      heroDown = true;
      break;
    }
  }

  void stepBattle()
  {
    if (monBlink)
      monBlink--;
    if (shake)
      shake--;
    if (cmdT)
    {
      cmdT--;
      return;
    }
    if (qN == 0)
    {
      if (battleOver)
      {
        mode = FIELD;
        calm = 4;
        return;
      }
      if (heroDown)
      {
        reset();
        return;
      }
      heroTurn();
      return;
    }
    Msg &m = q[0];
    if (qT == 0)
    {
      // 新しい文: 下の窓を 1 行送って、動き (ダメージなど) を起こす
      strlcpy(lines[0], lines[1], sizeof(lines[0]));
      strlcpy(lines[1], m.text, sizeof(lines[1]));
      shown = 0;
      doAct(m);
    }
    qT++;
    int len = strlen(lines[1]);
    if (shown < len) // 1 文字ずつ出す (UTF-8 の 1 文字ぶん進める)
    {
      uint8_t c = lines[1][shown];
      shown += c < 0x80 ? 1 : c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4;
    }
    int wait = m.kind == A_DOWN ? 90 : m.kind == A_LEVEL ? 50 : 26;
    if (shown >= len && qT > len / 3 + wait)
    {
      memmove(&q[0], &q[1], sizeof(Msg) * (qN - 1));
      qN--;
      qT = 0;
    }
  }

  // ---- 描く ----
  void drawTile(LGFX_Sprite &g, int x, int y, uint8_t t, int mx, int my)
  {
    uint32_t h = (uint32_t)(hash2(mx * 7, my * 13) * 65535);
    switch (t)
    {
    case SEA:
      g.fillRect(x, y, TS, TS, (uint32_t)0x2050A8);
      if ((mx + my + frame / 15) % 3 == 0)
      {
        g.drawFastHLine(x + 2 + (h & 3), y + 4 + ((h >> 2) & 3), 5, (uint32_t)0x5080D0);
        g.drawFastHLine(x + 8 + ((h >> 4) & 3), y + 10 + ((h >> 6) & 3), 4, (uint32_t)0x5080D0);
      }
      break;
    case SAND:
      g.fillRect(x, y, TS, TS, (uint32_t)0xD8C890);
      g.drawPixel(x + (h & 15), y + ((h >> 4) & 15), (uint32_t)0xB8A870);
      g.drawPixel(x + ((h >> 8) & 15), y + ((h >> 12) & 15), (uint32_t)0xB8A870);
      break;
    case GRASS:
      g.fillRect(x, y, TS, TS, (uint32_t)0x48A040);
      for (int k = 0; k < 2; k++) // 草の房
      {
        int gx = x + 2 + ((h >> (k * 6)) & 7) + k * 5, gy = y + 3 + ((h >> (k * 6 + 3)) & 7) + k * 2;
        g.drawPixel(gx, gy, (uint32_t)0x2E7A2A);
        g.drawPixel(gx + 2, gy, (uint32_t)0x2E7A2A);
        g.drawPixel(gx + 1, gy - 1, (uint32_t)0x2E7A2A);
      }
      break;
    case FOREST:
      g.fillRect(x, y, TS, TS, (uint32_t)0x48A040);
      g.fillRect(x + 7, y + 11, 2, 4, (uint32_t)0x6A4020);
      g.fillCircle(x + 8, y + 7, 6, (uint32_t)0x1E6A28);
      g.fillCircle(x + 6, y + 5, 2, (uint32_t)0x30883A);
      break;
    case MOUNT:
      g.fillRect(x, y, TS, TS, (uint32_t)0x48A040);
      g.fillTriangle(x + 1, y + 15, x + 8, y + 1, x + 15, y + 15, (uint32_t)0x8A7A6A);
      g.fillTriangle(x + 8, y + 1, x + 15, y + 15, x + 8, y + 15, (uint32_t)0x6A5A4A);
      g.fillTriangle(x + 6, y + 5, x + 8, y + 1, x + 10, y + 5, (uint32_t)0xF0F0F0);
      break;
    case TOWN:
      g.fillRect(x, y, TS, TS, (uint32_t)0x48A040);
      for (int k = 0; k < 2; k++) // 家が 2 軒
      {
        int hx = x + 1 + k * 7, hy = y + 4 + (1 - k) * 4;
        g.fillRect(hx, hy + 3, 7, 5, (uint32_t)0xE8D8B8);
        g.fillTriangle(hx - 1, hy + 3, hx + 3, hy, hx + 7, hy + 3, (uint32_t)0xC83030);
        g.fillRect(hx + 3, hy + 5, 2, 3, (uint32_t)0x6A4020);
      }
      break;
    }
  }

  void drawField(LGFX_Sprite &g)
  {
    // 勇者を真ん中にして描く
    float hx = tx * TS + dx * prog, hy = ty * TS + dy * prog;
    int camX = (int)hx - GAME_W / 2 + TS / 2, camY = (int)hy - GAME_H / 2 + TS / 2;
    int mx0 = (int)floorf((float)camX / TS), my0 = (int)floorf((float)camY / TS);
    for (int my = my0; my <= my0 + GAME_H / TS + 1; my++)
      for (int mx = mx0; mx <= mx0 + GAME_W / TS + 1; mx++)
      {
        uint8_t t = mx < 0 || my < 0 || mx >= MW || my >= MH ? SEA : map[my][mx];
        drawTile(g, mx * TS - camX, my * TS - camY, t, mx, my);
      }
    int sx = (int)hx - camX + 2, sy = (int)hy - camY + 2;
    bool walkFrame = dx || dy ? (prog / 4) % 2 : (frame / 20) % 2;
    blit(g, sx, sy, walkFrame ? HERO_B : HERO_A, 13, HERO_PAL, 7, dx < 0);
    // 左上に小さな状態の窓
    char b[24];
    g.fillRoundRect(3, 3, 62, 22, 3, (uint32_t)0x000000);
    g.drawRoundRect(3, 3, 62, 22, 3, (uint32_t)0xF0F0F0);
    snprintf(b, sizeof(b), "LV %d", lv);
    hud(g, 8, 6, C_TEXT, b);
    snprintf(b, sizeof(b), "HP %d", hp);
    hud(g, 8, 15, hp < maxHp / 4 ? C_YELLOW : C_TEXT, b);
  }

  void window(LGFX_Sprite &g, int x, int y, int w, int h)
  {
    g.fillRoundRect(x, y, w, h, 4, (uint32_t)0x000000);
    g.drawRoundRect(x, y, w, h, 4, (uint32_t)0xF0F0F0);
    g.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 3, (uint32_t)0x606060);
  }

  void jtxt(LGFX_Sprite &g, int x, int y, uint32_t c, const char *s)
  {
    g.setFont(FJ12);
    g.setTextColor(c);
    g.setTextDatum(TL_DATUM);
    g.drawString(s, x, y);
  }

  void drawBattle(LGFX_Sprite &g)
  {
    int ox = shake ? irand(-2, 2) : 0, oy = shake ? irand(-2, 2) : 0;
    g.fillScreen((uint32_t)0x000000);
    // 上: 勇者の状態
    window(g, 2 + ox, 2 + oy, GAME_W - 4, 34);
    char b[64];
    jtxt(g, 8 + ox, 6 + oy, hp == 0 ? C_RED : hp < maxHp / 4 ? C_YELLOW : C_TEXT, "ゆうしゃ");
    snprintf(b, sizeof(b), "LV%d", lv);
    hud(g, GAME_W - 8 + ox, 8 + oy, C_TEXT, b, TR_DATUM);
    snprintf(b, sizeof(b), "H%3d M%3d", hp, mp);
    hud(g, 8 + ox, 23 + oy, hp < maxHp / 4 ? C_YELLOW : C_TEXT, b);

    // 真ん中: 敵 (やられると点滅、倒すと消える)
    const PalEntry *pal = mon->pal;
    int mw = strlen(mon->rows[0]) * 3, mh = mon->h * 3;
    int mx = (GAME_W - mw) / 2 + ox, my = 46 + (66 - mh) / 2 + oy;
    bool dead = monHp == 0 && monBlink == 0;
    if (!dead && !(monBlink && (monBlink / 2) % 2))
      blitScaled(g, mx, my + (int)(sinf(frame * 0.12f) * 2), mon->rows, mon->h, pal, mon->palN, 3);
    if (monBlink && monHp == 0) // 倒したときは白く光りながら消える
      blitScaled(g, mx, my, mon->rows, mon->h, pal, mon->palN, 3, 0xFFFFFF, true);

    // コマンドの窓
    window(g, 2 + ox, 120 + oy, 62, 52);
    static const char *CMDS[] = {"たたかう", "じゅもん", "にげる"};
    for (int i = 0; i < 3; i++)
      jtxt(g, 16 + ox, 126 + i * 15 + oy, C_TEXT, CMDS[i]);
    // カーソルは、考えている間は上から順に動いてから止まる
    int at = cmdT > 0 ? min(cmd, (24 - cmdT) / 8) : cmd;
    if (cmdT > 0 || qN > 0)
      if ((frame / 8) % 2 || cmdT == 0)
        g.fillTriangle(7 + ox, 128 + at * 15 + oy, 7 + ox, 136 + at * 15 + oy, 12 + ox, 132 + at * 15 + oy, C_TEXT);

    // 下: メッセージの窓。いまの文を 1 文字ずつ出し、幅が狭いので 3 行まで折り返す
    window(g, 2 + ox, 174 + oy, GAME_W - 4, 50);
    g.setFont(FJ12);
    static char line[3][LINE_CAP];
    char part[64];
    strlcpy(part, lines[1], min((int)sizeof(part), shown + 1));
    int n = *part ? wrapText(g, part, GAME_W - 14, line, 3) : 0;
    for (int i = 0; i < n; i++)
      jtxt(g, 8 + ox, 179 + i * 14 + oy, C_TEXT, line[i]);
  }

public:
  const char *name() const override { return "QUEST"; }

  void reset() override
  {
    makeMap();
    tx = townX[0];
    ty = townY[0];
    dx = dy = prog = 0;
    lv = 1;
    maxHp = hp = 26;
    maxMp = mp = 6;
    exp = 0;
    steps = 0;
    calm = 3;
    mode = FIELD;
    modeT = frame = 0;
    goalX = tx;
    goalY = ty;
    chooseGoal();
  }

  void step() override
  {
    frame++;
    modeT++;
    switch (mode)
    {
    case FIELD:
      if (dx || dy)
      {
        prog += 2;
        if (prog >= TS)
          arrive();
      }
      else
        nextStep();
      break;
    case ENCOUNTER:
      if (modeT > 24)
        startBattle();
      break;
    case BATTLE:
      stepBattle();
      break;
    case INN:
      if (modeT == 60)
      {
        hp = maxHp;
        mp = maxMp;
      }
      if (modeT > 120)
      {
        mode = FIELD;
        chooseGoal();
      }
      break;
    }
  }

  void draw(LGFX_Sprite &g) override
  {
    switch (mode)
    {
    case FIELD:
      drawField(g);
      break;
    case ENCOUNTER:
      // 白黒に点滅してから、黒く閉じる
      if (modeT < 16)
      {
        drawField(g);
        if ((modeT / 2) % 2)
          g.fillScreen((uint32_t)0xFFFFFF);
      }
      else
      {
        drawField(g);
        int h = (modeT - 16) * GAME_H / 16;
        g.fillRect(0, GAME_H / 2 - h / 2, GAME_W, h, (uint32_t)0x000000);
      }
      break;
    case BATTLE:
      drawBattle(g);
      break;
    case INN:
    {
      drawField(g);
      // 夜になって、また朝になる (上下から暗くして戻す)
      float k = modeT < 60 ? modeT / 40.0f : (120 - modeT) / 40.0f;
      int h = (int)(min(1.0f, k) * GAME_H / 2);
      g.fillRect(0, 0, GAME_W, h, (uint32_t)0x000010);
      g.fillRect(0, GAME_H - h, GAME_W, h, (uint32_t)0x000010);
      window(g, 4, GAME_H - 52, GAME_W - 8, 46);
      jtxt(g, 10, GAME_H - 45, C_TEXT, "やどやに とまった");
      if (modeT >= 60)
        jtxt(g, 10, GAME_H - 27, C_TEXT, "HPと MPが もどった");
      break;
    }
    }
  }
};
} // namespace

Demo *makeQuest() { return new Quest(); }
