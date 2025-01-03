#ifndef MAP_MODULE_H
#define MAP_MODULE_H

#include <stdbool.h>
#include <stdlib.h>

// Constants
#define MAP_SIZE 40
#define MAP_HEIGHT 20

// Object Types
#define VOID_TYPE 0
#define OBSTACLE_TYPE 1
#define MIRROR_TYPE 2
#define PLAYER_TYPE 3

// Object Symbols
#define OBSTACLE_SYMBOL '#'
#define MIRROR_SYMBOL 'M'
#define EMPTY_SYMBOL '.'

// Structure Definitions
typedef struct {
    int color;
    int type;
    char symbol;
} object_t;

typedef struct {
    double x;
    double y;
    double z;
} position_t;

typedef struct {
    position_t position;
    double angleXY;
    double angleZY;
    int color;
} player_t;

typedef struct {
    double x;
    double y;
    double z;

    int index;
} player_position_t;

// Global Map
extern object_t map[MAP_HEIGHT][MAP_SIZE][MAP_SIZE];

// Function Prototypes
void initialize_map();
void add_random_obstacles();
object_t create_object(int type);

// map de-/serilaization
char* serialize_map();
void deserialize_map(const char* data);

// player de-/serilaization
char* serialize_player(player_t* player);
int deserialize_player(char* str, player_t* player);

// positions on the map de-/serilaization
char* serialize_positions(position_t* positions, size_t count);
position_t* deserialize_positions(char* str, size_t* count);

// player-positions on the map de-/serilaization
char* serialize_player_positions(player_position_t* position);
int deserialize_player_positions(char* str, player_position_t* position);

#endif // MAP_MODULE_H