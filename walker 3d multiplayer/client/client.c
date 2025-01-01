#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <math.h> 
#include <ncurses.h>
#include <stdbool.h>
#include <enet/enet.h>
#include <string.h>  // For memcpy, memset

#define MAP_SIZE 40
#define MAP_HEIGHT 20
#define PLAYER_AVATAR '@'
#define OBSTACLE '#'
#define MIRROR 'M'
#define MINIMAP_HEIGHT 20
#define MINIMAP_WIDTH 36

const double max_ray_lenght = MAP_SIZE * 2;
const double HEIGHT_ANGLE = M_PI / 4;
const double VIEW_ANGLE = M_PI / 2;
const double CAMERA_SPEED = M_PI / 24;
const double brightnest_level = 5;

typedef struct object {
    int color;
    int type;
    char symbol;
} object_t;

typedef struct position {
    double x;
    double y;
    double z;
} position_t;

typedef struct other_player_position {
    double x;
    double y;
    double z;
    int index;
} other_player_position_t;

typedef struct player {
    position_t position;
    double angleXY;
    double angleZY;
    int color;
} player_t;

typedef struct ray {
    double end_x;
    double end_y;
    double end_z;
    int index;
    double lenght;
    bool is_player;
    int color;
} ray_t;

typedef struct rays_list {
    ray_t* rays;
    int i;
    int j;
    int mirrored_count;
    int rays_into_walls_counter;
    int rays_into_player_counter;
    int rays_to_long_counter;
} rays_list_t;

typedef struct frame {
    char buffer[MAP_SIZE][MAP_SIZE];
    rays_list_t* rays;
} frame_t;

typedef struct server_init_response {
    object_t map[MAP_HEIGHT][MAP_SIZE][MAP_SIZE];
    position_t other_players[64];
    int player_count;
    player_t player;
} server_init_response_t;

const int VOID_TYPE = 0;
const int OBSTACLE_TYPE = 1;
const int MIRROR_TYPE = 2;
const int PLAYER_TYPE = 3;

const char OBSTACLE_SYMBOL = '#';
const char MIRROR_SYMBOL = 'M';
const char EMPTY_SYMBOL = ' ';

static object_t map[MAP_HEIGHT][MAP_SIZE][MAP_SIZE];
player_t* this_player;
position_t other_players[64];
int player_count;

void pull_server_updates(ENetHost* client, ENetPeer* peer); // Forward declaration

void init_ncyrses();
void initialize_map();
void add_random_obsticles();
void enable_raw_mode();
void disable_raw_mode();
void init_player(player_t* player);
frame_t create_frame(bool write_map);
void update_player(int input);
rays_list_t* create_rays(char buffer[MAP_SIZE][MAP_SIZE],
                         object_t map_with_players_added[MAP_HEIGHT][MAP_SIZE][MAP_SIZE]);
void draw_frame(frame_t* frame);
char get_wall_char(ray_t* ray);
bool wall_collision(object_t map_to_use[MAP_HEIGHT][MAP_SIZE][MAP_SIZE],
                    double pos_x, double pos_y, double pos_z);
bool mirror_collision(object_t map_to_use[MAP_HEIGHT][MAP_SIZE][MAP_SIZE],
                      double pos_x, double pos_y, double pos_z);
bool player_colision(object_t map_to_use[MAP_HEIGHT][MAP_SIZE][MAP_SIZE],
                     double pos_x, double pos_y, double pos_z);
int sign(int a);
object_t create_object(int type);
int min_int(int a, int b);
int max_int(int a, int b);
void render_minimap(frame_t* frame, bool frame_color);
void send_data_to_server(ENetPeer* peer, ENetHost* client);
ENetPeer* init_connection(ENetHost* client);
ENetHost* init_enet();

