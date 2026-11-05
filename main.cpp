#define GLAD_GL_IMPLEMENTATION
#include <glad/gl.h>
#undef GLAD_GL_IMPLEMENTATION
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "audio.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>
#include <random>
#include <vector>


// 1. Character types and data

enum class PlayerType {
    TempleWarrior,
    RoyalArcher,
    ShieldGuardian
};

enum class EnemyType {
    SandSpirit,
    CursedScarab,
    JackalBrute
};

enum class CharacterAction {
    Idle,
    Walking,
    Attacking,
    Defending,
    Jumping,
    Ducking,
    Defeated
};

struct Projectile {
    int lane;
    float x;
    float y;
    bool active = true;
};

struct Collectible {
    int lane;
    float x;
    float y;
    bool collected = false;
};

// Jumping raises the drawing; the stored lane and position stay on the ground.
struct Player {
    PlayerType type;
    int lane;
    float x;
    float y;
    float previousX;
    int maxHealth = 5;
    int health = 5;
    CharacterAction action = CharacterAction::Walking;
    float attackTimer = 0.0f;
    float jumpTimer = 0.0f;
    float duckTimer = 0.0f;
    float walkPhase = 0.0f;

    Player(PlayerType playerType, int laneIndex, float startX, float startY)
        : type(playerType), lane(laneIndex), x(startX), y(startY), previousX(startX) {
        if (type == PlayerType::ShieldGuardian) {
            maxHealth = 7;
            health = maxHealth;
            action = CharacterAction::Defending;
        }
    }
};

struct Enemy {
    EnemyType type;
    int lane;
    float x;
    float y;
    int maxHealth = 5;
    int health = 5;
    CharacterAction action = CharacterAction::Walking;
    float attackTimer = 0.0f;
    float walkPhase = 0.0f;

    Enemy(EnemyType enemyType, int laneIndex, float startX, float startY)
        : type(enemyType), lane(laneIndex), x(startX), y(startY) {
        if (type == EnemyType::JackalBrute) {
            maxHealth = 7;
            health = maxHealth;
        }
    }
};


// 2. Wave data and game settings

constexpr int waveCount = 3;
constexpr int waveEnemyCounts[waveCount] = {3, 5, 10};

struct WaveProgress {
    int waveIndex = 0; // Zero-based; waveCount means all waves are cleared.
    int enemiesSpawned = 0;
    float spawnTimer = 0.0f;
};

constexpr float menuCenterX = 0.1f;
constexpr float menuButtonWidth = 0.64f;
constexpr float menuButtonHeight = 0.17f;
constexpr float menuButtonY[] = {0.0f, -0.26f};
constexpr float mainMenuButtonY[] = {0.06f, -0.18f, -0.42f};
constexpr float soundButtonY[] = {0.16f, -0.07f, -0.30f, -0.53f};
constexpr float pauseButtonY[] = {0.18f, -0.08f, -0.34f};
constexpr float pauseButtonWidth = 0.72f;

// Drawing and click detection must use these same boundaries.
constexpr int laneCount = 4;
constexpr float fieldLeft = -0.65f;
constexpr float fieldRight = 0.85f;
constexpr float fieldTop = 0.55f;
constexpr float laneHeight = 0.3f;
constexpr float fieldBottom = fieldTop - laneCount * laneHeight;
constexpr int playerCardCount = 3;
constexpr float cardFirstX = -0.48f;
constexpr float cardSpacing = 0.28f;
constexpr float cardY = 0.82f;
constexpr float cardWidth = 0.23f;
constexpr float cardHeight = 0.30f;
constexpr PlayerType cardTypes[] = {
    PlayerType::TempleWarrior, PlayerType::RoyalArcher, PlayerType::ShieldGuardian
};
// The brief's starting balance buys three melee units; written cost ratios are 2x/3x.
constexpr int meleePurchaseCost = 10;
constexpr int cardCosts[playerCardCount] = {meleePurchaseCost, 2 * meleePurchaseCost, 3 * meleePurchaseCost};
constexpr int collectibleValue = 10; // Balance setting: the brief gives no drop value.

// Durations are in seconds.
constexpr float jumpDuration = 0.65f;
constexpr float jumpHeight = 0.06f;
constexpr float duckDuration = 0.65f;
constexpr float duckHeightScale = 0.55f;
constexpr float archerAttackInterval = 0.9f;
constexpr float encounterDistance = 0.16f;
constexpr float meleeAttackInterval = 0.8f; // Seconds per swing; a tuning value.
constexpr float bruteAttackInterval = 1.1f; // Heavy axe swings have a longer windup.


// 3. Game states and necessary drawing declarations

enum class GameState {
    MainMenu,
    Cutscene,
    Playing,
    Paused,
    Victory,
    Defeat,
    SoundSettings
};

constexpr int circleSegments = 48;

// These shapes and numbers are used before their definitions below.


void drawRectangle(float x, float y, float width, float height, float angle,
                   float red, float green, float blue, bool outline = false);
void drawCircle(float x, float y, float diameter, glm::vec3 color);
void drawTriangle(float x, float y, float width, float height, glm::vec3 color);
void drawNumber(int number, float originX, float originY, float size, bool centered = false);


// 4. Cutscene timeline

constexpr float cutsceneDuration = 18.0f;

float cutsceneProgress(float time, float start, float end) {
    return std::clamp((time - start) / (end - start), 0.0f, 1.0f);
}

struct CutscenePose {
    float warriorX = -0.52f;
    float walkPhase = 0.0f;
    bool ducking = false;
    bool swinging = false;
    float swordSwingTime = 0.0f;
    bool spiritVisible = false;
    float spiritX = 1.15f;
    float spiritY = -0.13f;
    float staffAngle = 0.0f;
    float spiritDamageTint = 0.0f;
    bool shotVisible = false;
    float shotX = 0.0f;
    bool invasionVisible = false;
    float scarabX = 1.15f;
    float bruteX = 1.4f;
    const char* firstLine = "THE SOLAR HEART";
    const char* secondLine = "GUARDS OUR KINGDOM";
};

CutscenePose getCutscenePose(float time) {
    CutscenePose pose;
    pose.warriorX += 0.27f * cutsceneProgress(time, 3.0f, 6.0f);
    if (time >= 3.0f && time < 6.0f) pose.walkPhase = (time - 3.0f) * 9.0f;
    if (time >= 3.0f) {
        pose.firstLine = "THE SACRED SUN";
        pose.secondLine = "MUST NEVER FALL";
    }
    if (time >= 6.0f) {
        pose.spiritVisible = true;
        pose.spiritX = 1.15f - 0.8f * cutsceneProgress(time, 6.0f, 9.5f);
        pose.spiritY += 0.008f * std::sin((time - 6.0f) * 4.0f);
        pose.staffAngle = 30.0f * (1.0f - cutsceneProgress(time, 8.0f, 10.0f));
        pose.firstLine = "THE SOLAR HEART";
        pose.secondLine = "BELONGS TO THE DARKNESS";
    }
    if (time >= 10.0f) {
        pose.firstLine = "";
        pose.secondLine = "";
    }
    pose.ducking = time >= 10.25f && time < 11.5f;
    pose.shotVisible = time >= 10.2f && time < 11.4f;
    pose.shotX = 0.222f - 0.822f * cutsceneProgress(time, 10.2f, 11.4f);
    pose.warriorX += 0.33f * cutsceneProgress(time, 11.5f, 13.2f);
    if (time >= 11.5f && time < 13.2f) pose.walkPhase = (time - 11.5f) * 9.0f;
    pose.swinging = time >= 13.2f && time < 14.0f;
    pose.swordSwingTime = std::clamp(time - 13.2f, 0.0f, 0.8f);
    if (time >= 13.6f) {
        pose.spiritX += 0.18f * cutsceneProgress(time, 13.6f, 14.8f);
        pose.spiritDamageTint = 1.0f - cutsceneProgress(time, 13.6f, 14.3f);
    }
    if (time >= 15.0f) {
        pose.invasionVisible = true;
        pose.scarabX = 1.15f - 0.43f * cutsceneProgress(time, 15.0f, 17.5f);
        pose.bruteX = 1.4f - 0.48f * cutsceneProgress(time, 15.3f, 17.8f);
        pose.firstLine = "GUARDIANS";
        pose.secondLine = "PROTECT THE TEMPLE";
    }
    return pose;
}


// 5. Text patterns and coordinate/layout helpers

// A small hand-defined 5x7 alphabet. Each row uses five bits, left to right.
const unsigned char* getLetterPattern(char letter) {
    static constexpr unsigned char alphabet[26][7] = {
        {14,17,17,31,17,17,17}, // A
        {30,17,17,30,17,17,30}, // B
        {14,17,16,16,16,17,14}, // C
        {30,17,17,17,17,17,30}, // D
        {31,16,16,30,16,16,31}, // E
        {31,16,16,30,16,16,16}, // F
        {14,17,16,23,17,17,14}, // G
        {17,17,17,31,17,17,17}, // H
        {31,4,4,4,4,4,31},     // I
        {7,2,2,2,18,18,12},    // J
        {17,18,20,24,20,18,17}, // K
        {16,16,16,16,16,16,31}, // L
        {17,27,21,21,17,17,17}, // M
        {17,25,21,19,17,17,17}, // N
        {14,17,17,17,17,17,14}, // O
        {30,17,17,30,16,16,16}, // P
        {14,17,17,17,21,18,13}, // Q
        {30,17,17,30,20,18,17}, // R
        {15,16,16,14,1,1,30},  // S
        {31,4,4,4,4,4,4},      // T
        {17,17,17,17,17,17,14}, // U
        {17,17,17,17,17,10,4},  // V
        {17,17,17,21,21,21,10}, // W
        {17,17,10,4,10,17,17},  // X
        {17,17,10,4,4,4,4},     // Y
        {31,1,2,4,8,16,31}     // Z
    };
    return letter >= 'A' && letter <= 'Z' ? alphabet[letter - 'A'] : nullptr;
}

bool mouseToGamePosition(double mouseX, double mouseY, int width, int height, glm::vec2& position) {
    if (width <= 0 || height <= 0) return false;
    if (mouseX < 0 || mouseX >= width || mouseY < 0 || mouseY >= height) return false;
    position.x = static_cast<float>(2 * mouseX / width - 1);
    position.y = static_cast<float>(1 - 2 * mouseY / height);
    return true;
}

bool overlapsHealthBar(glm::vec2 position, const std::vector<glm::vec2>& occupied) {
    for (glm::vec2 other : occupied) {
        bool sameArea = glm::abs(other.x - position.x) < 0.185f &&
                        glm::abs(other.y - position.y) < 0.03f;
        if (sameArea) return true;
    }
    return false;
}

// Find a nearby free position, above or below the character, for a 0.178 x 0.026 bar.
glm::vec2 findHealthBarPosition(glm::vec2 preferred, const std::vector<glm::vec2>& occupied) {
    const float columns[] = {0.0f, -0.19f, 0.19f, -0.38f, 0.38f, -0.57f, 0.57f, -0.76f, 0.76f};
    for (int attempt = 0; attempt < 80; ++attempt) {
        // Alternate candidate positions above and below the character.
        int row = attempt / 2;
        float y = preferred.y + row * 0.035f;
        if (attempt % 2 == 1) y = preferred.y - 0.34f - row * 0.035f;
        if (y < -0.64f || y > 0.65f) continue;

        for (float offset : columns) {
            glm::vec2 candidate(preferred.x + offset, y);
            if (candidate.x < -0.6f || candidate.x > 0.89f) continue;
            if (overlapsHealthBar(candidate, occupied)) continue;
            return candidate;
        }
    }
    return preferred;
}


// 6. Game state, purchasing, movement, combat, and waves

GameState gameState = GameState::MainMenu;
float cutsceneTime = 0.0f;
float worldAnimationTime = 0.0f;
bool selectedLanes[laneCount] = {};
int selectedCard = -1;
bool cardAvailable[playerCardCount] = {true, false, false};
bool hasCollectedCoins = false;
int coins = 30;
std::vector<Player> players;
std::vector<Enemy> enemies;
WaveProgress wave;
std::vector<Collectible> collectibles;
std::vector<Projectile> arrows;
std::vector<Projectile> highProjectiles;
std::vector<Projectile> lowProjectiles;

void refreshCardAvailability() {
    for (int card = 0; card < playerCardCount; ++card) {
        // The current economy interpretation enables advanced cards after the first pickup.
        cardAvailable[card] = (card == 0 || hasCollectedCoins) &&
                              coins >= cardCosts[card];
    }
    if (selectedCard >= 0 && !cardAvailable[selectedCard]) selectedCard = -1;
}

bool purchaseGuardian(int card, int lane) {
    if (gameState != GameState::Playing || card < 0 || card >= playerCardCount ||
        lane < 0 || lane >= laneCount) return false;
    refreshCardAvailability();
    if (!cardAvailable[card]) return false;
    float startY = fieldTop - (lane + 0.5f) * laneHeight;
    players.emplace_back(cardTypes[card], lane, fieldLeft + 0.07f, startY);
    coins -= cardCosts[card];
    selectedLanes[lane] = true;
    selectedCard = -1;
    refreshCardAvailability();
    return true;
}

static bool canPlayerEvade(const Player& player) {
    return player.health > 0 && player.type != PlayerType::ShieldGuardian &&
           player.lane >= 0 && player.lane < laneCount && selectedLanes[player.lane] &&
           player.action != CharacterAction::Jumping && player.action != CharacterAction::Ducking;
}

void startJump() {
    for (Player& player : players) {
        if (!canPlayerEvade(player)) continue;
        player.action = CharacterAction::Jumping;
        player.jumpTimer = 0.0f;
    }
}

void updateJumps(float deltaTime) {
    for (Player& player : players) {
        if (player.action != CharacterAction::Jumping) continue;
        if (player.health <= 0) {
            player.action = CharacterAction::Defeated;
            continue;
        }
        player.jumpTimer += deltaTime;
        if (player.jumpTimer >= jumpDuration) {
            player.jumpTimer = 0.0f;
            player.action = CharacterAction::Walking;
        }
    }
}

