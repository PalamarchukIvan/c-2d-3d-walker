#include <stdio.h>
#include <stdlib.h>
#include <math.h> 
#include <string.h>
#include <enet/enet.h>

#define MAP_SIZE 40
#define MAP_HEIGHT 20

#define MAX_CLIENTS 64
#define SERVER_PORT 1111
#define CHANEL_COUNT 64

#define PLAYER_UPDATE_EVENT 1
#define MAP_UPDATE_EVENT 2

#define MAP_SIZE 40
#define MAP_HEIGHT 20
#define PLAYER_AVATAR '@'
#define OBSTICLE '#'
#define MIRROR 'M'

int GLOBAL_PLAYER_COUNT = 0;

// outcome
const int OUTCOME_MAP_UPDATES_CHANEL = 0;
const int OUTCOME_NEW_PLAYER_CHANEL = 1;
const int OUTCOME_LIST_OF_PLAYERS_CHANEL = 3;
const int OUTCOME_NEW_PLAYER_POSITION = 4;

// income
const int INCOME_PLAYER_UPDATED_CHANEL = 2;

const int VOID_TYPE = 0;
const int OBSTICLE_TYPE = 1;
const int MIRROR_TYPE = 2;

const char OBSTICLE_SYMBOL = '#';
const char MIRROR_SYMBOL = 'M';
const char EMPTY_SYMBOL = '.';

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

void init_player(player_t* player);
ENetHost* create_game_server();
void initialize_map();
void add_random_obsticles();
void print_packet_hex(ENetPacket* packet);
object_t create_object(int type);

void send_data_to_new_player(player_t* new_player, ENetEvent* event, ENetHost* server);

char* serialize_player_t(player_t* player);
char* serialize_positions(position_t* positions, size_t count);
char* serialize_map();

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
        ENetEvent event;
        // Wait up to 10000 ms for an event.
        while (enet_host_service(server, &event, 1000) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: 
                    GLOBAL_PLAYER_COUNT++;
                    printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
                    fflush(stdout); 

                    // Initialize the client's state:
                    player_t new_player;
                    init_player(&new_player);
                    
                    send_data_to_new_player(&new_player, &event, server);

                    // {
                    //     position_to_feed_t* pos_to_feed = malloc(sizeof(position_to_feed_t));
                    //     pos_to_feed->x = new_player.position.x;
                    //     pos_to_feed->y = new_player.position.y;
                    //     pos_to_feed->z = new_player.position.z;

                    //     pos_to_feed->index = event.peer->incomingPeerID;

                    //     ENetPacket* packet = enet_packet_create(pos_to_feed, 
                    //                                             sizeof(pos_to_feed), 
                    //                                             ENET_PACKET_FLAG_RELIABLE);
                    //     printf("send new_player_position data of size: %zu\n", sizeof(*pos_to_feed));
                    //     enet_host_broadcast(server, 0, packet);
                    //     print_packet_hex(packet);
                    // }
                    break;

                case ENET_EVENT_TYPE_RECEIVE: 
                    switch (event.channelID) {
                    case INCOME_PLAYER_UPDATED_CHANEL:
                        
                        break;
                    default:
                        printf("Unrecognisable event from chanel %d, data: %s", event.channelID, event.packet->data);
                        break;
                    }
                    break;

                case ENET_EVENT_TYPE_DISCONNECT: 
                    GLOBAL_PLAYER_COUNT--;
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

void send_data_to_new_player(player_t* new_player, ENetEvent* event, ENetHost* server) {
        players[event->peer->incomingPeerID] = *new_player;

        char* serialised_map = serialize_map(map);
        // sending the world to peer
        ENetPacket* map_packet = enet_packet_create(serialised_map, 
                                                strlen(serialised_map), 
                                                ENET_PACKET_FLAG_RELIABLE);
        printf("send world data to peer of size: %zu\n", sizeof(map));
        enet_peer_send(event->peer, OUTCOME_MAP_UPDATES_CHANEL, map_packet);
        // print_packet_hex(map_packet);
        printf("serialised map: %s\n", serialised_map);
        for (int i = 0; i < MAP_SIZE; i++) {
            for (int j = 0; j < MAP_SIZE; j++) {
                printf("%d", map[0][i][j].type);
            }
            printf("\n");
        }
        printf("\n");

        // sending a new player to peer
        char* serialised_player = serialize_player_t(new_player);
        ENetPacket* player_packet = enet_packet_create(serialised_player, 
                                                strlen(serialised_player), 
                                                ENET_PACKET_FLAG_RELIABLE);
        printf("send player data to peer of size: %zu\n", strlen(serialised_player));
        printf("data: %s\n", serialised_player);
        printf("player: x: %f, y: %f, z: %f, xy: %f, zy: %f color: %d\n",
                 new_player->position.x, new_player->position.y, new_player->position.z, new_player->angleXY, new_player->angleZY, new_player->color);
        enet_peer_send(event->peer, OUTCOME_NEW_PLAYER_CHANEL, player_packet);
        print_packet_hex(player_packet);

        // sending a other_players to peer
        position_t other_players[MAX_CLIENTS];
        memset(other_players, 0, sizeof(other_players));
        for (int i = 0; i < 64; i++) {
            other_players[i] = players[i].position;
        }
        char* serialised_positions = serialize_positions(other_players, GLOBAL_PLAYER_COUNT);
        ENetPacket* player_positions_data_packet = enet_packet_create(serialised_positions, 
                                                strlen(serialised_positions), 
                                                ENET_PACKET_FLAG_RELIABLE);
        printf("send players positions data to peer of size: %zu\n", strlen(player_positions_data_packet));
        printf("data: %s\n", player_positions_data_packet);
        enet_peer_send(event->peer, OUTCOME_LIST_OF_PLAYERS_CHANEL, player_positions_data_packet);
        print_packet_hex(player_positions_data_packet);
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
                            CHANEL_COUNT,           // Channels to use
                            0,           // incoming bandwidth
                            0);          // outgoing bandwidth
}

