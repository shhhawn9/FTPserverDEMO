#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdio.h>
#include "dir.h"
#include "usage.h"
#include <pthread.h>
#include <string.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdbool.h>
#include <ifaddrs.h>


#define SERVER_PORT 8080
#define USER_NAME "shawn"
#define BUFFER_SIZE 4096
#define SERVER_BACKLOG 10
#define PORT_TIMEOUT 30
// Define dbug
#ifdef DEBUG
#define _DEBUG(fmt, args...) printf("%s:%s:%d: " fmt, __FILE__, __FUNCTION__, __LINE__, args)
#else
#define _DEBUG(fmt, args...)
#endif

// Here is an example of how to use the above function. It also shows
// one how to get the arguments passed on the command line.
/* this function is run by the second thread */

// handle commands
void* commandHandler(void* socket_client_ptr);

// send response
int sendResponse(int socket_descriptor, int code);

// send file to socket
int sendHandler(int socket_descriptor, char* fileName);

// send file helper
int sendFileHelper(int socket_descriptor, FILE* file);

// ref: https://stackoverflow.com/questions/32496497/standard-function-to-replace-character-or-substring-in-a-char-array
char* replace_char(char* str, char find, char replace);

// ref: https://stackoverflow.com/questions/4139405/how-can-i-get-to-know-the-ip-address-for-interfaces-in-c
char* getIPAddress();

int main(int argc, char** argv) {
    // Check the command line arguments
    int port = atoi(argv[1]);
    if (argc != 2) {
        usage(argv[0]);
        return -1;
    }
    int socket_descriptor,
        socket_client, addr_size;
    struct sockaddr_in server, client;
    char buffer[BUFFER_SIZE];
    int n;

    socket_descriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_descriptor < 0) {
        perror("Fail to create socket");
        return -1;
    }
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    server.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_descriptor, (struct sockaddr*)&server, sizeof(server)) < 0) {
        perror("Fail to bind socket");
        return -1;
    }
    listen(socket_descriptor, SERVER_BACKLOG);

    while (socket_client = accept(socket_descriptor, (struct sockaddr*)&client, (socklen_t*)&addr_size)) {
        int i;
        pthread_t t;
        pthread_create(&t, NULL, commandHandler, &socket_client);
        pthread_detach(t);
    }

    return 0;
}


