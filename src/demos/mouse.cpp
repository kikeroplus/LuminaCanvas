// 迷路のネズミ: 毎回作る迷路でエサを集め、追ってくるネコから逃げる。四隅のチーズを食べるとネコが逃げ出し、捕まえられる。
// ネズミは一番近いエサを目指しつつ、ネコとの距離を見て危ない道を避ける
#include "demo.h"

namespace
{
const int TS = 8;                   // 1 マス
const int MW = GAME_W / TS;         // 14
const int MH = 27;                  // 迷路の高さ (マス)
const int OY = GAME_H - MH * TS;    // 上の得点の帯 (10px)
const int HALF = MW / 2;            // 左半分を作って右へ写す
const int SCARED_T = 240;           // チーズが効いている時間
const uint32_t BG = 0x05030C, WALL = 0x24104A, EDGE = 0xA868F0;

enum Cell : uint8_t
{
  WALLC,
  PATH,
  SEED,
  CHEESE,
};

struct Mover
{
  int x, y;     // マス
  int dx, dy;   // 進んでいる向き
  int prog;     // 0〜TS (次のマスへの進み)
  int speedAcc; // 速さの端数
  int faceX, faceY;
};

class Mouse : public Demo
{
  uint8_t maze[MH][MW];
  int16_t distSeed[MH][MW], distCat[MH][MW], distTmp[MH][MW];
  Mover me, cat[3];
  int catState[3]; // 0 = 追う、1 = 逃げる (青い)、2 = 捕まって巣で待つ
  int catWait[3];
  int homeX, homeY;
  int scared, lives, score, round, seeds, frame, deadT, clearT, overT, chain;
  Particles fx;

  bool open(int x, int y) const { return x >= 0 && y >= 0 && x < MW && y < MH && maze[y][x] != WALLC; }

  // ---- 迷路を作る ----
  void makeMaze()
  {
    memset(maze, WALLC, sizeof(maze));
    // 左半分: 奇数のマスを部屋にして、穴を掘って一筆の迷路にする
    static int8_t stx[64], sty[64];
    int sp = 0;
    maze[1][1] = PATH;
    stx[sp] = 1;
    sty[sp++] = 1;
    while (sp)
    {
      int x = stx[sp - 1], y = sty[sp - 1];
      int dirs[4] = {0, 1, 2, 3};
      for (int i = 3; i > 0; i--)
        std::swap(dirs[i], dirs[irand(0, i)]);
      bool moved = false;
      for (int d : dirs)
      {
        static const int D[4][2] = {{2, 0}, {-2, 0}, {0, 2}, {0, -2}};
        int nx = x + D[d][0], ny = y + D[d][1];
        if (nx < 1 || nx > HALF - 2 || ny < 1 || ny > MH - 2 || maze[ny][nx] != WALLC)
          continue;
        maze[y + D[d][1] / 2][x + D[d][0] / 2] = PATH;
        maze[ny][nx] = PATH;
        stx[sp] = nx;
        sty[sp++] = ny;
        moved = true;
        break;
      }
      if (!moved)
        sp--;
    }
    // 真ん中へ抜ける道 (右半分とつながる)
    for (int y = 1; y < MH - 1; y += 2)
      if (irand(0, 2) == 0 || y == MH / 2)
        maze[y][HALF - 1] = PATH;
    // 行き止まりをつぶして、回り道ができるようにする
    for (int y = 1; y < MH - 1; y += 2)
      for (int x = 1; x < HALF - 1; x += 2)
      {
        int n = open(x + 1, y) + open(x - 1, y) + open(x, y + 1) + open(x, y - 1);
        if (n > 1)
          continue;
        static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
        for (int k = 0, s = irand(0, 3); k < 4; k++)
        {
          auto &d = D[(s + k) % 4];
          int wx = x + d[0], wy = y + d[1];
          if (wx >= 1 && wx <= HALF - 1 && wy >= 1 && wy <= MH - 2 && maze[wy][wx] == WALLC)
          {
            maze[wy][wx] = PATH;
            break;
          }
        }
      }
    // 右半分へ写す
    for (int y = 0; y < MH; y++)
      for (int x = 0; x < HALF; x++)
        maze[y][MW - 1 - x] = maze[y][x];
    // ネコの巣 (真ん中の少し上の広場)
    homeX = HALF - 1;
    homeY = MH / 2 - 4 + ((MH / 2 - 4) % 2 == 0);
    for (int x = HALF - 2; x <= HALF + 1; x++)
      maze[homeY][x] = PATH;
    // エサとチーズ
    seeds = 0;
    for (int y = 0; y < MH; y++)
      for (int x = 0; x < MW; x++)
        if (maze[y][x] == PATH)
        {
          maze[y][x] = SEED;
          seeds++;
        }
    const int CX[4] = {1, MW - 2, 1, MW - 2}, CY[4] = {1, 1, MH - 2, MH - 2};
    for (int i = 0; i < 4; i++)
      if (maze[CY[i]][CX[i]] == SEED)
        maze[CY[i]][CX[i]] = CHEESE;
    for (int x = HALF - 2; x <= HALF + 1; x++) // 巣にはエサを置かない
      if (maze[homeY][x] == SEED)
      {
        maze[homeY][x] = PATH;
        seeds--;
      }
  }

