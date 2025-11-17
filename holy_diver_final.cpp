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
#include <fstream>
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
const int BATTERY_COST = 5;

// Coin and collectible types
const char COIN = '*';
const char BATTERY_PACK = 'B';
const char OXYGEN_TANK = 'O';

/****************************************************/
// Forward declarations
/****************************************************/
class World;
class Enemy;

/****************************************************/
// Player Class
/****************************************************/
class Player {
private:
    int x, y;
    int health;
    int oxygen;
    int battery;
    int lives;

public:
    Player(int startX, int startY) : x(startX), y(startY), 
                                      health(MAX_HEALTH), 
                                      oxygen(MAX_OXYGEN),
                                      battery(MAX_BATTERY),
                                      lives(3) {}

    int getX() const { return x; }
    int getY() const { return y; }
    int getHealth() const { return health; }
    int getOxygen() const { return oxygen; }
    int getBattery() const { return battery; }
    int getLives() const { return lives; }

    void setPosition(int newX, int newY) { x = newX; y = newY; }
    
    void takeDamage(int amount) {
        health -= amount;
        if (health < 0) health = 0;
    }

    void consumeOxygen(int amount) {
        oxygen -= amount;
        if (oxygen < 0) oxygen = 0;
    }

    void addOxygen(int amount) {
        oxygen += amount;
        if (oxygen > MAX_OXYGEN) oxygen = MAX_OXYGEN;
    }

    bool useBattery() {
        if (battery >= BATTERY_COST) {
            battery -= BATTERY_COST;
            return true;
        }
        return false;
    }

    void rechargeBattery(int amount) {
        battery += amount;
        if (battery > MAX_BATTERY) battery = MAX_BATTERY;
    }

    bool isDead() const {
        return health <= 0 || oxygen <= 0;
    }
};

/****************************************************/
// Enemy Base Class
/****************************************************/
class Enemy {
protected:
    int x, y;
    int damage;
    bool active;
    bool visible;

public:
    Enemy(int startX, int startY, int dmg) : x(startX), y(startY), 
                                               damage(dmg), 
                                               active(false),
                                               visible(false) {}
    
    virtual ~Enemy() {}

    int getX() const { return x; }
    int getY() const { return y; }
    int getDamage() const { return damage; }
    bool isActive() const { return active; }
    bool isVisible() const { return visible; }

    void activate() { active = true; }
    void makeVisible() { visible = true; }
    
    virtual void move(World* world) = 0;  // Pure virtual for polymorphism
    
    int giveDamage() const { return damage; }
};

/****************************************************/
// Stationary Enemy Class
/****************************************************/
class StationaryEnemy : public Enemy {
public:
    StationaryEnemy(int startX, int startY) : Enemy(startX, startY, 20) {}
    
    void move(World* world) override {
        // Stationary enemies don't move
    }
};

/****************************************************/
// Moving Enemy Class
/****************************************************/
class MovingEnemy : public Enemy {
public:
    MovingEnemy(int startX, int startY) : Enemy(startX, startY, 15) {}
    
    void move(World* world) override;  // Defined after World class
};

/****************************************************/
// World Class
/****************************************************/
class World {
private:
    char map[MAP_HEIGHT][MAP_WIDTH];
    bool illuminated[MAP_HEIGHT][MAP_WIDTH];
    Player* player;
    vector<Enemy*> enemies;
    int score;
    
