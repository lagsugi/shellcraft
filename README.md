# sherukura

A simple Minecraft-like sandbox game that runs in the console (Linux only). It doesn’t even use ncurses.

---

## Build

1. Clone the repository:

   ```bash
   git clone https://github.com/lagsugi/sherukura.git
   cd sherukura
   ```
2. Compile the game:

   ```bash
   make
   ```

---

## Run

```bash
./shellcraftm
```

---

## Controls

You can also see the in-game cheatsheet.

* **Movement**:

  * `W` / `A` / `S` / `D` – move forward, left, backward, right
  * `Space` – move up
  * `V` – move down

* **View Rotation**:

  * `I` / `J` / `K` / `L` – look up, left, down, right

* **Block Interaction**:

  * Number keys (`1`–`9`, `0`) – select block type to place or destroy

* **Other**:

  * `X` – save the world
  * `Q` – quit the game

---

## Notes

* The default display size is **90×30** characters. To change this, edit `shellcraftm.c`.
* Your world is saved automatically to `world.txt` and will be reloaded on your next session.
* **Do not** hold down keys when playing via TeraTerm. This may introduce input lag.

---

## Non-multiplayer Version

`shellcraft.c` does not have multiplayer feature at all.

---

## Multiplayer

### Server

Compile and start the server:

```bash
gcc scserver.c -o scserver -lpthread -lm
./scserver
```

By default, the server listens on port 8080.

### Client

Run the multiplayer client using -m option:

```bash
./shellcraftm -m
```

The client is configured to connect to `localhost:8080` by default. To change the server address or port, edit `shellcraftm.c` before compiling.