void startDuck() {
    for (Player& player : players) {
        if (!canPlayerEvade(player)) continue;
        player.action = CharacterAction::Ducking;
        player.duckTimer = 0.0f;
    }
}

void updateDucks(float deltaTime) {
    for (Player& player : players) {
        if (player.action != CharacterAction::Ducking) continue;
        if (player.health <= 0) {
            player.action = CharacterAction::Defeated;
            continue;
        }
        player.duckTimer += deltaTime;
        if (player.duckTimer >= duckDuration) {
            player.duckTimer = 0.0f;
            player.action = CharacterAction::Walking;
        }
    }
}

float getJumpHeight(const Player& player) {
    if (player.action != CharacterAction::Jumping) return 0.0f;
    return jumpHeight * std::sin(3.14159265f * player.jumpTimer / jumpDuration);
}

Player* findShieldProtector(const Player& ally, float attackX) {
    Player* protector = nullptr;
    for (Player& candidate : players) {
        if (candidate.health <= 0 || candidate.type != PlayerType::ShieldGuardian ||
            candidate.lane != ally.lane || &candidate == &ally) continue;
        // New units overlap at the entry point, so equality must count as shield protection.
        if (candidate.x >= ally.x && candidate.x <= attackX &&
            (!protector || candidate.x > protector->x)) {
            protector = &candidate;
        }
    }
    return protector;
}

void damagePlayer(Player& intendedTarget, int damage, float attackX) {
    Player* protector = findShieldProtector(intendedTarget, attackX);
    Player& receiver = protector ? *protector : intendedTarget;
    receiver.health = glm::max(0, receiver.health - damage);
    if (receiver.health == 0) receiver.action = CharacterAction::Defeated;
}

void resetGame() {
    cutsceneTime = 0.0f;
    worldAnimationTime = 0.0f;
    players.clear();
    enemies.clear();
    collectibles.clear();
    arrows.clear();
    highProjectiles.clear();
    lowProjectiles.clear();
    wave = WaveProgress{};
    coins = 30;
    hasCollectedCoins = false;
    selectedCard = -1;
    std::fill(std::begin(selectedLanes), std::end(selectedLanes), false);
    refreshCardAvailability();
}

void updateCutscene(float deltaTime) {
    if (gameState != GameState::Cutscene) return;
    cutsceneTime += deltaTime;
    if (cutsceneTime >= cutsceneDuration) {
        resetGame();
        gameState = GameState::Playing;
    }
}

void checkGameResult() {
    if (gameState != GameState::Playing) return;
    for (const Enemy& enemy : enemies) {
        if (enemy.health > 0 && enemy.x <= fieldLeft) {
            gameState = GameState::Defeat;
            return;
        }
    }
    if (wave.waveIndex >= waveCount && enemies.empty())
        gameState = GameState::Victory;
}

static void updateEnemyRangedAttacks(EnemyType type, std::vector<Projectile>& shots,
                                     float shotOffsetX, float shotOffsetY, float deltaTime) {
    const float attackRange = 0.65f;
    const float attackInterval = 1.2f;
    for (Enemy& enemy : enemies) {
        if (enemy.type != type) continue;
        if (enemy.health <= 0) {
            enemy.attackTimer = 0.0f;
            continue;
        }

        bool hasTarget = false;
        for (const Player& player : players) {
            if (player.health <= 0 || player.lane != enemy.lane) continue;
            float distance = enemy.x - player.x;
            bool inRange = distance >= 0.0f && distance <= attackRange;
            if (!inRange) continue;
            hasTarget = true;
            break;
        }
        if (!hasTarget) {
            enemy.attackTimer = 0.0f;
            continue;
        }

        enemy.attackTimer += deltaTime;
        while (enemy.attackTimer >= attackInterval) {
            enemy.attackTimer -= attackInterval;
            shots.push_back({enemy.lane, enemy.x + shotOffsetX, enemy.y + shotOffsetY});
        }
    }
}


void updateHighAttacks(float deltaTime) {
    updateEnemyRangedAttacks(EnemyType::SandSpirit, highProjectiles, -0.08f, 0.08f, deltaTime);
}

static void updateEnemyProjectiles(std::vector<Projectile>& shots,
                                   CharacterAction dodgeAction, float deltaTime) {
    const float projectileSpeed = 0.55f;
    const float playerHalfWidth = 0.04f;
    for (Projectile& projectile : shots) {
        float nextX = projectile.x - projectileSpeed * deltaTime;
        Player* target = nullptr;
        for (Player& player : players) {
            if (player.health <= 0 || player.lane != projectile.lane) continue;
            // A shield stays solid; moving guardians can dodge the matching shot.
            if (player.type != PlayerType::ShieldGuardian &&
                player.action == dodgeAction) continue;
            bool crossesPlayer = player.x - playerHalfWidth <= projectile.x &&
                                 player.x + playerHalfWidth >= nextX;
            if (!crossesPlayer) continue;
            if (target && player.x <= target->x) continue;
            target = &player;
        }
        if (target) {
            damagePlayer(*target, 1, projectile.x);
            projectile.active = false;
            continue;
        }
        projectile.x = nextX;
        if (projectile.x < fieldLeft - 0.2f) projectile.active = false;
    }
    for (int index = static_cast<int>(shots.size()) - 1; index >= 0; --index) {
        if (!shots[index].active) shots.erase(shots.begin() + index);
    }
}


void updateHighProjectiles(float deltaTime) {
    updateEnemyProjectiles(highProjectiles, CharacterAction::Ducking, deltaTime);
}

void updateLowAttacks(float deltaTime) {
    updateEnemyRangedAttacks(EnemyType::CursedScarab, lowProjectiles, -0.13f, -0.05f, deltaTime);
}

void updateLowProjectiles(float deltaTime) {
    updateEnemyProjectiles(lowProjectiles, CharacterAction::Jumping, deltaTime);
}

void updateArcherAttacks(float deltaTime) {
    const float attackRange = 0.75f;
    for (Player& player : players) {
        if (player.type != PlayerType::RoyalArcher) continue;
        if (player.health <= 0) {
            player.attackTimer = 0.0f;
            continue;
        }

        bool hasTarget = false;
        for (const Enemy& enemy : enemies) {
            if (enemy.health <= 0 || enemy.lane != player.lane) continue;
            float distance = enemy.x - player.x;
            bool inRange = distance >= 0.0f && distance <= attackRange;
            if (!inRange) continue;
            hasTarget = true;
            break;
        }
        if (!hasTarget) {
            player.attackTimer = 0.0f;
            continue;
        }

        player.attackTimer += deltaTime;
        while (player.attackTimer >= archerAttackInterval) {
            player.attackTimer -= archerAttackInterval;
            arrows.push_back({player.lane, player.x + 0.1f, player.y + 0.025f});
        }
    }
}

void updateArrows(float deltaTime) {
    const float arrowSpeed = 0.8f;
    const float enemyHalfWidth = 0.04f;
    for (Projectile& arrow : arrows) {
        float nextX = arrow.x + arrowSpeed * deltaTime;
        Enemy* target = nullptr;
        for (Enemy& enemy : enemies) {
            // Test the whole traveled segment so fast arrows cannot skip enemies.
            if (enemy.health <= 0 || enemy.lane != arrow.lane) continue;
            bool crossesEnemy = enemy.x + enemyHalfWidth >= arrow.x &&
                                enemy.x - enemyHalfWidth <= nextX;
            if (!crossesEnemy) continue;
            if (target && enemy.x >= target->x) continue;
            target = &enemy;
        }
        if (target) {
            target->health = glm::max(0, target->health - 1);
            if (target->health == 0) target->action = CharacterAction::Defeated;
            arrow.active = false;
            continue;
        }
        arrow.x = nextX;
        if (arrow.x > fieldRight + 0.2f) arrow.active = false;
    }
    for (int index = static_cast<int>(arrows.size()) - 1; index >= 0; --index) {
        if (!arrows[index].active) arrows.erase(arrows.begin() + index);
    }
}

void updateMeleeCombat(float deltaTime) {
    for (Player& player : players) {
        if (player.type != PlayerType::TempleWarrior) continue;
        if (player.health <= 0) {
            player.attackTimer = 0.0f;
            continue;
        }

        Enemy* target = nullptr;
        for (Enemy& enemy : enemies) {
            if (enemy.health <= 0 || enemy.lane != player.lane) continue;
            float distance = glm::abs(enemy.x - player.x);
            bool inRange = distance <= encounterDistance + 0.0001f;
            if (!inRange) continue;
            target = &enemy;
            break; // One opponent per swing.
        }
        if (!target) {
            player.attackTimer = 0.0f;
            continue;
        }

        player.attackTimer += deltaTime;
        while (player.attackTimer >= meleeAttackInterval && target->health > 0) {
            player.attackTimer -= meleeAttackInterval;
            target->health = glm::max(0, target->health - 1);
        }
        if (target->health > 0) continue;
        target->action = CharacterAction::Defeated;
        player.attackTimer = 0.0f;
    }

    for (Enemy& enemy : enemies) {
        if (enemy.type != EnemyType::JackalBrute) continue;
        if (enemy.health <= 0) {
            enemy.attackTimer = 0.0f;
            continue;
        }

        Player* target = nullptr;
        for (Player& player : players) {
            if (player.health <= 0 || player.lane != enemy.lane) continue;
            float distance = glm::abs(enemy.x - player.x);
            bool inRange = distance <= encounterDistance + 0.0001f;
            if (!inRange) continue;
            target = &player;
            break;
        }
        if (!target) {
            enemy.attackTimer = 0.0f;
            continue;
        }

        enemy.attackTimer += deltaTime;
        while (enemy.attackTimer >= bruteAttackInterval && target->health > 0) {
            enemy.attackTimer -= bruteAttackInterval;
            damagePlayer(*target, 2, enemy.x);
        }
        if (target->health > 0) continue;
        target->action = CharacterAction::Defeated;
        enemy.attackTimer = 0.0f;
    }
}

void updateCollectibles() {
    // Include the front of the model so a drop at the right edge is still reachable.
    const float guardianBackReach = 0.07f;
    const float guardianFrontReach = 0.13f;
    const float tokenHalfWidth = 0.035f;
    for (Collectible& collectible : collectibles) {
        for (const Player& player : players) {
            if (player.health <= 0 || player.lane != collectible.lane) continue;
            float left = glm::min(player.previousX, player.x) - guardianBackReach - tokenHalfWidth;
            float right = glm::max(player.previousX, player.x) + guardianFrontReach + tokenHalfWidth;
            bool withinReach = collectible.x >= left && collectible.x <= right;
            if (!withinReach) continue;
            coins += collectibleValue;
            hasCollectedCoins = true;
            collectible.collected = true;
            break; // Overlapping guardians cannot collect the same token twice.
        }
    }
    for (int index = static_cast<int>(collectibles.size()) - 1; index >= 0; --index) {
        if (collectibles[index].collected) collectibles.erase(collectibles.begin() + index);
    }
    refreshCardAvailability();
}

void removeDefeatedCharacters() {
    // Finish attacks before erasing: targets are pointers into these vectors.
    // Drop the coin before removing the enemy, so each enemy pays out once.
    for (const Enemy& enemy : enemies) {
        if (enemy.health <= 0)
            collectibles.push_back({enemy.lane, enemy.x, enemy.y});
    }
    for (int index = static_cast<int>(players.size()) - 1; index >= 0; --index) {
        if (players[index].health <= 0) players.erase(players.begin() + index);
    }
    for (int index = static_cast<int>(enemies.size()) - 1; index >= 0; --index) {
        if (enemies[index].health <= 0) enemies.erase(enemies.begin() + index);
    }
}

void updateCombatStates() {
    // Re-evaluate contact so survivors can move again after an opponent is removed.
    for (Player& player : players) {
        if (player.action == CharacterAction::Attacking)
            player.action = CharacterAction::Walking;
    }
    for (Enemy& enemy : enemies) {
        if (enemy.action == CharacterAction::Attacking)
            enemy.action = CharacterAction::Walking;
    }
    for (Player& player : players) {
        if (player.health <= 0) continue;
        for (Enemy& enemy : enemies) {
            if (enemy.health <= 0 || enemy.lane != player.lane) continue;
            float distance = glm::abs(enemy.x - player.x);
            bool inRange = distance <= encounterDistance + 0.0001f;
            if (!inRange) continue;
            enemy.action = CharacterAction::Attacking;
            if (player.action == CharacterAction::Ducking ||
                player.action == CharacterAction::Jumping) continue;
            player.action = player.type == PlayerType::ShieldGuardian
                ? CharacterAction::Defending : CharacterAction::Attacking;
        }
    }
}

void updatePlayers(float deltaTime) {
    // Tuning value in drawing units per second; the brief specifies no exact speed.
    const float movementSpeed = 0.12f;
    const float rightLimit = fieldRight - 0.14f;

    for (Player& player : players) {
        player.previousX = player.x;
        if (player.health <= 0) continue;
        if (player.type == PlayerType::ShieldGuardian) continue;
        if (player.action != CharacterAction::Walking) continue;

        float targetX = player.x + movementSpeed * deltaTime;
        for (const Enemy& enemy : enemies) {
            if (enemy.health > 0 && enemy.lane == player.lane && enemy.x >= player.x)
                targetX = glm::min(targetX, enemy.x - encounterDistance);
        }
        player.x = glm::max(player.x, targetX);
        // Keep the full model on screen. End-of-lane behavior is not specified.
        if (player.x >= rightLimit) {
            player.x = rightLimit;
            player.action = CharacterAction::Idle;
        }
        player.walkPhase = std::fmod(player.walkPhase + (player.x - player.previousX) * 65.0f, 6.2831853f);
    }
}

