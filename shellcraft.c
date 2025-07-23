// ヘッダーのインクルード
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// kbhit.hのインクルード
#include "kbhit.h"

// 画面の設定
#define SCREEN_X 90
#define SCREEN_Y 30
#define FPS 30

// 世界の広さ
#define WORLD_X 64
#define WORLD_Y 32
#define WORLD_Z 64

// グローバル変数
char display[SCREEN_X][SCREEN_Y];
int display_color[SCREEN_X][SCREEN_Y];
char display_output[SCREEN_X * SCREEN_Y * 16];
float z_buffer[SCREEN_X][SCREEN_Y];
int world[WORLD_X][WORLD_Y][WORLD_Z];

char colorlist[9][16] = {"",
                         "\x1b[49m\x1b[37m",
                         "\x1b[49m\x1b[31m",
                         "\x1b[49m\x1b[32m",
                         "\x1b[49m\x1b[33m",
                         "\x1b[49m\x1b[34m",
                         "\x1b[49m\x1b[35m",
                         "\x1b[49m\x1b[36m",
                         "\x1b[46m"};
// 1白 2赤 3緑 4黄 5青 6紫 7空 8背景

char blocktexture[10][3] = {
    {' ', ' ', ' '},   // 0 air
    {'=', '#', '|'},   // 1stone
    {'w', '#', '|'},   // 2grass1
    {'W', '|', '#'},   // 3grass2
    {'=', '!', '|'},   // 4wood
    {'"', '.', '\''},  // 5leaves
    {'=', '#', '|'},   // 6赤
    {'~', '#', '|'},   // 7青
    {'=', '#', '|'},   // 8紫
    {'=', '#', '|'}    // 9空色
};
int blockcolor[10][3] = {
    // 上面　側面　下面
    {0, 0, 0},  // 0 air
    {1, 1, 1},  // 1stone
    {3, 4, 4},  // 2grass1
    {3, 4, 4},  // 3grass2
    {4, 4, 4},  // 4wood
    {3, 3, 3},  // 5leaves
    {2, 2, 2},  // 6赤
    {5, 5, 5},  // 7青
    {6, 6, 6},  // 8紫
    {7, 7, 7}   // 9空色
};

// 二次元座標とポリゴンの深度
typedef struct {
  int x, y;
  float depth;
} Vec2d;

// プレイヤーの情報&初期化
struct Player {
  float x, y, z;
  float theta;
  float phi;
} player = {WORLD_X / 2.0, WORLD_Y / 2.0, WORLD_Z / 2.0, 0.0, 0.0};

// プレイヤーの視点のsin cosを事前に計算しておくためのやつなぜここにいるのか...
float cosT;
float sinT;
float cosP;
float sinP;

// 乱数
int r(int min, int max) { return min + rand() % (max - min + 1); }

// 三次元の座標をプレイヤーの情報をもとに二次元に変換する。
Vec2d projection(int x, int y, int z) {
  // https://www.desmos.com/calculator/zgqofiyvop

  // 相対的な位置を算出
  float dx = x - player.x;
  float dy = y - player.y;
  float dz = z - player.z;

  // カメラの座標系に変換1
  float cam_z = -sinT * cosP * dx - sinP * dy + cosT * cosP * dz;

  Vec2d vec2d;

  // カメラの後ろは描画しないため判定しておく
  if (cam_z < 0.05) {
    vec2d.depth = -1.0;
    return vec2d;
  }

  // カメラの座標系に変換2
  float cam_x = cosT * dx + sinT * dz;
  float cam_y = -sinT * sinP * dx + cosP * dy + cosT * sinP * dz;

  //
  vec2d.depth = cam_z;
  vec2d.x = cam_x * SCREEN_Y / cam_z + SCREEN_X * 0.5;
  vec2d.y = -(cam_y * SCREEN_Y * 0.5 / cam_z) + SCREEN_Y * 0.5;
  return vec2d;
}

