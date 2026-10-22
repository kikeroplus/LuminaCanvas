// LuminaCanvas: 左に天気とニュースのページ、右の小窓にゲームのデモを次々に流す
#include <WiFi.h>
#include "common.h"
#include "pages.h"
#include "demos/demo.h"

LGFX lcd;
LGFX_Sprite info(&lcd); // 左のページ (INFO_W x 240)
LGFX_Sprite game(&lcd); // 右のデモ (GAME_W x GAME_H)

#define FJ12 (&fonts::lgfxJapanGothicP_12)
#define FJ16 (&fonts::lgfxJapanGothicP_16)

#define DEMO_MS   45000 // 1 つのデモを流す時間
#define WIPE_MS   300   // ページが替わるときの拭き取り
#define SHUTTER_N 24    // デモが替わるときのシャッター (フレーム)

// ---- ブザー (InfoPanel と同じ仕組み) ----
struct Note
{
  uint16_t freq, ms; // freq 0 は休み
};
const Note SND_CLICK[] = {{3200, 12}};
const Note SND_BOOT[] = {{784, 70}, {1175, 70}, {1568, 70}, {2349, 120}};
const Note SND_MUTE[] = {{1200, 40}, {0, 30}, {800, 60}};
// XEVIOUS のデモが始まるときのオリジナルの曲: Dm → C → B♭ → A を速い分散和音で下り、A の和音で上がって伸ばす
const Note SND_XEVIOUS[] = {
    {587, 70}, {698, 70}, {880, 70}, {1175, 70}, {0, 30},  // Dm
    {523, 70}, {659, 70}, {784, 70}, {1047, 70}, {0, 30},  // C
    {466, 70}, {587, 70}, {698, 70}, {932, 70}, {0, 30},   // B♭
    {440, 70}, {554, 70}, {659, 70}, {880, 70},            // A
    {1109, 100}, {1319, 100}, {0, 20}, {1760, 320},
};
#define PLAY(s) play(s, sizeof(s) / sizeof(s[0]))

bool muted = false;
const Note *seq = nullptr;
int seqN = 0, seqI = 0;
uint32_t seqAt = 0;

void startNote()
{
  if (seq[seqI].freq)
    tone(BUZZER_PIN, seq[seqI].freq);
  else
    noTone(BUZZER_PIN);
  seqAt = millis();
}

void play(const Note *s, int n)
{
  if (muted)
    return;
  seq = s;
  seqN = n;
  seqI = 0;
  startNote();
}

void soundUpdate()
{
  if (!seq || millis() - seqAt < seq[seqI].ms)
    return;
  if (++seqI < seqN)
    startNote();
  else
  {
    noTone(BUZZER_PIN);
    seq = nullptr;
  }
}

// ---- スイッチ: 同じ状態が 25ms 続いたら確定。0.7 秒押し続けると長押し ----
enum BtnEv
{
  EV_NONE,
  EV_SHORT,
  EV_LONG,
};
struct Button
{
  uint8_t pin;
  bool down, raw, longFired;
  uint32_t changedAt, downAt;
};
Button sw1 = {SW1_PIN}, sw2 = {SW2_PIN}, sw3 = {SW3_PIN}, sw4 = {SW4_PIN};

BtnEv buttonUpdate(Button &b)
{
  uint32_t now = millis();
  bool raw = digitalRead(b.pin) == LOW;
  if (raw != b.raw)
  {
    b.raw = raw;
    b.changedAt = now;
  }
  else if (raw != b.down && now - b.changedAt >= 25)
  {
    b.down = raw;
    if (raw)
    {
      b.downAt = now;
      b.longFired = false;
    }
    else if (!b.longFired)
      return EV_SHORT;
  }
  if (b.down && !b.longFired && now - b.downAt >= 700)
  {
    b.longFired = true;
    return EV_LONG;
  }
  return EV_NONE;
}

// ---- バックライト ----
const uint8_t BL_LEVELS[] = {255, 90, 20};
const char *BL_NAMES[] = {"明るい", "ふつう", "暗い"};
int blLevel = 0;