  // 何か所かを出発点にした歩数の地図 (srcN 個)
  void bfs(int16_t d[MH][MW], const int *sx, const int *sy, int srcN)
  {
    static int16_t qx[MW * MH], qy[MW * MH];
    memset(d, 0x7F, sizeof(int16_t) * MW * MH);
    int head = 0, tail = 0;
    for (int i = 0; i < srcN; i++)
    {
      d[sy[i]][sx[i]] = 0;
      qx[tail] = sx[i];
      qy[tail++] = sy[i];
    }
    while (head < tail)
    {
      int x = qx[head], y = qy[head++];
      static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
      for (auto &e : D)
      {
        int nx = x + e[0], ny = y + e[1];
        if (!open(nx, ny) || d[ny][nx] <= d[y][x] + 1)
          continue;
        d[ny][nx] = d[y][x] + 1;
        qx[tail] = nx;
        qy[tail++] = ny;
      }
    }
  }

  void placeAll()
  {
    // 下の段から順に、真ん中寄りの道を探して出発点にする (ネコの巣から離す)
    me = {HALF - 1, MH - 2, 0, 0, 0, 0, -1, 0};
    static const int XS[] = {HALF - 1, HALF, HALF - 2, HALF + 1, HALF - 3, HALF + 2};
    for (int y = MH - 2, found = 0; y > homeY + 2 && !found; y--)
      for (int x : XS)
        if (open(x, y))
        {
          me.x = x;
          me.y = y;
          found = 1;
          break;
        }
    for (int i = 0; i < 3; i++)
    {
      cat[i] = {homeX - 1 + i, homeY, 0, 0, 0, 0, 0, 1};
      catState[i] = 0;
      catWait[i] = 30 + i * 60; // 1 匹ずつ出てくる
    }
    scared = 0;
  }

  void newRound()
  {
    makeMaze();
    placeAll();
    clearT = 0;
  }

  // ---- 動き ----
  // 速さ (8 で 1px/フレーム) に合わせて進める。マスの真ん中に着いたら true
  bool advance(Mover &m, int speed)
  {
    if (!m.dx && !m.dy)
      return true;
    m.speedAcc += speed;
    while (m.speedAcc >= 8)
    {
      m.speedAcc -= 8;
      if (++m.prog >= TS)
      {
        m.x += m.dx;
        m.y += m.dy;
        m.prog = 0;
        return true;
      }
    }
    return false;
  }

