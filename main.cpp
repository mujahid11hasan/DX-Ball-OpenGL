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
     // Bricks collision
    for(auto &b : bricks){
        if(!b.alive) continue;
        if(circleRectCollide(ballX, ballY, ballR, b.x, b.y, b.w, b.h)){
            b.hits -= 1;
            if(b.hits <= 0){
                b.alive = false;
                scoreVal += 10;
                maybeSpawnItem(b.x + b.w/2.0f, b.y + b.h/2.0f);
            } else {
                // partial hit score
                scoreVal += 5;
            }
            ballVelY = -ballVelY;
            playSoundOnce("hit.wav");
            break;
        }
    }

    // Items update
    for(auto &it: items){
        if(!it.alive) continue;
        it.y -= it.speed * dt;
        if(it.y <= PADDLE_Y + PADDLE_H && it.y >= PADDLE_Y - it.h){
            if(it.x + it.w >= paddleX && it.x <= paddleX + paddleW){
                if(it.type == IT_EXTRA_LIFE){ lives++; }
                else if(it.type == IT_FAST_BALL){
                    if(!fastActive){ fastActive=true; fastTimer=0.0; ballSpeed *= 1.45f; }
                    else fastTimer=0.0;
                } else if(it.type == IT_WIDE_PADDLE){
                    if(!wideActive){ wideActive=true; wideTimer=0.0; paddleW *= 1.5f; paddleX = clampf(paddleX,0.0f,WINW-paddleW); }
                    else wideTimer = 0.0;
                }
                it.alive = false;
            }
        }
        if(it.y + it.h < 0.0f) it.alive = false;
    }
    items.erase(std::remove_if(items.begin(), items.end(), [](const Item &i){ return !i.alive; }), items.end());

    // Powerup timers
    if(wideActive){ wideTimer += dt; if(wideTimer >= WIDE_DURATION){ wideActive=false; paddleW /= 1.5f; paddleX = clampf(paddleX,0.0f,WINW-paddleW); } }
    if(fastActive){ fastTimer += dt; if(fastTimer >= FAST_DURATION){ fastActive=false; ballSpeed /= 1.45f; } }

    // progressive speed
    speedIncTimer += dt;
    if(speedIncTimer >= 12.0){ speedIncTimer = 0.0; ballSpeed *= 1.06f; }

    // check win
    bool anyAlive=false;
    for(auto &b: bricks) if(b.alive){ anyAlive = true; break; }
    if(!anyAlive) nextLevelOrWin();
}

