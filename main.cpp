
// Features: 2 Levels, Timer, Help Screen, Sound (PlaySound), Perks (3), Highscore
// Compile with MinGW (Code::Blocks)

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")
#endif

#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include <vector>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <chrono>

using namespace std::chrono;

// Window size
const int WINW = 800;
const int WINH = 600;

// Paddle
float paddleW = 120.0f;
const float PADDLE_H = 12.0f;
float paddleX = (WINW - 120.0f) / 2.0f;
const float PADDLE_Y = 40.0f;
const float PADDLE_SPEED = 600.0f;

// Ball
float ballR = 8.0f;
float ballX, ballY;
float ballVelX = 0.0f, ballVelY = -1.0f;
float ballSpeed = 420.0f;
bool ballAttached = true;

// Bricks
struct Brick { float x,y,w,h; bool alive; int colorIndex; int hits; };
std::vector<Brick> bricks;
float BR_GAP = 4.0f;
float BR_LEFT_MARGIN = 20.0f;
float BR_TOP_MARGIN = 120.0f;
float BR_W = 0.0f;
float BR_H = 20.0f;

// Items / perks
enum ItemType { IT_EXTRA_LIFE=0, IT_FAST_BALL=1, IT_WIDE_PADDLE=2 };
struct Item { float x,y,w,h; ItemType type; float speed; bool alive; };
std::vector<Item> items;

// Game state
enum GameState { MENU, HELP, PLAYING, PAUSED, GAMEOVER, LEVEL_COMPLETE, FINAL_WIN };
GameState gameState = MENU;

int lives = 3;
int scoreVal = 0;
int highScore = 0;
int currentLevel = 1;

const char* HIGHSCORE_FILE = "dx_highscore.txt";

// Perk timers
bool wideActive=false; double wideTimer=0.0; const double WIDE_DURATION=8.0;
bool fastActive=false; double fastTimer=0.0; const double FAST_DURATION=8.0;

// Input
bool keyLeft=false, keyRight=false;

// Timing (elapsed time)
steady_clock::time_point playStartTime;
double pausedAccumSec = 0.0;
steady_clock::time_point pausedStart;

// Misc
double lastTimeSec = 0.0;
double speedIncTimer = 0.0;

// Utility
float clampf(float v, float a, float b){ if(v<a) return a; if(v>b) return b; return v; }