  void mouseThink()
  {
    int sx[3], sy[3], n = 0;
    for (int i = 0; i < 3; i++)
      if (catState[i] == 0)
      {
        sx[n] = cat[i].x;
        sy[n++] = cat[i].y;
      }
    if (n)
      bfs(distCat, sx, sy, n);
    else
      memset(distCat, 0x7F, sizeof(distCat));
    // チーズが効いていれば一番近い青いネコ、いなければエサ
    static int tx[MW * MH], ty[MW * MH];
    int tn = 0;
    if (scared > 40)
      for (int i = 0; i < 3; i++)
        if (catState[i] == 1)
        {
          tx[tn] = cat[i].x;
          ty[tn++] = cat[i].y;
        }
    if (!tn)
      for (int y = 0; y < MH; y++)
        for (int x = 0; x < MW; x++)
          if (maze[y][x] == SEED || maze[y][x] == CHEESE)
          {
            tx[tn] = x;
            ty[tn++] = y;
          }
    if (!tn)
      return;
    bfs(distSeed, tx, ty, tn);

    static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    float best = 1e9f;
    int bi = -1;
    for (int k = 0; k < 4; k++)
    {
      int nx = me.x + D[k][0], ny = me.y + D[k][1];
      if (!open(nx, ny))
        continue;
      float cost = distSeed[ny][nx];
      int dc = distCat[ny][nx];
      if (dc < 5)
        cost += (5 - dc) * 12; // ネコに近い道は避ける
      if (D[k][0] == -me.dx && D[k][1] == -me.dy && (me.dx || me.dy))
        cost += 1.5f; // 引き返すのは少しだけ嫌う (うろうろしないように)
      cost += frand(0, 0.3f);
      if (cost < best)
      {
        best = cost;
        bi = k;
      }
    }
    if (bi >= 0)
    {
      me.dx = D[bi][0];
      me.dy = D[bi][1];
      me.faceX = me.dx;
      me.faceY = me.dy;
    }
    else
      me.dx = me.dy = 0;
  }