void display(){
    glClearColor(0.06f,0.06f,0.06f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if(gameState == MENU){
        glColor3f(0.95f,0.88f,0.1f);
        drawText(WINW/2 - 120, WINH/2 + 80, "DX-BALL (Final)");
        glColor3f(1,1,1);
        drawText(WINW/2 - 220, WINH/2 + 40, "Controls: Mouse or A/D or Left/Right to move paddle");
        drawText(WINW/2 - 220, WINH/2 + 20, "Space - Launch ball | P - Pause | H - Help | Esc - Quit");
        drawText(WINW/2 - 140, WINH/2 - 10, "Press ENTER to Start (Level 1)");
        std::ostringstream hs; hs << "High Score: " << highScore; drawText(20, WINH - 30, hs.str());
    } else if(gameState == HELP){
        glColor3f(1,1,1);
        drawText(60, WINH - 60, "HELP / INSTRUCTIONS:");
        drawText(60, WINH - 100, "- Use Mouse or Arrow Keys (A/D) to move paddle");
        drawText(60, WINH - 130, "- Space to launch the ball when attached");
        drawText(60, WINH - 160, "- Collect falling items for perks:");
        drawText(80, WINH - 190, "* Extra Life (purple) | Faster Ball (cyan) | Wider Paddle (orange)");
        drawText(60, WINH - 220, "- Press P to pause, Esc to quit.");
        drawText(60, WINH - 270, "- Win level by clearing all bricks. Level 1 -> Level 2 -> Final Win");
        drawText(60, WINH - 320, "Press ENTER to start, or M to return to menu.");
    } else {
        // draw bricks
        for(auto &b: bricks){
            if(!b.alive) continue;
            switch(b.colorIndex % 6){
                case 0: glColor3f(0.9f,0.2f,0.2f); break;
                case 1: glColor3f(0.2f,0.9f,0.2f); break;
                case 2: glColor3f(0.2f,0.4f,0.9f); break;
                case 3: glColor3f(0.9f,0.6f,0.2f); break;
                case 4: glColor3f(0.7f,0.2f,0.9f); break;
                default: glColor3f(0.5f,0.8f,0.9f); break;
            }
            drawRect(b.x, b.y, b.w, b.h);
            glColor3f(0,0,0);
            glBegin(GL_LINE_LOOP);
            glVertex2f(b.x,b.y); glVertex2f(b.x+b.w,b.y); glVertex2f(b.x+b.w,b.y+b.h); glVertex2f(b.x,b.y+b.h);
            glEnd();
        }

        // paddle
        glColor3f(0.95f,0.95f,0.95f);
        drawRect(paddleX, PADDLE_Y, paddleW, PADDLE_H);

        // ball
        glColor3f(1.0f,0.9f,0.2f);
        drawCircle(ballX, ballY, ballR, 24);

        // items
        for(auto &it: items){
            if(!it.alive) continue;
            switch(it.type){
                case IT_EXTRA_LIFE: glColor3f(0.8f,0.1f,0.8f); break;
                case IT_FAST_BALL:  glColor3f(0.2f,0.8f,1.0f); break;
                case IT_WIDE_PADDLE:glColor3f(0.9f,0.5f,0.1f); break;
            }
            drawRect(it.x, it.y, it.w, it.h);
        }

        // HUD
        glColor3f(1,1,1);
        std::ostringstream s1; s1 << "Score: " << scoreVal; drawText(12, WINH - 24, s1.str());
        std::ostringstream s2; s2 << "Lives: " << lives; drawText(120, WINH - 24, s2.str());
        std::ostringstream s3; s3 << "High: " << highScore; drawText(220, WINH - 24, s3.str());
        // elapsed time
        double elapsed = 0.0;
        if(gameState == PLAYING){
            auto now = steady_clock::now();
            elapsed = duration_cast<duration<double>>(now - playStartTime).count() - pausedAccumSec;
        } else {
            auto now = steady_clock::now();
            elapsed = duration_cast<duration<double>>(now - playStartTime).count() - pausedAccumSec;
        }
        int es = static_cast<int>(elapsed);
        int mm = es / 60; int ss = es % 60;
        std::ostringstream st; st << "Time: " << mm << ":" << (ss<10?"0":"") << ss; drawText(320, WINH - 24, st.str());

        if(gameState == PAUSED){
            glColor3f(1.0f, 1.0f, 0.8f);
            drawText(WINW/2 - 50, WINH/2, "PAUSED - Press P to Resume");
        }
        if(gameState == GAMEOVER){
            glColor3f(1.0f,0.4f,0.4f);
            drawText(WINW/2 - 90, WINH/2 + 10, "GAME OVER");
            std::ostringstream so; so << "Final Score: " << scoreVal; drawText(WINW/2 - 90, WINH/2 - 10, so.str());
            drawText(WINW/2 - 140, WINH/2 - 40, "Press ENTER to Restart or ESC to Exit");
        }
        if(gameState == LEVEL_COMPLETE){
            glColor3f(0.6f,1.0f,0.6f);
            drawText(WINW/2 - 120, WINH/2 + 20, "LEVEL 1 CLEAR!");
            drawText(WINW/2 - 160, WINH/2 - 0, "Press ENTER to proceed to LEVEL 2");
        }
        if(gameState == FINAL_WIN){
            glColor3f(0.6f,1.0f,0.6f);
            drawText(WINW/2 - 80, WINH/2 + 10, "YOU WIN! CONGRATS");
            std::ostringstream so; so << "Final Score: " << scoreVal; drawText(WINW/2 - 80, WINH/2 - 10, so.str());
            drawText(WINW/2 - 120, WINH/2 - 40, "Press ENTER to Restart or ESC to Exit");
        }
    }

    glutSwapBuffers();
}

void timerFunc(int val){
    double now = glutGet(GLUT_ELAPSED_TIME)/1000.0;
    double dt = now - lastTimeSec;
    if(dt < 0.0) dt = 0.0;
    lastTimeSec = now;

    if(gameState == PLAYING) updateGame(dt);

    glutPostRedisplay();
    glutTimerFunc(16, timerFunc, 0);
}

void keyDown(unsigned char key, int x, int y){
    if(key == 27){ // ESC
        saveHighScore();
        stopSound();
        exit(0);
    }
    if(gameState == MENU){
        if(key == 13 || key == '\n'){ startLevel(1); } // Enter -> start level 1
        if(key == 'h' || key == 'H'){ gameState = HELP; }
        return;
    }
    if(gameState == HELP){
        if(key == 'm' || key == 'M'){ gameState = MENU; }
        if(key == 13 || key == '\n'){ startLevel(1); }
        return;
    }
    if(gameState == GAMEOVER || gameState == FINAL_WIN){
        if(key == 13 || key == '\n'){ startLevel(1); }
        return;
    }
    if(gameState == LEVEL_COMPLETE){
        if(key == 13 || key == '\n'){ startLevel(2); }
        return;
    }

    if(key == 'p' || key == 'P'){
        if(gameState == PLAYING){
            gameState = PAUSED;
            pausedStart = steady_clock::now();
            // pause sound if needed
            // PlaySound(NULL, NULL, 0);
        } else if(gameState == PAUSED){
            gameState = PLAYING;
            auto now = steady_clock::now();
            pausedAccumSec += duration_cast<duration<double>>(now - pausedStart).count();
            // resume sound if needed
        }
    }

    if(key == ' '){
        if(ballAttached && gameState == PLAYING){
            ballAttached = false;
            float angle = (float)((rand()%80 - 40) * (3.14159265/180.0));
            ballVelX = sinf(angle); ballVelY = cosf(angle);
            float n = sqrtf(ballVelX*ballVelX + ballVelY*ballVelY);
            if(n!=0.0f){ ballVelX /= n; ballVelY /= n; }
            // start background sound if exists
            // playSoundLoop("bg.wav");
        }
    }
    if(key == 'a' || key == 'A') keyLeft = true;
    if(key == 'd' || key == 'D') keyRight = true;
}

void keyUp(unsigned char key, int x, int y){
    if(key == 'a' || key == 'A') keyLeft = false;
    if(key == 'd' || key == 'D') keyRight = false;
}

void specialKeyDown(int key, int x, int y){
    if(key == GLUT_KEY_LEFT) keyLeft = true;
    if(key == GLUT_KEY_RIGHT) keyRight = true;
}
void specialKeyUp(int key, int x, int y){
    if(key == GLUT_KEY_LEFT) keyLeft = false;
    if(key == GLUT_KEY_RIGHT) keyRight = false;
}

void passiveMotion(int x, int y){
    static float prevX = x;
    float targetX = (float)x - paddleW / 2.0f;

    // difference between current and target position
    float dx = targetX - paddleX;

    // apply smooth movement speed limit (same speed as keyboard)
    float maxMove = PADDLE_SPEED * (1.0f / 60.0f);  // assuming 60fps
    if (dx > maxMove) dx = maxMove;
    else if (dx < -maxMove) dx = -maxMove;

    paddleX += dx;
    paddleX = clampf(paddleX, 0.0f, WINW - paddleW);
    prevX = x;
}


int main(int argc, char** argv){
    srand((unsigned int)time(nullptr));
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WINW, WINH);
    glutCreateWindow("DX-Ball (Final)");

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINW, 0, WINH, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    loadHighScore();
    initBricksForLevel(1);
    attachBallToPaddle();
    lastTimeSec = glutGet(GLUT_ELAPSED_TIME)/1000.0;

    glutDisplayFunc(display);
    glutTimerFunc(16, timerFunc, 0);
    glutKeyboardFunc(keyDown);
    glutKeyboardUpFunc(keyUp);
    glutSpecialFunc(specialKeyDown);
    glutSpecialUpFunc(specialKeyUp);
    glutPassiveMotionFunc(passiveMotion);

    glutMainLoop();
    return 0;
}