    struct Collectible {
        int x, y;
        char type;
        bool collected;
    };
    vector<Collectible> collectibles;

public:
    World() : player(nullptr), score(0) {
        // Initialize illumination map
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                illuminated[y][x] = false;
            }
        }
    }

    ~World() {
        if (player) delete player;
        for (Enemy* e : enemies) delete e;
    }

    void loadMap(const string& filepath) {
        ifstream file(filepath);
        if (file.is_open()) {
            for (int y = 0; y < MAP_HEIGHT; y++) {
                for (int x = 0; x < MAP_WIDTH; x++) {
                    char c;
                    if (file.get(c)) {
                        if (c == '\n') file.get(c);  // skip newlines
                        
                        if (c == 'P') {
                            player = new Player(x, y);
                            map[y][x] = 'o';
                            illuminated[y][x] = true;  // Start position visible
                        } else if (c == 'M') {
                            // Randomly create stationary or moving enemy
                            if (rand() % 2 == 0) {
                                enemies.push_back(new StationaryEnemy(x, y));
                            } else {
                                enemies.push_back(new MovingEnemy(x, y));
                            }
                            map[y][x] = 'o';
                        } else {
                            map[y][x] = c;
                        }
                    }
                }
            }
            file.close();
        } else {
            // Default map if file not found
            createDefaultMap();
        }
    }

    void createDefaultMap() {
        // Fill with walls on edges, empty inside
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                if (y == 0 || y == MAP_HEIGHT - 1 || x == 0 || x == MAP_WIDTH - 1) {
                    map[y][x] = 'x';
                } else {
                    map[y][x] = 'o';
                }
            }
        }
        
        // Add more obstacles for interesting layout
        for (int i = 0; i < 25; i++) {
            int x = 2 + rand() % (MAP_WIDTH - 4);
            int y = 2 + rand() % (MAP_HEIGHT - 4);
            map[y][x] = 'x';
        }

        // Create player
        player = new Player(5, 5);
        illuminated[5][5] = true;

        // Add many more enemies (mix of stationary and moving)
        for (int i = 0; i < 15; i++) {
            int x = 2 + rand() % (MAP_WIDTH - 4);
            int y = 2 + rand() % (MAP_HEIGHT - 4);
            if (map[y][x] == 'o' && !(x == 5 && y == 5)) {
                if (rand() % 3 == 0) {  // 1/3 chance stationary
                    enemies.push_back(new StationaryEnemy(x, y));
                } else {
                    enemies.push_back(new MovingEnemy(x, y));
                }
            }
        }
        
        // Spawn collectibles (coins, battery packs, oxygen tanks)
        spawnCollectibles();
    }

    void spawnCollectibles() {
        // Spawn 10-15 coins
        for (int i = 0; i < 10 + rand() % 6; i++) {
            int x = 1 + rand() % (MAP_WIDTH - 2);
            int y = 1 + rand() % (MAP_HEIGHT - 2);
            if (map[y][x] == 'o') {
                collectibles.push_back({x, y, COIN, false});
            }
        }
        
        // Spawn 3-5 battery packs
        for (int i = 0; i < 3 + rand() % 3; i++) {
            int x = 1 + rand() % (MAP_WIDTH - 2);
            int y = 1 + rand() % (MAP_HEIGHT - 2);
            if (map[y][x] == 'o') {
                collectibles.push_back({x, y, BATTERY_PACK, false});
            }
        }
        
        // Spawn 3-5 oxygen tanks
        for (int i = 0; i < 3 + rand() % 3; i++) {
            int x = 1 + rand() % (MAP_WIDTH - 2);
            int y = 1 + rand() % (MAP_HEIGHT - 2);
            if (map[y][x] == 'o') {
                collectibles.push_back({x, y, OXYGEN_TANK, false});
            }
        }
    }

    bool canMoveTo(int x, int y) const {
        if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) return false;
        return map[y][x] != 'x';
    }

    bool requestMove(int fromX, int fromY, int toX, int toY, bool isPlayer) {
        if (!canMoveTo(toX, toY)) {
            if (isPlayer) {
                player->consumeOxygen(2);  // Consume oxygen even on failed move
            }
            return false;
        }

        if (isPlayer) {
            // Check for enemy collision
            for (Enemy* enemy : enemies) {
                if (enemy->getX() == toX && enemy->getY() == toY) {
                    player->takeDamage(enemy->giveDamage());
                    return false;  // Can't move into enemy
                }
            }
            player->setPosition(toX, toY);
            player->consumeOxygen(2);
            
            // Illuminate current position
            illuminated[toY][toX] = true;
            
            // Check for collectibles
            for (auto& col : collectibles) {
                if (!col.collected && col.x == toX && col.y == toY) {
                    col.collected = true;
                    if (col.type == COIN) {
                        score += 50;
                    } else if (col.type == BATTERY_PACK) {
                        player->rechargeBattery(30);
                        score += 20;
                    } else if (col.type == OXYGEN_TANK) {
                        player->addOxygen(40);
                        score += 20;
                    }
                }
            }
        }

        return true;
    }

    void illuminateTile(int x, int y) {
        if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
            if (player->useBattery()) {
                illuminated[y][x] = true;
                
                // Check if enemy is on this tile and activate/make visible
                for (Enemy* enemy : enemies) {
                    if (enemy->getX() == x && enemy->getY() == y) {
                        enemy->makeVisible();
                        enemy->activate();
                    }
                }
            }
        }
    }

    void updateEnemies() {
        for (Enemy* enemy : enemies) {
            // Check if player can see enemy
            int px = player->getX();
            int py = player->getY();
            int ex = enemy->getX();
            int ey = enemy->getY();
            
            // Simple visibility check - within 3 tiles and illuminated
            if (abs(px - ex) <= 3 && abs(py - ey) <= 3 && illuminated[ey][ex]) {
                enemy->makeVisible();
                enemy->activate();
            }

            // Active enemies move
            if (enemy->isActive()) {
                enemy->move(this);
            }

            // Check collision with player
            if (enemy->getX() == player->getX() && enemy->getY() == player->getY()) {
                player->takeDamage(enemy->giveDamage());
            }
        }
    }

    void render() const {
        cout << "\033[2J\033[1;1H";  // Clear screen
        cout << "=== HOLY DIVER - Exploration Mode ===" << endl;
        cout << "Health: " << player->getHealth() 
             << " | Oxygen: " << player->getOxygen() 
             << " | Battery: " << player->getBattery()
             << " | Score: " << score << endl;

        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                if (x == player->getX() && y == player->getY()) {
                    cout << 'P';
                } else if (illuminated[y][x]) {
                    // Check if collectible is here
                    bool foundItem = false;
                    for (const auto& col : collectibles) {
                        if (!col.collected && col.x == x && col.y == y) {
                            cout << col.type;
                            foundItem = true;
                            break;
                        }
                    }
                    
                    if (!foundItem) {
                        // Check if enemy is here
                        bool enemyHere = false;
                        for (Enemy* enemy : enemies) {
                            if (enemy->getX() == x && enemy->getY() == y && enemy->isVisible()) {
                                cout << 'M';
                                enemyHere = true;
                                break;
                            }
                        }
                        if (!enemyHere) {
                            cout << map[y][x];
                        }
                    }
                } else {
                    cout << ' ';  // Dark/unknown tile
                }
            }
            cout << endl;
        }

        cout << "\nControls:" << endl;
        cout << "WASD: Move | IJKL: Illuminate (I=up, J=left, K=down, L=right)" << endl;
        cout << "R: Reload | Q: Quit" << endl;
        cout << "\nCollect: * (Coins +50pts), B (Battery +30%), O (Oxygen +40%)" << endl;
    }

    Player* getPlayer() { return player; }
    bool isGameOver() const { return player->isDead(); }
    
    void reset(const string& filepath) {
        if (player) delete player;
        for (Enemy* e : enemies) delete e;
        enemies.clear();
        collectibles.clear();
        score = 0;
        
        for (int y = 0; y < MAP_HEIGHT; y++) {
            for (int x = 0; x < MAP_WIDTH; x++) {
                illuminated[y][x] = false;
            }
        }
        
        loadMap(filepath);
    }
    
    int getScore() const { return score; }
};