void updateEnemies(float deltaTime) {
    // Exact speeds are tuning values; easy/medium share a normal speed,
    // and the hard melee enemy runs faster as required by the brief.
    const float normalSpeed = 0.08f;
    const float bruteSpeed = 0.14f;

    for (Enemy& enemy : enemies) {
        if (enemy.health <= 0 || enemy.action != CharacterAction::Walking) continue;

        float speed = enemy.type == EnemyType::JackalBrute ? bruteSpeed : normalSpeed;
        float oldX = enemy.x;
        float targetX = enemy.x - speed * deltaTime;
        for (const Player& player : players) {
            if (player.health > 0 && player.lane == enemy.lane && player.x <= enemy.x)
                targetX = glm::max(targetX, player.x + encounterDistance);
        }
        enemy.x = glm::min(enemy.x, targetX);
        // checkGameResult() ends the game when a living enemy reaches this boundary.
        if (enemy.x <= fieldLeft) {
            enemy.x = fieldLeft;
            enemy.action = CharacterAction::Idle;
        }
        enemy.walkPhase = std::fmod(enemy.walkPhase + (oldX - enemy.x) * 75.0f, 6.2831853f);
    }
}

void updateWaves(std::mt19937& randomGenerator, float deltaTime) {
    if (wave.waveIndex >= waveCount) return;

    // The next wave waits until every enemy from this wave has been removed.
    if (wave.enemiesSpawned == waveEnemyCounts[wave.waveIndex]) {
        if (!enemies.empty()) return;
        ++wave.waveIndex;
        wave.enemiesSpawned = 0;
        wave.spawnTimer = 0.0f;
        if (wave.waveIndex == waveCount) return;
    }

    const float spawnInterval = 4.5f; // More time to reinforce lanes between enemies.
    std::uniform_int_distribution<int> randomLane(0, laneCount - 1);
    wave.spawnTimer += deltaTime;
    while (wave.enemiesSpawned < waveEnemyCounts[wave.waveIndex] &&
           (wave.enemiesSpawned == 0 || wave.spawnTimer >= spawnInterval)) {
        if (wave.enemiesSpawned > 0) wave.spawnTimer -= spawnInterval;
        EnemyType type = EnemyType::SandSpirit;
        // Alternating types guarantees the required mixture in every wave.
        if (wave.waveIndex == 1 && wave.enemiesSpawned % 2 == 1)
            type = EnemyType::CursedScarab;
        if (wave.waveIndex == 2) {
            const EnemyType types[] = {
                EnemyType::SandSpirit, EnemyType::CursedScarab, EnemyType::JackalBrute
            };
            type = types[wave.enemiesSpawned % 3];
        }
        int lane = randomLane(randomGenerator);
        float y = fieldTop - (lane + 0.5f) * laneHeight;
        enemies.emplace_back(type, lane, fieldRight, y);
        ++wave.enemiesSpawned;
    }
}

void updateGameStep(float deltaTime, std::mt19937& randomGenerator) {
    if (gameState != GameState::Playing) return;
    worldAnimationTime += deltaTime;
    updateJumps(deltaTime);
    updateDucks(deltaTime);
    updateCombatStates();
    updatePlayers(deltaTime);
    updateEnemies(deltaTime);
    checkGameResult();
    if (gameState != GameState::Playing) return;

    updateCombatStates();
    updateMeleeCombat(deltaTime);
    updateArcherAttacks(deltaTime);
    updateArrows(deltaTime);
    updateHighAttacks(deltaTime);
    updateHighProjectiles(deltaTime);
    updateLowAttacks(deltaTime);
    updateLowProjectiles(deltaTime);
    removeDefeatedCharacters();
    updateCollectibles();
    updateCombatStates();
    updateWaves(randomGenerator, deltaTime);
    checkGameResult();
}

void updateGame(float deltaTime, std::mt19937& randomGenerator) {
    if (gameState != GameState::Playing || !std::isfinite(deltaTime) || deltaTime <= 0.0f) return;
    // A slow frame must not let a projectile jump over a moving target.
    const float maximumStep = 1.0f / 60.0f;
    double remaining = deltaTime;
    while (remaining > 0.0 && gameState == GameState::Playing) {
        float step = glm::min(static_cast<float>(remaining), maximumStep);
        updateGameStep(step, randomGenerator);
        remaining -= step;
    }
}


// 7. Keyboard, mouse, and menu input

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (action != GLFW_PRESS && action != GLFW_REPEAT) return;

    // Volume keys work on every screen. '+' is Shift+'=' on most keyboards.
    if (key == GLFW_KEY_EQUAL || key == GLFW_KEY_KP_ADD) {
        audio::volumeUp();
        return;
    }
    if (key == GLFW_KEY_MINUS || key == GLFW_KEY_KP_SUBTRACT) {
        audio::volumeDown();
        return;
    }
    if (action != GLFW_PRESS) return; // Only volume keys repeat when held.
    if (key == GLFW_KEY_N) {
        audio::toggleMute();
        return;
    }

    if (key == GLFW_KEY_M) {
        resetGame();
        gameState = GameState::MainMenu;
        return;
    }

    switch (gameState) {
    case GameState::MainMenu:
        if (key == GLFW_KEY_ENTER) {
            resetGame();
            gameState = GameState::Cutscene;
        }
        if (key == GLFW_KEY_ESCAPE) glfwSetWindowShouldClose(window, true);
        break;
    case GameState::Cutscene:
        if (key == GLFW_KEY_ENTER || key == GLFW_KEY_SPACE) {
            resetGame();
            gameState = GameState::Playing;
        }
        break;
    case GameState::Playing:
        if (key == GLFW_KEY_W || key == GLFW_KEY_UP) startJump();
        if (key == GLFW_KEY_S || key == GLFW_KEY_DOWN) startDuck();
        if (key == GLFW_KEY_ESCAPE) gameState = GameState::Paused;
        break;
    case GameState::Paused:
        if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_ENTER)
            gameState = GameState::Playing;
        break;
    case GameState::Victory:
    case GameState::Defeat:
        if (key == GLFW_KEY_ENTER) {
            resetGame();
            gameState = GameState::Playing;
        }
        break;
    case GameState::SoundSettings:
        if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_ENTER)
            gameState = GameState::MainMenu;
        break;
    }
}

void framebufferSizeCallback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

bool getMousePosition(GLFWwindow* window, glm::vec2& position) {
    int width, height;
    glfwGetWindowSize(window, &width, &height);
    double mouseX, mouseY;
    glfwGetCursorPos(window, &mouseX, &mouseY);
    return mouseToGamePosition(mouseX, mouseY, width, height, position);
}

int menuButtonAtPosition(const glm::vec2& position, float centerX, float width,
                         const float buttonY[], int buttonCount) {
    if (glm::abs(position.x - centerX) > width * 0.5f) return -1;
    for (int button = 0; button < buttonCount; ++button) {
        if (glm::abs(position.y - buttonY[button]) <= menuButtonHeight * 0.5f)
            return button;
    }
    return -1;
}

int mainMenuButtonAtPosition(const glm::vec2& position) {
    return menuButtonAtPosition(position, menuCenterX, menuButtonWidth, mainMenuButtonY, 3);
}

int soundMenuButtonAtPosition(const glm::vec2& position) {
    return menuButtonAtPosition(position, 0.0f, pauseButtonWidth, soundButtonY, 4);
}

void activateSoundMenuButton(int button) {
    if (gameState != GameState::SoundSettings) return;
    if (button == 0) audio::volumeUp();
    if (button == 1) audio::volumeDown();
    if (button == 2) audio::toggleMute();
    if (button == 3) gameState = GameState::MainMenu;
}

int pauseMenuButtonAtPosition(const glm::vec2& position) {
    return menuButtonAtPosition(position, 0.0f, pauseButtonWidth, pauseButtonY, 3);
}

int resultMenuButtonAtPosition(const glm::vec2& position) {
    return menuButtonAtPosition(position, 0.0f, pauseButtonWidth, menuButtonY, 2);
}

void activatePauseMenuButton(int button) {
    if (gameState != GameState::Paused) return;
    if (button == 0) {
        gameState = GameState::Playing;
        return;
    }
    if (button == 1) {
        resetGame();
        gameState = GameState::Playing;
        return;
    }
    if (button == 2) {
        resetGame();
        gameState = GameState::MainMenu;
    }
}

void activateResultMenuButton(int button) {
    if (gameState != GameState::Victory && gameState != GameState::Defeat) return;
    if (button == 0) {
        resetGame();
        gameState = GameState::Playing;
        return;
    }
    if (button == 1) {
        resetGame();
        gameState = GameState::MainMenu;
    }
}

int cardAtPosition(const glm::vec2& position) {
    if (position.y < cardY - cardHeight * 0.5f ||
        position.y > cardY + cardHeight * 0.5f) return -1;
    for (int card = 0; card < playerCardCount; ++card) {
        float centerX = cardFirstX + card * cardSpacing;
        if (position.x >= centerX - cardWidth * 0.5f &&
            position.x <= centerX + cardWidth * 0.5f) return card;
    }
    return -1;
}

int laneAtPosition(const glm::vec2& position) {
    if (position.x < fieldLeft || position.x >= fieldRight ||
        position.y > fieldTop || position.y <= fieldBottom)
        return -1;

    int lane = static_cast<int>((fieldTop - position.y) / laneHeight);
    return (lane >= 0 && lane < laneCount) ? lane : -1;
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
        return;

    glm::vec2 position;
    if (!getMousePosition(window, position)) return;
    if (gameState == GameState::MainMenu) {
        int menuButton = mainMenuButtonAtPosition(position);
        if (menuButton != -1) audio::playEffect(SoundEffect::MenuClick);
        if (menuButton == 0) {
            resetGame();
            gameState = GameState::Cutscene;
        } else if (menuButton == 1) {
            gameState = GameState::SoundSettings;
        } else if (menuButton == 2) {
            glfwSetWindowShouldClose(window, true);
        }
        return;
    }
    if (gameState == GameState::SoundSettings) {
        int menuButton = soundMenuButtonAtPosition(position);
        if (menuButton != -1) audio::playEffect(SoundEffect::MenuClick);
        activateSoundMenuButton(menuButton);
        return;
    }
    if (gameState == GameState::Paused) {
        int menuButton = pauseMenuButtonAtPosition(position);
        if (menuButton != -1) audio::playEffect(SoundEffect::MenuClick);
        activatePauseMenuButton(menuButton);
        return;
    }
    if (gameState == GameState::Victory || gameState == GameState::Defeat) {
        int menuButton = resultMenuButtonAtPosition(position);
        if (menuButton != -1) audio::playEffect(SoundEffect::MenuClick);
        activateResultMenuButton(menuButton);
        return;
    }
    if (gameState != GameState::Playing) return;
    int card = cardAtPosition(position);
    if (card != -1) {
        if (cardAvailable[card]) {
            selectedCard = card;
            audio::playEffect(SoundEffect::CardSelect);
        }
        return; // Card clicks must not change lane selection.
    }
    int lane = laneAtPosition(position);
    if (lane == -1) return;

    if (selectedCard != -1) {
        if (purchaseGuardian(selectedCard, lane))
            audio::playEffect(SoundEffect::GuardianPlaced);
        return;
    }
    selectedLanes[lane] = !selectedLanes[lane];
    audio::playEffect(SoundEffect::LaneSelect);
}


// 8. Desert and temple scenery

// Scenery coordinates are local to each object's ground position.
struct SceneryParts {
    float x, groundY, scale;
    void drawPart(float offsetX, float offsetY, float width, float height, float angle,
              float red, float green, float blue) const {
        drawRectangle(x + offsetX * scale, groundY + offsetY * scale,
                      width * scale, height * scale, angle, red, green, blue);
    }
};

void drawDesertBackdrop() {
    // Sky color changes from dark blue at the top to warm colors at the horizon.
    for (int band = 0; band < 64; ++band) {
        float t = band / 63.0f;
        glm::vec3 shade = glm::mix(glm::vec3(.11f, .14f, .23f),
                                  glm::vec3(.53f, .30f, .24f), t);
        drawRectangle(0, 1.0f - (band + .5f) / 64 * 1.2f,
                      2, 1.2f / 64 + .001f, 0, shade.r, shade.g, shade.b);
    }
    drawCircle(.72f, .48f, .30f, glm::vec3(.91f, .61f, .32f));
    // Two triangles give each pyramid a lit face and a darker face.
    for (int pyramid = 0; pyramid < 2; ++pyramid) {
        float x = pyramid == 0 ? .36f : -.23f;
        float height = pyramid == 0 ? .48f : .32f;
        float halfWidth = height * .7f;
        float centerY = .02f + height * .5f;
        drawTriangle(x - halfWidth * .5f, centerY, halfWidth, height, glm::vec3(.44f, .30f, .23f));
        drawTriangle(x + halfWidth * .5f, centerY, -halfWidth, height, glm::vec3(.31f, .24f, .23f));
    }
    // Distant dunes.
    for (int layer = 0; layer < 3; ++layer) {
        for (int strip = 0; strip < 100; ++strip) {
            float x = -1 + (strip + .5f) * .02f;
            float crest = .08f - layer * .22f + .055f * std::sin(x * 4 + layer * 2);
            drawRectangle(x, (crest - 1) * .5f,
                          .021f, crest + 1, 0, .44f - layer * .055f,
                          .32f - layer * .045f, .23f - layer * .03f);
        }
    }
}

void drawObelisk(float x, float groundY, float scale = 1) {
    SceneryParts scenery{x, groundY, scale};
    scenery.drawPart(0, .015f, .11f, .03f, 0, .55f, .38f, .2f);
    scenery.drawPart(0, .12f, .055f, .19f, 0, .86f, .67f, .4f);
    scenery.drawPart(.018f, .12f, .018f, .19f, 0, .63f, .44f, .24f);
    scenery.drawPart(0, .22f, .039f, .039f, 45, .97f, .78f, .3f);
    for (int mark = 0; mark < 4; ++mark) {
        float y = .065f + mark * .035f;
        scenery.drawPart(-.005f, y, .018f, .005f, 0, .35f, .25f, .15f);
        scenery.drawPart(-.005f, y + .007f, .005f, .015f, 0, .35f, .25f, .15f);
    }
}