void playSoundOnce(const char* fname){
#ifdef _WIN32
    // Non-blocking play
    PlaySoundA(fname, NULL, SND_FILENAME | SND_ASYNC);
#endif
}
void playSoundLoop(const char* fname){
#ifdef _WIN32
    PlaySoundA(fname, NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
#endif
}
void stopSound(){
#ifdef _WIN32
    PlaySoundA(NULL, NULL, 0);
#endif
}

void saveHighScore(){
    if(scoreVal > highScore){
        std::ofstream ofs(HIGHSCORE_FILE);
        if(ofs){ ofs << scoreVal; ofs.close(); highScore = scoreVal; }
    }
}
void loadHighScore(){
    std::ifstream ifs(HIGHSCORE_FILE);
    if(ifs){ ifs >> highScore; ifs.close(); } else highScore = 0;
}

void initBricksForLevel(int level){
    bricks.clear();
    if(level==1){
        int BR_ROWS = 6, BR_COLS = 10;
        BR_W = (WINW - 2*BR_LEFT_MARGIN - (BR_COLS-1)*BR_GAP) / BR_COLS;
        BR_H = 20.0f;
        for(int r=0;r<BR_ROWS;r++){
            for(int c=0;c<BR_COLS;c++){
                Brick b;
                b.w=BR_W; b.h=BR_H;
                b.x = BR_LEFT_MARGIN + c*(BR_W + BR_GAP);
                b.y = WINH - BR_TOP_MARGIN - r*(BR_H + BR_GAP);
                b.alive = true;
                b.colorIndex = r%6;
                b.hits = 1;
                bricks.push_back(b);
            }
        }
    } else if(level==2){
        // Level 2: fewer but stronger bricks and different pattern
        int BR_ROWS = 7, BR_COLS = 9;
        BR_W = (WINW - 2*BR_LEFT_MARGIN - (BR_COLS-1)*BR_GAP) / BR_COLS;
        BR_H = 20.0f;
        for(int r=0;r<BR_ROWS;r++){
            for(int c=0;c<BR_COLS;c++){
                // Create a hole pattern
                if((r==0 && (c%2==0)) || (r==3 && c%3==0)) {
                    // skip some to make pattern
                    continue;
                }
                Brick b;
                b.w=BR_W; b.h=BR_H;
                b.x = BR_LEFT_MARGIN + c*(BR_W + BR_GAP);
                b.y = WINH - BR_TOP_MARGIN - r*(BR_H + BR_GAP);
                b.alive = true;
                b.colorIndex = (r+1)%6;
                // make some bricks require 2 hits
                b.hits = (r%3==0) ? 2 : 1;
                bricks.push_back(b);
            }
        }
    }
}

void attachBallToPaddle(){
    ballAttached = true;
    ballX = paddleX + paddleW/2.0f;
    ballY = PADDLE_Y + PADDLE_H + ballR + 1.0f;
    ballVelX = 0.0f;
    ballVelY = 1.0f;
    ballSpeed = 420.0f;
}

void startLevel(int level){
    currentLevel = level;
    items.clear();
    wideActive=false; wideTimer=0.0; fastActive=false; fastTimer=0.0;
    initBricksForLevel(level);
    attachBallToPaddle();
    lives = 3;
    scoreVal = 0;
    pausedAccumSec = 0.0;
    playStartTime = steady_clock::now();
    lastTimeSec = glutGet(GLUT_ELAPSED_TIME)/1000.0;
    speedIncTimer = 0.0;
    gameState = PLAYING;
    loadHighScore();
    // background music if any (file "bg.wav" in exe folder)
    // playSoundLoop("bg.wav");
}

void nextLevelOrWin(){
    if(currentLevel==1){
        gameState = LEVEL_COMPLETE;
    } else {
        gameState = FINAL_WIN;
        saveHighScore();
        // stopSound(); play win sound
        playSoundOnce("win.wav");
    }
}

// Drawing utilities
void drawRect(float x,float y,float w,float h){
    glBegin(GL_QUADS);
    glVertex2f(x,y);
    glVertex2f(x+w,y);
    glVertex2f(x+w,y+h);
    glVertex2f(x,y+h);
    glEnd();
}
void drawCircle(float cx,float cy,float r,int seg=30){
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(cx,cy);
    for(int i=0;i<=seg;i++){
        float a = (float)i/(float)seg * 2.0f * 3.14159265f;
        glVertex2f(cx + cosf(a)*r, cy + sinf(a)*r);
    }
    glEnd();
}
void drawText(float x,float y,const std::string &s){
    glRasterPos2f(x,y);
    for(char c: s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
}

bool circleRectCollide(float cx,float cy,float r,float rx,float ry,float rw,float rh){
    float closestX = clampf(cx, rx, rx+rw);
    float closestY = clampf(cy, ry, ry+rh);
    float dx = cx-closestX, dy = cy-closestY;
    return (dx*dx + dy*dy) <= (r*r);
}

void maybeSpawnItem(float bx,float by){
    int chance = rand()%100;
    if(chance < 30){
        Item it; it.w=18; it.h=18;
        it.x = bx - it.w/2.0f; it.y = by - it.h/2.0f;
        it.speed = 140.0f + (rand()%80);
        it.type = static_cast<ItemType>(rand()%3);
        it.alive = true;
        items.push_back(it);
    }
}

void updateGame(double dt){
    if(gameState != PLAYING) return;

    // Input move
    if(keyLeft) paddleX -= PADDLE_SPEED * dt;
    if(keyRight) paddleX += PADDLE_SPEED * dt;
    paddleX = clampf(paddleX, 0.0f, WINW - paddleW);

    if(ballAttached){
        ballX = paddleX + paddleW/2.0f;
        ballY = PADDLE_Y + PADDLE_H + ballR + 1.0f;
    } else {
        ballX += ballVelX * ballSpeed * dt;
        ballY += ballVelY * ballSpeed * dt;

        if(ballX - ballR < 0.0f){ ballX = ballR; ballVelX = -ballVelX;  }
        if(ballX + ballR > WINW){ ballX = WINW - ballR; ballVelX = -ballVelX; }
        if(ballY + ballR > WINH){ ballY = WINH - ballR; ballVelY = -ballVelY; }

        // Paddle collision
        if(ballY - ballR <= PADDLE_Y + PADDLE_H && ballY - ballR >= PADDLE_Y - 5.0f){
            if(ballX >= paddleX && ballX <= paddleX + paddleW){
                float rel = (ballX - (paddleX + paddleW/2.0f)) / (paddleW/2.0f);
                float vx = rel; float vy = 1.0f;
                float n = sqrtf(vx*vx + vy*vy);
                ballVelX = vx / n; ballVelY = fabs(vy)/n;

            }
        }

        if(ballY - ballR < 0.0f){
            lives--;
            if(lives <= 0){ gameState = GAMEOVER; saveHighScore(); stopSound(); }
            else attachBallToPaddle();
        }
    }