// ---- 画面の下に数秒だけ出す通知 ----
char toastMsg[48] = "";
uint32_t toastUntil = 0;
void toast(const char *msg)
{
  strlcpy(toastMsg, msg, sizeof(toastMsg));
  toastUntil = millis() + 3000;
}

// ---- ページ ----
int page = PG_NOW;
uint32_t pageAt = 0, wipeAt = 0;
bool autoRotate = true;
bool infoDirty = true;

void gotoPage(int p)
{
  page = (p + PG_N) % PG_N;
  pageAt = millis();
  wipeAt = millis();
  pageEnter(page);
  infoDirty = true;
}

void drawHeader(LGFX_Sprite &g, float prog)
{
  struct tm t;
  time_t now = time(nullptr);
  bool synced = now > 1700000000;
  localtime_r(&now, &t);
  char b[16];
  g.setTextDatum(TL_DATUM);
  if (synced)
  {
    snprintf(b, sizeof(b), "%d/%d", t.tm_mon + 1, t.tm_mday);
    g.setFont(FJ16);
    g.setTextColor(C_TEXT);
    g.drawString(b, 6, 6);
    int x = 6 + g.textWidth(b) + 3;
    g.setTextColor(t.tm_wday == 0 ? 0xFF7070 : t.tm_wday == 6 ? 0x70A8FF : C_DIM);
    snprintf(b, sizeof(b), "(%s)", WDAY_JA[t.tm_wday]);
    g.drawString(b, x, 6);
  }
  g.setFont(&fonts::DejaVu24);
  g.setTextColor(synced ? C_TEXT : C_DIM);
  g.setTextDatum(TR_DATUM);
  if (synced)
    snprintf(b, sizeof(b), "%d:%02d", t.tm_hour, t.tm_min);
  else
    strcpy(b, "--:--");
  g.drawString(b, INFO_W - 6, 2);

  // 自動で次のページへ行くまでの残り
  g.drawFastHLine(0, 27, INFO_W - 1, C_LINE);
  g.drawFastHLine(0, 27, (INFO_W - 1) * constrain(prog, 0.0f, 1.0f), autoRotate ? C_CYAN : C_AMBER);
}

void drawFooter(LGFX_Sprite &g, uint32_t ms)
{
  g.drawFastHLine(0, 225, INFO_W - 1, C_LINE);
  g.setFont(FJ12);
  g.setTextDatum(TL_DATUM);
  g.setTextColor(C_DIM);
  g.drawString(PAGE_NAMES[page], 5, 228);
  for (int i = 0; i < PG_N; i++)
    g.fillCircle(104 + i * 8, 233, i == page ? 2 : 1, i == page ? C_CYAN : C_LINE);
  char sub[16];
  pageSubInfo(page, ms, sub, sizeof(sub));
  g.setTextDatum(TR_DATUM);
  g.drawString(sub, INFO_W - 16, 228);
  // 通信の様子: 緑 = つながっている、黄の点滅 = 取得中、赤 = WiFi なし
  uint32_t c = !net.wifi ? C_RED : net.busy ? ((millis() / 250) % 2 ? C_YELLOW : C_LINE) : C_GREEN;
  g.fillCircle(INFO_W - 8, 233, 3, c);
}

void renderInfo()
{
  uint32_t ms = millis() - pageAt;
  info.fillScreen(C_BG);
  drawHeader(info, pageProgress(page, ms));
  drawPage(info, page, ms);
  drawFooter(info, ms);
  if (millis() < toastUntil)
  {
    info.setFont(FJ12);
    int w = info.textWidth(toastMsg) + 20;
    info.fillRoundRect((INFO_W - w) / 2, 200, w, 20, 6, C_PANEL);
    info.drawRoundRect((INFO_W - w) / 2, 200, w, 20, 6, C_CYAN);
    info.setTextColor(C_TEXT);
    info.setTextDatum(MC_DATUM);
    info.drawString(toastMsg, INFO_W / 2, 210);
  }
  info.drawFastVLine(INFO_W - 1, 0, SCREEN_H, C_LINE); // 右の小窓との仕切り

  // ページが替わった直後は左から拭き取るように出す
  uint32_t w = millis() - wipeAt;
  if (w < WIPE_MS)
  {
    int x = INFO_W * w / WIPE_MS;
    lcd.setClipRect(0, 0, x, SCREEN_H);
    info.pushSprite(0, 0);
    lcd.clearClipRect();
    lcd.drawFastVLine(x, 0, SCREEN_H, C_CYAN);
  }
  else
    info.pushSprite(0, 0);
}

