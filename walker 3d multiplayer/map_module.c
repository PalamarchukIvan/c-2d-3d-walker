#include "map_module.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

// Define the global map
object_t map[MAP_HEIGHT][MAP_SIZE][MAP_SIZE];

void initialize_map() {
    for (int k = 0; k < MAP_HEIGHT; k++) {
        for (int i = 0; i < MAP_SIZE; i++) {
            for (int j = 0; j < MAP_SIZE; j++) {
                if (i == 0 || i == MAP_SIZE - 1 ||
                    j == 0 || j == MAP_SIZE - 1 ||
                    k == 0 || k == MAP_HEIGHT - 1) {
                    map[k][i][j] = create_object(OBSTACLE_TYPE);
                } else {
                    map[k][i][j] = create_object(VOID_TYPE);
                }
            }
        }
    }
    // add_random_obstacles();
}

void add_random_obstacles() {
    srand(time(NULL));
    int wall_count = 100;
    for (int i = 0; i < wall_count; i++) {
        int x = rand() % (MAP_SIZE - 2) + 1;
        int y = rand() % (MAP_SIZE - 2) + 1;
        int z = rand() % (MAP_HEIGHT - 2) + 1;
        if (rand() % 2 == 0) {
            map[z][y][x] = create_object(MIRROR_TYPE);
        } else {
            map[z][y][x] = create_object(OBSTACLE_TYPE);
        }
    }
}

object_t create_object(int type) {
    object_t object;
    switch (type) {
    case OBSTACLE_TYPE:
        object.color = rand() % 7 + 1;
        object.symbol = OBSTACLE_SYMBOL;
        object.type = OBSTACLE_TYPE;
        break;
    case MIRROR_TYPE:
        object.symbol = MIRROR_SYMBOL;
        object.type = MIRROR_TYPE;
        break;
    case VOID_TYPE:
        object.symbol = EMPTY_SYMBOL;
        object.type = VOID_TYPE;
        object.color = 0;
        break;
    default:
        object.symbol = '?';
        object.type = -1;
        object.color = 0;
        break;
    }
    return object;
}

char* serialize_map() {
    size_t buffer_size = MAP_HEIGHT * MAP_SIZE * MAP_SIZE * 10 + 1;
    char* buffer = malloc(buffer_size);
    if (!buffer) {
        fprintf(stderr, "Failed to allocate memory for serialization.\n");
        return NULL;
    }
    char temp[50];
    buffer[0] = '\0';

    for (int i = 0; i < MAP_HEIGHT; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            for (int k = 0; k < MAP_SIZE; k++) {
                snprintf(temp, sizeof(temp), "%d %d %c|", map[i][j][k].color, map[i][j][k].type, map[i][j][k].symbol);
                if (strlen(buffer) + strlen(temp) >= buffer_size - 1) {
                    fprintf(stderr, "Buffer overflow during serialization.\n");
                    free(buffer);
                    return NULL;
                }
                strcat(buffer, temp);
            }
        }
    }
    return buffer;
}

void deserialize_map(const char* data) {
    const char* ptr = data;
    for (int i = 0; i < MAP_HEIGHT; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            for (int k = 0; k < MAP_SIZE; k++) {
                if (!ptr || *ptr == '\0') {
                    fprintf(stderr, "Error: Insufficient data for deserialization.\n");
                    return;
                }

                int color, type;
                char symbol;
                const char* next_sep = strchr(ptr, '|');
                if (!next_sep) {
                    fprintf(stderr, "Error: Missing separator.\n");
                    return;
                }

                int matched = sscanf(ptr, "%d %d %c", &color, &type, &symbol);
                if (matched != 3) {
                    fprintf(stderr, "Error: Malformed data.\n");
                    return;
                }

                map[i][j][k].color = color;
                map[i][j][k].type = type;
                map[i][j][k].symbol = symbol;

                ptr = next_sep + 1;
            }
        }
    }
}

char* serialize_player(player_t* player) {
    if (!player) return NULL;
    char* buffer = malloc(256);
    if (!buffer) {
        perror("Failed to allocate memory.");
        return NULL;
    }
    snprintf(buffer, 256, "%lf|%lf|%lf|%lf|%lf|%d",
             player->position.x, player->position.y, player->position.z,
             player->angleXY, player->angleZY, player->color);
    return buffer;
}

int deserialize_player(char* str, player_t* player) {
    if (!str || !player) return -1;

    // Parse the string and populate the player_t struct
    int scanned = sscanf(str, "%lf|%lf|%lf|%lf|%lf|%d",
                         &player->position.x, &player->position.y, &player->position.z,
                         &player->angleXY, &player->angleZY, &player->color);

    // Ensure all fields were successfully read
    if (scanned != 6) {
        printf("Failed to deserialize player. Expected 6 fields but got %d\n", scanned);
        return -1;
    }

    return 0;
}

char* serialize_positions(position_t* positions, size_t count) {
    if (!positions || count == 0) return NULL;
    size_t buffer_size = count * 64;
    char* buffer = malloc(buffer_size);
    if (!buffer) {
        perror("Failed to allocate memory.");
        return NULL;
    }
    buffer[0] = '\0';

    for (size_t i = 0; i < count; ++i) {
        char temp[64];
        snprintf(temp, sizeof(temp), "%lf|%lf|%lf\n", positions[i].x, positions[i].y, positions[i].z);
        strcat(buffer, temp);
    }

    return buffer;
}

int deserialize_player_positions(char* str, player_position_t* position) {
    if (!str || !position) return -1;

    // Parse the string and populate the player_t struct
    int scanned = sscanf(str, "%f|%f|%f|%d",
                         &position->x, &position->y, &position->z, &position->index);

    // Ensure all fields were successfully read
    if (scanned != 4) {
        printf("Failed to deserialize player_position. Expected 4 fields but got %d\n", scanned);
        return -1;
    }

    return 0;
}

char* serialize_player_positions(player_position_t* position) {
    if (!position) return NULL;
    char* buffer = malloc(256);
    if (!buffer) {
        perror("Failed to allocate memory.");
        return NULL;
    }
    snprintf(buffer, 256, "%f|%f|%f|%d",
             position->x, position->y, position->z, position->index);
    return buffer;
}

position_t* deserialize_positions(char* str, size_t* count) {
    if (!str || !count) return NULL;

    size_t line_count = 0;
    for (const char* p = str; *p; ++p) {
        if (*p == '\n') ++line_count;
    }

    position_t* positions = malloc(line_count * sizeof(position_t));
    if (!positions) {
        perror("Failed to allocate memory.");
        return NULL;
    }

    size_t parsed_count = 0;
    const char* line_start = str;
    while (*line_start) {
        double x, y, z;
        if (sscanf(line_start, "%lf|%lf|%lf", &x, &y, &z) == 3) {
            positions[parsed_count++] = (position_t){x, y, z};
        }
        line_start = strchr(line_start, '\n');
        if (line_start) ++line_start;
        else break;
    }

    *count = parsed_count;
    return positions;
}