// コンソール上に座標を指定してポリゴンを描く関数
void polygon(Vec2d p1, Vec2d p2, Vec2d p3, char c, int color) {
  int x, y;
  int max_x =
      p1.x > p2.x ? (p1.x > p3.x ? p1.x : p3.x) : (p2.x > p3.x ? p2.x : p3.x);
  int min_x =
      p1.x < p2.x ? (p1.x < p3.x ? p1.x : p3.x) : (p2.x < p3.x ? p2.x : p3.x);
  int max_y =
      p1.y > p2.y ? (p1.y > p3.y ? p1.y : p3.y) : (p2.y > p3.y ? p2.y : p3.y);
  int min_y =
      p1.y < p2.y ? (p1.y < p3.y ? p1.y : p3.y) : (p2.y < p3.y ? p2.y : p3.y);

  float current_depth = (p1.depth + p2.depth + p3.depth) / 3.0;

  if (min_x < 0) min_x = 0;
  if (max_x >= SCREEN_X) max_x = SCREEN_X - 1;
  if (min_y < 0) min_y = 0;
  if (max_y >= SCREEN_Y) max_y = SCREEN_Y - 1;

  for (y = min_y; y <= max_y; y++) {
    for (x = min_x; x <= max_x; x++) {
      if ((y - p1.y) * (p1.x - p2.x) >= (x - p1.x) * (p1.y - p2.y) &&
          (y - p2.y) * (p2.x - p3.x) >= (x - p2.x) * (p2.y - p3.y) &&
          (y - p3.y) * (p3.x - p1.x) >= (x - p3.x) * (p3.y - p1.y)) {
        if (current_depth < z_buffer[x][y]) {
          z_buffer[x][y] = current_depth;
          display[x][y] = c;
          display_color[x][y] = color;
        }
      }
    }
  }
}

// ポリゴンを二つつなげて平面を描く
void plane(Vec2d p1, Vec2d p2, Vec2d p3, Vec2d p4, char c, int color) {
  if (p1.depth < 0.05 || p2.depth < 0.05 || p3.depth < 0.05 || p4.depth < 0.05)
    return;
  polygon(p1, p2, p3, c, color);
  polygon(p3, p2, p4, c, color);
}

void cube(int x, int y, int z, int blocktype) {
  Vec2d cv[8] = {projection(x, y, z),         projection(x, y, z + 1),
                 projection(x, y + 1, z),     projection(x, y + 1, z + 1),
                 projection(x + 1, y, z),     projection(x + 1, y, z + 1),
                 projection(x + 1, y + 1, z), projection(x + 1, y + 1, z + 1)};
  // xy平面
  if (z - 1 < 0 || world[x][y][z - 1] == 0) {
    plane(cv[2], cv[0], cv[6], cv[4], blocktexture[blocktype][1],
          blockcolor[blocktype][1]);
  }
  if (z + 1 >= WORLD_Z || world[x][y][z + 1] == 0) {
    plane(cv[3], cv[7], cv[1], cv[5], blocktexture[blocktype][1],
          blockcolor[blocktype][1]);
  }
  // yz平面
  if (x - 1 < 0 || world[x - 1][y][z] == 0) {
    plane(cv[3], cv[1], cv[2], cv[0], blocktexture[blocktype][2],
          blockcolor[blocktype][1]);
  }
  if (x + 1 >= WORLD_X || world[x + 1][y][z] == 0) {
    plane(cv[7], cv[6], cv[5], cv[4], blocktexture[blocktype][2],
          blockcolor[blocktype][1]);
  }
  // zx平面
  if (y - 1 < 0 || world[x][y - 1][z] == 0) {
    plane(cv[0], cv[1], cv[4], cv[5], blocktexture[blocktype][0],
          blockcolor[blocktype][2]);
  }
  if (y + 1 >= WORLD_Y || world[x][y + 1][z] == 0) {
    plane(cv[2], cv[6], cv[3], cv[7], blocktexture[blocktype][0],
          blockcolor[blocktype][0]);
  }
}

void swap(int* a, int* b) {
  int tmp;
  tmp = *a;
  *a = *b;
  *b = tmp;
}

void dotlineByBresenham(int x1, int y1, int x2, int y2) {
  int dx, dy, ystep, x, y, err = 0, flag;
  if (flag = (abs(y2 - y1) > abs(x2 - x1))) {
    swap(&x1, &y1);
    swap(&x2, &y2);
  }
  if (x1 > x2) {
    swap(&x1, &x2);
    swap(&y1, &y2);
  }
  dx = x2 - x1, dy = abs(y2 - y1), y = y1;
  ystep = (y1 < y2) ? 1 : -1;
  for (x = x1; x <= x2; x++) {
    if (flag) {
      if (y >= 0 && y < SCREEN_X && x >= 0 && x < SCREEN_Y) {
        display[y][x] = '+';
        display_color[y][x] = 1;
      }
    } else {
      if (x >= 0 && x < SCREEN_X && y >= 0 && y < SCREEN_Y) {
        display[x][y] = '+';
        display_color[x][y] = 1;
      }
    }
    err -= dy;
    if (err < 0) {
      y += ystep;
      err = err + dx;
    }
  }
}