void* commandHandler(void* socket_client_ptr) {
    int socket_client = *(int*)socket_client_ptr;
    //flags
    bool isLoggedIn = false;
    bool asciiFlag = false;
    bool imageFlag = false;
    bool streamFlag = false;
    bool fileFlag = false;
    bool isPassiveMode = false;
    //passive socket, server, port
    int passive_socket;
    struct sockaddr_in passive_server, passive_client;
    int passivePort;
    int socket_passive_client;
    int passive_addr_size;
    //data socket
    int data_socket;
    struct sockaddr_in data_socket_client;
    int data_socket_len = sizeof(data_socket_client);
    //directory path
    char actualPath[PATH_MAX];
    char startPath[PATH_MAX];
    getcwd(actualPath, PATH_MAX);
    getcwd(startPath, PATH_MAX);

    sendResponse(socket_client, 220);

    // waiting for next command
    while (true) {
        char buffer[BUFFER_SIZE];
        size_t byte_read;
        int msgsize = 0;

        while ((byte_read = read(socket_client, buffer + msgsize, sizeof(buffer) - msgsize - 1)) > 0) {
            msgsize += byte_read;
            if (msgsize > BUFFER_SIZE - 1 || buffer[msgsize - 1] == '\n')
                break;
        }
        if (byte_read <= 0) {
            break;
        }
        buffer[msgsize - 1] = 0;

        printf("REQUEST: %s\n", buffer);

        // parse the command
        char* command[1024];
        int i = 0;
        char* split = strtok(buffer, " ");

        while (split != NULL) {
            command[i] = split;
            printf("ARG%d: %s\n", i, command[i]);
            i++;
            split = strtok(NULL, " ");
        }

        if (strcasecmp(command[0], "user") == 0) {
            if (i != 2) {
                sendResponse(socket_client, 501);
                isLoggedIn = false;
            } else if (strcasecmp(command[1], "cs317") == 0) {
                isLoggedIn = true;
                sendResponse(socket_client, 230);
            } else {
                isLoggedIn = false;
                sendResponse(socket_client, 530);
            }
        } else if (strcasecmp(command[0], "quit") == 0) {
            if (i != 1) {
                sendResponse(socket_client, 501);
            } else {
                sendResponse(socket_client, 221);
                isLoggedIn = false;
                close(socket_client);
                break;
            }
        } else {
            if (!isLoggedIn) {
                sendResponse(socket_client, 503);
            } else if ((strcasecmp(command[0], "cwd") == 0)) {
                if (i != 2) {
                    sendResponse(socket_client, 501);
                } else {
                    if (strstr(command[1], "./") != NULL || strstr(command[1], "../") != NULL) {
                        sendResponse(socket_client, 550);
                    } else {
                        actualPath[strlen(actualPath)] = '/';
                        strcat(actualPath, command[1]);
                        if (opendir(actualPath) == NULL) {
                            //+1 for slash
                            int length = strlen(command[1]) + 1;
                            while (length > 0) {
                                actualPath[strlen(actualPath) - 1] = 0;
                                length--;
                            }
                            sendResponse(socket_client, 550);
                        } else {
                            printf("pathAfterCWD:%s\n", actualPath);
                            sendResponse(socket_client, 250);
                        }
                    }
                }
            } else if ((strcasecmp(command[0], "cdup") == 0)) {
                if (i != 1) {
                    sendResponse(socket_client, 501);
                } else {
                    if ((strcasecmp(actualPath, startPath) == 0)) {
                        printf("acpath:%s\n", actualPath);
                        printf("stpath:%s\n", actualPath);
                        sendResponse(socket_client, 550);
                    } else {
                        int length = strlen(actualPath);
                        char slash = '/';
                        if (actualPath[9] == slash) {
                        }
                        while (length > 0) {
                            if (actualPath[strlen(actualPath) - 1] == '/') {
                                actualPath[strlen(actualPath) - 1] = 0;
                                break;
                            }
                            actualPath[strlen(actualPath) - 1] = 0;
                            length--;
                        }
                        sendResponse(socket_client, 200);
                        printf("pathAfterCDUP:%s\n", actualPath);
                    }
                }
            } else if ((strcasecmp(command[0], "type") == 0)) {
                if (i != 2) {
                    sendResponse(socket_client, 501);
                } else {
                    if ((strcasecmp(command[1], "A") == 0)) {
                        asciiFlag = true;
                        imageFlag = false;
                        sendResponse(socket_client, 200);
                    } else if ((strcasecmp(command[1], "I") == 0)) {
                        asciiFlag = false;
                        imageFlag = true;
                        sendResponse(socket_client, 200);
                    } else {
                        sendResponse(socket_client, 504);
                    }
                }
            } else if ((strcasecmp(command[0], "mode") == 0)) {
                if (i != 2) {
                    sendResponse(socket_client, 501);
                } else {
                    if ((strcasecmp(command[1], "S") == 0)) {
                        streamFlag = true;
                        sendResponse(socket_client, 200);
                    } else {
                        sendResponse(socket_client, 504);
                    }
                }
            } else if ((strcasecmp(command[0], "stru") == 0)) {
                if (i != 2) {
                    sendResponse(socket_client, 501);
                } else {
                    if ((strcasecmp(command[1], "F") == 0)) {
                        fileFlag = true;
                        sendResponse(socket_client, 200);
                    } else {
                        sendResponse(socket_client, 504);
                    }
                }
            } else if ((strcasecmp(command[0], "pasv") == 0)) {
                if (i != 1) {
                    sendResponse(socket_client, 501);
                } else {
                    if (isLoggedIn) {
                        // find the available port
                        while (1) {
                            int seed = abs(time(0) * rand());
                            printf("%d\n", seed);
                            passivePort = 1024 + seed % 64512;
                            passive_socket = socket(AF_INET, SOCK_STREAM, 0);
                            passive_server.sin_family = AF_INET;
                            passive_server.sin_addr.s_addr = INADDR_ANY;
                            passive_server.sin_port = htons(passivePort);
                            if (bind(passive_socket, (struct sockaddr*)&passive_server, sizeof(passive_server)) >= 0) {
                                break;
                            }
                        }
                        if (passive_socket < 0) {
                            sendResponse(socket_client, 500);
                        } else {
                            printf("connection established\n");
                            if (listen(passive_socket, 1) != 0) {
                                sendResponse(socket_client, 500);
                                exit(-1);
                            }
                            isPassiveMode = true;
                            char* ip = replace_char(getIPAddress(), '.', ',');
                            char response[64];
                            int portOne = passivePort >> 8;
                            int portTwo = passivePort & 0xff;
                            sprintf(response, "227 Entering Passive Mode (%s,%d,%d)\n", ip, portOne, portTwo);
                            printf("%s\n", response);
                            send(socket_client, response, strlen(response), 0);
                        }
                    }
                }
            } else if ((strcasecmp(command[0], "nlst") == 0)) {
                if (i != 1) {
                    sendResponse(socket_client, 501);
                } else {
                    if (!isPassiveMode) {
                        sendResponse(socket_client, 425);
                    } else {
                        if (passive_socket < 0) {
                            sendResponse(socket_client, 500);
                        }
                        // timeout setup
                        struct timeval timeInterval;
                        fd_set readfd;
                        timeInterval.tv_sec = PORT_TIMEOUT;
                        timeInterval.tv_usec = 0;
                        FD_ZERO(&readfd);
                        FD_SET(passive_socket, &readfd);
                        // timeout
                        int res = select(passive_socket + 1, &readfd, NULL, NULL, &timeInterval);
                        if (res == 0) {
                            sendResponse(socket_client, 421);
                        } else {
                            socket_passive_client = accept(passive_socket, (struct sockaddr*)&passive_client, (socklen_t*)&passive_addr_size);
                            if (socket_passive_client < 0) {
                                sendResponse(socket_client, 425);
                            } else {
                                sendResponse(socket_client, 125);
                                listFiles(socket_passive_client, actualPath);
                                sendResponse(socket_client, 226);
                                close(socket_passive_client);
                            }
                        }
                    }
                }
            } else if (strcasecmp(command[0], "retr") == 0) {
                if (i != 2) {
                    sendResponse(socket_client, 501);
                } else if (isLoggedIn) {
                    if (isPassiveMode) {
                        if (passive_socket > -1) {
                            listen(passive_socket, 3);
                            // timeout setup
                            struct timeval timeInterval;
                            fd_set readfd;
                            timeInterval.tv_sec = PORT_TIMEOUT;
                            timeInterval.tv_usec = 0;
                            FD_ZERO(&readfd);
                            FD_SET(passive_socket, &readfd);
                            // timeout
                            int res = select(passive_socket + 1, &readfd, NULL, NULL, &timeInterval);
                            if (res == 0) {
                                sendResponse(socket_client, 421);
                            } else {
                                data_socket = accept(passive_socket, (struct sockaddr*)&passive_client, &passive_addr_size);
                                if (data_socket < 0) {
                                    sendResponse(socket_client, 425);
                                } else {
                                    sendResponse(socket_client, 125);
                                    char filePath[PATH_MAX * 2];
                                    sprintf(filePath, "%s/%s", actualPath, command[1]);
                                    if (sendHandler(data_socket, filePath) < 1) {
                                        sendResponse(socket_client, 125);
                                    } else {
                                        sendResponse(socket_client, 226);
                                    }
                                    close(data_socket);
                                }
                            }
                        }
                    } else {
                        sendResponse(socket_client, 425);
                    }
                }
            } else {
                sendResponse(socket_client, 500);
            }
        }
    }
}


