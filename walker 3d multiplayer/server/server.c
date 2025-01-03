#include <stdio.h>
#include <stdlib.h>
#include <math.h> 
#include <string.h>
#include <enet/enet.h>
#include "../map_module.h"

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

position_t spawn_point;
player_t players[MAX_CLIENTS];

void init_player(player_t* player);
ENetHost* create_game_server();
void add_random_obsticles();
void print_packet_hex(ENetPacket* packet);

void send_data_to_new_player(player_t* new_player, ENetEvent* event, ENetHost* server);
void send_player_position_update_data(player_t* player, ENetHost* server, int index);

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
        while (enet_host_service(server, &event, 0) > 0) {
            switch (event.type) {
                case ENET_EVENT_TYPE_CONNECT: 
                    GLOBAL_PLAYER_COUNT++;
                    printf("A new client connected from %x:%u.\n", event.peer->address.host, event.peer->address.port);
                    fflush(stdout); 

                    // Initialize the client's state:
                    player_t new_player;
                    init_player(&new_player);
                    
                    send_data_to_new_player(&new_player, &event, server);
                    send_player_position_update_data(&new_player, server, event.peer->incomingPeerID);
                    break;

                case ENET_EVENT_TYPE_RECEIVE: 
                    switch (event.channelID) {
                    case INCOME_PLAYER_UPDATED_CHANEL:
                        printf("Player %d send an update: %s\n", event.peer->incomingPeerID, event.packet->data);
                        player_t* update_from_player = malloc(sizeof(player_t));
                        int update_result = deserialize_player(event.packet->data, update_from_player);
                        if(update_result == -1) {
                            printf("Failed to deserialise player data\n");
                            free(update_from_player);
                            break;
                        }

                        players[event.peer->incomingPeerID] = *update_from_player;
                        send_player_position_update_data(update_from_player, server, event.peer->incomingPeerID);
                        break;
                    default:
                        printf("Unrecognisable event from chanel %d, data: %s\n", event.channelID, event.packet->data);
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

void send_player_position_update_data(player_t* player, ENetHost* server, int index) {
    player_position_t* pos_to_feed = malloc(sizeof(player_position_t));
    pos_to_feed->x = player->position.x;
    pos_to_feed->y = player->position.y;
    pos_to_feed->z = player->position.z;

    pos_to_feed->index = index;

    char* serialised_pos = serialize_player_positions(pos_to_feed);
    printf("serialised_pos: %s\n", serialised_pos);

    ENetPacket* packet = enet_packet_create(serialised_pos, 
                                            strlen(serialised_pos), 
                                            ENET_PACKET_FLAG_RELIABLE);
    printf("send new_player_position data of size: %zu\n", strlen(serialised_pos));
    enet_host_broadcast(server, OUTCOME_NEW_PLAYER_POSITION, packet);
    printf("%s\n", serialised_pos);
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
        char* serialised_player = serialize_player(new_player);
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

void print_packet_hex(ENetPacket* packet) {
    for (size_t i = 0; i < packet->dataLength; ++i) {
        printf("%02X ", packet->data[i]); // Print byte as hex
    }
    printf("\n");
}