void target(int x, int y, int z) {
  Vec2d cv[8] = {projection(x, y, z),         projection(x, y, z + 1),
                 projection(x, y + 1, z),     projection(x, y + 1, z + 1),
                 projection(x + 1, y, z),     projection(x + 1, y, z + 1),
                 projection(x + 1, y + 1, z), projection(x + 1, y + 1, z + 1)};
  dotlineByBresenham(cv[0].x, cv[0].y, cv[1].x, cv[1].y);
  dotlineByBresenham(cv[0].x, cv[0].y, cv[2].x, cv[2].y);
  dotlineByBresenham(cv[0].x, cv[0].y, cv[4].x, cv[4].y);
  dotlineByBresenham(cv[1].x, cv[1].y, cv[3].x, cv[3].y);
  dotlineByBresenham(cv[1].x, cv[1].y, cv[5].x, cv[5].y);
  dotlineByBresenham(cv[2].x, cv[2].y, cv[3].x, cv[3].y);
  dotlineByBresenham(cv[2].x, cv[2].y, cv[6].x, cv[6].y);
  dotlineByBresenham(cv[3].x, cv[3].y, cv[7].x, cv[7].y);
  dotlineByBresenham(cv[4].x, cv[4].y, cv[5].x, cv[5].y);
  dotlineByBresenham(cv[4].x, cv[4].y, cv[6].x, cv[6].y);
  dotlineByBresenham(cv[5].x, cv[5].y, cv[7].x, cv[7].y);
  dotlineByBresenham(cv[6].x, cv[6].y, cv[7].x, cv[7].y);
}

void worldgenerate() {
  int x, y, z, surface;
  for (x = 0; x < WORLD_X; x++) {
    for (z = 0; z < WORLD_Z; z++) {
      surface = (int)((int)(10 * 3 * sin(x * 0.1) * cos(z * 0.1)) / 10) + 5;
      for (y = 0; y < surface; y++) {
        world[x][y][z] = 1;
      }

      world[x][y][z] = r(0, 1) == 0 ? 2 : 3;
      if (y == 3) {
        world[x][y][z] = 7;
      }

      if (r(0, 200) == 0 && world[x][y][z] != 7 && 1 <= x && x <= WORLD_X - 2 &&
          1 <= z && z <= WORLD_Z - 2) {
        for (y = surface + 1; y <= surface + 4; y++) {
          world[x][y][z] = 4;
        }
        world[x][surface + 5][z] = 5;
        world[x + 1][surface + 4][z] = 5;
        world[x - 1][surface + 4][z] = 5;
        world[x][surface + 4][z + 1] = 5;
        world[x][surface + 4][z - 1] = 5;
      }
    }
  }
}