void init_player(player_t* player) {
    // player->position = spawn_point;
    player->position.x = 12;
    player->position.y = 19;
    player->position.z = MAP_HEIGHT / 2;
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

char* serialize_player_t(player_t* player) {
    if (!player) return NULL;

    // Allocate memory for the serialized string
    char* buffer = malloc(256);
    if (!buffer) {
        perror("Failed to allocate memory for serialization");
        return NULL;
    }

    // Format the struct into the string
    snprintf(buffer, 256, "|%f|%f|%f|%f|%f|%d",
             player->position.x, player->position.y, player->position.z,
             player->angleXY, player->angleZY, player->color);

    return buffer; // Caller must free this memory
}

char* serialize_positions(position_t* positions, size_t count) {
    if (!positions || count == 0) return NULL;

    // Allocate a buffer for serialization
    size_t buffer_size = count * 64; // Estimate: 64 characters per position
    char* buffer = malloc(buffer_size);
    if (!buffer) {
        perror("Failed to allocate memory for serialization");
        return NULL;
    }
    buffer[0] = '\0'; // Initialize as an empty string

    // Append each position to the buffer
    for (size_t i = 0; i < count; ++i) {
        char line[64];
        snprintf(line, sizeof(line), "%lf|%lf|%lf\n",
                 positions[i].x, positions[i].y, positions[i].z);
        strcat(buffer, line);
    }

    return buffer; // Caller must free this memory
}

char* serialize_map() {
    // Calculate required buffer size
    size_t buffer_size = MAP_HEIGHT * MAP_SIZE * MAP_SIZE * 10 + 1; // 10 chars for |{color} {type} {symbol}| per object
    char* buffer = malloc(buffer_size);

    if (buffer == NULL) {
        fprintf(stderr, "Failed to allocate memory for serialization.\n");
        return NULL;
    }

    char temp[50]; // Temporary buffer for each object
    buffer[0] = '\0'; // Initialize buffer as an empty string

    for (int i = 0; i < MAP_HEIGHT; i++) {
        for (int j = 0; j < MAP_SIZE; j++) {
            for (int k = 0; k < MAP_SIZE; k++) {
                snprintf(temp, sizeof(temp), "%d %d %c|", map[i][j][k].color, map[i][j][k].type, map[i][j][k].symbol);
                if (strlen(buffer) + strlen(temp) >= buffer_size - 1) {
                    fprintf(stderr, "Buffer overflow detected during serialization.\n");
                    free(buffer);
                    return NULL;
                }
                strcat(buffer, temp);
            }
        }
    }
    return buffer;
}