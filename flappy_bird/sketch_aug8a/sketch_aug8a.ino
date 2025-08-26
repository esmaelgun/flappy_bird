#include <Deneyap_OLED.h>
#include <Deneyap_KumandaKolu.h>

OLED ekran;
Joystick kumanda;

const uint8_t OLED_ADDRESS = 0x7A;     
const uint8_t JOYSTICK_ADDRESS = 0x1A; 

// ---------------- Menü ----------------
const int menuCount = 3;
String menuItems[menuCount] = {"Kolay Seviye", "Orta Seviye", "Zor Seviye"};
int menuIndex = 0;
int lastIndex = -1;
int menuRows[menuCount] = {2, 4, 6};

uint16_t xCenter = 2048;
int DEADZONE = 550;
bool joyNeutral = true;
bool prevBtn = false;

// ---------------- Durum makinesi ----------------
enum State { MENU, GAME, GAMEOVER, WIN };
State state = MENU;
int selectedMode = 0;

// ---------------- Kuş ----------------
int birdY = 3;
int birdX = 5;
unsigned long lastUpdate;
int gravityDelay = 300;

// ---------------- Engeller ----------------
struct Obstacle {
  int x;
  int gapY;
  int gapSize;
  bool passed;
};

#define MAX_OBS 5
Obstacle obstacles[MAX_OBS];
int obsCount = 0;
unsigned long lastObsTime;
int obsInterval = 2000;   // engel çıkış süresi (ms)

// ---------------- Skor ve Seviye ----------------
int score = 0;
int targetScore = 30;
int gameSpeed = 200;      // engel kayma hızı (ms)
unsigned long lastMoveTime;

// ---------------- Yardımcı Fonksiyonlar ----------------
void putCentered(int row, const String &text) {
  int col = (21 - text.length()) / 2;
  if (col < 0) col = 0;
  ekran.setTextXY(row, col);
  ekran.putString(text);
}

void drawMenu() {
  ekran.clearDisplay();
  for (int i = 0; i < menuCount; i++) {
    String t = menuItems[i];
    int col = (21 - t.length()) / 2;
    if (i == menuIndex) {
      ekran.setTextXY(menuRows[i], col - 2 < 0 ? 0 : col - 2);
      ekran.putString("> ");
    }
    ekran.setTextXY(menuRows[i], col);
    ekran.putString(t);
  }
}

void calibrateX() {
  uint32_t sum = 0;
  const int N = 30;
  for (int i = 0; i < N; i++) {
    sum += kumanda.xRead();
    delay(5);
  }
  xCenter = sum / N;
  DEADZONE = max(400, (int)(0.15 * xCenter));
}

// Kuş
void drawBird(int y) {
  ekran.setTextXY(y, birdX);
  ekran.putString("o");
}
void clearBird(int y) {
  ekran.setTextXY(y, birdX);
  ekran.putChar(' ');
}

// Engeller
void addObstacle() {
  if (obsCount < MAX_OBS) {
    obstacles[obsCount].x = 20;
    obstacles[obsCount].gapY = random(1, 5); // rastgele boşluk
    obstacles[obsCount].gapSize = 2;
    obstacles[obsCount].passed = false;
    obsCount++;
  }
}

void drawObstacles() {
  for (int i = 0; i < obsCount; i++) {
    for (int row = 1; row < 8; row++) { // 0. satır skor için boş
      if (row < obstacles[i].gapY || row > obstacles[i].gapY + obstacles[i].gapSize) {
        ekran.setTextXY(row, obstacles[i].x);
        ekran.putChar('|');
      }
    }
  }
}

void moveObstacles() {
  for (int i = 0; i < obsCount; i++) {
    // eski engeli sil
    for (int row = 1; row < 8; row++) {
      if (row < obstacles[i].gapY || row > obstacles[i].gapY + obstacles[i].gapSize) {
        ekran.setTextXY(row, obstacles[i].x);
        ekran.putChar(' ');
      }
    }
    obstacles[i].x--;
  }

  // ekrandan çıkanları temizle
  int k = 0;
  for (int i = 0; i < obsCount; i++) {
    if (obstacles[i].x >= 0) {
      obstacles[k++] = obstacles[i];
    }
  }
  obsCount = k;

  drawObstacles();
}