void drawCourtyardColumn(float x, float groundY, float scale = 1) {
    SceneryParts scenery{x, groundY, scale};
    scenery.drawPart(0, .015f, .12f, .03f, 0, .7f, .51f, .28f);
    scenery.drawPart(0, .12f, .065f, .18f, 0, .84f, .68f, .44f);
    scenery.drawPart(-.015f, .12f, .009f, .17f, 0, .96f, .81f, .56f);
    scenery.drawPart(.02f, .12f, .012f, .17f, 0, .59f, .42f, .23f);
    scenery.drawPart(0, .21f, .095f, .025f, 0, .2f, .43f, .49f);
    scenery.drawPart(0, .23f, .125f, .018f, 0, .9f, .7f, .3f);
    for (int band = 0; band < 2; ++band)
        scenery.drawPart(0, .065f + band * .095f, .07f, .012f, 0, .2f, .43f, .49f);
}

void drawPalm(float x, float groundY,
                     float scale = 1, float sway = 0) {
    SceneryParts scenery{x, groundY, scale};
    scenery.drawPart(0, .09f, .027f, .18f, -4, .53f, .34f, .18f);
    for (int ring = 0; ring < 5; ++ring)
        scenery.drawPart(0, .025f + ring * .03f, .033f, .006f, -4, .74f, .5f, .25f);
    for (int side = -1; side <= 1; side += 2) {
        scenery.drawPart(side * .05f, .195f, .12f, .018f, side * 18.0f + sway, .28f, .48f, .25f);
        scenery.drawPart(side * .055f, .17f, .12f, .019f, -side * 20.0f + sway, .21f, .39f, .22f);
        scenery.drawPart(side * .025f, .22f, .08f, .016f, side * 48.0f + sway, .37f, .55f, .27f);
    }
    scenery.drawPart(0, .174f, .025f, .025f, 45, .48f, .29f, .13f);
}

void drawCourtyardTorch(float x, float groundY,
                               float flameScale = 1, float scale = 1) {
    SceneryParts scenery{x, groundY, scale};
    scenery.drawPart(0, .012f, .08f, .024f, 0, .53f, .38f, .22f);
    scenery.drawPart(0, .075f, .023f, .12f, 0, .42f, .28f, .16f);
    scenery.drawPart(0, .072f, .03f, .012f, 0, .93f, .7f, .28f);
    scenery.drawPart(0, .14f, .075f, .026f, 0, .64f, .41f, .2f);
    scenery.drawPart(0, .157f, .095f, .012f, 0, .94f, .71f, .29f);
    scenery.drawPart(0, .17f + .02f * flameScale, .044f, .055f * flameScale, 15, .92f, .32f, .08f);
    scenery.drawPart(.006f, .17f + .042f * flameScale, .027f, .04f * flameScale, -18, 1, .65f, .13f);
    scenery.drawPart(0, .17f + .017f * flameScale, .018f, .035f * flameScale, 0, 1, .9f, .46f);
}

void drawPottery(float x, float groundY, float scale = 1) {
    SceneryParts scenery{x, groundY, scale};
    for (int side = -1; side <= 1; side += 2) {
        scenery.drawPart(side * .043f, .085f, .035f, .052f, side * 12.0f, .62f, .31f, .19f);
        scenery.drawPart(side * .045f, .086f, .016f, .032f, side * 12.0f, .42f, .31f, .2f);
    }
    scenery.drawPart(0, .062f, .082f, .095f, 0, .72f, .4f, .25f);
    scenery.drawPart(0, .012f, .055f, .024f, 0, .52f, .28f, .17f);
    scenery.drawPart(0, .12f, .041f, .045f, 0, .72f, .4f, .25f);
    scenery.drawPart(0, .143f, .062f, .013f, 0, .9f, .64f, .38f);
    scenery.drawPart(0, .15f, .043f, .006f, 0, .24f, .15f, .1f);
    for (int band = 0; band < 2; ++band)
        scenery.drawPart(0, .04f + band * .045f, .084f, .01f, 0, .2f, .4f, .47f);
    scenery.drawPart(0, .063f, .019f, .019f, 45, .96f, .76f, .35f);
}

void drawEnvironment(float animationTime = 0,
                            bool sidePath = true) {
    drawRectangle(.12f, -.85f, 1.55f, .28f, 0, .42f, .31f, .2f);
    for (int stone = 0; stone < 8; ++stone)
        drawRectangle(-.57f + stone * .2f, -.85f, .003f, .26f, 0, .37f, .27f, .18f);
    drawRectangle(.12f, -.985f, 1.55f, .012f, 0, .63f, .46f, .26f);
    float flame = 1 + .12f * std::sin(animationTime * 6);
    float sway = 3 * std::sin(animationTime * 1.5f);
    // Gate columns and pottery.
    drawObelisk(-.57f, -.96f);
    drawCourtyardColumn(-.37f, -.97f, .88f);
    drawCourtyardTorch(-.66f, -.96f, flame, .8f);
    drawPottery(-.45f, -.985f, .75f);
    drawPottery(-.27f, -.98f, .6f);
    // Palms near the entrance.
    drawPalm(.61f, -.94f, .88f, sway);
    drawPalm(.83f, -.97f, 1, sway);
    drawPottery(.72f, -.985f, .8f);
    drawCourtyardTorch(.31f, -.96f, flame, .8f);
    // Keep the side decorations outside the lanes.
    if (!sidePath) return;
    drawRectangle(.944f, -.035f, .105f, 1.43f, 0, .31f, .25f, .18f);
    for (int stone = 0; stone < 6; ++stone)
        drawRectangle(.944f, -.68f + stone * .25f,
                      .1f, .004f, 0, .4f, .32f, .22f);
    drawCourtyardColumn(.945f, -.64f, .55f);
    drawObelisk(.945f, -.12f, .55f);
    drawPalm(.93f, .53f, .48f, sway);
}


// 9. Shaders, shapes, characters, and screen drawing

const char* vertexShaderSrc = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 u_Transform;
out vec2 v_Local;
void main() {
    gl_Position = u_Transform * vec4(aPos, 1.0);
    v_Local = aPos.xy + vec2(0.5);
}
)";

const char* fragmentShaderSrc = R"(
#version 330 core
out vec4 FragColor;
uniform vec3 u_Color;
uniform bool u_Grayscale;
uniform float u_DamageAmount;
uniform bool u_Outline;
uniform float u_Opacity;
in vec2 v_Local;
void main() {
    vec3 color = mix(u_Color, vec3(1.0, 0.05, 0.05), u_DamageAmount * 0.8);
    float edge = min(min(v_Local.x, 1.0 - v_Local.x), min(v_Local.y, 1.0 - v_Local.y));
    if (u_Outline && edge < 0.075) color *= 0.32;
    if (u_Grayscale) {
        float gray = (color.r + color.g + color.b) / 3.0;
        color = vec3(gray);
    }
    FragColor = vec4(color, u_Opacity);
}
)";

// Filled by initializeDrawing() after OpenGL setup.
static GLuint shapeVAO, activeShaderProgram;
static GLint colorLocation, transformLocation, grayscaleLocation, damageLocation;
static GLint outlineLocation = -1;
static GLint opacityLocation = -1;
static float drawingOpacity = 1.0f;

void initializeDrawing(GLuint vao, GLuint shaderProgram) {
    shapeVAO = vao;
    activeShaderProgram = shaderProgram;
    colorLocation = glGetUniformLocation(shaderProgram, "u_Color");
    transformLocation = glGetUniformLocation(shaderProgram, "u_Transform");
    grayscaleLocation = glGetUniformLocation(shaderProgram, "u_Grayscale");
    damageLocation = glGetUniformLocation(shaderProgram, "u_DamageAmount");
    outlineLocation = glGetUniformLocation(shaderProgram, "u_Outline");
    opacityLocation = glGetUniformLocation(shaderProgram, "u_Opacity");
}

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char log[512];
        glGetShaderInfoLog(shader, 512, nullptr, log);
        std::cerr << "Shader compile error:\n" << log << "\n";
    }
    return shader;
}

static void drawShape(GLenum primitive, int first, int count,
                      float x, float y, float width, float height, float angle,
                      glm::vec3 color, bool outline = false) {
    glm::mat4 transform(1.0f);
    transform = glm::translate(transform, glm::vec3(x, y, 0.0f));
    transform = glm::rotate(transform, glm::radians(angle), glm::vec3(0.0f, 0.0f, 1.0f));
    transform = glm::scale(transform, glm::vec3(width, height, 1.0f));
    glUniformMatrix4fv(transformLocation, 1, GL_FALSE, glm::value_ptr(transform));
    glUniform3f(colorLocation, color.r, color.g, color.b);
    glUniform1i(outlineLocation, outline ? GL_TRUE : GL_FALSE);
    glUniform1f(opacityLocation, drawingOpacity);
    glBindVertexArray(shapeVAO);
    glDrawArrays(primitive, first, count);
}

void drawRectangle(float x, float y, float width, float height, float angle,
                   float red, float green, float blue, bool outline) {
    drawShape(GL_TRIANGLES, 0, 6, x, y, width, height, angle, glm::vec3(red, green, blue), outline);
}

void drawTriangle(float x, float y, float width, float height, glm::vec3 color) {
    drawShape(GL_TRIANGLES, 6, 3, x, y, width, height, 0, color);
}

void drawCircle(float x, float y, float diameter, glm::vec3 color) {
    drawShape(GL_TRIANGLE_FAN, 9, circleSegments + 2, x, y, diameter, diameter, 0, color);
}

void drawText(const char* text, float centerX, float centerY, float pixelSize,
              glm::vec3 color) {
    int length = 0;
    while (text[length] != '\0') ++length;
    float left = centerX - (length * 6 - 1) * pixelSize * 0.5f;
    for (int letter = 0; letter < length; ++letter) {
        const unsigned char* rows = getLetterPattern(text[letter]);
        if (!rows) continue; // Spaces leave a gap.
        for (int row = 0; row < 7; ++row) {
            for (int column = 0; column < 5; ++column) {
                if ((rows[row] & (1 << (4 - column))) == 0) continue;
                drawRectangle(left + (letter * 6 + column + 0.5f) * pixelSize,
                              centerY + (3 - row) * pixelSize,
                              pixelSize * 0.92f, pixelSize * 0.92f, 0.0f,
                              color.r, color.g, color.b);
            }
        }
    }
}

void drawMenuButton(const char* label, float x, float y, float width, bool hovered) {
    glm::vec3 border = hovered
        ? glm::vec3(1.0f, 0.84f, 0.35f) : glm::vec3(0.62f, 0.45f, 0.22f);
    glm::vec3 fill = hovered
        ? glm::vec3(0.22f, 0.36f, 0.43f) : glm::vec3(0.12f, 0.2f, 0.27f);
    drawRectangle(x, y, width, menuButtonHeight, 0.0f, border.r, border.g, border.b);
    drawRectangle(x, y, width - 0.025f, menuButtonHeight - 0.025f,
                  0.0f, fill.r, fill.g, fill.b);
    // Long labels shrink to fit inside the button; shorter ones keep the usual size.
    int length = 0;
    while (label[length] != '\0') ++length;
    float textSize = 0.011f;
    float textWidth = (length * 6 - 1) * textSize;
    float maxWidth = width - 0.07f;
    if (textWidth > maxWidth) textSize *= maxWidth / textWidth;
    drawText(label, x, y, textSize, glm::vec3(1.0f, 0.93f, 0.72f));
}

// Parts are positioned relative to the character currently being drawn.
static float characterOriginX, characterOriginY, characterScale, characterHeightScale, characterPartAngle;
static glm::mat4 characterPartRotation(1.0f);

static void setCharacterOrigin(float x, float y, float scale = 1.0f, float heightScale = 1.0f) {
    characterOriginX = x;
    characterOriginY = y;
    characterScale = scale;
    characterHeightScale = heightScale;
    characterPartAngle = 0.0f;
    characterPartRotation = glm::mat4(1.0f);
}

static void rotateCharacterParts(float pivotX, float pivotY, float angle) {
    characterPartAngle = angle;
    characterPartRotation = glm::translate(glm::mat4(1.0f), glm::vec3(pivotX, pivotY, 0));
    characterPartRotation = glm::rotate(characterPartRotation, glm::radians(angle), glm::vec3(0, 0, 1));
    characterPartRotation = glm::translate(characterPartRotation, glm::vec3(-pivotX, -pivotY, 0));
}

static void drawPart(float x, float y, float width, float height, float angle, glm::vec3 color) {
    glm::vec4 position = characterPartRotation * glm::vec4(x, y, 0, 1);
    drawRectangle(characterOriginX + position.x * characterScale, characterOriginY + position.y * characterScale * characterHeightScale,
                  width * characterScale, height * characterScale * characterHeightScale, angle + characterPartAngle,
                  color.r, color.g, color.b, width >= 0.014f && height >= 0.014f);
}

static void drawPanelRectangle(float x, float y, float width, float height, glm::vec3 color, float angle = 0) {
    drawRectangle(x, y, width, height, angle, color.r, color.g, color.b);
}