int sendResponse(int socket_descriptor, int code) {
    char* response;
    switch (code) {
    case 220:
        response = "220 Service ready for new user.\r\n";
        break;
    case 230:
        response = "230 user logged in, proceed.\r\n";
        break;
    case 530:
        response = "530 Not logged in.\r\n";
        break;
    case 501:
        response = "501 Syntax error in parameters or arguments.\r\n";
        break;
    case 221:
        response = "221 Service closing control connection.\r\n";
        break;
    case 503:
        response = "503 Bad sequence of commands.\r\n";
        break;
    case 550:
        response = "550 Requested action not taken. File unavailable.\r\n";
        break;
    case 250:
        response = "250 Requested file action okay, completed.\r\n";
        break;
    case 200:
        response = "200 Command okay.\r\n";
        break;
    case 504:
        response = "504 Command not implemented for that parameter.\r\n";
        break;
    case 500:
        response = "500 Syntax error, command unrecognized.\r\n";
        break;
    case 425:
        response = "425 Can't open data connection.\r\n";
        break;
    case 125:
        response = "125 Data connection already open; transfer starting.\r\n";
        break;
    case 150:
        response = "150 File status okay; about to open data connection.\r\n";
        break;
    case 226:
        response = "226 Closing data connection. Requested file action successful.\r\n";
        break;
    case 421:
        response = "421 Passive data channel timed out.\r\n";
        break;
    }
    return send(socket_descriptor, response, strlen(response), 0);
}

int sendHandler(int socket_descriptor, char* fileName) {
    FILE* file;
    file = fopen(fileName, "r");
    if (!file) return -1;
    fseek(file, 0, SEEK_SET);
    if (sendFileHelper(socket_descriptor, file) < 0) {
        fclose(file);
        return -1;
    }
    if (fclose(file) != 0) return -1;
    return 1;
}

int sendFileHelper(int socket_descriptor, FILE* file) {
    char buffer[4096];
    bzero(buffer, 4096);
    int n = fread(buffer, 1, sizeof(buffer), file);
    if (write(socket_descriptor, buffer, n) < 0) {
        return -1;
    }
    return 1;
}

char* replace_char(char* str, char find, char replace) {
    char* current_pos = strchr(str, find);
    while (current_pos) {
        *current_pos = replace;
        current_pos = strchr(current_pos, find);
    }
    return str;
}

char* getIPAddress() {
    struct ifaddrs* ifap, * ifa;
    struct sockaddr_in* sa;
    char* addr;
    getifaddrs(&ifap);
    for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET) {
            if (!strcmp(ifa->ifa_name, "en0") || !strcmp(ifa->ifa_name, "eth0")) {
                sa = (struct sockaddr_in*)ifa->ifa_addr;
                addr = inet_ntoa(sa->sin_addr);
            }
        }
    }
    freeifaddrs(ifap);
    return addr;
}