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
void send_data_to_server(ENetPeer* peer, ENetHost* client);
ENetPeer* init_connection(ENetHost* client);
ENetHost* init_enet();
void print_server_data(ENetEvent* event);

int main() {
    ENetHost* client = init_enet();
    if (client == NULL) return 1;

    ENetPeer* peer = init_connection(client);
    if (peer == NULL) return 1;

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

    // Input the server IP and port
    printf("Enter server address (IP:PORT): ");
    scanf("%63[^:]:%d", server_ip, &port);
    printf("Try to connect to IP %s, with port %d\n", server_ip, port);

    enet_address_set_host(&address, server_ip);
    address.port = port;

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
        } else {
            enet_peer_reset(peer);
            fprintf(stderr, "Connection to server failed.\n");
            return NULL;
        }
    }

    printf("Connection to server established. Listening for data...\n");

    // Event loop to handle incoming packets
    ENetEvent event;
    while (enet_host_service(client, &event, 10000) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE:
                printf("Packet received from server:\n");
                printf("\nClient: Received packet size: %zu\n", event.packet->dataLength);

                print_server_data(&event);

                enet_packet_destroy(event.packet);
                break;

            case ENET_EVENT_TYPE_DISCONNECT:
                printf("Disconnected from server.\n");
                return NULL;

            default:
                break;
        }
    }

    printf("Connection to server ended.\n");
    return peer;
}

void print_server_data(ENetEvent* event) {
    for (size_t i = 0; i < event->packet->dataLength; ++i) {
        printf("%02X ", event->packet->data[i]); // Print byte as hex
    }
    printf("\n");
}