void drawTempleWarrior(const Player& player, float scale = 1.0f, float walkPhase = 0.0f) {
    float legSwingAngle = 14.0f * std::sin(walkPhase);
    float heightScale = player.action == CharacterAction::Ducking ? duckHeightScale : 1.0f;
    float swing = player.action == CharacterAction::Attacking
        || (player.health > 0 && player.attackTimer > 0.0f)
        ? -55.0f * std::sin(3.14159265f * player.attackTimer / meleeAttackInterval) : 0.0f;
    setCharacterOrigin(player.x, player.y, scale, heightScale);

    const glm::vec3 skin(0.8f, 0.55f, 0.34f);
    const glm::vec3 linen(0.94f, 0.88f, 0.7f);
    const glm::vec3 blue(0.12f, 0.33f, 0.57f);
    const glm::vec3 gold(0.94f, 0.7f, 0.22f);
    const glm::vec3 bronze(0.65f, 0.39f, 0.12f);

    // Legs, sandals, and tunic.
    drawPart(-0.018f, -0.075f, 0.02f, 0.07f, -5.0f + legSwingAngle, skin);
    drawPart(0.018f, -0.075f, 0.02f, 0.07f, 5.0f - legSwingAngle, skin);
    drawPart(-0.015f + legSwingAngle * 0.0004f, -0.112f + 0.007f * glm::max(0.0f, std::sin(walkPhase)),
         0.035f, 0.012f, 0.0f, bronze);
    drawPart(0.022f - legSwingAngle * 0.0004f, -0.112f + 0.007f * glm::max(0.0f, -std::sin(walkPhase)),
         0.035f, 0.012f, 0.0f, bronze);
    drawPart(0.0f, -0.022f, 0.075f, 0.065f, 0.0f, linen);
    drawPart(0.0f, 0.005f, 0.06f, 0.06f, 0.0f, linen);
    drawPart(0.0f, -0.02f, 0.078f, 0.012f, 0.0f, gold);
    drawPart(0.0f, -0.022f, 0.012f, 0.018f, 0.0f, blue);

    // Arms and wrist guards.
    drawPart(-0.041f, 0.005f, 0.018f, 0.065f, -12.0f - legSwingAngle * 0.5f, skin);
    drawPart(0.044f, 0.011f, 0.018f, 0.06f, 30.0f, skin);
    drawPart(-0.047f, -0.017f, 0.021f, 0.017f, -12.0f, gold);
    drawPart(0.058f, -0.009f, 0.022f, 0.017f, 30.0f, gold);

    // Head and collar.
    drawPart(0.0f, 0.044f, 0.022f, 0.025f, 0.0f, skin);
    drawPart(-0.006f, 0.079f, 0.064f, 0.075f, 0.0f, blue);
    drawPart(-0.032f, 0.047f, 0.018f, 0.065f, -12.0f, blue);
    drawPart(0.007f, 0.074f, 0.041f, 0.047f, 0.0f, skin);
    drawPart(0.031f, 0.072f, 0.012f, 0.013f, 0.0f, skin);
    drawPart(0.0f, 0.103f, 0.063f, 0.012f, 0.0f, gold);
    drawPart(0.019f, 0.083f, 0.007f, 0.007f, 0.0f, glm::vec3(0.1f));
    drawPart(0.0f, 0.032f, 0.069f, 0.018f, 0.0f, gold);
    drawPart(0.0f, 0.029f, 0.041f, 0.009f, 0.0f, blue);

    // Rotate the sword around the hand, rather than around its own center.
    rotateCharacterParts(0.068f, 0.0f, swing);
    drawPart(0.068f, 0.0f, 0.012f, 0.035f, -12.0f, bronze);
    drawPart(0.072f, 0.018f, 0.032f, 0.009f, -12.0f, gold);
    drawPart(0.08f, 0.043f, 0.012f, 0.05f, -18.0f, gold);
    drawPart(0.096f, 0.071f, 0.012f, 0.032f, -48.0f, gold);
    drawPart(0.11f, 0.09f, 0.013f, 0.028f, -18.0f, gold);
    drawPart(0.107f, 0.109f, 0.009f, 0.018f, 20.0f, linen);
}

void drawRoyalArcher(const Player& player, float scale = 1.0f, float walkPhase = 0.0f) {
    float legSwingAngle = 14.0f * std::sin(walkPhase);
    float heightScale = player.action == CharacterAction::Ducking ? duckHeightScale : 1.0f;
    setCharacterOrigin(player.x, player.y, scale, heightScale);

    const glm::vec3 skin(0.8f, 0.55f, 0.34f);
    const glm::vec3 linen(0.94f, 0.88f, 0.7f);
    const glm::vec3 gold(0.94f, 0.7f, 0.22f);
    const glm::vec3 leather(0.35f, 0.2f, 0.11f);

    // Draw the quiver behind the body.
    drawPart(-0.035f, 0.025f, 0.027f, 0.085f, -15.0f, leather);
    for (int arrow = 0; arrow < 3; ++arrow) {
        float arrowX = -0.05f + arrow * 0.012f;
        drawPart(arrowX, 0.083f, 0.004f, 0.055f, -15.0f, gold);
        drawPart(arrowX - 0.006f, 0.109f, 0.009f, 0.014f, -15.0f, linen);
    }

    drawPart(-0.014f, -0.075f, 0.017f, 0.07f, -4.0f + legSwingAngle, skin);
    drawPart(0.014f, -0.075f, 0.017f, 0.07f, 4.0f - legSwingAngle, skin);
    drawPart(-0.012f + legSwingAngle * 0.0004f, -0.112f + 0.007f * glm::max(0.0f, std::sin(walkPhase)),
         0.029f, 0.012f, 0.0f, leather);
    drawPart(0.018f - legSwingAngle * 0.0004f, -0.112f + 0.007f * glm::max(0.0f, -std::sin(walkPhase)),
         0.029f, 0.012f, 0.0f, leather);
    drawPart(0.0f, -0.025f, 0.055f, 0.06f, 0.0f, linen);
    drawPart(0.0f, 0.012f, 0.042f, 0.058f, 0.0f, linen);
    drawPart(0.0f, -0.019f, 0.057f, 0.009f, 0.0f, leather);
    drawPart(0.0f, 0.015f, 0.009f, 0.065f, -25.0f, leather);

    // Head and headband.
    drawPart(0.0f, 0.046f, 0.018f, 0.02f, 0.0f, skin);
    drawPart(0.0f, 0.08f, 0.042f, 0.05f, 0.0f, skin);
    drawPart(0.0f, 0.103f, 0.047f, 0.016f, 0.0f, leather);
    drawPart(0.0f, 0.094f, 0.05f, 0.009f, 0.0f, gold);
    drawPart(0.023f, 0.078f, 0.012f, 0.012f, 0.0f, skin);
    drawPart(0.013f, 0.085f, 0.006f, 0.006f, 0.0f, glm::vec3(0.1f));
    drawPart(0.0f, 0.038f, 0.048f, 0.012f, 0.0f, gold);

    // Arms holding the bow.
    drawPart(0.05f, 0.027f, 0.075f, 0.015f, -8.0f, skin);
    drawPart(-0.027f, 0.015f, 0.017f, 0.04f, -25.0f, skin);
    drawPart(-0.012f, 0.001f, 0.04f, 0.014f, 10.0f, skin);

    // Four connected segments approximate a bow curved toward the right.
    drawPart(0.092f, 0.055f, 0.01f, 0.07f, 12.0f, leather);
    drawPart(0.074f, 0.108f, 0.01f, 0.05f, 30.0f, leather);
    drawPart(0.092f, -0.01f, 0.01f, 0.07f, -12.0f, leather);
    drawPart(0.074f, -0.063f, 0.01f, 0.05f, -30.0f, leather);
    drawPart(0.06f, 0.023f, 0.003f, 0.21f, 0.0f, linen);
    drawPart(0.098f, 0.023f, 0.014f, 0.025f, 0.0f, gold);
    // The nocked arrow is part of the pose; fired arrows are drawn separately.
    drawPart(0.065f, 0.025f, 0.14f, 0.004f, 0.0f, gold);
    drawPart(0.132f, 0.025f, 0.012f, 0.012f, 45.0f, linen);
}

void drawShieldGuardian(const Player& player, float scale = 1.0f) {
    setCharacterOrigin(player.x, player.y, scale);

    const glm::vec3 skin(0.8f, 0.55f, 0.34f);
    const glm::vec3 linen(0.94f, 0.88f, 0.7f);
    const glm::vec3 gold(0.94f, 0.7f, 0.22f);
    const glm::vec3 bronze(0.52f, 0.33f, 0.14f);
    const glm::vec3 blue(0.12f, 0.33f, 0.57f);

    // Legs and armor.
    drawPart(-0.028f, -0.082f, 0.027f, 0.065f, -8.0f, skin);
    drawPart(0.028f, -0.082f, 0.027f, 0.065f, 8.0f, skin);
    drawPart(-0.031f, -0.115f, 0.044f, 0.015f, 0.0f, bronze);
    drawPart(0.035f, -0.115f, 0.044f, 0.015f, 0.0f, bronze);
    drawPart(0.0f, -0.035f, 0.092f, 0.06f, 0.0f, linen);
    drawPart(0.0f, 0.007f, 0.088f, 0.074f, 0.0f, bronze);
    drawPart(0.0f, 0.012f, 0.066f, 0.06f, 0.0f, gold);
    drawPart(0.0f, 0.012f, 0.038f, 0.037f, 0.0f, blue);
    drawPart(0.0f, -0.026f, 0.095f, 0.012f, 0.0f, gold);
    drawPart(-0.052f, 0.03f, 0.038f, 0.03f, -15.0f, gold);
    drawPart(0.052f, 0.03f, 0.038f, 0.03f, 15.0f, gold);
    drawPart(-0.054f, -0.005f, 0.023f, 0.055f, -8.0f, skin);
    drawPart(-0.057f, -0.025f, 0.028f, 0.025f, 0.0f, bronze);

    // Helmet and face.
    drawPart(0.0f, 0.05f, 0.025f, 0.02f, 0.0f, skin);
    drawPart(0.0f, 0.082f, 0.062f, 0.06f, 0.0f, bronze);
    drawPart(0.009f, 0.077f, 0.038f, 0.038f, 0.0f, skin);
    drawPart(0.03f, 0.076f, 0.012f, 0.014f, 0.0f, skin);
    drawPart(-0.023f, 0.073f, 0.014f, 0.055f, 0.0f, gold);
    drawPart(0.0f, 0.107f, 0.07f, 0.016f, 0.0f, gold);
    drawPart(0.0f, 0.125f, 0.024f, 0.025f, 0.0f, blue);
    drawPart(0.019f, 0.085f, 0.007f, 0.006f, 0.0f, glm::vec3(0.1f));

    // Forearm and shield are in front of the body, facing incoming enemies.
    drawPart(0.055f, 0.0f, 0.052f, 0.024f, 0.0f, skin);
    drawPart(0.085f, -0.005f, 0.09f, 0.19f, 0.0f, bronze);
    drawPart(0.085f, -0.005f, 0.075f, 0.173f, 0.0f, gold);
    drawPart(0.085f, -0.005f, 0.058f, 0.15f, 0.0f, blue);
    drawPart(0.085f, -0.005f, 0.012f, 0.145f, 0.0f, gold);
    drawPart(0.085f, -0.005f, 0.056f, 0.012f, 0.0f, gold);
    drawPart(0.085f, -0.005f, 0.028f, 0.028f, 45.0f, linen);
    for (int end = -1; end <= 1; end += 2) {
        drawPart(0.085f, -0.005f + end * 0.065f, 0.025f, 0.009f, 0.0f, linen);
    }
}

void drawPlayerCards(int hoveredCard) {
    const char* names[] = {"WARRIOR", "ARCHER", "SHIELD"};
    const glm::vec3 accents[] = {
        {0.85f, 0.48f, 0.23f}, {0.28f, 0.65f, 0.61f}, {0.49f, 0.52f, 0.78f}
    };
    for (int card = 0; card < playerCardCount; ++card) {
        bool available = cardAvailable[card];
        bool selected = available && selectedCard == card;
        bool hovered = available && hoveredCard == card;
        glUniform1i(grayscaleLocation, available ? GL_FALSE : GL_TRUE);
        float x = cardFirstX + card * cardSpacing;
        glm::vec3 accent = accents[card];
        glm::vec3 border = selected ? glm::vec3(1.0f, 0.8f, 0.24f)
            : hovered ? glm::vec3(0.85f, 0.72f, 0.43f) : glm::vec3(0.46f, 0.35f, 0.23f);
        drawRectangle(x, cardY, cardWidth, cardHeight, 0.0f,
                      border.r, border.g, border.b);
        drawRectangle(x, cardY, cardWidth - 0.012f, cardHeight - 0.012f, 0.0f,
                      0.075f, 0.11f, 0.15f);
        glm::vec3 panel = accent * (hovered || selected ? 0.32f : 0.22f);
        drawRectangle(x, cardY - 0.005f, cardWidth - 0.028f, 0.184f, 0,
                      panel.r, panel.g, panel.b);
        drawRectangle(x, cardY + 0.121f, cardWidth - 0.012f, 0.045f, 0,
                      accent.r * 0.45f, accent.g * 0.45f, accent.b * 0.45f);
        drawRectangle(x, cardY + 0.098f, cardWidth - 0.028f, 0.003f, 0,
                      border.r, border.g, border.b);

        drawText(names[card],
                 x, cardY + 0.122f, 0.0048f, glm::vec3(1.0f, 0.91f, 0.7f));
        Player portrait(cardTypes[card], 0, x - 0.02f, cardY - 0.007f);
        switch (portrait.type) {
        case PlayerType::TempleWarrior:
            drawTempleWarrior(portrait, 0.66f);
            break;
        case PlayerType::RoyalArcher:
            drawRoyalArcher(portrait, 0.66f);
            break;
        case PlayerType::ShieldGuardian:
            drawShieldGuardian(portrait, 0.66f);
            break;
        }
        drawRectangle(x, cardY - 0.115f, 0.114f, 0.052f, 0,
                      border.r, border.g, border.b);
        drawRectangle(x, cardY - 0.115f, 0.108f, 0.046f, 0, 0.10f, 0.12f, 0.14f);
        drawNumber(cardCosts[card],
                   x, cardY - 0.115f, 0.40f, true);
        if (!available) {
            float lockX = x + 0.083f, lockY = cardY - 0.116f;
            drawRectangle(lockX, lockY + 0.014f, 0.018f, 0.024f, 0, 0.6f, 0.6f, 0.6f);
            drawRectangle(lockX, lockY + 0.014f, 0.010f, 0.016f, 0, 0.10f, 0.12f, 0.14f);
            drawRectangle(lockX, lockY - 0.005f, 0.028f, 0.025f, 0, 0.6f, 0.6f, 0.6f);
        }
        if (selected) {
            for (int side : {-1, 1})
                drawRectangle(x + side * 0.084f, cardY - 0.115f, 0.012f, 0.012f, 45,
                              1.0f, 0.8f, 0.24f);
        }
    }
    // Keep card grayscale from affecting the courtyard and characters.
    glUniform1i(grayscaleLocation, GL_FALSE);
}

