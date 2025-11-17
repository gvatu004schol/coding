#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#include <errno.h>
#include <cctype>
#include <sys/types.h>
#include <vector>

using namespace std;

/****************************************************/
// Constants
/****************************************************/
const int MAP_WIDTH = 20;
const int MAP_HEIGHT = 20;
const int MAX_HEALTH = 100;
const int MAX_OXYGEN = 100;
const int MAX_BATTERY = 100;
const int INIT_LIVES = 3;
const int INITIAL_HEALTH = 5;

const char PLAYER_CHAR = 'P';
const char WALL = 'x';
const char EMPTY = 'o';
const char OBSTACLE = '#';
const char COIN = '*';
const char ENEMY = 'M';
const char DARK_TILE = '?';

/****************************************************/
// Forward declarations
/****************************************************/
class Enemy;
class World;

/****************************************************/
// Player Class
/****************************************************/
class Player {
public:
    int x, y;
    int health;
    int oxygen;
    int battery;
    int lives;

    Player(int startX, int startY) 
        : x(startX), y(startY), health(INITIAL_HEALTH), oxygen(MAX_OXYGEN), 
          battery(MAX_BATTERY), lives(INIT_LIVES) {}

    void takeDamage(int amount) {
        health -= amount;
        if (health < 0) health = 0;
    }

    void consumeOxygen(int amount) {
        oxygen -= amount;
        if (oxygen < 0) oxygen = 0;
    }

    void restoreOxygen(int amount) {
        oxygen += amount;
        if (oxygen > MAX_OXYGEN) oxygen = MAX_OXYGEN;
    }

    void consumeBattery(int amount) {
        battery -= amount;
        if (battery < 0) battery = 0;
    }

    bool canMove() { return health > 0 && oxygen > 0; }
};

/****************************************************/
// Enemy Class (Base)
/****************************************************/
class Enemy {
public:
    int x, y;
    bool isMoving;
    bool isActive;

    Enemy(int startX, int startY, bool moving)
        : x(startX), y(startY), isMoving(moving), isActive(false) {}

    virtual ~Enemy() {}

    virtual void move(World& world) = 0;

    int giveDamage() { return 1; }
};

/****************************************************/
// Stationary Enemy
/****************************************************/
class StationaryEnemy : public Enemy {
public:
    StationaryEnemy(int startX, int startY)
        : Enemy(startX, startY, false) {}

    void move(World& world) override {
        // Stationary enemies do nothing
    }
};

/****************************************************/
// Moving Enemy
/****************************************************/
class MovingEnemy : public Enemy {
public:
    MovingEnemy(int startX, int startY)
        : Enemy(startX, startY, true) {}

    void move(World& world) override;
};

/****************************************************/
// World Class
/****************************************************/
class World {
public:
    char map[MAP_HEIGHT][MAP_WIDTH];
    bool visibility[MAP_HEIGHT][MAP_WIDTH];  // tracks illuminated tiles
    Player player;
    vector<Enemy*> enemies;

    World(int playerStartX, int playerStartY)
        : player(playerStartX, playerStartY) {
        // Initialize visibility (all dark)
        for (int y = 0; y < MAP_HEIGHT; ++y) {
            for (int x = 0; x < MAP_WIDTH; ++x) {
                visibility[y][x] = false;
            }
        }
    }

    ~World() {
        for (auto e : enemies) delete e;
    }

    void loadMap(const char staticMap[MAP_HEIGHT][MAP_WIDTH]) {
        for (int y = 0; y < MAP_HEIGHT; ++y) {
            for (int x = 0; x < MAP_WIDTH; ++x) {
                map[y][x] = staticMap[y][x];
                if (staticMap[y][x] == ENEMY) {
                    // For now, create moving enemies at M positions
                    enemies.push_back(new MovingEnemy(x, y));
                    map[y][x] = EMPTY;  // clear the map tile
                }
            }
        }
    }

    bool canMoveTo(int x, int y) {
        if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return false;
        if (map[y][x] == WALL) return false;
        return true;
    }

    bool tryMovePlayer(int newX, int newY) {
        if (!canMoveTo(newX, newY)) {
            player.consumeOxygen(2);  // consume oxygen even if blocked
            return false;
        }

        player.x = newX;
        player.y = newY;
        player.consumeOxygen(2);  // 2% per move

        // Check collision with enemies
        for (auto e : enemies) {
            if (e->x == newX && e->y == newY) {
                player.takeDamage(e->giveDamage());
                break;
            }
        }

        return true;
    }

    void illuminateTile(int x, int y) {
        if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
            visibility[y][x] = true;
            player.consumeBattery(5);  // 5% per illuminate
        }
    }

    void updateEnemies() {
        for (auto e : enemies) {
            if (e->isActive) {
                e->move(*this);
                // Check collision with player after move
                if (e->x == player.x && e->y == player.y) {
                    player.takeDamage(e->giveDamage());
                }
            }
        }
    }

    char getTileContent(int x, int y) {
        if (!visibility[x][y]) return DARK_TILE;
        
        // Check if player is here
        if (player.x == x && player.y == y) return PLAYER_CHAR;
        
        // Check if enemy is here
        for (auto e : enemies) {
            if (e->x == x && e->y == y) return ENEMY;
        }
        
        return map[x][y];
    }

    void reset(int playerStartX, int playerStartY) {
        player.x = playerStartX;
        player.y = playerStartY;
        player.health = INITIAL_HEALTH;
        player.oxygen = MAX_OXYGEN;
        player.battery = MAX_BATTERY;

        for (int y = 0; y < MAP_HEIGHT; ++y) {
            for (int x = 0; x < MAP_WIDTH; ++x) {
                visibility[y][x] = false;
            }
        }

        for (auto e : enemies) {
            e->isActive = false;
        }
    }
};

