#include <stdio.h>
#include <stdlib.h>
#include <math.h> 
#include <string.h>
#include <enet/enet.h>

#define MAP_SIZE 40
#define MAP_HEIGHT 20

#define MAX_CLIENTS 64
#define SERVER_PORT 1111

#define   1
#define MAP_UPDATE_EVENT 2

#define MAP_SIZE 40
#define MAP_HEIGHT 20
#define PLAYER_AVATAR '@'
#define OBSTICLE '#'
#define MIRROR 'M'

const int VOID_TYPE = 0;
const int OBSTICLE_TYPE = 1;
const int MIRROR_TYPE = 2;
const int PLAYER_TYPE = 3;

const char OBSTICLE_SYMBOL = '#';
const char MIRROR_SYMBOL = 'M';
const char EMPTY_SYMBOL = ' ';

typedef struct position {
    double x;
    double y;
    double z;
} position_t;

position_t spawn_point;

typedef struct player {
    position_t position;
    double angleXY;
    double angleZY;
    int color;
} player_t;

player_t players[MAX_CLIENTS];

typedef struct object {
    int color;
    int type;
    char symbol;
} object_t;

typedef struct position_to_feed {
    double x;
    double y;
    double z;

    int index;
} position_to_feed_t;

static object_t map[MAP_HEIGHT][MAP_SIZE][MAP_SIZE];

typedef struct new_player_data {
    object_t map[MAP_HEIGHT][MAP_SIZE][MAP_SIZE];
    position_t other_players[MAX_CLIENTS];
    int player_count;
    player_t player;
} new_player_data_t;

void init_player(player_t* player);
ENetHost* create_game_server();
void initialize_map();
void add_random_obsticles();
void print_packet_hex(ENetPacket* packet);
object_t create_object(int type);

int main(int argc, char** argv) {
    
    spawn_point.x = 12;
    spawn_point.y = 19;
    spawn_point.z = MAP_HEIGHT / 2;

    ENetHost* server = create_game_server();

    if (!server) {
        fprintf(stderr, "Failed to create ENet server host.\n");
        return EXIT_FAILURE;
    }

    printf("Server started on port %d...\n", SERVER_PORT);

    initialize_map();
    printf("Map initialised\n");

    while (1) {
        // printf("working\n");

        ENetEvent event;
        // Wait up to 10000 ms for an event.
        while (enet_host_service(server, &event, 1000) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: 
                    printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
                    fflush(stdout); 

                    // Initialize the client's state:
                    player_t new_player;
                    init_player(&new_player);
                    {
                        // Store some application-specific info in peer->data
                        new_player_data_t* new_player_data = malloc(sizeof(new_player_data_t));
                        memcpy(new_player_data->map, map, sizeof(map));
                        new_player_data->player = new_player;
                        new_player_data->player_count = server->peerCount;

                        for (int i = 0; i < 64; i++) {
                            new_player_data->other_players[i] = players[i].position;
                        }

                        event.peer->data = new_player_data;


                        players[event.peer->incomingPeerID] = new_player;
                        ENetPacket* packet = enet_packet_create(new_player_data, 
                                                                sizeof(new_player_data), 
                                                                ENET_PACKET_FLAG_RELIABLE);
                        printf("send init data to peer of size: %zu\n", sizeof(*new_player_data));
                        enet_peer_send(event.peer, 0, packet);
                        print_packet_hex(packet);
                    }
                    
                    {
                        position_to_feed_t* pos_to_feed = malloc(sizeof(position_to_feed_t));
                        pos_to_feed->x = new_player.position.x;
                        pos_to_feed->y = new_player.position.y;
                        pos_to_feed->z = new_player.position.z;

                        pos_to_feed->index = event.peer->incomingPeerID;

                        ENetPacket* packet = enet_packet_create(&pos_to_feed, 
                                                                sizeof(pos_to_feed), 
                                                                ENET_PACKET_FLAG_RELIABLE);
                        printf("send notify others data of size: %zu\n", sizeof(*pos_to_feed));
                        enet_host_broadcast(server, 0, packet);
                        print_packet_hex(packet);
                    }
                    break;

                case ENET_EVENT_TYPE_RECEIVE: 
                    // We received a packet from a client.
                    // The packet's data is in event.packet->data
                    // The length is event.packet->dataLength

                    // Example: decode data to update the player's position
                    if (event.packet->dataLength == sizeof(player_t)) {
                        player_t* event_player_state_data = (player_t*)event.packet->data;
                        player_t* update_player_state = (player_t*)event.peer->data;

                        // Update the player's state on the server
                        update_player_state->position.x = event_player_state_data->position.x;
                        update_player_state->position.y = event_player_state_data->position.y;
                        update_player_state->position.z = event_player_state_data->position.z;
                        update_player_state->angleXY = event_player_state_data->angleXY;
                        update_player_state->angleZY = event_player_state_data->color;
                        update_player_state->color = event_player_state_data->angleZY;

                        // Broadcast this updated state to all other connected peers
                        ENetPacket* packet = enet_packet_create(update_player_state, 
                                                               sizeof(player_t), 
                                                               ENET_PACKET_FLAG_RELIABLE);
                        enet_host_broadcast(server, 0, packet);
                    }

                    enet_packet_destroy(event.packet);
                    break;

                case ENET_EVENT_TYPE_DISCONNECT: 
                    printf("Client disconnected.\n");
                    // Clean up any client-specific data
                    free(event.peer->data);
                    event.peer->data = NULL;
                    break;

                default:
                    break;
            }
        }
    }

    enet_host_destroy(server);
    printf("server was destroyed");
    return 0;
}