int main() {
    ENetHost* client = init_enet();
    if (client == NULL) return 1;

    ENetPeer* peer = init_connection(client);
    if (peer == NULL) return 1;

    enable_raw_mode();
    init_ncyrses();

    int input = 'x';
    do {
        frame_t frame = create_frame(false);
        draw_frame(&frame);

        input = getchar();
        update_player(input);
        send_data_to_server(peer, client);

        free(frame.rays->rays);
        free(frame.rays);

        pull_server_updates(client, peer);
    } while (input != 'x');

    enet_peer_disconnect(peer, 0);
    {
        ENetEvent event;
        while (enet_host_service(client, &event, 3000) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_RECEIVE:
                    enet_packet_destroy(event.packet);
                    break;
                case ENET_EVENT_TYPE_DISCONNECT:
                    printf("Received disconnect from server.\n");
                    break;
                default:
                    break;
            }
        }
    }
    enet_host_destroy(client);
    disable_raw_mode();
    endwin();
    return 0;
}

ENetHost* init_enet() {
    if (enet_initialize() != 0) {
        printf("An error occured while initializing ENet!\n");
        return NULL;
    }
    atexit(enet_deinitialize);
    ENetHost* client = enet_host_create(NULL, 1, 1, 0, 0);
    return client;
}

ENetPeer* init_connection(ENetHost* client) {
    ENetAddress address;
    char server_ip[64];
    int port;
    scanf("%63[^:]:%d", server_ip, &port);
    printf("Try to connect to ip %s, with port %d", server_ip, port);

    enet_address_set_host(&address, server_ip);
    address.port = 1111;

    ENetPeer* peer = enet_host_connect(client, &address, 2, 0);
    if (!peer) {
        fprintf(stderr, "No available peers for initiating an ENet connection.\n");
        return NULL;
    }

    {
        ENetEvent event;
        if (enet_host_service(client, &event, 5000) > 0 &&
            event.type == ENET_EVENT_TYPE_CONNECT) {
            printf("Connection to server succeeded.\n");

            // If your server sets 'event.data' with something meaningful,
            // you can cast it. But typically you'd receive an ENET_EVENT_TYPE_RECEIVE
            // event with 'event.packet->data'.
            // Minimal fix: just cast to avoid the warning
            server_init_response_t* init = (server_init_response_t*) (enet_uint32) event.data;

            // If 'init' is actually not valid here, you might need to handle it differently.
            memcpy(map, init->map, sizeof(init->map));
            memset(other_players, 0, sizeof(other_players));
            memcpy(other_players, init->other_players, sizeof(init->other_players));
            player_count = init->player_count;

            this_player = malloc(sizeof(player_t));
            this_player->position = init->player.position;
            this_player->angleXY = init->player.angleXY;
            this_player->angleZY = init->player.angleZY;
            this_player->color = init->player.color;
        } else {
            enet_peer_reset(peer);
            fprintf(stderr, "Connection to server failed.\n");
            return NULL;
        }
    }
    return peer;
}

void pull_server_updates(ENetHost* client, ENetPeer* peer) {
    ENetEvent event;
    while (enet_host_service(client, &event, 100) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE: {
                switch (event.packet->dataLength) {
                    case sizeof(player_t): {
                        player_t* other_player = (player_t*) event.packet->data;
                        mvprintw(LINES - 1, COLS / 2 - 15,
                                 "Received update => x: %f, y: %f, z: %f\n",
                                 other_player->position.x,
                                 other_player->position.y,
                                 other_player->position.z);
                        break;
                    }
                    case sizeof(other_player_position_t): {
                        other_player_position_t* new_player_pos =
                            (other_player_position_t*) event.packet->data;
                        position_t pos_to_add;
                        pos_to_add.x = new_player_pos->x;
                        pos_to_add.y = new_player_pos->y;
                        pos_to_add.z = new_player_pos->z;
                        other_players[new_player_pos->index] = pos_to_add;
                        break;
                    }
                    default:
                        break;
                }
                enet_packet_destroy(event.packet);
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
                printf("Disconnected from server.\n");
                enet_peer_disconnect(peer, 0);
                {
                    ENetEvent e;
                    while (enet_host_service(client, &e, 3000) > 0) {
                        switch (e.type) {
                            case ENET_EVENT_TYPE_RECEIVE:
                                enet_packet_destroy(e.packet);
                                break;
                            case ENET_EVENT_TYPE_DISCONNECT:
                                printf("Received disconnect from server.\n");
                                break;
                            default:
                                break;
                        }
                    }
                }
                enet_host_destroy(client);
                break;
            default:
                break;
        }
    }
}

