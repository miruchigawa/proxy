/*
The observer program help you analyze and monitor packet traffic.

gcc -DVERBOSE ../src/core/HttpService.c ../src/config.c ../src/utils/string.c main.c -o main \
-I ../include/ \
-I ../thirdparty/tlse \
-L ../build/ -lEnet -lTlse

don't forget to copy the config file.
or just run make command.

TODO: switch to subserver (?)
*/

#include <ctype.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

#include <enet/enet.h>
#include "core/HttpService.h"
#include "config.h"
#include "utils/string.h"

typedef struct {
    char* meta;
    char isMetaMalloc;
    char* wk;
    char* rid;
    char* mac;
    char* gid;
    char* deviceID;
    char isLogin;
} user_t;

user_t user;
ENetPeer *ENetServerPeer;
ENetPeer *ENetRelayPeer;
char isSendToServer;

int main(void)
{
    ENetAddress *ENetGrowtopiaAddress;
    ENetAddress *ENetProxyAddress;

    if (!InitConfig()) {
        printf("Failed to initialize config.\n");
        return 1;
    }

    puts("Starting web server...");
    pthread_t server;
    pthread_create(&server, NULL, HTTPSServer, NULL);
    
    memset(&user, 0, sizeof(user_t));
    user.wk = generateHex(16);
    user.rid = generateHex(16);
    user.deviceID = generateHex(16);
    user.mac = generateHex(0);

    ENetProxyAddress = malloc(sizeof(ENetAddress));

    ENetProxyAddress->host = 0;
    ENetProxyAddress->port = 17091;
    
    enet_initialize();

    ENetHost *enetServer = enet_host_create(ENetProxyAddress, 1024, 10, 0, 0);
    enetServer->checksum = enet_crc32;
    enet_host_compress_with_range_coder(enetServer);

    ENetHost *enetRelay = enet_host_create(NULL, 1, 2, 0, 0);
    enetRelay->checksum = enet_crc32;
    enetRelay->usingNewPacket = 1;
    enet_host_compress_with_range_coder(enetRelay);
    
    puts("starting event loop...");
    while (1) {
        ENetEvent enetServerEvent;
        ENetEvent enetRelayEvent;

        while (enet_host_service(enetServer, &enetServerEvent, 5) > 0) {
            ENetServerPeer = enetServerEvent.peer;
            switch (enetServerEvent.type) {
            case ENET_EVENT_TYPE_CONNECT:
                puts("[SERVER EVENT] connected to the server's peer");

                struct HTTPInfo info;
                info = HTTPSClient(config->serverDataIP);

                char** arr = strsplit(info.buffer + (findstr(info.buffer, "server|") - 7), "\n", 0);

                enet_address_set_host(ENetGrowtopiaAddress, arr[findarray(arr, "server|")] + 7);
                ENetGrowtopiaAddress->port = atoi(arr[findarray(arr, "port|")] + 5);
                ENetRelayPeer = enet_host_connect(enetRelay, ENetGrowtopiaAddress, 2, 0);
                asprintf(&user.meta, "%s", arr[findarray(arr, "meta|")] + 5);

                free(arr);
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                puts("[SERVER EVENT] packet received from the server peer...");
                puts("[SERVER EVENT] forward it to the relay peer...");
                for (int i = 0; i < enetServerEvent.packet->dataLength; i++) {
                    char byte = enetServerEvent.packet->data[i];
                    if (isalnum(byte))
                        putchar(byte);
                    else
                        printf(" %02hhX ", byte);
                }
                puts("");
                enet_peer_send(ENetRelayPeer, 0, enetServerEvent.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                puts("[SERVER EVENT] the server's peer disconnected.");
                break;
            default:
                break;
            }
        }

        while(enet_host_service(enetRelay, &enetRelayEvent, 5) > 0) {
            switch (enetRelayEvent.type) {
            case ENET_EVENT_TYPE_CONNECT:
                puts("[RELAY EVENT] the relay connected to the Growtopia server peer");
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                puts("[RELAY EVENT] packet received from the relay peer...");
                puts("[RELAY EVENT] forward it to the server peer...");
                for (int i = 0; i < enetRelayEvent.packet->dataLength; i++) {
                    char byte = enetRelayEvent.packet->data[i];
                    if (isalnum(byte))
                        putchar(byte);
                    else
                        printf(" %02hhX ", byte);
                }
                puts("");
                enet_peer_send(ENetServerPeer, 0, enetRelayEvent.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                puts("[RELAY EVENT] the relay peer disconnected.");
                break;
            default:
                break;
            }
        }
    }
    return 0;
}