ENetHost* create_game_server() {
    if (enet_initialize() != 0) {
        fprintf(stderr, "Failed to initialize ENet.\n");
        return NULL;
    }
    atexit(enet_deinitialize);

    // Create the server host.
    ENetAddress address;
    address.host = ENET_HOST_ANY;      // Accept connections from any IP
    address.port = SERVER_PORT;

    return enet_host_create(&address,    // Our address
                            MAX_CLIENTS, // Max connections
                            2,           // Channels to use
                            0,           // incoming bandwidth
                            0);          // outgoing bandwidth
}

void init_player(player_t* player) {
    player->position = spawn_point;
    player->angleXY = M_PI / 2;
    player->angleZY = 0;
    player->color = 0;
}

void initialize_map() {
    for (int k = 0; k < MAP_HEIGHT; k++) {
        for (int i = 0; i < MAP_SIZE; i++) {
            for (int j = 0; j < MAP_SIZE; j++) {
                // Set borders
                if (i == 0 || i == MAP_SIZE - 1 || j == 0 || j == MAP_SIZE - 1 || k == 0 || k == MAP_HEIGHT - 1) {
                    map[k][i][j] = create_object(OBSTICLE_TYPE); 
                } else {
                    map[k][i][j] = create_object(VOID_TYPE); 
                }
            }
        }
        printf("Finished z level: %d \n", k);
    }
    printf("Adding obsticles\n");
    // add_random_obsticles();
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
            map[z][y][x] = create_object(OBSTICLE_TYPE); 
        }
        // map[z][y][x] = create_object(OBSTICLE_TYPE); 

    }

    for (int k = MAP_HEIGHT*0.1; k < MAP_HEIGHT * 0.8; k++){
        for (int j = 10; j < 40; j++) {
            map[k][20][j] = create_object(MIRROR_TYPE); // Horizontal wall at row 20
        }
    }
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
        object.color = 0;
        break;
    default:
        return object;
    }

    return object;
}

void print_packet_hex(ENetPacket* packet) {
    for (size_t i = 0; i < packet->dataLength; ++i) {
        printf("%02X ", packet->data[i]); // Print byte as hex
    }
    printf("\n");
}