#include <SPI.h>
#include "SdFat.h"
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>


#define TFT_CS        7
#define TFT_RST        9
#define TFT_DC         8

#define SD_CS          10

// https://github.com/greiman/SdFat
SdFat SD;

Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

// Color definitions
#define BLACK 0x0000
#define BLUE 0x001F
#define RED 0xF800
#define GREEN 0x07E0
#define CYAN 0x07FF
#define MAGENTA 0xF81F
#define YELLOW 0xFFE0
#define WHITE 0xFFFF

// Buttons
#define BTN_DOWN 3
#define BTN_UP 2
#define BTN_FIRE 5

// Game Rules
#define METEOR_SIZE 8
uint16_t LaserSpeed = 10;
uint16_t FighterMovementSpeed = 100;
uint16_t MeteorMovementSpeed = 100;
uint16_t MeteorSpawnRate = 2000;

// Timings
unsigned long currentMillis = 0;
unsigned long prevMeteorMillis = 0;
unsigned long prevShootMillis = 0;
unsigned long prevFighterMillis = 0;
unsigned long prevMeteorSpawnMillis = 0;
unsigned long gameStartMillis = 0;
unsigned long prevUpdateScore = 0;

class Fighter {
  private:
    uint16_t color;
  public:
    uint16_t x0;
    uint16_t x1;
    uint16_t y0;
    uint16_t y1;
    uint16_t x2;
    uint16_t y2;
    Fighter(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
      setPosition(x0, y0, x1, y1, x2, y2);
      init();
    }
    void init() {
      this->color = GREEN;
    }
    void draw(uint16_t color) {
      tft.drawTriangle(x0, y0, x1, y1, x2, y2, color);
    }
    void moveUp(uint16_t amount) {
      draw(BLACK);
      this->y0 -= amount;
      this->y1 -= amount;
      this->y2 -= amount;
      draw(GREEN);
    }
    void moveDown(uint16_t amount) {
      draw(BLACK);
      this->y0 += amount;
      this->y1 += amount;
      this->y2 += amount;
      draw(GREEN);
    }
    void setPosition(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2) {
      this->x0 = x0;
      this->x1 = x1;
      this->x2 = x2;
      this->y0 = y0;
      this->y1 = y1;
      this->y2 = y2;
    }
};

class Meteor {
  private:
    uint16_t color;
    uint16_t radius;
  public:
    uint16_t width;
    uint16_t height;
    uint16_t y0;
    uint16_t x0;
    bool visible;
    Meteor() {
      init();
    }
    void init() {
      // https://www.programmingelectronics.com/using-random-numbers-with-arduino/
      randomSeed(analogRead(0));
      this->y0 = random(5, 75);
      this->x0 = 160;
      this->radius = 2;
      this->height = METEOR_SIZE;
      this->width = METEOR_SIZE;
      this->color = MAGENTA;
      this->visible = false;
    }
    void draw(uint16_t color) {
      tft.fillRoundRect(x0, y0, width, height, radius, color);
    }
    void move() {
      draw(BLACK);
      x0 -= 4;
      draw(YELLOW);
      if (x0 > 160) {
        draw(BLACK);
        init();
      }
    }
    void setVisible(bool visible) {
      this->visible = visible;
    }
};

class Laser {
  public:
    Laser() {
      init(0, 0);
    }
    uint16_t x0;
    uint16_t x1;
    uint16_t y0;
    uint16_t y1;
    bool visible = false;
    init(uint16_t x0, uint16_t y0) {
      this->x0 = x0;
      this->y0 = y0;
      this->x1 = x0 - 3;
      this->y1 = y0;
    }
    setVisible(bool visible) {
      this->visible = visible;
    }
    void move() {
      draw(BLACK);
      x0 += 4;
      x1 += 4;
      draw(YELLOW);
      if (x0 > 160) {
        init(0, 0);
      }
    }
    void draw(uint16_t color) {
      tft.drawLine(x0, y0, x1, y1, color);
    }
    void shoot(uint16_t x2, uint16_t y2) {
      visible = true;
      tone(6, 800, 50);
      delay(1);
      tone(6, 300, 50);
      init(x2, y2);
    }
};

Fighter fighter(30, 35, 30, 15, 40, 25); // initial position

// States
bool crashed = false;
bool startgame = false;
bool updateMenu = true;
bool menuHighscore = false;
bool menuResetHighscore = false;
uint16_t selectedMenuItem = 1;

Meteor meteors[5] = Meteor();
uint16_t spawned = 0;

Laser lasers[3] = Laser();
uint16_t shots = 0;
uint16_t hits = 0;

bool printed = false;

uint16_t currentHighscore;