void save_world() {
  int x, y, z;
  FILE* fp = fopen("world.txt", "w");

  for (x = 0; x < WORLD_X; x++) {
    for (y = 0; y < WORLD_Y; y++) {
      for (z = 0; z < WORLD_Z; z++) {
        fprintf(fp, "%d ", world[x][y][z]);
      }
      fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
  }
  fclose(fp);
}

int load_world() {
  int x, y, z;
  FILE* fp = fopen("world.txt", "r");
  if (fp == NULL) {
    return 0;
  }

  for (x = 0; x < WORLD_X; x++) {
    for (y = 0; y < WORLD_Y; y++) {
      for (z = 0; z < WORLD_Z; z++) {
        fscanf(fp, "%d", &world[x][y][z]);
      }
    }
  }

  fclose(fp);
  return 1;
}

int main() {
  // 乱数初期化
  srand((unsigned int)time(NULL));

  // 変数定義
  int x, y, z;
  int exit = 1;
  clock_t start_time;
  double frame_us = 1000000.0 / FPS;
  int wait_us;
  unsigned int frame_count = 0;
  int skipframe = 0;

  char c;
  char* p;
  int current_color;
  char* color_code;
  int block_x = 0, block_y = 0, block_z = 0;

  int render_distance = 32;
  int min_x;
  int max_x;
  int min_y;
  int max_y;
  int min_z;
  int max_z;

  if (!load_world()) {
    worldgenerate();
  }

  // 画面初期化
  printf("\x1b[2J");

  cosT = cos(player.theta);
  sinT = sin(player.theta);
  cosP = cos(player.phi);
  sinP = sin(player.phi);

  while (exit) {
    // ループ内の時間初期化
    start_time = clock();

    // フレームスキップのフラグを初期化
    skipframe = 0;

    for (y = 0; y < SCREEN_Y; y++) {
      for (x = 0; x < SCREEN_X; x++) {
        display[x][y] = ' ';
        display_color[x][y] = 8;
        z_buffer[x][y] = 9999;  // 非常に大きな値（無限遠）
      }
    }

    // プレイヤーの位置更新
    if (kbhit()) {
      c = getchar();
      switch (c) {
        // WASDで移動
        case 'd':
          player.x += cosT * 0.1;
          player.z += sinT * 0.1;
          break;
        case 'a':
          player.x -= cosT * 0.1;
          player.z -= sinT * 0.1;
          break;
        case 'w':
          player.x -= sinT * 0.1;
          player.z += cosT * 0.1;
          break;
        case 's':
          player.x += sinT * 0.1;
          player.z -= cosT * 0.1;
          break;
        // スペースで上昇
        case ' ':
          player.y += 0.1;
          break;
        // Vで下降
        case 'v':
          player.y -= 0.1;
          break;
        // IJKLで視点移動
        case 'i':
          player.phi -= 0.1;
          break;
        case 'k':
          player.phi += 0.1;
          break;
        case 'j':
          player.theta += 0.1;
          break;
        case 'l':
          player.theta -= 0.1;
          break;
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case '0':
          // 境界チェック
          if (block_x >= 0 && block_x < WORLD_X && block_y >= 0 &&
              block_y < WORLD_Y && block_z >= 0 && block_z < WORLD_Z) {
            world[block_x][block_y][block_z] = c - '0';
          }
          break;
        case 'x':
          save_world();
          break;
        // Qでゲームをやめる
        case 'q':
          exit = 0;
          break;
      }
    } else {
      if (frame_count != 1) {
        skipframe = 1;
      }
    }

    if (skipframe == 0) {
      cosT = cos(player.theta);
      sinT = sin(player.theta);
      cosP = cos(player.phi);
      sinP = sin(player.phi);

      block_x = (int)(player.x - 3 * sinT * cosP);
      block_y = (int)(player.y - 3 * sinP);
      block_z = (int)(player.z + 3 * cosT * cosP);

      // for (x = 0; x < WORLD_X; x++) {
      //   for (y = 0; y < WORLD_Y; y++) {
      //     for (z = 0; z < WORLD_Z; z++) {
      //       if (world[x][y][z] != 0) {
      //         cube(x, y, z, world[x][y][z]);
      //       }
      //     }
      //   }
      // }
      min_x = (int)player.x - render_distance;
      max_x = (int)player.x + render_distance;
      min_y = (int)player.y - render_distance;
      max_y = (int)player.y + render_distance;
      min_z = (int)player.z - render_distance;
      max_z = (int)player.z + render_distance;

      // ワールドの境界を超えないようにクランプする
      if (min_x < 0) min_x = 0;
      if (max_x >= WORLD_X) max_x = WORLD_X - 1;
      if (min_y < 0) min_y = 0;
      if (max_y >= WORLD_Y) max_y = WORLD_Y - 1;
      if (min_z < 0) min_z = 0;
      if (max_z >= WORLD_Z) max_z = WORLD_Z - 1;

      // 限定された範囲のブロックのみを描画
      for (x = min_x; x <= max_x; x++) {
        for (y = min_y; y <= max_y; y++) {
          for (z = min_z; z <= max_z; z++) {
            if (world[x][y][z] != 0) {
              cube(x, y, z, world[x][y][z]);
            }
          }
        }
      }

      target(block_x, block_y, block_z);

      p = display_output;
      current_color = -1;
      for (y = 0; y < SCREEN_Y; y++) {
        for (x = 0; x < SCREEN_X; x++) {
          if (display_color[x][y] != current_color) {
            current_color = display_color[x][y];
            color_code = colorlist[current_color];
            while (*color_code != '\0') {
              *p++ = *color_code++;
            }
          }
          *p++ = display[x][y];
        }
        *p++ = '\n';
      }
      *p = '\0';

      printf(
          "\x1b[49m\x1b[39m\x1b[1;1HMove:WASD Look:IJKL Up:Space Down:V Save:X "
          "0:Destroy 1:Stone 2:Grass1 3:Grass2 4:Wood 5:Leaf 6:Red 7:Blue "
          "8:Purple 9:Light Blue\n%s",
          display_output);
      fflush(stdout);
    }

    // FPS処理
    wait_us = frame_us -
              ((double)(clock() - start_time) / CLOCKS_PER_SEC) * 1000000.0;
    if (wait_us > 0) {
      usleep(wait_us);
    }
    frame_count++;
  }
  printf("\x1b[0m\x1b[39m");
  return 0;
}
