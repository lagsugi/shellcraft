
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

// --- マルチプレイ用ヘッダーの追加 ---
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <netdb.h>

// 画面の設定
#define SCREEN_X 180
#define SCREEN_Y 55
#define FPS 30

// 世界の広さ
#define WORLD_X 64
#define WORLD_Y 32
#define WORLD_Z 64

// --- マルチプレイ用設定 ---
#define SERVER_IP "localhost"
#define SERVER_PORT 8080

int kbhit(void);

// --- 構造体の定義 ---
typedef struct {
    int x, y, z, type;
} BlockUpdate;

typedef struct {
  int x, y;
  float depth;
} Vec2d;

// グローバル変数
char display[SCREEN_X][SCREEN_Y];
int display_color[SCREEN_X][SCREEN_Y];
char display_output[SCREEN_X * SCREEN_Y * 8];

int world[WORLD_X][WORLD_Y][WORLD_Z];
float z_buffer[SCREEN_X][SCREEN_Y];

char colorlist[9][16] = {"",
                         "\x1b[49m\x1b[37m",
                         "\x1b[49m\x1b[31m",
                         "\x1b[49m\x1b[32m",
                         "\x1b[49m\x1b[33m",
                         "\x1b[49m\x1b[34m",
                         "\x1b[49m\x1b[35m",
                         "\x1b[49m\x1b[36m",
                         "\x1b[46m"};

char blocktexture[10][3] = {
    {' ', ' ', ' '},
    {'=', '#', '|'},
    {'w', '#', '|'},
    {'W', '|', '#'},
    {'=', '!', '|'},
    {'"', '.', '\''},
    {'=', '#', '|'},
    {'~', '#', '|'},
    {'=', '#', '|'},
    {'=', '#', '|'}
};
int blockcolor[10][3] = {
    {0, 0, 0},
    {1, 1, 1},
    {3, 4, 4},
    {3, 4, 4},
    {4, 4, 4},
    {3, 3, 3},
    {2, 2, 2},
    {5, 5, 5},
    {6, 6, 6},
    {7, 7, 7}
};

// プレイヤーの情報
struct Player {
  float x, y, z;
  float theta;
  float phi;
} player = {WORLD_X / 2.0, WORLD_Y / 2.0, WORLD_Z / 2.0, 0.0, 0.0};

float cosT;
float sinT;
float cosP;
float sinP;

int multiplayer_mode = 0;
int server_socket;


// --- 新しいヘルパー関数: recv_all ---
int recv_all(int socket, void *buffer, size_t length) {
    char *ptr = (char *)buffer;
    while (length > 0) {
        int i = recv(socket, ptr, length, 0);
        if (i < 1) {
            return -1;
        }
        ptr += i;
        length -= i;
    }
    return 0;
}


int r(int min, int max) { return min + rand() % (max - min + 1); }

Vec2d projection(int x, int y, int z) {
  float dx = x - player.x;
  float dy = y - player.y;
  float dz = z - player.z;

  float cam_z = -sinT * cosP * dx - sinP * dy + cosT * cosP * dz;

  Vec2d vec2d;
  if (cam_z < 0.05) {
    vec2d.depth = -1.0;
    return vec2d;
  }

  float cam_x = cosT * dx + sinT * dz;
  float cam_y = -sinT * sinP * dx + cosP * dy + cosT * sinP * dz;

  vec2d.depth = cam_z;
  vec2d.x = cam_x * SCREEN_Y / cam_z + SCREEN_X * 0.5;
  vec2d.y = -(cam_y * SCREEN_Y * 0.5 / cam_z) + SCREEN_Y * 0.5;
  return vec2d;
}

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

void plane(Vec2d p1, Vec2d p2, Vec2d p3, Vec2d p4, char c, int color) {
  if (p1.depth < 0.05 || p2.depth < 0.05 || p3.depth < 0.05 || p4.depth < 0.05)
    return;
  polygon(p1, p2, p3, c, color);
  polygon(p3, p2, p4, c, color);
}