void setup() {
  Serial.begin(9600);
  SPI.begin();

  // From SD example.
  Serial.print("Initializing SD card...");

  if (!SD.begin(SD_CS)) {
    Serial.println("initialization failed!");
    return;
  }
  Serial.println("initialization done.");

  tft.initR(INITR_MINI160x80);
  tft.invertDisplay(true);
  tft.setRotation(3);
  tft.fillScreen(BLACK);

  pinMode(BTN_DOWN, INPUT);
  pinMode(BTN_UP, INPUT);
  pinMode(BTN_FIRE, INPUT);

  currentHighscore = getHighscore();
  fighter.setPosition(32, 35, 32, 15, 42, 25);
}

void loop()
{
  while (!startgame) {
    showStartMenu();
  }
  if (!crashed) {
    currentMillis = millis();

    // Laser shots
    if (currentMillis - prevShootMillis >= LaserSpeed) {
      prevShootMillis = currentMillis;
      for (int i = 0; i < 3; i++) {
        if (lasers[i].visible) {
          lasers[i].move();
          for (int j = 0; j < 5; j++) {
            if (meteors[j].visible) {
              if (checkShotHit(lasers[i], meteors[j])) {
                tone(6, 1000, 100);
                meteors[j].draw(BLACK);
                meteors[j].init();
                hits++;
              }
            }
          }
        }
      }
    }

    // Meteor spawn
    if (currentMillis - prevMeteorSpawnMillis >= MeteorSpawnRate) {
      prevMeteorSpawnMillis = currentMillis;
      Serial.println(spawned);
      meteors[spawned].setVisible(true);
      if (spawned == 4) {
        spawned = 0;
        if (MeteorMovementSpeed > 30) {
          MeteorMovementSpeed -= 10;
        }
        if (MeteorSpawnRate > 500) {
          MeteorSpawnRate -= 500;
        }
      } else {
        spawned ++;
      }
    }

    // Meteor movement
    if (currentMillis - prevMeteorMillis >= MeteorMovementSpeed) {
      prevMeteorMillis = currentMillis;
      for (int i = 0; i < 5; i++) {
        if (meteors[i].visible) {
          meteors[i].move();
          if (checkCollision(fighter, meteors[i])) {
            crashed = true;
          }

        }
      }
    }

    // update score
    if (currentMillis - prevUpdateScore >= 500) {
      prevUpdateScore = currentMillis;
      uint16_t currentScore = getScore();
      tft.setCursor(120, 5);
      tft.setTextSize(1);
      tft.fillRect(120, 5, 40, 10, BLACK);
      tft.setTextColor(GREEN);
      tft.print(currentScore);


    }

    if (currentMillis - prevFighterMillis >= FighterMovementSpeed) {
      prevFighterMillis = currentMillis;
      fighter.draw(GREEN);

      if (digitalRead(BTN_FIRE) == HIGH) {

        if (shots < 3) {
          lasers[shots].shoot(fighter.x2, fighter.y2);
          shots++;
        } else {
          shots = 0;
        }

      }
      if (digitalRead(BTN_UP) == HIGH) {
        fighter.moveUp(5);
      }
      if (digitalRead(BTN_DOWN) == HIGH) {
        fighter.moveDown(5);
      }
    }
  } else {
    gameOver();
  }

}


void showStartMenu() {

  while (updateMenu) {
    menuHighscore = false;
    tft.fillRoundRect(30, 10, 100, 60, 5, RED);
    tft.setCursor(50, 15);
    tft.setTextColor(BLACK);
    tft.setTextSize(1, 3);
    tft.print("New Game");
    tft.setCursor(50, 40);
    tft.print("Highscore");
    fighter.draw(GREEN);
    updateMenu = false;
  }

  // Handle button-actions
  if (digitalRead(BTN_FIRE) == HIGH) {
    if (menuHighscore) {
      if (selectedMenuItem == 1 && menuResetHighscore) {
        resetHighscore();
        currentHighscore = getHighscore();
        showHighscore();
        menuResetHighscore = false;
        selectedMenuItem = 0;
        delay(250);
        return;
      } else if (selectedMenuItem == 2) {
        showHighscore();
        menuResetHighscore = false;
        selectedMenuItem = 0;
        delay(250);
        return;
      }
      showResetHighscore();
      delay(250);
    } else {
      if (selectedMenuItem == 1) {
        tft.fillScreen(BLACK);
        fighter.setPosition(10, 35, 10, 15, 20, 25);
        startgame = true;
        gameStartMillis = millis();
      } else if (selectedMenuItem == 2) {
        showHighscore();
        selectedMenuItem = 0;
        delay(250);
      }
    }
  }

  if (digitalRead(BTN_UP) == HIGH) {
    if (menuHighscore && !menuResetHighscore) {
      // Go back
      updateMenu = true;
      selectedMenuItem = 2;
      delay(250);
    } else {
      fighter.draw(RED);
      fighter.setPosition(32, 35, 32, 15, 42, 25);
      fighter.draw(GREEN);
      selectedMenuItem = 1;
      delay(250);
    }
  }

  if (digitalRead(BTN_DOWN) == HIGH) {
    if (menuHighscore && !menuResetHighscore) {
      // Go back
      updateMenu = true;
      selectedMenuItem = 2;
      delay(250);
    } else {
      fighter.draw(RED);
      fighter.setPosition(32, 60, 32, 40, 42, 50);
      fighter.draw(GREEN);
      selectedMenuItem = 2;
      delay(250);
    }
  }
}

