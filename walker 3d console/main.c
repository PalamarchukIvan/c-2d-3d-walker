#include <stdio.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <math.h> 
#include <ncurses.h>
#include <stdbool.h>

#define MAP_SIZE 40
#define PLAYER_AVATAR '@'
#define OBSTICLE '#'
#define MIRROR 'M'

#define MINIMAP_HEIGHT 20
#define MINIMAP_WIDTH 36


const double max_ray_lenght = 80;
const double RENDER_STEP = 0.005;
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
    double angleXY;
    int color;
} player_t;

typedef struct ray {
    double end_x;
    double end_y;
    int index;
    double lenght;
    bool is_player;

    int color;
} ray_t;

typedef struct frame {
    char buffer[MAP_SIZE][MAP_SIZE];
    ray_t* rays;
} frame_t;


const int VOID_TYPE = 0;
const int OBSTICLE_TYPE = 1;
const int MIRROR_TYPE = 2;

const char OBSTICLE_SYMBOL = '#';
const char MIRROR_SYMBOL = 'M';
const char EMPTY_SYMBOL = ' ';

static object_t map[MAP_SIZE][MAP_SIZE];
const double obsticle_width = 2;

void init_ncyrses();
void initialize_map();
void add_random_obsticles();
void enable_raw_mode();
void disable_raw_mode();
void init_player(player_t* player);
frame_t create_frame(player_t* player, bool write_map);
void update_player(int input, player_t* player);
ray_t* create_rays(player_t* player, char buffer[MAP_SIZE][MAP_SIZE]);
void draw_frame(frame_t* frame, player_t* player);
char get_wall_char(ray_t* ray);
bool wall_collision(double dx, double dy, double pos_x, double pos_y);
bool player_colision(player_t* player, double dx, double dy, double pos_x, double pos_y);
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
    int input;
    do {
        frame_t frame = create_frame(&player, false);
        draw_frame(&frame, &player);

        input = getchar();
        update_player(input, &player);

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
    for (int i = 0; i < MAP_SIZE; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            // Set borders
            if (i == 0 || i == MAP_SIZE - 1 || j == 0 || j == MAP_SIZE - 1) {
                map[i][j] = create_object(OBSTICLE_TYPE); 
            } else {
                map[i][j] = create_object(VOID_TYPE); 
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

    for (int j = 10; j < 40; j++) {
        map[20][j] = create_object(MIRROR_TYPE); // Horizontal wall at row 20
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
    player->x = 6;
    player->y = 6;
    player->angleXY = 0;
    player->color = COLOR_BLACK;
}

void update_player(int input, player_t* player) {
    double cos_ = cos(player->angleXY);
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

        // Camera
        case 68: player->angleXY -= CAMERA_SPEED; break; // left
        case 67: player->angleXY += CAMERA_SPEED; break; // right

        default: break;
    }
}

frame_t create_frame(player_t* player, bool write_map) {
    char buffer[MAP_SIZE][MAP_SIZE];
    for (int i = 0; i < MAP_SIZE; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            buffer[i][j] = map[i][j].symbol;
        }
    }

    ray_t* rays = create_rays(player, buffer);
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

bool wall_collision(double dx, double dy, double pos_x, double pos_y) {
    return map[(int)(pos_y + dy)][(int)(pos_x + dx)].type == OBSTICLE_TYPE;
}

bool player_colision(player_t* player, double dx, double dy, double pos_x, double pos_y) {
    return ((int) player -> x == (int)(pos_x + dx)) && ((int)player -> y == (int)(pos_y + dy));
}

ray_t* create_rays(player_t* player, char buffer[MAP_SIZE][MAP_SIZE]) {
    ray_t* rays = malloc(sizeof(ray_t) * (int) (VIEW_ANGLE / RENDER_STEP));

    for (int i = 0; (double) i * RENDER_STEP < VIEW_ANGLE; i += 1) {
        double pos_x = (double) player->x;
        double pos_y = (double) player->y;

        double ray_angle = player->angleXY - VIEW_ANGLE / 2 + i * RENDER_STEP;

        double dx = cos(ray_angle);
        double dy = sin(ray_angle);

        bool is_reflected = false;
        bool is_player = false;

        double total_distance = 0;
        while (!wall_collision(dx, dy, pos_x, pos_y) && total_distance < max_ray_lenght) {
            if (is_reflected && player_colision(player, dx, dy, pos_x, pos_y)) {
                pos_x += dx;
                pos_y += dy;
                buffer[(int)(pos_y)][(int)(pos_x)] = '.';
                total_distance += sqrt(dx * dx + dy * dy);
                is_player = true;
                break;
            }
            switch (map[(int)(pos_y + dy)][(int)(pos_x + dx)].type) {
                case VOID_TYPE:
                    pos_x += dx;
                    pos_y += dy;
                    buffer[(int)(pos_y)][(int)(pos_x)] = '.';
                    total_distance += sqrt(dx * dx + dy * dy);
                break;
                case MIRROR_TYPE:
                    is_reflected = true;
                    if (map[(int) pos_y][(int)(pos_x + sign(dx))].type == MIRROR_TYPE) {
                        dx *= -1;
                    } else {
                        dy *= -1;
                    }
                    pos_x += dx;
                    pos_y += dy;
                    buffer[(int)(pos_y)][(int)(pos_x)] = '.';
                    total_distance += sqrt(dx * dx + dy * dy);
                break;
            }
        }

        ray_t ray;

        // ray.end_x = pos_x;
        // ray.end_y = pos_y;
        // ray.obsticle = map[(int)(pos_y + dy)][(int)(pos_x + dx)];
        ray.index = i;
        ray.lenght = total_distance;
        ray.is_player = is_player;
        if (is_player) {
            ray.color = player->color;
        } else {
            ray.color = map[(int) (pos_y + dy)][(int) (pos_x + dx)].color;
        }
        *(rays + i) = ray;
    }


    double view_x = player->x;
    double view_y = player->y;

    double dx = cos(player->angleXY);
    double dy = sin(player->angleXY);

    // where player looks
    while (map[(int)(view_y + dy)][(int)(view_x + dx)].type == VOID_TYPE) {
        buffer[(int)(view_y + dy)][(int)(view_x + dx)] = '^';
        view_x += dx;
        view_y += dy;
    }


    return rays;
}

void draw_frame(frame_t* frame, player_t* player) {
    clear(); 

    double screen_width = COLS; // Terminal width
    double screen_height = LINES; // Terminal height

    ray_t* rays = frame->rays;
    int num_rays = (int)(VIEW_ANGLE / RENDER_STEP);

    for (int i = 0; i < num_rays; i++) {
        ray_t current_ray = rays[i];

        double distance = current_ray.lenght;

        int wall_height = (int)(screen_height / (distance + 0.001)); 

        int start_y = (screen_height / 2) - (wall_height / 2);
        int end_y = (screen_height / 2) + (wall_height / 2);


        int color = current_ray.color;

        attron(COLOR_PAIR(color));
        for (int y = start_y; y < end_y; y++) {
            if (y >= 0 && y < screen_height) { 
                char wall = get_wall_char(&current_ray);

                mvaddch(y, i * (screen_width / num_rays), wall); 

            }
        }
        attroff(COLOR_PAIR(color));

    }

    mvprintw(LINES*0.8, COLS*0.8, "X: %f Y: %f", player->x, player->y);
    mvprintw(LINES*0.85, COLS*0.8, "angle %f", player->angleXY / M_PI * 180);

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
                if(map[i][j].color == COLOR_BLACK) {
                    continue;
                } else if(map[i][j].color == COLOR_RED) {
                    color_type = 'R';
                } else if(map[i][j].color == COLOR_GREEN) {
                    color_type = 'G';
                } else if(map[i][j].color == COLOR_YELLOW) {
                    color_type = 'Y';
                } else if(map[i][j].color == COLOR_BLUE) {
                    color_type = 'b';
                } else if(map[i][j].color == COLOR_MAGENTA) {
                    color_type = 'M';
                } else if(map[i][j].color == COLOR_CYAN) {
                    color_type = 'C';
                } else if(map[i][j].color == COLOR_WHITE) {
                    color_type = 'W';
                }
        
                mvaddch(i - start_i, j - start_j, color_type);
            } else {
                mvaddch(i - start_i, j - start_j, frame->buffer[i][j]);
            }
        }
    }
}

char get_wall_char(ray_t* ray) {
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