void drawSandSpirit(const Enemy& enemy, float scale = 1.0f,
                    float clothSway = 0.0f, float staffAngle = 0.0f) {
    setCharacterOrigin(enemy.x, enemy.y, scale);

    const glm::vec3 cloth(0.55f, 0.49f, 0.59f);
    const glm::vec3 shadow(0.25f, 0.16f, 0.32f);
    const glm::vec3 wrap(0.73f, 0.65f, 0.69f);
    const glm::vec3 wood(0.29f, 0.18f, 0.14f);
    const glm::vec3 magic(0.91f, 0.31f, 0.63f);

    // Robe and loose cloth.
    drawPart(0.008f, -0.018f, 0.055f, 0.1f, -5.0f, shadow);
    drawPart(0.0f, 0.017f, 0.048f, 0.065f, 0.0f, cloth);
    drawPart(-0.018f, -0.077f, 0.018f, 0.065f, -18.0f + clothSway, cloth);
    drawPart(0.008f, -0.083f, 0.015f, 0.07f, 12.0f - clothSway, shadow);
    drawPart(0.032f, -0.067f, 0.013f, 0.06f, 30.0f + clothSway, cloth);
    for (int band = 0; band < 4; ++band) {
        drawPart(0.002f, -0.038f + band * 0.023f, 0.052f, 0.008f,
             -12.0f, wrap);
    }

    // Head and face slit.
    drawPart(0.0f, 0.076f, 0.055f, 0.062f, 8.0f, shadow);
    drawPart(0.0f, 0.101f, 0.057f, 0.015f, 8.0f, wrap);
    drawPart(-0.005f, 0.054f, 0.049f, 0.015f, -8.0f, cloth);
    drawPart(-0.008f, 0.079f, 0.041f, 0.015f, 0.0f, glm::vec3(0.09f, 0.05f, 0.13f));
    drawPart(-0.022f, 0.081f, 0.008f, 0.006f, 0.0f, magic);
    drawPart(-0.005f, 0.081f, 0.008f, 0.006f, 0.0f, magic);
    drawPart(0.033f, 0.074f, 0.045f, 0.011f, -18.0f, wrap);

    // Arms.
    drawPart(0.035f, 0.0f, 0.016f, 0.065f, 25.0f, cloth);
    drawPart(-0.034f, 0.04f, 0.018f, 0.05f, -38.0f, cloth);
    drawPart(-0.057f, 0.061f, 0.035f, 0.014f, 0.0f, wrap);

    // Staff and magic tip.
    rotateCharacterParts(-0.075f, 0.073f, staffAngle);
    drawPart(-0.075f, 0.026f, 0.009f, 0.18f, 0.0f, wood);
    drawPart(-0.075f, 0.073f, 0.015f, 0.015f, 0.0f, wrap);
    drawPart(-0.075f, 0.122f, 0.04f, 0.04f, 45.0f, shadow);
    drawPart(-0.075f, 0.122f, 0.027f, 0.027f, 45.0f, magic);
    drawPart(-0.075f, 0.122f, 0.009f, 0.009f, 45.0f, glm::vec3(1.0f, 0.8f, 0.87f));
}

void drawCursedScarab(const Enemy& enemy, float walkPhase = 0.0f) {
    setCharacterOrigin(enemy.x, enemy.y);

    const glm::vec3 shell(0.22f, 0.14f, 0.32f);
    const glm::vec3 plate(0.38f, 0.24f, 0.46f);
    const glm::vec3 rim(0.59f, 0.42f, 0.28f);
    const glm::vec3 dark(0.12f, 0.09f, 0.16f);
    const glm::vec3 magic(0.85f, 0.3f, 0.58f);

    // Scarab legs.
    for (int leg = 0; leg < 3; ++leg) {
        float legX = -0.055f + leg * 0.05f;
        float step = 12.0f * std::sin(walkPhase + leg * 2.0f);
        drawPart(legX, -0.075f, 0.013f, 0.055f, -35.0f + step, rim);
        drawPart(legX + 0.02f + step * 0.0005f, -0.103f, 0.035f, 0.01f, 0.0f, dark);
        drawPart(legX + 0.008f, -0.064f, 0.011f, 0.048f, 35.0f - step, plate);
    }

    // Shell layers.
    drawPart(0.0f, -0.041f, 0.16f, 0.065f, 0.0f, dark);
    drawPart(0.0f, -0.021f, 0.14f, 0.065f, 0.0f, rim);
    drawPart(0.0f, -0.015f, 0.12f, 0.065f, 0.0f, shell);
    drawPart(-0.048f, 0.005f, 0.04f, 0.04f, -25.0f, plate);
    drawPart(0.048f, 0.005f, 0.04f, 0.04f, 25.0f, plate);
    drawPart(0.0f, 0.016f, 0.075f, 0.034f, 0.0f, plate);
    drawPart(0.0f, -0.005f, 0.006f, 0.074f, 0.0f, rim);
    for (int band = 0; band < 2; ++band) {
        drawPart(0.0f, -0.025f + band * 0.024f, 0.11f, 0.005f, 0.0f, rim);
    }

    // Head, antennae, and projectile emitter.
    drawPart(-0.083f, -0.032f, 0.046f, 0.04f, 0.0f, dark);
    drawPart(-0.092f, -0.024f, 0.009f, 0.008f, 0.0f, magic);
    drawPart(-0.072f, -0.024f, 0.009f, 0.008f, 0.0f, magic);
    drawPart(-0.102f, -0.002f, 0.006f, 0.035f, -40.0f, rim);
    drawPart(-0.078f, 0.004f, 0.006f, 0.032f, 20.0f, rim);
    drawPart(-0.109f, -0.05f, 0.04f, 0.025f, 0.0f, rim);
    drawPart(-0.129f, -0.05f, 0.012f, 0.018f, 0.0f, magic);
}

void drawJackalBrute(const Enemy& enemy, float walkPhase = 0.0f) {
    float legSwingAngle = 12.0f * std::sin(walkPhase);
    float swing = enemy.action == CharacterAction::Attacking
        ? 55.0f * std::sin(3.14159265f * enemy.attackTimer / bruteAttackInterval) : 0.0f;
    setCharacterOrigin(enemy.x, enemy.y);

    const glm::vec3 fur(0.17f, 0.13f, 0.2f);
    const glm::vec3 armor(0.36f, 0.23f, 0.4f);
    const glm::vec3 bronze(0.62f, 0.4f, 0.23f);
    const glm::vec3 metal(0.65f, 0.61f, 0.64f);
    const glm::vec3 cloth(0.4f, 0.13f, 0.22f);

    // Legs and lower armor.
    drawPart(-0.028f, -0.077f, 0.03f, 0.072f, -12.0f + legSwingAngle, fur);
    drawPart(0.028f, -0.077f, 0.03f, 0.072f, 12.0f - legSwingAngle, fur);
    drawPart(-0.035f + legSwingAngle * 0.0004f, -0.112f, 0.049f, 0.018f, 0.0f, bronze);
    drawPart(0.029f - legSwingAngle * 0.0004f, -0.112f, 0.049f, 0.018f, 0.0f, bronze);
    drawPart(0.0f, -0.035f, 0.09f, 0.045f, 0.0f, cloth);
    drawPart(0.0f, 0.008f, 0.09f, 0.075f, 0.0f, armor);
    drawPart(0.0f, -0.02f, 0.1f, 0.012f, 0.0f, bronze);
    drawPart(0.0f, 0.015f, 0.065f, 0.05f, 0.0f, bronze);
    drawPart(0.0f, 0.015f, 0.045f, 0.034f, 0.0f, armor);
    // Armor seams.
    drawPart(-0.008f, 0.019f, 0.005f, 0.026f, -30.0f, fur);
    drawPart(0.0f, 0.0f, 0.005f, 0.021f, 25.0f, fur);
    drawPart(-0.055f, 0.028f, 0.044f, 0.035f, -18.0f, bronze);
    drawPart(0.055f, 0.028f, 0.044f, 0.035f, 18.0f, bronze);
    drawPart(0.057f, -0.007f, 0.026f, 0.065f, 12.0f, fur);
    drawPart(0.06f, -0.029f, 0.032f, 0.025f, 0.0f, armor);
    drawPart(-0.06f, 0.005f, 0.027f, 0.06f, -35.0f, fur);
    drawPart(-0.08f, -0.015f, 0.038f, 0.022f, 0.0f, bronze);

    // Jackal head and ears.
    drawPart(0.0f, 0.056f, 0.036f, 0.03f, 0.0f, fur);
    drawPart(-0.005f, 0.087f, 0.063f, 0.05f, 0.0f, fur);
    drawPart(-0.026f, 0.12f, 0.017f, 0.04f, -12.0f, fur);
    drawPart(0.014f, 0.12f, 0.017f, 0.04f, 12.0f, fur);
    drawPart(-0.026f, 0.123f, 0.006f, 0.022f, -12.0f, cloth);
    drawPart(0.014f, 0.123f, 0.006f, 0.022f, 12.0f, cloth);
    drawPart(-0.045f, 0.078f, 0.05f, 0.023f, -8.0f, fur);
    drawPart(-0.068f, 0.08f, 0.012f, 0.017f, 0.0f, glm::vec3(0.07f));
    drawPart(-0.025f, 0.094f, 0.012f, 0.007f, 0.0f, glm::vec3(0.96f, 0.3f, 0.24f));
    drawPart(0.0f, 0.064f, 0.062f, 0.009f, 0.0f, bronze);

    // Axe.
    rotateCharacterParts(-0.096f, -0.015f, swing);
    drawPart(-0.096f, 0.01f, 0.011f, 0.2f, -10.0f, bronze);
    drawPart(-0.08f, 0.097f, 0.022f, 0.032f, -10.0f, armor);
    drawPart(-0.111f, 0.098f, 0.06f, 0.052f, -10.0f, metal);
    drawPart(-0.14f, 0.096f, 0.014f, 0.067f, -10.0f, glm::vec3(0.88f, 0.85f, 0.77f));
    drawPart(-0.11f, 0.098f, 0.025f, 0.018f, -10.0f, armor);
}

void drawGoldenScarab(float x, float y, float scale) {
    for (int side = -1; side <= 1; side += 2) {
        for (int leg = 0; leg < 3; ++leg) {
            drawRectangle(x + side * 0.035f * scale, y + (-0.03f + leg * 0.025f) * scale,
                          0.035f * scale, 0.006f * scale, side * 25.0f, 0.9f, 0.62f, 0.16f);
        }
    }
    drawRectangle(x, y, 0.055f * scale, 0.075f * scale, 0.0f, 0.95f, 0.74f, 0.22f);
    drawRectangle(x, y + 0.047f * scale, 0.025f * scale, 0.02f * scale, 0.0f, 0.95f, 0.74f, 0.22f);
    drawRectangle(x, y, 0.005f * scale, 0.065f * scale, 0.0f, 0.4f, 0.26f, 0.08f);
}

void drawNumber(int number, float originX, float originY, float size, bool centered) {
    // Seven-segment digits: bits represent top, upper-right, lower-right,
    // bottom, lower-left, upper-left, and middle segments respectively.
    const unsigned int digitMasks[] = {
        0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F
    };
    const float segmentX[] = {0.0f, 0.024f, 0.024f, 0.0f, -0.024f, -0.024f, 0.0f};
    const float segmentY[] = {0.05f, 0.025f, -0.025f, -0.05f, -0.025f, 0.025f, 0.0f};
    char digits[16];
    std::snprintf(digits, sizeof(digits), "%d", number);
    int digitCount = 0;
    while (digits[digitCount] != '\0') ++digitCount;
    // Fit longer balances into the same panel rather than overflowing it.
    float scale = size * (digitCount > 4 ? 4.0f / digitCount : 1.0f);
    if (centered) {
        // Center the visible segments, including narrow digits such as 1.
        float left = 100.0f, right = -100.0f;
        for (int digit = 0; digit < digitCount; ++digit) {
            int value = digits[digit] - '0';
            if (value < 0 || value > 9) continue;
            for (int segment = 0; segment < 7; ++segment) {
                if ((digitMasks[value] & (1u << segment)) == 0) continue;
                bool horizontal = segment == 0 || segment == 3 || segment == 6;
                float center = digit * 0.09f + segmentX[segment];
                float halfWidth = (horizontal ? 0.048f : 0.008f) * 0.5f;
                left = std::min(left, center - halfWidth);
                right = std::max(right, center + halfWidth);
            }
        }
        originX -= (left + right) * 0.5f * scale;
    }
    for (int digit = 0; digit < digitCount; ++digit) {
        int value = digits[digit] - '0';
        if (value < 0 || value > 9) continue;
        float x = originX + digit * 0.09f * scale;
        for (int segment = 0; segment < 7; ++segment) {
            if ((digitMasks[value] & (1u << segment)) == 0) continue;
            bool horizontal = segment == 0 || segment == 3 || segment == 6;
            drawRectangle(x + segmentX[segment] * scale, originY + segmentY[segment] * scale,
                          (horizontal ? 0.048f : 0.008f) * scale,
                          (horizontal ? 0.008f : 0.05f) * scale,
                          0.0f, 1.0f, 0.9f, 0.55f);
        }
    }
}