// ---- デモ ----
Demo *demos[9];
const int DEMO_N = sizeof(demos) / sizeof(demos[0]);
int demoI = 0;
uint32_t demoAt = 0;
int shutter = 0; // 0 でなければ切り替え中 (SHUTTER_N から数え下げる)

void drawDemoLabel()
{
  lcd.fillRect(GAME_X, 0, GAME_W, GAME_LABEL_H, C_PANEL);
  lcd.fillTriangle(GAME_X + 5, 3, GAME_X + 5, 10, GAME_X + 10, 6, C_CYAN);
  lcd.setFont(&fonts::Font0);
  lcd.setTextSize(1);
  lcd.setTextDatum(TL_DATUM);
  lcd.setTextColor(C_TEXT);
  lcd.drawString(demos[demoI]->name(), GAME_X + 14, 3);
  char b[8];
  snprintf(b, sizeof(b), "%d/%d", demoI + 1, DEMO_N);
  lcd.setTextDatum(TR_DATUM);
  lcd.setTextColor(C_DIM);
  lcd.drawString(b, SCREEN_W - 4, 3);
}

void nextDemo()
{
  if (!shutter)
    shutter = SHUTTER_N;
}

void gameFrame()
{
  if (shutter)
  {
    shutter--;
    if (shutter == SHUTTER_N / 2) // 閉じきったところで入れ替える
    {
      demoI = (demoI + 1) % DEMO_N;
      demos[demoI]->reset();
      demoAt = millis();
      drawDemoLabel();
      if (!strcmp(demos[demoI]->name(), "XEVIOUS"))
        PLAY(SND_XEVIOUS);
    }
  }
  else
  {
    demos[demoI]->step();
    if (millis() - demoAt >= DEMO_MS)
      nextDemo();
  }
  demos[demoI]->draw(game);

  if (shutter)
  {
    // 上下から閉じて、また開く
    int half = SHUTTER_N / 2;
    int k = shutter > half ? SHUTTER_N - shutter : shutter; // 0〜half
    int h = (GAME_H / 2) * k / half;
    game.fillRect(0, 0, GAME_W, h, C_PANEL);
    game.fillRect(0, GAME_H - h, GAME_W, h, C_PANEL);
    game.drawFastHLine(0, h, GAME_W, C_CYAN);
    game.drawFastHLine(0, GAME_H - h - 1, GAME_W, C_CYAN);
    if (k >= half - 2)
    {
      int nextI = shutter > half ? (demoI + 1) % DEMO_N : demoI;
      hud(game, GAME_W / 2, GAME_H / 2 - 4, C_CYAN, demos[nextI]->name(), TC_DATUM);
    }
  }
  game.pushSprite(GAME_X, GAME_LABEL_H);
}