// Çarpışma kontrolü
bool checkCollision() {
  for (int i = 0; i < obsCount; i++) {
    if (obstacles[i].x == birdX) {
      if (birdY < obstacles[i].gapY || birdY > obstacles[i].gapY + obstacles[i].gapSize) {
        return true;
      }
    }
  }
  return false;
}

// ---------------- Setup ----------------
void setup() {
  ekran.begin(OLED_ADDRESS);
  ekran.init();
  ekran.setFont(font5x7);
  ekran.clearDisplay();

  kumanda.begin(JOYSTICK_ADDRESS);
  calibrateX();
  randomSeed(analogRead(0));

  drawMenu();
  lastIndex = menuIndex;
  lastUpdate = millis();
  lastMoveTime = millis();
  lastObsTime = millis();
}

// ---------------- Loop ----------------
void loop() {
  bool btn = kumanda.swRead();
  uint16_t x = kumanda.xRead();

  switch (state) {
    case MENU: {
      if (joyNeutral) {
        if ((int)x < (int)xCenter - DEADZONE) {
          menuIndex++;
          if (menuIndex >= menuCount) menuIndex = 0;
          joyNeutral = false;
        } else if ((int)x > (int)xCenter + DEADZONE) {
          menuIndex--;
          if (menuIndex < 0) menuIndex = menuCount - 1;
          joyNeutral = false;
        }
      }
      if (abs((int)x - (int)xCenter) <= DEADZONE / 3) {
        joyNeutral = true;
      }
      if (menuIndex != lastIndex) {
        drawMenu();
        lastIndex = menuIndex;
      }
      if (btn && !prevBtn) {
        state = GAME;
        selectedMode = menuIndex;

        // ✅ Seviye ayarları
        if (selectedMode == 0) { 
          targetScore = 30; 
          gameSpeed = 280;    // kolay → yavaş
          obsInterval = 2500; // seyrek engel
        }
        if (selectedMode == 1) { 
          targetScore = 50; 
          gameSpeed = 180;    // orta hız
          obsInterval = 2000; // orta sıklık
        }
        if (selectedMode == 2) { 
          targetScore = 70; 
          gameSpeed = 80;     // zor → çok hızlı
          obsInterval = 1200; // sık engel
        }

        ekran.clearDisplay();
        birdY = 3;
        score = 0;
        obsCount = 0;
        lastUpdate = millis();
        lastMoveTime = millis();
        lastObsTime = millis();

        drawBird(birdY);
      }
      break;
    }

    case GAME: {
      unsigned long now = millis();

      // skor göster
      ekran.setTextXY(0, 0);
      ekran.putString("Skor:" + String(score) + "   ");

      // Yerçekimi
      if (now - lastUpdate > gravityDelay) {
        clearBird(birdY);
        birdY++;
        if (birdY > 7) birdY = 7;
        drawBird(birdY);
        lastUpdate = now;
      }

      // Zıplama
      if (btn && !prevBtn) {
        clearBird(birdY);
        birdY -= 2;
        if (birdY < 1) birdY = 1; // skor satırı 0 olduğu için min 1
        drawBird(birdY);
      }

      // Yeni engel ekle
      if (now - lastObsTime > obsInterval) {
        addObstacle();
        lastObsTime = now;
      }

      // Engelleri kaydır
      if (now - lastMoveTime > gameSpeed) {
        moveObstacles();
        lastMoveTime = now;
      }

      // Çarpışma kontrolü
      if (checkCollision() || birdY >= 7) {
        state = GAMEOVER;
      }

      // Skor arttır
      for (int i = 0; i < obsCount; i++) {
        if (!obstacles[i].passed && obstacles[i].x < birdX) {
          score++;
          obstacles[i].passed = true;
        }
      }
      if (score >= targetScore) {
        state = WIN;
      }
      break;
    }

    case GAMEOVER: {
      ekran.clearDisplay();
      putCentered(3, "OYUN BITTI!");
      putCentered(5, "Skor: " + String(score));
      delay(3000);

      // oyun bitince engelleri sıfırla
      obsCount = 0;
      ekran.clearDisplay();

      state = MENU;
      drawMenu();
      break;
    }

    case WIN: {
      ekran.clearDisplay();
      putCentered(3, "TEBRIKLER!");
      putCentered(5, "Skor: " + String(score));
      delay(3000);

      // oyun bitince engelleri sıfırla
      obsCount = 0;
      ekran.clearDisplay();

      state = MENU;
      drawMenu();
      break;
    }
  }

  prevBtn = btn;
  delay(10);
}