void showHighscore() {
  tft.fillScreen(BLACK);
  tft.fillRoundRect(30, 10, 100, 60, 5, RED);
  tft.setCursor(50, 15);
  tft.setTextColor(BLACK);
  tft.setTextSize(1, 3);
  tft.print("Highscore:");
  tft.setCursor(50, 40);
  tft.print(currentHighscore);
  menuHighscore = true;
}

void showResetHighscore() {
  tft.fillRoundRect(30, 10, 100, 60, 5, RED);
  tft.setCursor(50, 15);
  tft.setTextColor(BLACK);
  tft.setTextSize(1, 3);
  tft.print("Reset?");
  tft.setCursor(50, 40);
  tft.print("Back");
  fighter.setPosition(32, 35, 32, 15, 42, 25);
  fighter.draw(GREEN);
  selectedMenuItem = 1;
  menuResetHighscore = true;
}


bool checkCollision(Fighter fighter, Meteor meteor) {
  if (fighter.x0 - meteor.x0 < METEOR_SIZE && fighter.y0 - meteor.y0 < METEOR_SIZE) {
    return true;
  } else if (fighter.x1 - meteor.x0 < METEOR_SIZE && fighter.y1 - meteor.y0 < METEOR_SIZE) {
    return true;
  } else if (fighter.x2 - meteor.x0 < METEOR_SIZE && fighter.y2 - meteor.y0 < METEOR_SIZE) {
    return true;
  }
  return false;
}

bool checkShotHit(Laser laser, Meteor meteor) {
  if (laser.x1 - meteor.x0 < METEOR_SIZE && laser.y1 - meteor.y0 < METEOR_SIZE) {
    return true;
  }
  return false;
}

uint16_t getHighscore() {
  File myFile;
  uint16_t highscore = 0;

  myFile = SD.open("highscore.txt");
  if (myFile) {

    while (myFile.available()) {
      uint16_t readLine = myFile.parseInt();
      if (readLine > highscore) {
        highscore = readLine;
      }
    }
    myFile.close();
  } else {
    Serial.println("error reading file");
  }
  return highscore;
}

uint16_t getScore() {
  return ((currentMillis - gameStartMillis) / 100) * (1.0 + (hits / 10.0));
}

void gameOver() {

  if (!printed) {
    printed = true;
    uint16_t score = getScore();

    tft.setCursor(40, 20);
    tft.setTextColor(BLACK);
    tft.setTextSize(1, 3);

    if (score > currentHighscore) {
      tone(6, 400, 100);
      delay(100);
      tone(6, 800, 200);
      tft.fillRoundRect(30, 10, 100, 60, 5, GREEN);
      tft.print("New Highscore!");
      saveScore(score);
    } else {
      tone(6, 400, 100);
      delay(100);
      tone(6, 100, 200);
      tft.fillRoundRect(30, 10, 100, 60, 5, BLUE);
      tft.print("Your score:");
    }
    tft.setTextSize(2);
    tft.setCursor(60, 50);
    tft.print(score);
    delay(3000);
  }
  if (digitalRead(BTN_FIRE) == HIGH) {
    restartGame();
  }
}

void restartGame() {
  printed = false;
  spawned = 0;
  shots = 0;
  hits = 0;
  crashed = false;
  startgame = false;
  updateMenu = true;
  MeteorMovementSpeed = 100;
  MeteorSpawnRate = 3000;
  for (int i = 0; i < 5; i++) {
    meteors[i].init();
  }
  tft.fillScreen(BLACK);
  currentHighscore = getHighscore();
  fighter.setPosition(32, 35, 32, 15, 42, 25);
}

void resetHighscore() {
  SD.remove("highscore.txt");
  saveScore(0);
}

// From arduino examples
void saveScore(uint16_t score) {
  File myFile;

  myFile = SD.open("highscore.txt", FILE_WRITE);

  if (myFile) {
    myFile.println(score);
    myFile.close();
  } else {
    Serial.println("error writing to file");
  }

}