/****************************************************/
// MovingEnemy implementation (needs World definition)
/****************************************************/
void MovingEnemy::move(World* world) {
    if (!active) return;
    
    // Random chance to skip movement
    if (rand() % 3 == 0) return;
    
    // Try random direction
    int dir = rand() % 4;
    int newX = x, newY = y;
    
    switch(dir) {
        case 0: newY--; break;  // up
        case 1: newY++; break;  // down
        case 2: newX--; break;  // left
        case 3: newX++; break;  // right
    }
    
    if (world->canMoveTo(newX, newY)) {
        x = newX;
        y = newY;
    }
}

/****************************************************/
// Terminal handling
/****************************************************/
static struct termios saved_term;

void setup_terminal() {
    tcgetattr(STDIN_FILENO, &saved_term);
    struct termios newt = saved_term;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN] = 0;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
}

void restore_terminal() {
    tcsetattr(STDIN_FILENO, TCSANOW, &saved_term);
}

char read_key() {
    char ch = '\0';
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n > 0) return ch;
    return '\0';
}

/****************************************************/
// Main game loop
/****************************************************/
int main() {
    srand(time(NULL));
    
    cout << "=== HOLY DIVER ===" << endl;
    cout << "Enter map filepath (or press Enter for default): ";
    string filepath;
    getline(cin, filepath);
    
    if (filepath.empty()) filepath = "default";

    bool playAgain = true;
    
    while (playAgain) {
        World* world = new World();
        world->loadMap(filepath);
        
        setup_terminal();
        
        bool running = true;
        while (running) {
            world->render();
            
            char input = read_key();
            if (input != '\0') {
                Player* p = world->getPlayer();
                int px = p->getX();
                int py = p->getY();
                
                switch(tolower(input)) {
                    case 'w': world->requestMove(px, py, px, py-1, true); break;
                    case 's': world->requestMove(px, py, px, py+1, true); break;
                    case 'a': world->requestMove(px, py, px-1, py, true); break;
                    case 'd': world->requestMove(px, py, px+1, py, true); break;
                    case 'i': world->illuminateTile(px, py-1); break;
                    case 'k': world->illuminateTile(px, py+1); break;
                    case 'j': world->illuminateTile(px-1, py); break;
                    case 'l': world->illuminateTile(px+1, py); break;
                    case 'r': world->reset(filepath); break;
                    case 'q': running = false; playAgain = false; break;
                }
                
                world->updateEnemies();
                
                if (world->isGameOver()) {
                    world->render();
                    restore_terminal();
                    cout << "\n=== GAME OVER ===" << endl;
                    cout << "Final Score: " << world->getScore() << endl;
                    cout << "\nPress Enter to play again, or Q then Enter to quit: ";
                    
                    string response;
                    getline(cin, response);
                    
                    if (!response.empty() && (response[0] == 'q' || response[0] == 'Q')) {
                        playAgain = false;
                    }
                    running = false;
                }
            }
            
            usleep(100000);  // 100ms delay
        }
        
        restore_terminal();
        delete world;
    }
    
    cout << "\nThanks for playing!" << endl;
    return 0;
}
/* This Holy_diver game was done with the help of the Visual studio code co pilot. 
Co pilot helped to understand the basics of advanced programming and me and Daria were doing this assignment together. 
We got extra help for some difficult to understand things like Constructor and destructor and encapsulation from Deepseek ai. 
In VS code I encountered some problems with running and debugging the game but then I installed different launcher and it worked. 
For the visual studio it needed some adjustment but it started working in the end.
*/