// ---- 操作 ----
void handleButtons()
{
  BtnEv e1 = buttonUpdate(sw1), e2 = buttonUpdate(sw2), e3 = buttonUpdate(sw3), e4 = buttonUpdate(sw4);
  if (e1 || e2 || e3 || e4)
  {
    PLAY(SND_CLICK);
    infoDirty = true;
  }
  if (e1 == EV_SHORT)
    gotoPage(page + 1);
  if (e2 == EV_SHORT)
    gotoPage(page - 1);
  if (e3 == EV_SHORT)
    nextDemo();
  if (e4 == EV_SHORT)
    pageNextItem(page);
  if (e1 == EV_LONG)
  {
    blLevel = (blLevel + 1) % (sizeof(BL_LEVELS) / sizeof(BL_LEVELS[0]));
    lcd.setBrightness(BL_LEVELS[blLevel]);
    toast((String("明るさ: ") + BL_NAMES[blLevel]).c_str());
  }
  if (e2 == EV_LONG)
  {
    muted = !muted;
    if (muted)
      noTone(BUZZER_PIN);
    else
      PLAY(SND_MUTE);
    toast(muted ? "音: 消す" : "音: 鳴らす");
  }
  if (e3 == EV_LONG)
  {
    autoRotate = !autoRotate;
    pageAt = millis();
    pageEnter(page);
    toast(autoRotate ? "ページの自動切り替え: する" : "ページの自動切り替え: 止める");
  }
  if (e4 == EV_LONG)
  {
    netRefreshAll();
    toast("天気とニュースを取り直します");
  }
}

void splash()
{
  lcd.fillScreen(C_BG);
  lcd.setTextDatum(MC_DATUM);
  lcd.setFont(&fonts::Orbitron_Light_24);
  lcd.setTextColor(C_CYAN);
  lcd.drawString("LuminaCanvas", SCREEN_W / 2, 96);
  lcd.setFont(FJ16);
  lcd.setTextColor(C_DIM);
  lcd.drawString("WiFi に接続しています…", SCREEN_W / 2, 140);
  for (int i = 0; i < 5; i++)
    lcd.fillRect(SCREEN_W / 2 - 60 + i * 26, 118, 18, 3, mix(C_CYAN, C_AMBER, i / 4.0f));
}

void setup()
{
  Serial.begin(115200);
  delay(200);
  Serial.printf("psram: %s, size %u\n", psramFound() ? "found" : "NOT FOUND", ESP.getPsramSize());

  lcd.init();
  lcd.setRotation(TFT_ROTATION);
  lcd.setBrightness(BL_LEVELS[blLevel]);
  splash();

  info.setColorDepth(16);
  info.setPsram(true);
  game.setColorDepth(16);
  game.setPsram(true);
  if (!info.createSprite(INFO_W, SCREEN_H) || !game.createSprite(GAME_W, GAME_H))
    Serial.println("sprite alloc failed!");

  pinMode(SW1_PIN, INPUT_PULLUP);
  pinMode(SW2_PIN, INPUT_PULLUP);
  pinMode(SW3_PIN, INPUT_PULLUP);
  pinMode(SW4_PIN, INPUT_PULLUP);

  demos[0] = makeTetris();
  demos[1] = makeInvaders();
  demos[2] = makeBreakout();
  demos[3] = makePlatformer();
  demos[4] = makeShooter();
  demos[5] = makeXevious();
  demos[6] = makeQuest();
  demos[7] = makeMouse();
  demos[8] = makeSnake();
  demos[0]->reset();

  netBegin();
  PLAY(SND_BOOT);
  // WiFi がつながるまで (長くても 6 秒) 起動画面を出しておく
  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - t0 < 6000)
  {
    soundUpdate();
    delay(10);
  }

  lcd.fillScreen(C_BG);
  demoAt = millis();
  drawDemoLabel();
  gotoPage(PG_NOW);
  Serial.printf("heap: free %u, psram free %u\n", ESP.getFreeHeap(), ESP.getFreePsram());
}

void loop()
{
  handleButtons();
  soundUpdate();

  uint32_t now = millis();
  if (autoRotate && pageProgress(page, now - pageAt) >= 1.0f)
    gotoPage(page + 1);

  static uint32_t lastGame = 0;
  if (now - lastGame >= 33) // 約 30 フレーム/秒
  {
    lastGame = now;
    gameFrame();
  }

  // 左は 0.25 秒ごと (時計と進み具合の線)。替わった直後と拭き取り中はすぐ
  static uint32_t lastInfo = 0;
  if (infoDirty || millis() - wipeAt < WIPE_MS + 40 || millis() - lastInfo >= 250)
  {
    infoDirty = false;
    lastInfo = millis();
    renderInfo();
  }
  delay(1);
}