void cube(int x, int y, int z, int blocktype) {
  Vec2d cv[8] = {projection(x, y, z),       projection(x, y, z + 1),
                 projection(x, y + 1, z),     projection(x, y + 1, z + 1),
                 projection(x + 1, y, z),     projection(x + 1, y, z + 1),
                 projection(x + 1, y + 1, z), projection(x + 1, y + 1, z + 1)};
  if (z - 1 < 0 || world[x][y][z - 1] == 0) {
    plane(cv[2], cv[0], cv[6], cv[4], blocktexture[blocktype][1],
          blockcolor[blocktype][1]);
  }
  if (z + 1 >= WORLD_Z || world[x][y][z + 1] == 0) {
    plane(cv[3], cv[7], cv[1], cv[5], blocktexture[blocktype][1],
          blockcolor[blocktype][1]);
  }
  if (x - 1 < 0 || world[x - 1][y][z] == 0) {
    plane(cv[3], cv[1], cv[2], cv[0], blocktexture[blocktype][2],
          blockcolor[blocktype][1]);
  }
  if (x + 1 >= WORLD_X || world[x + 1][y][z] == 0) {
    plane(cv[7], cv[6], cv[5], cv[4], blocktexture[blocktype][2],
          blockcolor[blocktype][1]);
  }
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
  Vec2d cv[8] = {projection(x, y, z),       projection(x, y, z + 1),
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
  if (!fp) return;
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

void send_block_update(int x, int y, int z, int type) {
    if (!multiplayer_mode) return;
    BlockUpdate update = {x, y, z, type};
    if (send(server_socket, &update, sizeof(update), 0) < 0) {
        perror("Send failed");
    }
}

void check_server_updates() {
    if (!multiplayer_mode) return;
    BlockUpdate update;
    int bytes_received = recv(server_socket, &update, sizeof(update), MSG_DONTWAIT);
    if (bytes_received > 0) {
        if (update.x >= 0 && update.x < WORLD_X &&
            update.y >= 0 && update.y < WORLD_Y &&
            update.z >= 0 && update.z < WORLD_Z) {
            world[update.x][update.y][update.z] = update.type;
        }
    }
}


int main(int argc, char *argv[]) {
  srand((unsigned int)time(NULL));

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

  if (argc > 1 && strcmp(argv[1], "-m") == 0) {
      multiplayer_mode = 1;
      printf("Multiplayer mode enabled. Connecting to server...\n");

      struct sockaddr_in serv_addr;
      struct hostent *server;

      if ((server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
          printf("\n Socket creation error \n");
          return -1;
      }

      server = gethostbyname(SERVER_IP);
      if (server == NULL) {
          fprintf(stderr, "ERROR, no such host: %s\n", SERVER_IP);
          return -1;
      }

      serv_addr.sin_family = AF_INET;
      serv_addr.sin_port = htons(SERVER_PORT);
      memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);


      if (connect(server_socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
          printf("\nConnection Failed \n");
          return -1;
      }

      printf("Connected to server. Receiving world data...\n");
      if (recv_all(server_socket, world, sizeof(world)) < 0) {
          fprintf(stderr, "Failed to receive complete world data.\n");
          close(server_socket);
          return -1;
      }
      printf("World data received.\n");

  } else {
      printf("Singleplayer mode enabled.\n");
      if (!load_world()) {
        worldgenerate();
      }
  }

  printf("\x1b[2J");

  cosT = cos(player.theta);
  sinT = sin(player.theta);
  cosP = cos(player.phi);
  sinP = sin(player.phi);

  while (exit) {

    start_time = clock();

    check_server_updates();

    skipframe = 0;

    for (y = 0; y < SCREEN_Y; y++) {
      for (x = 0; x < SCREEN_X; x++) {
        display[x][y] = ' ';
        display_color[x][y] = 8;
        z_buffer[x][y] = 9999;
      }
    }

    if (kbhit()) {
      c = getchar();
      switch (c) {
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
        case ' ':
          player.y += 0.1;
          break;
        case 'v':
          player.y -= 0.1;
          break;
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
          if (block_x >= 0 && block_x < WORLD_X && block_y >= 0 &&
              block_y < WORLD_Y && block_z >= 0 && block_z < WORLD_Z) {
            world[block_x][block_y][block_z] = c - '0';
            send_block_update(block_x, block_y, block_z, c - '0');
          }
          break;
        case 'x':
          save_world();
          break;
        case 'q':
          exit = 0;
          break;
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

      for (x = 0; x < WORLD_X; x++) {
        for (y = 0; y < WORLD_Y; y++) {
          for (z = 0; z < WORLD_Z; z++) {
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

    wait_us = frame_us -
              ((double)(clock() - start_time) / CLOCKS_PER_SEC) * 1000000.0;
    if (wait_us > 0) {
      usleep(wait_us);
    }
    frame_count++;
  }

  if (multiplayer_mode) {
      close(server_socket);
  }

  printf("\x1b[0m\x1b[39m");
  return 0;
}