void drawGameStatus(int balance, int waveIndex) {
    const glm::vec3 gold(0.88f, 0.68f, 0.34f);
    drawPanelRectangle(.6f, .82f, .60f, .26f, glm::vec3(.66f, .46f, .25f));
    drawPanelRectangle(.6f, .82f, .584f, .242f, glm::vec3(.23f, .18f, .14f));
    drawPanelRectangle(.6f, .939f, .584f, .006f, glm::vec3(.92f, .74f, .43f));
    drawPanelRectangle(.6f, .701f, .584f, .006f, glm::vec3(.43f, .30f, .18f));
    for (int side : {-1, 1}) {
        float x = .6f + side * .288f;
        drawPanelRectangle(x, .82f, .018f, .218f, gold);
        drawPanelRectangle(x, .918f, .036f, .019f, gold);
        drawPanelRectangle(x, .722f, .036f, .019f, gold);
    }
    drawPanelRectangle(.37f, .82f, .104f, .194f, gold);
    drawPanelRectangle(.37f, .82f, .092f, .182f, glm::vec3(.09f, .25f, .31f));
    drawGoldenScarab(.37f, .82f, .90f);
    drawNumber(balance, .53f, .842f,
               balance < 1000 ? .80f : .58f, true);
    drawText("SCARABS",
             .53f, .755f, .0034f, glm::vec3(.81f, .70f, .51f));
    drawPanelRectangle(.767f, .82f, .206f, .194f, gold);
    drawPanelRectangle(.767f, .82f, .194f, .182f, glm::vec3(.09f, .25f, .31f));
    drawText("WAVE",
             .767f, .876f, .0042f, glm::vec3(.95f, .84f, .61f));
    drawNumber(std::min(waveIndex + 1, waveCount), .717f, .79f, .52f, true);
    drawPanelRectangle(.767f, .79f, .003f, .046f, gold, -25);
    drawNumber(waveCount, .814f, .79f, .38f, true);
}

void drawArrow(const Projectile& arrow) {
    drawRectangle(arrow.x - 0.025f, arrow.y, 0.05f, 0.005f, 0.0f, 0.9f, 0.7f, 0.3f);
    drawRectangle(arrow.x, arrow.y, 0.012f, 0.012f, 45.0f, 0.95f, 0.91f, 0.8f);
    drawRectangle(arrow.x - 0.047f, arrow.y, 0.012f, 0.012f, 45.0f, 0.25f, 0.55f, 0.7f);
}

void drawHighProjectile(const Projectile& projectile) {
    drawRectangle(projectile.x + 0.025f, projectile.y, 0.06f, 0.012f, 0.0f,
                  0.55f, 0.18f, 0.47f);
    drawRectangle(projectile.x, projectile.y, 0.026f, 0.026f, 45.0f,
                  0.95f, 0.34f, 0.67f);
    drawRectangle(projectile.x, projectile.y, 0.012f, 0.012f, 45.0f,
                  1.0f, 0.86f, 0.91f);
}

void drawLowProjectile(const Projectile& projectile) {
    drawRectangle(projectile.x + 0.025f, projectile.y, 0.055f, 0.008f, 0.0f,
                  0.55f, 0.3f, 0.08f);
    drawRectangle(projectile.x, projectile.y, 0.033f, 0.023f, 0.0f,
                  0.97f, 0.65f, 0.14f);
    drawRectangle(projectile.x - 0.005f, projectile.y, 0.014f, 0.011f, 0.0f,
                  1.0f, 0.92f, 0.55f);
}

void setDamageTint(int health, int maxHealth) {
    float remaining = maxHealth > 0
        ? glm::clamp(static_cast<float>(health) / maxHealth, 0.0f, 1.0f)
        : 0.0f;
    glUniform1f(damageLocation, 1.0f - remaining);
}

void drawHealthBar(float x, float y, int health, int maxHealth) {
    const float width = 0.17f;
    const float height = 0.018f;
    float fraction = maxHealth > 0
        ? glm::clamp(static_cast<float>(health) / maxHealth, 0.0f, 1.0f)
        : 0.0f;

    drawRectangle(x, y, width + 0.008f, height + 0.008f, 0.0f,
                  0.08f, 0.06f, 0.05f);
    drawRectangle(x, y, width, height, 0.0f, 0.4f, 0.12f, 0.12f);

    if (fraction > 0.0f) {
        float filledWidth = width * fraction;
        // Anchor the fill to the left edge rather than shrinking toward the center.
        float filledCenter = x - width * 0.5f + filledWidth * 0.5f;
        drawRectangle(filledCenter, y, filledWidth, height, 0.0f,
                      0.25f, 0.85f, 0.35f);
    }

    // Each divider represents one health point.
    for (int point = 1; point < maxHealth; ++point) {
        float separatorX = x - width * 0.5f + width * point / maxHealth;
        drawRectangle(separatorX, y, 0.003f, height, 0.0f, 0.08f, 0.06f, 0.05f);
    }
}

bool shouldShowPlacementPreview(int lane) {
    return gameState == GameState::Playing && lane >= 0 && lane < laneCount &&
           selectedCard >= 0 && selectedCard < playerCardCount && cardAvailable[selectedCard];
}

void drawPlacementPreview(int lane) {
    if (!shouldShowPlacementPreview(lane)) return;
    Player ghost(cardTypes[selectedCard], lane, fieldLeft + 0.07f,
                 fieldTop - (lane + 0.5f) * laneHeight);
    ghost.action = CharacterAction::Idle;
    drawingOpacity = 0.5f;
    switch (ghost.type) {
    case PlayerType::TempleWarrior: drawTempleWarrior(ghost); break;
    case PlayerType::RoyalArcher: drawRoyalArcher(ghost); break;
    case PlayerType::ShieldGuardian: drawShieldGuardian(ghost); break;
    }
    drawingOpacity = 1.0f;
}

void drawTemple(float heartScale = 1.0f) {
    const float centerX = -0.82f;
    const float centerY = (fieldTop + fieldBottom) * 0.5f;

    // Temple steps.
    drawRectangle(centerX, -0.69f, 0.32f, 0.08f, 0.0f, 0.48f, 0.32f, 0.17f);
    drawRectangle(centerX, -0.64f, 0.29f, 0.06f, 0.0f, 0.78f, 0.59f, 0.34f);

    // Entrance.
    drawRectangle(centerX, centerY, 0.27f, 1.18f, 0.0f, 0.76f, 0.56f, 0.3f);
    drawRectangle(centerX, centerY, 0.12f, 1.08f, 0.0f, 0.19f, 0.13f, 0.1f);

    // Gate towers.
    for (int side = -1; side <= 1; side += 2) {
        float towerX = centerX + side * 0.1f;
        drawRectangle(towerX, centerY, 0.075f, 1.25f, 0.0f, 0.86f, 0.67f, 0.4f);
        drawRectangle(towerX, centerY, 0.04f, 1.08f, 0.0f, 0.67f, 0.46f, 0.24f);
        for (int band = 0; band < 4; ++band) {
            drawRectangle(towerX, -0.48f + band * 0.29f, 0.06f, 0.025f, 0.0f,
                          0.2f, 0.43f, 0.52f);
        }
    }

    // Roof and blue band.
    drawRectangle(centerX, 0.57f, 0.3f, 0.14f, 0.0f, 0.87f, 0.68f, 0.4f);
    drawRectangle(centerX, 0.58f, 0.26f, 0.045f, 0.0f, 0.15f, 0.36f, 0.5f);
    drawRectangle(centerX, 0.66f, 0.32f, 0.04f, 0.0f, 0.95f, 0.77f, 0.32f);

    // Solar Heart above the gate.
    for (int ray = 0; ray < 4; ++ray) {
        drawRectangle(centerX, 0.79f, 0.2f * heartScale, 0.02f * heartScale, ray * 45.0f,
                      0.92f, 0.6f, 0.13f);
    }
    drawRectangle(centerX, 0.79f, 0.085f * heartScale, 0.085f * heartScale, 45.0f, 1.0f, 0.87f, 0.3f);
    drawRectangle(centerX, 0.79f, 0.035f * heartScale, 0.035f * heartScale, 45.0f, 1.0f, 0.97f, 0.7f);
}

void drawCutscene() {
    CutscenePose scene = getCutscenePose(cutsceneTime);
    drawEnvironment(cutsceneTime, false);
    drawRectangle(0.13f, -0.16f, 1.5f, 0.61f, 0.0f, 0.63f, 0.46f, 0.27f);
    drawRectangle(0.13f, -0.41f, 1.5f, 0.02f, 0.0f, 0.84f, 0.66f, 0.4f);
    drawTemple(1.0f + 0.08f * std::sin(cutsceneTime * 4.0f));
    drawRectangle(0.12f, 0.62f, 1.12f, 0.18f, 0, 0.10f, 0.14f, 0.18f);
    drawText("THE SACRED SUN", 0.12f, 0.64f, 0.010f, glm::vec3(0.96f, 0.8f, 0.4f));
    drawText("ENTER OR SPACE TO SKIP", 0.12f, 0.57f, 0.0038f, glm::vec3(0.8f, 0.81f, 0.78f));

    // Temporary actors keep the introduction from spawning real gameplay units.
    Player warrior(PlayerType::TempleWarrior, 0, scene.warriorX, -0.13f);
    warrior.action = CharacterAction::Idle;
    if (scene.ducking) {
        warrior.action = CharacterAction::Ducking;
        warrior.y -= 0.12f * 1.6f * (1.0f - duckHeightScale);
    }
    if (scene.swinging) {
        warrior.action = CharacterAction::Attacking;
        warrior.attackTimer = scene.swordSwingTime;
    }
    if (scene.invasionVisible) {
        Enemy scarab(EnemyType::CursedScarab, 0, scene.scarabX, 0.03f);
        Enemy brute(EnemyType::JackalBrute, 0, scene.bruteX, 0.03f);
        drawCursedScarab(scarab);
        drawJackalBrute(brute);
    }
    if (scene.spiritVisible) {
        Enemy spirit(EnemyType::SandSpirit, 0, scene.spiritX, scene.spiritY);
        glUniform1f(damageLocation, scene.spiritDamageTint);
        drawSandSpirit(spirit, 1.6f,
                       8.0f * std::sin(cutsceneTime * 5.0f), scene.staffAngle);
        glUniform1f(damageLocation, 0.0f);
    }
    drawTempleWarrior(warrior, 1.6f, scene.walkPhase);
    if (scene.shotVisible) {
        Projectile shot{0, scene.shotX, -0.002f};
        drawHighProjectile(shot);
    }
    if (scene.firstLine[0] == '\0') return;
    drawRectangle(0.12f, -0.55f, 1.5f, 0.23f, 0.0f, 0.09f, 0.1f, 0.15f);
    drawText(scene.firstLine, 0.12f, -0.505f, 0.009f, glm::vec3(1.0f, 0.86f, 0.45f));
    drawText(scene.secondLine, 0.12f, -0.595f, 0.008f, glm::vec3(0.95f, 0.91f, 0.8f));
}

void drawMainMenu(int hoveredButton) {
    drawTemple();
    drawText("GUARDIANS OF", menuCenterX, 0.62f, 0.016f, glm::vec3(1.0f, 0.83f, 0.35f));
    drawText("THE SACRED SUN", menuCenterX, 0.45f, 0.014f, glm::vec3(1.0f, 0.83f, 0.35f));
    drawText("EGYPTIAN TEMPLE DEFENSE", menuCenterX, 0.28f, 0.007f, glm::vec3(0.85f, 0.8f, 0.67f));
    const char* labels[] = {"START", "SOUND SETTINGS", "EXIT"};
    for (int button = 0; button < 3; ++button) {
        drawMenuButton(labels[button],
                       menuCenterX, mainMenuButtonY[button], menuButtonWidth, hoveredButton == button);
    }
}

// The letter font has no percent sign, so it is built from two dots and a slash.
void drawPercentSign(float x, float y, float size, glm::vec3 color) {
    drawRectangle(x, y, size * 0.9f, size * 7.0f, -0.6f, color.r, color.g, color.b);
    drawRectangle(x - size * 1.8f, y + size * 2.3f, size * 1.6f, size * 1.6f, 0.0f,
                  color.r, color.g, color.b);
    drawRectangle(x + size * 1.8f, y - size * 2.3f, size * 1.6f, size * 1.6f, 0.0f,
                  color.r, color.g, color.b);
}

// Draws "VOLUME 70%" (or "MUTED 70%") centered on x.
void drawVolumeReadout(float x, float y, float size, glm::vec3 color) {
    int percent = audio::volumePercent();
    const char* label = audio::isMuted() ? "MUTED" : "VOLUME";
    int labelLength = audio::isMuted() ? 5 : 6;
    int digits = percent >= 100 ? 3 : (percent >= 10 ? 2 : 1);
    // Letters are 7 text pixels tall; this digit scale gives digits the same height.
    float numberScale = size * 65.0f;
    float labelWidth = labelLength * 6 * size;
    float numberWidth = digits * 0.09f * numberScale;
    float total = labelWidth + size * 4 + numberWidth + size * 6;
    float left = x - total * 0.5f;
    drawText(label, left + labelWidth * 0.5f, y, size, color);
    float numberLeft = left + labelWidth + size * 4;
    drawNumber(percent, numberLeft + numberWidth * 0.5f, y, numberScale, true);
    drawPercentSign(numberLeft + numberWidth + size * 3.5f, y, size, color);
}

void drawSoundMenu(int hoveredButton) {
    drawRectangle(0.0f, -0.05f, 1.08f, 1.5f, 0.0f, 0.69f, 0.51f, 0.24f);
    drawRectangle(0.0f, -0.05f, 1.04f, 1.46f, 0.0f, 0.08f, 0.1f, 0.16f);
    drawText("SOUND SETTINGS", 0.0f, 0.55f, 0.0105f, glm::vec3(1.0f, 0.83f, 0.35f));
    drawVolumeReadout(0.0f, 0.37f, 0.011f, audio::isMuted()
        ? glm::vec3(0.86f, 0.45f, 0.35f) : glm::vec3(0.95f, 0.91f, 0.8f));
    const char* labels[] = {"VOLUME UP", "VOLUME DOWN", audio::isMuted() ? "UNMUTE" : "MUTE", "BACK"};
    for (int button = 0; button < 4; ++button)
        drawMenuButton(labels[button],
                       0.0f, soundButtonY[button], pauseButtonWidth, hoveredButton == button);
}