void init_ncyrses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    start_color();
    init_pair(0, COLOR_BLACK, COLOR_BLACK);
    init_pair(1, COLOR_RED, COLOR_BLACK);
    init_pair(2, COLOR_GREEN, COLOR_BLACK);
    init_pair(3, COLOR_BLUE, COLOR_BLACK);
    init_pair(4, COLOR_YELLOW, COLOR_BLACK);
    init_pair(5, COLOR_MAGENTA, COLOR_BLACK);
    init_pair(6, COLOR_CYAN, COLOR_BLACK);
    init_pair(7, COLOR_WHITE, COLOR_BLACK);
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
        object.color = COLOR_BLACK;
        break;
    default:
        object.symbol = '?';
        object.type = -1;
        object.color = 0;
        break;
    }
    return object;
}

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
    add_random_obsticles();
}

void add_random_obsticles() {
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
    for (int k = MAP_HEIGHT * 0.1; k < MAP_HEIGHT * 0.8; k++) {
        for (int j = 10; j < 40; j++) {
            map[k][20][j] = create_object(MIRROR_TYPE);
        }
    }
}

void enable_raw_mode() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

void disable_raw_mode() {
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= (ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

void init_player(player_t* player) {
    position_t position;
    position.x = 12;
    position.y = 19;
    position.z = MAP_HEIGHT / 2;
    player->position = position;
    player->angleXY = M_PI / 2;
    player->angleZY = 0;
    player->color = COLOR_BLACK;
}

void update_player(int input) {
    double cos_ = cos(this_player->angleXY) * cos(this_player->angleZY);
    double sin_ = sin(this_player->angleXY);
    switch (input) {
    case 'd':
        if (!wall_collision(map, this_player->position.x - sin_,
                            this_player->position.y + cos_,
                            this_player->position.z)) {
            this_player->position.y += cos_;
            this_player->position.x -= sin_;
        }
        break;
    case 'w':
        if (!wall_collision(map, this_player->position.x + cos_,
                            this_player->position.y + sin_,
                            this_player->position.z)) {
            this_player->position.y += sin_;
            this_player->position.x += cos_;
        }
        break;
    case 'a':
        if (!wall_collision(map, this_player->position.x + sin_,
                            this_player->position.y - cos_,
                            this_player->position.z)) {
            this_player->position.y -= cos_;
            this_player->position.x += sin_;
        }
        break;
    case 's':
        if (!wall_collision(map, this_player->position.y - sin_,
                            this_player->position.x - cos_,
                            this_player->position.z)) {
            this_player->position.y -= sin_;
            this_player->position.x -= cos_;
        }
        break;
    case 'e':
        if (this_player->position.z < MAP_HEIGHT - 1) {
            this_player->position.z += 1;
        }
        break;
    case 'q':
        if (this_player->position.z > 1) {
            this_player->position.z -= 1;
        }
        break;
    case 68:
        this_player->angleXY -= CAMERA_SPEED;
        break;
    case 67:
        this_player->angleXY += CAMERA_SPEED;
        break;
    case 65:
        this_player->angleZY -= CAMERA_SPEED;
        break;
    case 66:
        this_player->angleZY += CAMERA_SPEED;
        break;
    case 'p':
        map[(int)this_player->position.z]
           [(int)this_player->position.y]
           [(int)this_player->position.x] = create_object(OBSTACLE_TYPE);
        break;
    default:
        break;
    }
}

void send_data_to_server(ENetPeer* peer, ENetHost* client) {
    ENetPacket* packet = enet_packet_create(&this_player,
                                            sizeof(this_player),
                                            ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 0, packet);
    enet_host_flush(client);
}

frame_t create_frame(bool write_map) {
    char buffer[MAP_SIZE][MAP_SIZE];
    for (int i = 0; i < MAP_SIZE; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            buffer[i][j] = map[(int)this_player->position.z][i][j].symbol;
        }
    }
    object_t map_with_players_added[MAP_HEIGHT][MAP_SIZE][MAP_SIZE];
    memcpy(map_with_players_added, map, sizeof(map));
    for (int i = 0; i < player_count; i++) {
        object_t another_player;
        another_player.type = PLAYER_TYPE;
        another_player.symbol = OBSTACLE_SYMBOL;
        another_player.color = COLOR_BLACK;
        map_with_players_added[(int)other_players[i].z]
                              [(int)other_players[i].y]
                              [(int)other_players[i].x] = another_player;
    }
    rays_list_t* rays = create_rays(buffer, map_with_players_added);
    buffer[(int)this_player->position.y][(int)this_player->position.x] = PLAYER_AVATAR;

    if (write_map) {
        for (int i = 0; i < MAP_SIZE; i++) {
            for (int j = 0; j < MAP_SIZE; j++) {
                putchar(buffer[i][j]);
            }
            putchar('\n');
        }
    }
    frame_t frame;
    for (size_t i = 0; i < MAP_SIZE; i++) {
        for (size_t j = 0; j < MAP_SIZE; j++) {
            frame.buffer[i][j] = buffer[i][j];
        }
    }
    frame.rays = rays;
    return frame;
}

bool wall_collision(object_t map_to_use[MAP_HEIGHT][MAP_SIZE][MAP_SIZE],
                    double pos_x, double pos_y, double pos_z) {
    int type = map_to_use[(int)(pos_z)][(int)(pos_y)][(int)(pos_x)].type;
    return (type == PLAYER_TYPE || type == OBSTACLE_TYPE);
}

bool mirror_collision(object_t map_to_use[MAP_HEIGHT][MAP_SIZE][MAP_SIZE],
                      double pos_x, double pos_y, double pos_z) {
    return map_to_use[(int)(pos_z)][(int)(pos_y)][(int)(pos_x)].type == MIRROR_TYPE;
}

bool player_colision(object_t map_to_use[MAP_HEIGHT][MAP_SIZE][MAP_SIZE],
                     double pos_x, double pos_y, double pos_z) {
    return map_to_use[(int)(pos_z)][(int)(pos_y)][(int)(pos_x)].type == PLAYER_TYPE;
}

rays_list_t* create_rays(char buffer[MAP_SIZE][MAP_SIZE],
                         object_t map_with_players_added[MAP_HEIGHT][MAP_SIZE][MAP_SIZE]) {
    int I = LINES;
    int J = COLS;
    ray_t* rays = malloc(sizeof(ray_t) * (I * J));
    if (!rays) return NULL;

    int mirrored_count = 0;
    int rays_into_walls_counter = 0;
    int rays_into_player_counter = 0;
    int rays_to_long_counter = 0;

    for (int i = 0; i < I; i++) {
        double ZY_angle = -HEIGHT_ANGLE / 2 + (((double)i + 1) / (double)I) * HEIGHT_ANGLE + this_player->angleZY;
        for (int j = 0; j < J; j++) {
            double XY_angle = -VIEW_ANGLE / 2 + (((double)j + 1) / (double)J) * VIEW_ANGLE + this_player->angleXY;
            double x = this_player->position.x + 0.5;
            double y = this_player->position.y + 0.5;
            double z = this_player->position.z + 0.5;
            double dx = 0, dy = 0, dz = 0;
            double total_distance = 0;
            int dx_dir = 1, dy_dir = 1, dz_dir = 1;
            ray_t ray;
            ray.index = i * J + j;
            ray.is_player = false;
            bool is_reflected = false;
            while (true) {
                if (total_distance > max_ray_lenght) {
                    rays_to_long_counter++;
                    ray.color = COLOR_WHITE;
                    break;
                } else if (wall_collision(map_with_players_added, x, y, z)) {
                    rays_into_walls_counter++;
                    ray.color = map_with_players_added[(int)z][(int)y][(int)x].color;
                    break;
                } else if (is_reflected && player_colision(map_with_players_added, x, y, z)) {
                    ray.is_player = true;
                    rays_into_player_counter++;
                    ray.color = this_player->color;
                    break;
                } else if (mirror_collision(map_with_players_added, x, y, z)) {
                    is_reflected = true;
                    double prev_x = x - dx;
                    double prev_y = y - dy;
                    double prev_z = z - dz;
                    if (mirror_collision(map_with_players_added, prev_x + dx, prev_y, prev_z)) dx_dir *= -1;
                    if (mirror_collision(map_with_players_added, prev_x, prev_y + dy, prev_z)) dy_dir *= -1;
                    if (mirror_collision(map_with_players_added, prev_x, prev_y, prev_z + dz)) dz_dir *= -1;
                    mirrored_count++;
                }
                dx = cos(ZY_angle) * cos(XY_angle) * dx_dir;
                dy = cos(ZY_angle) * sin(XY_angle) * dy_dir;
                dz = sin(ZY_angle) * dz_dir;
                total_distance += 1;
                x += dx;
                y += dy;
                z += dz;
            }
            ray.end_x = x;
            ray.end_y = y;
            ray.end_z = z;
            ray.lenght = total_distance;
            rays[ray.index] = ray;
        }
    }
    double view_x = this_player->position.x;
    double view_y = this_player->position.y;
    double dx = cos(this_player->angleXY);
    double dy = sin(this_player->angleXY);
    while (map_with_players_added[(int)this_player->position.z]
                                 [(int)(view_y + dy)]
                                 [(int)(view_x + dx)].type == VOID_TYPE) {
        buffer[(int)(view_y + dy)][(int)(view_x + dx)] = '^';
        view_x += dx;
        view_y += dy;
    }
    rays_list_t* list = malloc(sizeof(rays_list_t));
    if (!list) {
        free(rays);
        return NULL;
    }
    list->rays = rays;
    list->i = I;
    list->j = J;
    list->mirrored_count = mirrored_count;
    list->rays_into_player_counter = rays_into_player_counter;
    list->rays_into_walls_counter = rays_into_walls_counter;
    list->rays_to_long_counter = rays_to_long_counter;
    return list;
}

void draw_frame(frame_t* frame) {
    clear();
    ray_t* rays = frame->rays->rays;
    int I = frame->rays->i;
    int J = frame->rays->j;
    render_minimap(frame, true);
    int start_for_stats_on_screen = LINES * 0.80;
    for (int i = 0; i < I; i++) {
        for (int j = 0; j < J; j++) {
            ray_t current_ray = rays[i * J + j];
            int color = current_ray.color;
            char wall = get_wall_char(&current_ray);
            attron(COLOR_PAIR(color));
            mvaddch(i, j, wall);
            attroff(COLOR_PAIR(color));
        }
    }
    mvprintw(start_for_stats_on_screen, COLS * 0.8,
             "X: %f Y: %f", this_player->position.x, this_player->position.y);
    mvprintw(start_for_stats_on_screen + 1, COLS * 0.8,
             "angle XY %f", this_player->angleXY / M_PI * 180);
    mvprintw(start_for_stats_on_screen + 2, COLS * 0.8,
             "angle ZY %f", this_player->angleZY / M_PI * 180);
    mvprintw(start_for_stats_on_screen + 3, COLS * 0.8,
             "Position Z %f", this_player->position.z);
    mvprintw(start_for_stats_on_screen + 4, COLS * 0.8, "I %d", I);
    mvprintw(start_for_stats_on_screen + 5, COLS * 0.8, "J %d", J);
    mvprintw(start_for_stats_on_screen + 6, COLS * 0.8,
             "into walls %d", frame->rays->rays_into_walls_counter);
    mvprintw(start_for_stats_on_screen + 7, COLS * 0.8,
             "into player %d", frame->rays->rays_into_player_counter);
    mvprintw(start_for_stats_on_screen + 8, COLS * 0.8,
             "mirrored %d", frame->rays->mirrored_count);
    mvprintw(start_for_stats_on_screen + 9, COLS * 0.8,
             "too long %d", frame->rays->rays_to_long_counter);
    render_minimap(frame, true);
    refresh();
}

void render_minimap(frame_t* frame, bool frame_color) {
    int minimap_width = min_int(MINIMAP_WIDTH, MAP_SIZE);
    int minimap_height = min_int(MINIMAP_HEIGHT, MAP_SIZE);
    int start_i = 0;
    if (this_player->position.y + minimap_height / 2 > MAP_SIZE) {
        start_i = MAP_SIZE - minimap_height;
    } else {
        start_i = max_int(this_player->position.y - minimap_height / 2, 0);
    }
    int end_i = 0;
    if (this_player->position.y - minimap_height / 2 < 0) {
        end_i = minimap_height;
    } else {
        end_i = min_int(minimap_height / 2 + this_player->position.y, MAP_SIZE);
    }
    int start_j = 0;
    if (this_player->position.x + minimap_width / 2 > MAP_SIZE) {
        start_j = MAP_SIZE - minimap_width;
    } else {
        start_j = max_int(this_player->position.x - minimap_width / 2, 0);
    }
    int end_j = 0;
    if (this_player->position.x - minimap_width / 2 < 0) {
        end_j = minimap_width;
    } else {
        end_j = min_int(minimap_width / 2 + this_player->position.x, MAP_SIZE);
    }
    int z = (int)(this_player->position.z);
    for (int i = start_i; i < end_i; i++) {
        for (int j = start_j; j < end_j; j++) {
            if (frame_color && frame->buffer[i][j] == '#') {
                char color_type;
                if (map[0][i][j].color == COLOR_BLACK) {
                    continue;
                } else if (map[z][i][j].color == COLOR_RED) {
                    color_type = 'R';
                } else if (map[z][i][j].color == COLOR_GREEN) {
                    color_type = 'G';
                } else if (map[z][i][j].color == COLOR_YELLOW) {
                    color_type = 'Y';
                } else if (map[z][i][j].color == COLOR_BLUE) {
                    color_type = 'b';
                } else if (map[z][i][j].color == COLOR_MAGENTA) {
                    color_type = 'M';
                } else if (map[z][i][j].color == COLOR_CYAN) {
                    color_type = 'C';
                } else if (map[z][i][j].color == COLOR_WHITE) {
                    color_type = 'W';
                } else {
                    color_type = '#';
                }
                mvaddch(i - start_i, j - start_j + COLS - MINIMAP_WIDTH, color_type);
            } else {
                mvaddch(i - start_i, j - start_j + COLS - MINIMAP_WIDTH,
                        frame->buffer[i][j]);
            }
        }
    }
}

char get_wall_char(ray_t* ray) {
    if (ray->end_z < 1 || ray->end_z > MAP_HEIGHT - 1) {
        return '^';
    }
    if (ray->is_player) {
        return '#';
    }
    char bightnes[10] = {'@', '%', '*', ';', '+', '=', '-', ':', '.', ' '};
    int index = ray->lenght / brightnest_level;
    if (index >= (int)sizeof(bightnes)) {
        return ' ';
    }
    return bightnes[index];
}

int sign(int a) {
    return (a > 0) - (a < 0);
}

int min_int(int a, int b) {
    return (a < b) ? a : b;
}

int max_int(int a, int b) {
    return (a > b) ? a : b;
}
