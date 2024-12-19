#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <math.h> 
#include <ncurses.h>
#include <stdbool.h>

#define MAP_SIZE 40
#define MAP_HEIGHT 10
#define PLAYER_AVATAR '@'
#define OBSTICLE '#'
#define MIRROR 'M'

#define MINIMAP_HEIGHT 20
#define MINIMAP_WIDTH 36


const double max_ray_lenght = MAP_SIZE / 1.5;
const double RENDER_STEP = 0.04;
const double HEIGHT_RENDER_STEP = 0.04;
const double HEIGHT_ANGLE = M_PI/4;
const double VIEW_ANGLE = M_PI/2;
const double CAMERA_SPEED = M_PI / 24;
const double brightnest_level = 5;

typedef struct object {
    int color;
    int type;
    char symbol;
} object_t;

typedef struct player {
    double x;
    double y;
    double z;
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

typedef struct rays_list
{
    ray_t* rays;
    int i; // rays in width
    int j; // rays in height

    // debug puposes
    int rays_beyond_z_counter;
    int rays_into_walls_counter;
    int rays_into_player_counter;
    int rays_to_long_counter;
} rays_list_t;


typedef struct frame {
    char buffer[MAP_SIZE][MAP_SIZE];
    rays_list_t* rays;
} frame_t;


const int VOID_TYPE = 0;
const int OBSTICLE_TYPE = 1;
const int MIRROR_TYPE = 2;

const char OBSTICLE_SYMBOL = '#';
const char MIRROR_SYMBOL = 'M';
const char EMPTY_SYMBOL = ' ';

static object_t map[MAP_HEIGHT][MAP_SIZE][MAP_SIZE];
const double obsticle_width = 2;

void init_ncyrses();
void initialize_map();
void add_random_obsticles();
void enable_raw_mode();
void disable_raw_mode();
void init_player(player_t* player);
frame_t create_frame(player_t* player, bool write_map);
void update_player(int input, player_t* player);
rays_list_t* create_rays(player_t* player, char buffer[MAP_SIZE][MAP_SIZE]);
void draw_frame(frame_t* frame, player_t* player);
char get_wall_char(ray_t* ray);
bool wall_collision(double dx, double dy, double dz, double pos_x, double pos_y, double pos_z);
bool player_colision(player_t* player, double dx, double dy, double dz, double pos_x, double pos_y, double pos_z);
int sign(int a);
object_t create_object(int type);
int min_int(int a, int b);
int max_int(int a, int b);
void render_minimap(frame_t* frame, player_t* player, bool frame_color);

int main() {
    player_t player;
    init_player(&player);

    initialize_map();

    enable_raw_mode();

    init_ncyrses();
    int input = 'x';
    do {
        frame_t frame = create_frame(&player, false);
        draw_frame(&frame, &player);

        input = getchar();
        update_player(input, &player);

        free(frame.rays->rays);
        free(frame.rays);
    } while (input != 'x');

    disable_raw_mode();

    endwin();
    return 0;
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
    switch (type)
    {
    case OBSTICLE_TYPE:
        object.color = rand() % 7 + 1;
        object.symbol = OBSTICLE_SYMBOL;
        object.type = OBSTICLE_TYPE;
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
        return object;
    }

    return object;
}

void initialize_map() {
    for (int k = 0; k < MAP_HEIGHT; k++) {
        for (int i = 0; i < MAP_SIZE; i++) {
            for (int j = 0; j < MAP_SIZE; j++) {
                // Set borders
                if (i == 0 || i == MAP_SIZE - 1 || j == 0 || j == MAP_SIZE - 1) {
                    map[k][i][j] = create_object(OBSTICLE_TYPE); 
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

        // if (rand() % 2 == 0) {
        //     map[x][y] = create_object(MIRROR_TYPE); 
        // } else {
        //     map[x][y] = create_object(MIRROR_TYPE); 
        // }
        // map[x][y] = create_object(OBSTICLE_TYPE); 

    }

    // for (int j = 10; j < 40; j++) {
    //     map[0][20][j] = create_object(MIRROR_TYPE); // Horizontal wall at row 20
    // }
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
    player->x = 6;
    player->y = 6;
    player->z = MAP_HEIGHT / 2;
    player->angleXY = 0;
    player->angleZY = 0;
    player->color = COLOR_BLACK;
}

void update_player(int input, player_t* player) {
    double cos_ = cos(player->angleXY) * cos(player->angleZY);
    double sin_ = sin(player->angleXY);

    switch (input) {
        // Movement
        case 'd': 
            if (player->y + cos_ > 1 && player->x - sin_ > 1 && player->y + cos_ < MAP_SIZE -1 && player->x - sin_ < MAP_SIZE - 1) {
                player->y += cos_; 
                player->x -= sin_; 
            }
            break;
        case 'w': 
            if (player->y + sin_ > 1 && player->x + cos_ > 1 && player->y + sin_ < MAP_SIZE -1 && player->x + cos_ < MAP_SIZE - 1) {
                player->y += sin_; 
                player->x += cos_; 
            }
            break;
        case 'a': 
            if (player->y - cos_ > 1 && player->x + sin_ > 1 && player->y - cos_ < MAP_SIZE -1 && player->x + sin_ < MAP_SIZE - 1) {
                player->y -= cos_; 
                player->x += sin_; 
            }
            break;
        case 's':
            if (player->y - sin_ > 1 && player->x - cos_ > 1 && player->y - sin_ < MAP_SIZE -1 && player->x - cos_ < MAP_SIZE - 1) {
                player->y -= sin_; 
                player->x -= cos_; 
            }
            break;
        case 'e':
            if (player->z < MAP_HEIGHT - 1) {
                player->z += 1;
            }
            break;
        case 'q':
            if (player->z > 1) {
                player->z -= 1;
            }
            break;

        // Camera
        case 68: player->angleXY -= CAMERA_SPEED; break; // left
        case 67: player->angleXY += CAMERA_SPEED; break; // right

        case 66: player->angleZY -= CAMERA_SPEED; break; // down
        case 65: player->angleZY += CAMERA_SPEED; break; // up

        default: break;
    }
}

frame_t create_frame(player_t* player, bool write_map) {
    char buffer[MAP_SIZE][MAP_SIZE];
    
    for (int i = 0; i < MAP_SIZE; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            buffer[i][j] = map[(int) player->z][i][j].symbol;
        }
    }

    rays_list_t* rays = create_rays(player, buffer);
    buffer[(int) player->y][(int) player->x] = PLAYER_AVATAR;

    if (write_map) {
        for (int i = 0; i < MAP_SIZE; i++) {
            for (int j = 0; j < MAP_SIZE; j++) {
                putchar(buffer[i][j]);
            }
            putchar('\n');
        }
    }

    frame_t frame;
    for (size_t i = 0; i < MAP_SIZE; i++)
    {
        for (size_t j = 0; j < MAP_SIZE; j++)
        {
            frame.buffer[i][j] = buffer[i][j];
        }
        
    }
    frame.rays = rays;

    return frame;
}

bool wall_collision(double dx, double dy, double dz, double pos_x, double pos_y, double pos_z) {
    return map[(int)(pos_z + dz)][(int)(pos_y + dy)][(int)(pos_x + dx)].type == OBSTICLE_TYPE;
}

bool player_colision(player_t* player, double dx, double dy, double dz, double pos_x, double pos_y, double pos_z) {
    return ((int) player -> x == (int)(pos_x + dx)) && ((int)player -> y == (int)(pos_y + dy)) && ((int)player -> z == (int)(pos_z + dz));
}

rays_list_t* create_rays(player_t* player, char buffer[MAP_SIZE][MAP_SIZE]) {
    int I = VIEW_ANGLE / RENDER_STEP;
    int J = HEIGHT_ANGLE / HEIGHT_RENDER_STEP;
    ray_t* rays = malloc(sizeof(ray_t) * (int) (I * J));

    int rays_beyond_z_counter = 0;
    int rays_into_walls_counter = 0;
    int rays_into_player_counter = 0;
    int rays_to_long_counter = 0;
    
    for (int i = 0; i < I; i += 1) {
        double ray_angle_XOY = player->angleXY - VIEW_ANGLE / 2 + i * RENDER_STEP;
        
        // create a list of rays 
        for (int j = 0; j < J; j += 1) {
            double pos_x = (double) player->x;
            double pos_y = (double) player->y;
            double pos_z = (double) player->z;

            double ray_angle_OYZ = player->angleZY - HEIGHT_ANGLE / 2 + j * HEIGHT_RENDER_STEP;

            double dx = cos(ray_angle_XOY) * cos(ray_angle_OYZ);
            double dy = sin(ray_angle_XOY) * cos(ray_angle_OYZ);
            double dz = cos(ray_angle_OYZ);

            bool is_reflected = false;
            bool is_player = false;

            double total_distance = 0;
            while (total_distance < max_ray_lenght) {
                if (pos_z + dz < 0 || (int) (pos_z + dz) > MAP_HEIGHT) {
                    rays_beyond_z_counter++;
                    break;
                }
                if (wall_collision(dx, dy, dz, pos_x, pos_y, pos_z)) {
                    rays_into_walls_counter++;
                    break;
                }
                if (is_reflected && player_colision(player, dx, dy, dz, pos_x, pos_y, pos_z)) {
                    pos_x += dx;
                    pos_y += dy;
                    pos_z += dz;
                    buffer[(int)(pos_y)][(int)(pos_x)] = '.';
                    total_distance += sqrt(dx * dx + dy * dy + dz * dz);
                    is_player = true;

                    rays_into_player_counter++;
                    break;
                }
                switch (map[(int)(pos_z + dz)][(int)(pos_y + dy)][(int)(pos_x + dx)].type) {
                    case VOID_TYPE:
                        pos_x += dx;
                        pos_y += dy;
                        pos_z += dz;
                        buffer[(int)(pos_y)][(int)(pos_x)] = '.';
                        total_distance += sqrt(dx * dx + dy * dy + dz * dz);
                    break;
                    case MIRROR_TYPE:
                        is_reflected = true;
                        if (map[(int)(pos_z + dz)][(int) pos_y][(int)(pos_x + sign(dx))].type == MIRROR_TYPE) {
                            dx *= -1;
                        } else {
                            dy *= -1;
                        }
                        pos_x += dx;
                        pos_y += dy;
                        pos_z += dz;
                        buffer[(int)(pos_y)][(int)(pos_x)] = '.';
                        total_distance += sqrt(dx * dx + dy * dy + dz * dz);
                    break;
                }

                if (total_distance >= max_ray_lenght) {
                    rays_to_long_counter++;
                }
            }

            ray_t ray;

            ray.index = i*j + j;
            ray.lenght = total_distance;
            ray.is_player = is_player;
            ray.end_z = pos_z;
            ray.end_y = pos_y;
            ray.end_x = pos_x;
            if (is_player) {
                ray.color = player->color;
            } else {
                ray.color = map[(int)(pos_z + dz)][(int) (pos_y + dy)][(int) (pos_x + dx)].color;
            }
            *(rays + i*j + j) = ray;

        }
    }

    double view_x = player->x;
    double view_y = player->y;

    double dx = cos(player->angleXY);
    double dy = sin(player->angleXY);

    // where players looks
    while (map[(int)player->z][(int)(view_y + dy)][(int)(view_x + dx)].type == VOID_TYPE) {
        buffer[(int)(view_y + dy)][(int)(view_x + dx)] = '^';
        view_x += dx;
        view_y += dy;
    }

    rays_list_t* list = malloc(sizeof(rays_list_t));
    list->rays = rays;
    list->i = I;
    list->j = J;

    //debuging
    list->rays_beyond_z_counter = rays_beyond_z_counter;
    list->rays_into_player_counter = rays_into_player_counter;
    list->rays_into_walls_counter = rays_into_walls_counter;
    list->rays_to_long_counter = rays_to_long_counter;
    return list;
}

void draw_frame(frame_t* frame, player_t* player) {
    clear(); 

    double screen_width = COLS; // Terminal width
    double screen_height = LINES; // Terminal height

    ray_t* rays = frame->rays->rays;
    int I = frame->rays->i;
    int J = frame->rays->j;

    for (int i = 0; i < I; i++) {
        for (int j = 0; j < J; j++) {
            ray_t current_ray = rays[i * j + j];

            double distance = current_ray.lenght;
            int color = current_ray.color;
            char wall = get_wall_char(&current_ray);

            attron(COLOR_PAIR(color));
            mvaddch(i, j, wall); 
            attroff(COLOR_PAIR(color));
        }
    }

    int start_for_stats_on_screen = LINES*0.80;

    mvprintw(start_for_stats_on_screen, COLS*0.8, "X: %f Y: %f", player->x, player->y);
    mvprintw(start_for_stats_on_screen + 1, COLS*0.8, "angle XY %f", player->angleXY / M_PI * 180);
    mvprintw(start_for_stats_on_screen + 2, COLS*0.8, "angle ZY %f", player->angleZY / M_PI * 180);
    mvprintw(start_for_stats_on_screen + 3, COLS*0.8, "Position Z %f", player->z);
    mvprintw(start_for_stats_on_screen + 4, COLS*0.8, "I %d", I);
    mvprintw(start_for_stats_on_screen + 5, COLS*0.8, "J %d", J);

    mvprintw(start_for_stats_on_screen + 6, COLS*0.8, "into walls %d", frame->rays->rays_into_walls_counter);
    mvprintw(start_for_stats_on_screen + 7, COLS*0.8, "into player %d", frame->rays->rays_into_player_counter);
    mvprintw(start_for_stats_on_screen + 8, COLS*0.8, "beyond z %d", frame->rays->rays_beyond_z_counter);
    mvprintw(start_for_stats_on_screen + 9, COLS*0.8, "too long %d", frame->rays->rays_to_long_counter);
    render_minimap(frame, player, false);

    refresh(); 
}

void render_minimap(frame_t* frame, player_t* player, bool frame_color) {
    int minimap_width =  min_int(MINIMAP_WIDTH, MAP_SIZE);
    int minimap_height = min_int(MINIMAP_HEIGHT, MAP_SIZE);
    int start_i = 0;
    if (player->y + minimap_height/2 > MAP_SIZE) {
        start_i = MAP_SIZE - minimap_height;
    } else {
        start_i = max_int(player->y - minimap_height/2, 0);
    }
    
    int end_i = 0;
    if (player->y - minimap_height/2 < 0) {
        end_i = minimap_height;
    } else {
        end_i = min_int(minimap_height / 2 + player->y, MAP_SIZE);
    }
    
    int start_j = 0;
    if (player->x + minimap_width/2 > MAP_SIZE) {
        start_j = MAP_SIZE - minimap_width;
    } else {
        start_j = max_int(player->x - minimap_width/2, 0);
    }

    int end_j = 0;
    if (player->x - minimap_width/2 < 0) {
        end_j = minimap_width;
    } else {
        end_j = min_int(minimap_width / 2 + player->x, MAP_SIZE);
    }

    for (int i = start_i; i < end_i; i++) {
        for (int j = start_j; j < end_j; j++) {
            if (frame_color && frame->buffer[i][j] == '#') {
                char color_type;
                if (map[0][i][j].color == COLOR_BLACK) {
                    continue;
                } else if(map[0][i][j].color == COLOR_RED) {
                    color_type = 'R';
                } else if(map[0][i][j].color == COLOR_GREEN) {
                    color_type = 'G';
                } else if(map[0][i][j].color == COLOR_YELLOW) {
                    color_type = 'Y';
                } else if(map[0][i][j].color == COLOR_BLUE) {
                    color_type = 'b';
                } else if(map[0][i][j].color == COLOR_MAGENTA) {
                    color_type = 'M';
                } else if(map[0][i][j].color == COLOR_CYAN) {
                    color_type = 'C';
                } else if(map[0][i][j].color == COLOR_WHITE) {
                    color_type = 'W';
                }
        
                mvaddch(i - start_i, j - start_j + COLS - MINIMAP_WIDTH, color_type);
            } else {
                mvaddch(i - start_i, j - start_j + COLS - MINIMAP_WIDTH, frame->buffer[i][j]);
            }
        }
    }
}

char get_wall_char(ray_t* ray) {
    if (ray->end_z < 1 || ray->end_z > MAP_HEIGHT - 1) {
        return '^';
    }
    if (ray -> is_player) {
        return '#';
    }
    char bightnes[10] = {'@', '%', '*', ';',  '+', '=', '-', ':', '.', ' '};

    int index = ray -> lenght / brightnest_level;
    if (index > sizeof(bightnes)) {
        return ' ';
    }
    return bightnes[index];
}

int sign(int a) {
    return (a > 0) - (a < 0);
}

int min_int(int a, int b) {
    if (a > b) {
        return b;
    } else {
        return a;
    }
}

int max_int(int a, int b) {
    if (a < b) {
        return b;
    } else {
        return a;
    }
}