void drawPauseMenu(int hoveredButton) {
    drawRectangle(0.0f, 0.0f, 1.08f, 1.32f, 0.0f, 0.69f, 0.51f, 0.24f);
    drawRectangle(0.0f, 0.0f, 1.04f, 1.28f, 0.0f, 0.08f, 0.1f, 0.16f);
    drawText("PAUSED", 0.0f, 0.47f, 0.016f, glm::vec3(1.0f, 0.83f, 0.35f));
    const char* labels[] = {"CONTINUE", "RESTART", "MAIN MENU"};
    for (int button = 0; button < 3; ++button)
        drawMenuButton(labels[button],
                       0.0f, pauseButtonY[button], pauseButtonWidth, hoveredButton == button);
    drawText("ESC TO CONTINUE", 0.0f, -0.56f, 0.007f, glm::vec3(0.8f, 0.8f, 0.75f));
}

void drawResultMenu(int hoveredButton) {
    bool won = gameState == GameState::Victory;
    glm::vec3 accent = won ? glm::vec3(0.95f, 0.77f, 0.3f) : glm::vec3(0.86f, 0.3f, 0.23f);
    drawRectangle(0.0f, 0.0f, 1.12f, 1.25f, 0.0f, accent.r, accent.g, accent.b);
    drawRectangle(0.0f, 0.0f, 1.08f, 1.21f, 0.0f, 0.08f, 0.1f, 0.16f);
    drawText(won ? "VICTORY" : "DEFEAT", 0.0f, 0.44f, 0.016f, accent);
    drawText(won ? "ALL WAVES CLEARED" : "THE TEMPLE HAS FALLEN",
             0.0f, 0.27f, 0.007f, glm::vec3(0.9f, 0.86f, 0.74f));
    const char* labels[] = {"RESTART", "MAIN MENU"};
    for (int button = 0; button < 2; ++button)
        drawMenuButton(labels[button],
                       0.0f, menuButtonY[button], pauseButtonWidth, hoveredButton == button);
}

void drawPlayfield(int hoveredLane) {
    drawEnvironment(worldAnimationTime);
    const float centerX = (fieldLeft + fieldRight) * 0.5f;
    const float centerY = (fieldTop + fieldBottom) * 0.5f;
    const float width = fieldRight - fieldLeft;
    const float height = fieldTop - fieldBottom;

    drawRectangle(centerX, centerY, width + 0.04f, height + 0.04f, 0.0f,
                  0.35f, 0.24f, 0.13f);

    for (int lane = 0; lane < laneCount; ++lane) {
        float y = fieldTop - (lane + 0.5f) * laneHeight;
        float shade = (lane % 2 == 0) ? 0.0f : 0.05f;
        glm::vec3 color(0.74f + shade, 0.57f + shade, 0.34f + shade);
        if (selectedLanes[lane])
            color = glm::vec3(0.93f, 0.76f, 0.35f);
        if (hoveredLane == lane)
            color = glm::mix(color, glm::vec3(1.0f, 0.94f, 0.65f), 0.45f);
        drawRectangle(centerX, y, width, laneHeight - 0.02f, 0.0f,
                      color.r, color.g, color.b);
        // Paving joints.
        glm::vec3 grout = color * 0.9f;
        for (int stone = 0; stone < 6; ++stone) {
            float jointX = fieldLeft + 0.16f + stone * 0.25f + (lane % 2) * 0.1f;
            if (jointX >= fieldRight - 0.02f) continue;
            drawRectangle(jointX, y, 0.003f, laneHeight - 0.035f, 0,
                          grout.r, grout.g, grout.b);
        }
        drawRectangle(centerX, y - laneHeight * 0.5f + 0.024f, width - 0.03f, 0.018f, 0,
                      grout.r, grout.g, grout.b);
    }

    // Temple boundary (left) and enemy entry boundary (right).
    drawRectangle(fieldLeft, centerY, 0.025f, height, 0.0f, 0.9f, 0.74f, 0.3f);
    drawRectangle(fieldRight, centerY, 0.025f, height, 0.0f, 0.45f, 0.24f, 0.17f);
    drawTemple();
}

void drawFrame(const glm::vec2& mousePosition, bool mouseInside) {
    int hoveredLane = (gameState == GameState::Playing && mouseInside)
        ? laneAtPosition(mousePosition) : -1;
    int hoveredCard = (gameState == GameState::Playing && mouseInside)
        ? cardAtPosition(mousePosition) : -1;
    glClearColor(0.025f, 0.028f, 0.04f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(activeShaderProgram);
    drawingOpacity = 1.0f;
    glUniform1f(damageLocation, 0.0f);
    drawRectangle(0, 0, 2, 2, 0, 0.12f, 0.11f, 0.18f);
    drawDesertBackdrop();
    if (gameState == GameState::MainMenu) {
        int hoveredButton = mouseInside ? mainMenuButtonAtPosition(mousePosition) : -1;
        drawMainMenu(hoveredButton);
        drawText("PLUS MINUS VOLUME   N MUTE", menuCenterX, -0.62f, 0.0045f,
                 glm::vec3(0.8f, 0.8f, 0.75f));
        drawVolumeReadout(menuCenterX, -0.70f, 0.0045f, glm::vec3(0.8f, 0.8f, 0.75f));
        return;
    }
    if (gameState == GameState::SoundSettings) {
        drawTemple();
        int hoveredButton = mouseInside ? soundMenuButtonAtPosition(mousePosition) : -1;
        drawSoundMenu(hoveredButton);
        return;
    }
    if (gameState == GameState::Cutscene) {
        drawCutscene();
        return;
    }
    if (gameState != GameState::Playing && gameState != GameState::Paused &&
        gameState != GameState::Victory && gameState != GameState::Defeat) return;

    drawPlayfield(hoveredLane);
    for (const Collectible& collectible : collectibles) {
        drawGoldenScarab(collectible.x, collectible.y - 0.075f, 0.65f);
    }
    drawGameStatus(coins, wave.waveIndex);
    drawPlayerCards(hoveredCard);

    struct HealthLabel {
        glm::vec2 anchor;
        int health;
        int maximum;
    };
    std::vector<HealthLabel> healthLabels;
    for (const Player& player : players) {
        setDamageTint(player.health, player.maxHealth);
        // Draw a raised copy; lane ownership and collision baseline stay fixed.
        Player pose = player;
        pose.y += getJumpHeight(player);
        float heightScale = player.action == CharacterAction::Ducking ? duckHeightScale : 1.0f;
        // Lower the compressed pose to keep its feet near the lane baseline.
        pose.y -= 0.12f * (1.0f - heightScale);
        switch (player.type) {
        case PlayerType::TempleWarrior:
            drawTempleWarrior(pose, 1.0f,
                              player.action == CharacterAction::Walking ? player.walkPhase : 0.0f);
            break;
        case PlayerType::RoyalArcher:
            drawRoyalArcher(pose, 1.0f,
                            player.action == CharacterAction::Walking ? player.walkPhase : 0.0f);
            break;
        case PlayerType::ShieldGuardian:
            drawShieldGuardian(pose);
            break;
        }
        glUniform1f(damageLocation, 0.0f);
        healthLabels.push_back({glm::vec2(pose.x, pose.y + 0.17f * heightScale),
                                player.health, player.maxHealth});
    }
    for (const Enemy& enemy : enemies) {
        setDamageTint(enemy.health, enemy.maxHealth);
        float walkPhase = enemy.action == CharacterAction::Walking ? enemy.walkPhase : 0.0f;
        Enemy pose = enemy;
        if (enemy.type == EnemyType::SandSpirit) pose.y += 0.005f * std::sin(walkPhase);
        switch (enemy.type) {
        case EnemyType::SandSpirit:
            drawSandSpirit(pose, 1.0f, 6.0f * std::sin(walkPhase));
            break;
        case EnemyType::CursedScarab:
            drawCursedScarab(pose, walkPhase);
            break;
        case EnemyType::JackalBrute:
            drawJackalBrute(pose, walkPhase);
            break;
        }
        glUniform1f(damageLocation, 0.0f);
        healthLabels.push_back({glm::vec2(pose.x, pose.y + 0.17f), enemy.health, enemy.maxHealth});
    }
    drawPlacementPreview(hoveredLane);
    // Render health after every model so overlapping units cannot hide bars.
    std::vector<glm::vec2> occupiedBars;
    for (const HealthLabel& label : healthLabels) {
        glm::vec2 position = findHealthBarPosition(label.anchor, occupiedBars);
        occupiedBars.push_back(position);
        if (glm::length(position - label.anchor) <= 0.01f) continue;
        glm::vec2 line = position - label.anchor;
        glm::vec2 middle = (position + label.anchor) * 0.5f;
        drawRectangle(middle.x, middle.y, glm::length(line), 0.003f,
                      glm::degrees(std::atan2(line.y, line.x)), 0.95f, 0.82f, 0.5f);
    }
    for (size_t index = 0; index < healthLabels.size(); ++index) {
        const HealthLabel& label = healthLabels[index];
        drawHealthBar(occupiedBars[index].x, occupiedBars[index].y, label.health, label.maximum);
    }
    for (const Projectile& arrow : arrows)
        drawArrow(arrow);
    for (const Projectile& projectile : highProjectiles)
        drawHighProjectile(projectile);
    for (const Projectile& projectile : lowProjectiles)
        drawLowProjectile(projectile);

    if (gameState == GameState::Paused) {
        int hoveredButton = mouseInside ? pauseMenuButtonAtPosition(mousePosition) : -1;
        drawPauseMenu(hoveredButton);
    }
    if (gameState == GameState::Victory || gameState == GameState::Defeat) {
        int hoveredButton = mouseInside ? resultMenuButtonAtPosition(mousePosition) : -1;
        drawResultMenu(hoveredButton);
    }
    drawVolumeReadout(0.72f, -0.93f, 0.0045f, glm::vec3(0.9f, 0.88f, 0.8f));
}

// Music and result sounds follow the game state. Reacting to the change of state
// here plays each victory or game over sound once, when that screen opens.
void updateAudioForState(GameState previous, GameState current) {
    if (previous == current) return;
    bool result = current == GameState::Victory || current == GameState::Defeat;
    if (result) {
        audio::pauseMusic();
        audio::playResult(current == GameState::Victory
            ? ResultSound::Victory : ResultSound::GameOver);
        return;
    }
    audio::stopResults();
    audio::playMusic();
}


// 10. Window setup and the main loop

int main() {
    if (!glfwInit()) {
        std::cerr << "Failed to init GLFW\n";
        return -1;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Guardians of the Sacred Sun", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    int openGLVersion = gladLoadGL(glfwGetProcAddress);
    if (openGLVersion == 0) {
        printf("Failed to initialize OpenGL context\n");
        return -1;
    }

    // Framebuffer pixels can differ from window coordinates on scaled displays.
    int framebufferWidth, framebufferHeight;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    framebufferSizeCallback(window, framebufferWidth, framebufferHeight);
    glfwSetFramebufferSizeCallback(window, framebufferSizeCallback);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glfwSetKeyCallback(window, keyCallback);
    glfwSetMouseButtonCallback(window, mouseButtonCallback);

    // Compile the two shaders, then link them into one program.
    GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexShaderSrc);
    GLuint fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentShaderSrc);

    GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    int success;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char log[512];
        glGetProgramInfoLog(shaderProgram, 512, nullptr, log);
        std::cerr << "Shader link error:\n" << log << "\n";
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    // Shared vertex data.
    std::vector<float> vertices = {
        // A unit rectangle centered at the origin, ready to transform.
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        // Rectangle: bottom-left, top-right, top-left.
        -0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f,
        -0.5f,  0.5f, 0.0f,
        // A right triangle for each half of a pyramid.
        -0.5f, -0.5f, 0.0f,
         0.5f, -0.5f, 0.0f,
         0.5f,  0.5f, 0.0f
    };
    // Lab 3's circle method: a center followed by points around the edge.
    vertices.insert(vertices.end(), {0.0f, 0.0f, 0.0f});
    for (int point = 0; point <= circleSegments; ++point) {
        float angle = 6.2831853f * point / circleSegments;
        vertices.insert(vertices.end(), {0.5f * std::cos(angle), 0.5f * std::sin(angle), 0.0f});
    }

    GLuint VAO, VBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    initializeDrawing(VAO, shaderProgram);
    audio::initialize(); // Without a sound device the game simply runs silently.
    audio::playMusic();
    GameState audioState = gameState;
    std::mt19937 randomGenerator(std::random_device{}());
    double lastFrameTime = glfwGetTime();

    // Read input, update the game, then draw the next frame.
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        double currentTime = glfwGetTime();
        float deltaTime = static_cast<float>(currentTime - lastFrameTime);
        lastFrameTime = currentTime;

        // Ignore time spent minimized so restoring does not cause a large jump.
        if (glfwGetWindowAttrib(window, GLFW_ICONIFIED)) {
            glfwWaitEvents();
            lastFrameTime = glfwGetTime();
            continue;
        }

        if (gameState == GameState::Cutscene)
            updateCutscene(deltaTime);
        else
            updateGame(deltaTime, randomGenerator);

        glm::vec2 mousePosition;
        bool mouseInside = getMousePosition(window, mousePosition);
        drawFrame(mousePosition, mouseInside);
        updateAudioForState(audioState, gameState);
        audioState = gameState;

        glfwSwapBuffers(window);
    }

    audio::shutdown();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    glfwTerminate();
    return 0;
}
