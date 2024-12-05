#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>


#define MAP_SIZE 7
#define PLAYER_AVATAR '@'

typedef struct player {
    int x;
    int y;
} player_t;

static char map[MAP_SIZE][MAP_SIZE] = {
    "_______",
    "|     |",
    "|     |",
    "|     |",
    "|     |",
    "|     |",
    "-------"
};

void init_player(player_t* player);
void print_map(player_t* player);
void update_player(int input, player_t* player);
void enable_raw_mode();
void disable_raw_mode();

int main() {
    player_t player;
    init_player(&player);

    enable_raw_mode(); // Enable raw input mode

    print_map(&player);

    int input;
    do {
        input = getchar(); // Capture key press
        update_player(input, &player);
        print_map(&player);
    } while(input != 'x'); // Exit on 'x'

    disable_raw_mode(); // Restore terminal settings

    return 0;
}

void enable_raw_mode() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t); // Get current terminal settings
    t.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &t); // Apply new settings
}

void disable_raw_mode() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= (ICANON | ECHO); // Re-enable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

void init_player(player_t* player) {
    player->x = 4;
    player->y = 3;
}

void update_player(int input, player_t* player) {
    switch (input) {
    case 'w':
        if (player->y > 1) {
            player->y--;
        }
        break;
    case 'a':
        if (player->x > 1) {
            player->x--;
        }
        break;
    case 'd':
        if (player->x < MAP_SIZE - 2) { // Ensure movement is within bounds
            player->x++;
        }
        break;
    case 's':
        if (player->y < MAP_SIZE - 2) { // Ensure movement is within bounds
            player->y++;
        }
        break;
    default:
        break;
    }
}

void print_map(player_t* player) {
    system("clear");   
    for (int i = 0; i < MAP_SIZE; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            if (player->x == j && player->y == i) {
                putchar(PLAYER_AVATAR); // Print player's avatar
            } else {
                putchar(map[i][j]); // Print map characters
            }
        }
        putchar('\n'); // Move to the next line
    }
}