  void catThink(int i)
  {
    Mover &c = cat[i];
    // 追う相手: 0 はネズミそのもの、1 はネズミの 4 マス先、2 は近いときだけ追ってふだんは気まま
    int gx = me.x, gy = me.y;
    if (i == 1)
    {
      gx = constrain(me.x + me.faceX * 4, 0, MW - 1);
      gy = constrain(me.y + me.faceY * 4, 0, MH - 1);
      for (int k = 0; k < 4 && !open(gx, gy); k++) // 壁の中なら少し戻す
      {
        gx -= me.faceX;
        gy -= me.faceY;
      }
    }
    bool wander = i == 2 && abs(c.x - me.x) + abs(c.y - me.y) > 8;
    bfs(distTmp, &gx, &gy, 1);
    static const int D[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    float best = 1e9f;
    int bi = -1;
    for (int k = 0; k < 4; k++)
    {
      int nx = c.x + D[k][0], ny = c.y + D[k][1];
      if (!open(nx, ny))
        continue;
      if (D[k][0] == -c.dx && D[k][1] == -c.dy && (c.dx || c.dy))
        continue; // ネコは引き返さない
      float cost = distTmp[ny][nx];
      if (catState[i] == 1)
        cost = -cost; // 逃げるときは遠いほうへ
      if (wander || irand(0, 5) == 0)
        cost = frand(0, 10);
      if (cost < best)
      {
        best = cost;
        bi = k;
      }
    }
    if (bi < 0) // 行き止まりなら引き返す
    {
      c.dx = -c.dx;
      c.dy = -c.dy;
    }
    else
    {
      c.dx = D[bi][0];
      c.dy = D[bi][1];
    }
    c.faceX = c.dx;
    c.faceY = c.dy;
  }

  float px(const Mover &m) const { return m.x * TS + m.dx * m.prog + TS / 2.0f; }
  float py(const Mover &m) const { return OY + m.y * TS + m.dy * m.prog + TS / 2.0f; }

  // ---- 描く ----
  void drawMouse(LGFX_Sprite &g, int x, int y, int fx, int fy)
  {
    // しっぽは進む向きの反対、鼻は進む向き。耳は鼻の左右後ろ
    float wob = sinf(frame * 0.5f) * 1.5f;
    g.drawLine(x - fx * 3, y - fy * 3, x - fx * 6 + fy * wob, y - fy * 6 + fx * wob, (uint32_t)0xD08090);
    g.fillCircle(x, y, 3, (uint32_t)0xB8B8C8);
    int ex = -fx * 1, ey = -fy * 1;
    g.fillCircle(x + ex + fy * 3, y + ey + fx * 3, 2, (uint32_t)0xF0A0B8);
    g.fillCircle(x + ex - fy * 3, y + ey - fx * 3, 2, (uint32_t)0xF0A0B8);
    g.fillCircle(x + fx * 3, y + fy * 3, 1, (uint32_t)0xB8B8C8);
    g.drawPixel(x + fx * 4, y + fy * 4, (uint32_t)0xFF5070);
    g.drawPixel(x + fx + fy, y + fy + fx, (uint32_t)0x101010);
    g.drawPixel(x + fx - fy, y + fy - fx, (uint32_t)0x101010);
  }

  void drawCat(LGFX_Sprite &g, int i, int x, int y)
  {
    static const uint32_t COL[3] = {0xF09030, 0x404048, 0xF0F0F0};
    const Mover &c = cat[i];
    bool blue = catState[i] == 1;
    bool blink = blue && scared < 60 && (frame / 6) % 2;
    uint32_t body = blue ? (blink ? (uint32_t)0xF0F0FF : (uint32_t)0x6080F0) : COL[i];
    // 三角の耳と丸い顔
    g.fillTriangle(x - 4, y - 1, x - 4, y - 6, x - 1, y - 3, body);
    g.fillTriangle(x + 4, y - 1, x + 4, y - 6, x + 1, y - 3, body);
    g.fillCircle(x, y, 4, body);
    if (i == 1 && !blue)
      g.drawCircle(x, y, 4, (uint32_t)0x707078); // 黒ネコは縁を明るく
    if (blue)
    {
      // 困った顔
      g.drawPixel(x - 2, y - 1, (uint32_t)0xFFFFFF);
      g.drawPixel(x + 2, y - 1, (uint32_t)0xFFFFFF);
      for (int k = -2; k <= 2; k++)
        g.drawPixel(x + k, y + 2 + (k & 1), (uint32_t)0xFFFFFF);
    }
    else
    {
      // 目は進む向きを見る
      uint32_t eye = i == 2 ? (uint32_t)0x40A040 : (uint32_t)0xF0E040;
      g.fillRect(x - 3, y - 2, 2, 2, eye);
      g.fillRect(x + 1, y - 2, 2, 2, eye);
      g.drawPixel(x - 3 + (c.faceX > 0), y - 2 + (c.faceY > 0), (uint32_t)0x000000);
      g.drawPixel(x + 1 + (c.faceX > 0), y - 2 + (c.faceY > 0), (uint32_t)0x000000);
      g.drawPixel(x, y + 1, (uint32_t)0xFF7090);
    }
  }

public:
  const char *name() const override { return "MOUSE"; }

  void reset() override
  {
    lives = 3;
    score = 0;
    round = 1;
    frame = deadT = overT = chain = 0;
    fx.clear();
    newRound();
  }

  void step() override
  {
    frame++;
    fx.step();
    if (overT)
    {
      if (++overT > 90)
        reset();
      return;
    }
    if (clearT)
    {
      if (++clearT > 70)
      {
        round++;
        newRound();
      }
      return;
    }
    if (deadT)
    {
      if (--deadT == 0)
      {
        if (lives < 0)
          overT = 1;
        else
          placeAll();
      }
      return;
    }
    if (scared && --scared == 0)
      for (int &s : catState)
        if (s == 1)
          s = 0;

    // ネズミ
    if (advance(me, 9))
    {
      uint8_t &t = maze[me.y][me.x];
      if (t == SEED)
      {
        t = PATH;
        score += 10;
        seeds--;
      }
      else if (t == CHEESE)
      {
        t = PATH;
        score += 50;
        scared = SCARED_T;
        chain = 0;
        for (int i = 0; i < 3; i++)
          if (catState[i] == 0)
          {
            catState[i] = 1;
            cat[i].dx = -cat[i].dx; // 驚いて向きを変える
            cat[i].dy = -cat[i].dy;
            cat[i].prog = cat[i].prog ? TS - cat[i].prog : 0;
            if (cat[i].prog)
            {
              cat[i].x -= cat[i].dx;
              cat[i].y -= cat[i].dy;
            }
          }
      }
      if (seeds <= 0)
      {
        clearT = 1;
        return;
      }
      mouseThink();
    }

    // ネコ
    for (int i = 0; i < 3; i++)
    {
      Mover &c = cat[i];
      if (catState[i] == 2 || catWait[i] > 0)
      {
        if (--catWait[i] <= 0)
        {
          catWait[i] = 0;
          if (catState[i] == 2)
          {
            catState[i] = 0;
            c = {homeX, homeY, 0, 0, 0, 0, 0, 1};
          }
        }
        if (catWait[i] > 0)
          continue;
      }
      int speed = catState[i] == 1 ? 5 : 7 + min(round - 1, 2);
      if (advance(c, speed))
        catThink(i);
      // ぶつかった
      if (fabsf(px(c) - px(me)) < 5 && fabsf(py(c) - py(me)) < 5)
      {
        if (catState[i] == 1)
        {
          int pts = 200 << min(chain++, 3);
          score += pts;
          fx.burst(px(c), py(c), 0x80A0FF, 14, 1.5f, 18);
          catState[i] = 2;
          catWait[i] = 120; // しばらくして巣から出直す
        }
        else if (catState[i] == 0)
        {
          fx.burst(px(me), py(me), 0xD0D0E0, 20, 1.6f, 26);
          fx.burst(px(me), py(me), 0xF0A0B8, 10, 1.0f, 20);
          lives--;
          deadT = 70;
          return;
        }
      }
    }
  }

  void draw(LGFX_Sprite &g) override
  {
    g.fillScreen(BG);
    // 壁: 塗りつぶして、道に面した辺に明るい線
    bool flash = clearT && (clearT / 6) % 2;
    uint32_t edge = flash ? (uint32_t)0xFFFFFF : EDGE;
    for (int y = 0; y < MH; y++)
      for (int x = 0; x < MW; x++)
      {
        int sx = x * TS, sy = OY + y * TS;
        uint8_t t = maze[y][x];
        if (t == WALLC)
        {
          g.fillRect(sx, sy, TS, TS, WALL);
          if (open(x, y - 1))
            g.drawFastHLine(sx, sy, TS, edge);
          if (open(x, y + 1))
            g.drawFastHLine(sx, sy + TS - 1, TS, edge);
          if (open(x - 1, y))
            g.drawFastVLine(sx, sy, TS, edge);
          if (open(x + 1, y))
            g.drawFastVLine(sx + TS - 1, sy, TS, edge);
        }
        else if (t == SEED)
          g.fillRect(sx + 3, sy + 3, 2, 2, (uint32_t)0xE8D8A0);
        else if (t == CHEESE && (frame / 10) % 3)
        {
          // チーズのかけら (三角に穴)
          g.fillTriangle(sx + 1, sy + 6, sx + 7, sy + 6, sx + 7, sy + 1, (uint32_t)0xF8C830);
          g.drawPixel(sx + 5, sy + 4, (uint32_t)0xC89010);
        }
      }

    for (int i = 0; i < 3; i++)
      if (catState[i] != 2 && !(deadT && deadT < 50))
        drawCat(g, i, (int)px(cat[i]), (int)py(cat[i]));
    if (!deadT && !overT)
      drawMouse(g, (int)px(me), (int)py(me), me.faceX, me.faceY);
    fx.draw(g, BG);

    char s[16];
    snprintf(s, sizeof(s), "%05d", score);
    hud(g, 2, 1, C_TEXT, s);
    snprintf(s, sizeof(s), "R%d", round);
    hud(g, GAME_W / 2, 1, C_DIM, s, TC_DATUM);
    for (int i = 0; i < lives; i++)
      g.fillCircle(GAME_W - 5 - i * 9, 5, 3, (uint32_t)0xB8B8C8);
    if (clearT)
      hud(g, GAME_W / 2, OY + MH * TS / 2 - 4, C_YELLOW, "CLEAR!", TC_DATUM);
    if (overT > 10)
      hud(g, GAME_W / 2, OY + MH * TS / 2 - 4, C_RED, "GAME OVER", TC_DATUM);
  }
};
} // namespace

Demo *makeMouse() { return new Mouse(); }
