// 一応自分でもよく目は通したのですが、よくわかりません。
// 自分でやった作業は8080/TCPのポート開放くらいです。

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <errno.h>

// --- ワールド設定 ---
#define WORLD_X 64
#define WORLD_Y 32
#define WORLD_Z 64

// --- サーバー設定 ---
// #define SERVER_PORT 8888
#define SERVER_PORT 8080
#define MAX_CLIENTS 30
#define AUTOSAVE_INTERVAL_SECONDS 300 // 変更点: オートセーブの間隔を秒単位で設定 (300秒 = 5分)

// --- 構造体の定義 ---
typedef struct {
    int x, y, z, type;
} BlockUpdate;

// --- グローバル変数 ---
int world[WORLD_X][WORLD_Y][WORLD_Z];
pthread_mutex_t world_mutex = PTHREAD_MUTEX_INITIALIZER;

int client_sockets[MAX_CLIENTS] = {0};
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;


// --- consolecraft.c から移植した関数 ---
// consolecraft.cとはshellcraftmという名前にする前の仮の名前
// 名前の変遷 game.c -> consolecraft.c -> consolecraftm.c -> shellcraftm.c
// Geminiにマルチプレイ対応にさせたのちはmがつきます。
int r(int min, int max) { return min + rand() % (max - min + 1); }

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

// 変更点: save_world関数をスレッドセーフにする
void save_world() {
  FILE* fp = fopen("world.txt", "w");
  if (fp == NULL) {
      perror("Failed to open world.txt for writing");
      return;
  }

  // world配列へのアクセス中に他のスレッドが変更しないようにロックする
  pthread_mutex_lock(&world_mutex);
  
  for (int x = 0; x < WORLD_X; x++) {
    for (int y = 0; y < WORLD_Y; y++) {
      for (int z = 0; z < WORLD_Z; z++) {
        fprintf(fp, "%d ", world[x][y][z]);
      }
      fprintf(fp, "\n");
    }
    fprintf(fp, "\n");
  }
  
  // ロックを解除する
  pthread_mutex_unlock(&world_mutex);

  fclose(fp);
  printf("[AutoSave] World saved to world.txt\n");
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

// --- ヘルパー関数: send_all ---
int send_all(int socket, const void *buffer, size_t length) {
    const char *ptr = (const char *)buffer;
    while (length > 0) {
        int i = send(socket, ptr, length, 0);
        if (i < 1) {
            return -1;
        }
        ptr += i;
        length -= i;
    }
    return 0;
}


// --- 更新情報を全クライアントにブロードキャストする関数 ---
void broadcast_update(BlockUpdate* update, int sender_socket) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] != 0 && client_sockets[i] != sender_socket) {
            send(client_sockets[i], update, sizeof(BlockUpdate), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// --- 各クライアントを処理するスレッド関数 ---
void* handle_client(void* socket_desc) {
    int sock = *(int*)socket_desc;
    free(socket_desc);
    
    printf("Sending world data to client (socket: %d)\n", sock);
    if (send_all(sock, world, sizeof(world)) < 0) {
        perror("Send world failed");
        close(sock);
        return NULL;
    }

    BlockUpdate update;
    int read_size;
    while ((read_size = recv(sock, &update, sizeof(BlockUpdate), 0)) > 0) {
        pthread_mutex_lock(&world_mutex);
        if (update.x >= 0 && update.x < WORLD_X &&
            update.y >= 0 && update.y < WORLD_Y &&
            update.z >= 0 && update.z < WORLD_Z) {
            world[update.x][update.y][update.z] = update.type;
        }
        pthread_mutex_unlock(&world_mutex);

        // printf("Client %d updated block (%d, %d, %d) to %d\n", sock, update.x, update.y, update.z, update.type);

        broadcast_update(&update, sock);
    }

    if (read_size == 0) {
        printf("Client %d disconnected\n", sock);
    } else if (read_size == -1) {
        perror("recv failed");
    }

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (client_sockets[i] == sock) {
            client_sockets[i] = 0;
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    close(sock);
    return NULL;
}

// 変更点: オートセーブ用のスレッド関数を新しく作成
void* autosave_routine(void* arg) {
    while (1) {
        // 指定した時間だけ待機
        sleep(AUTOSAVE_INTERVAL_SECONDS);
        
        // ワールドを保存
        save_world();
    }
    return NULL;
}


int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    srand((unsigned int)time(NULL));

    if (!load_world()) {
        printf("world.txt not found. Generating new world...\n");
        worldgenerate();
    } else {
        printf("Loaded world from world.txt\n");
    }

    // 変更点: オートセーブスレッドを開始
    pthread_t autosave_thread;
    if (pthread_create(&autosave_thread, NULL, autosave_routine, NULL) < 0) {
        perror("could not create autosave thread");
        exit(EXIT_FAILURE);
    }
    // メインスレッドから切り離す
    pthread_detach(autosave_thread);
    printf("Autosave thread started. Saving every %d seconds.\n", AUTOSAVE_INTERVAL_SECONDS);


    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(SERVER_PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", SERVER_PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("accept");
            continue;
        }

        printf("New connection accepted, socket fd is %d\n", new_socket);

        pthread_mutex_lock(&clients_mutex);
        if (client_count < MAX_CLIENTS) {
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (client_sockets[i] == 0) {
                    client_sockets[i] = new_socket;
                    client_count++;
                    break;
                }
            }

            pthread_t sniffer_thread;
            int* new_sock = malloc(sizeof(int));
            if (new_sock == NULL) {
                perror("malloc for new_sock failed");
                close(new_socket);
            } else {
                *new_sock = new_socket;
                if (pthread_create(&sniffer_thread, NULL, handle_client, (void*)new_sock) < 0) {
                    perror("could not create thread");
                    free(new_sock);
                }
            }
        } else {
            printf("Max clients reached. Connection rejected.\n");
            close(new_socket);
        }
        pthread_mutex_unlock(&clients_mutex);
    }

    return 0;
}