/****************************************************/
// Moving Enemy Implementation
/****************************************************/
void MovingEnemy::move(World& world) {
    if (!isActive) return;

    // Random chance to move (70% chance)
    if (rand() % 100 > 70) return;

    // Try to move in a random direction
    int directions[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
    int dir = rand() % 4;
    int newX = x + directions[dir][0];
    int newY = y + directions[dir][1];

    if (world.canMoveTo(newX, newY)) {
        x = newX;
        y = newY;
    }
}

/****************************************************/
// Global
/****************************************************/
static struct termios saved_term;
World* gameWorld = nullptr;

/****************************************************/
// Functions
/****************************************************/
void start_splash_screen(void) {
    cout << endl << "WELCOME to epic Holy Diver v0.04" << endl;
    cout << "Use WASD to move, IJKL to illuminate, R to reload, Q to quit." << endl;
    cout << "Press Enter to start..." << endl;
    cin.ignore();
}

void startup_routines(void) {
    tcgetattr(STDIN_FILENO, &saved_term);
    struct termios newt = saved_term;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 0;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    if (gameWorld == nullptr) {
        gameWorld = new World(9, 9);
    } else {
        gameWorld->reset(9, 9);
    }
}

void quit_routines(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_term);
    if (gameWorld) {
        delete gameWorld;
        gameWorld = nullptr;
    }
    cout << endl << "BYE! Thanks for playing." << endl;
}

void clearScreen() {
    cout << "\033[2J\033[1;1H";
}

int read_input(char * input) {
    *input = '\0';
    char ch;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) return 0;
        return -1;
    }
    if (n == 0) return 0;
    *input = ch;
    if (ch == 'q' || ch == 'Q') return -2;
    return 0;
}

void update_state(char input) {
    if (input != '\0') {
        char c = tolower(input);
        int newX = gameWorld->player.x;
        int newY = gameWorld->player.y;

        if (c == 'w' && gameWorld->player.y > 0) newY--;
        else if (c == 's' && gameWorld->player.y < MAP_HEIGHT - 1) newY++;
        else if (c == 'a' && gameWorld->player.x > 0) newX--;
        else if (c == 'd' && gameWorld->player.x < MAP_WIDTH - 1) newX++;
        else if (c == 'i') gameWorld->illuminateTile(gameWorld->player.x, gameWorld->player.y - 1);
        else if (c == 'k') gameWorld->illuminateTile(gameWorld->player.x, gameWorld->player.y + 1);
        else if (c == 'j') gameWorld->illuminateTile(gameWorld->player.x - 1, gameWorld->player.y);
        else if (c == 'l') gameWorld->illuminateTile(gameWorld->player.x + 1, gameWorld->player.y);
        else if (c == 'r') {
            gameWorld->reset(9, 9);
            return;
        }

        if (newX != gameWorld->player.x || newY != gameWorld->player.y) {
            gameWorld->tryMovePlayer(newX, newY);
        }
    }

    gameWorld->updateEnemies();
}

void render_screen(void) {
    clearScreen();
    cout << "=== HOLY DIVER ===" << endl;
    cout << "Health: " << gameWorld->player.health << " | Oxygen: " << gameWorld->player.oxygen 
         << " | Battery: " << gameWorld->player.battery << endl;

    for (int y = 0; y < MAP_HEIGHT; ++y) {
        for (int x = 0; x < MAP_WIDTH; ++x) {
            cout << gameWorld->getTileContent(x, y);
        }
        cout << endl;
    }

    cout << "WASD=move, IJKL=illuminate, R=reload, Q=quit" << endl;
}

void load_level(string filepath) {
    // TODO: implement file loading
}

int main(void) {
    start_splash_screen();

    while (true) {
        startup_routines();

        while (true) {
            char input = '\0';
            int r = read_input(&input);
            if (r < 0) break;
            update_state(input);
            render_screen();
            usleep(140000);

            if (gameWorld->player.health <= 0 || gameWorld->player.oxygen <= 0) break;
        }

        tcsetattr(STDIN_FILENO, TCSANOW, &saved_term);
        cout << endl << "=== GAME OVER ===" << endl;
        cout << "Health: " << gameWorld->player.health << " | Oxygen: " << gameWorld->player.oxygen << endl;

        if (gameWorld->player.health <= 0) cout << "Defeated!" << endl;
        else if (gameWorld->player.oxygen <= 0) cout << "Drowned!" << endl;

        cout << "Press Enter to restart or 'q' to quit: ";
        string response;
        if (!std::getline(cin, response)) break;
        if (!response.empty() && (response[0] == 'q' || response[0] == 'Q')) break;
    }

    quit_routines();
    return 0;
}
