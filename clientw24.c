#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>

#define BUFFER_SIZE 1024
#define COUNTER_FILE_PATH "client_count.txt"

int globalSocket = -1; // Global socket descriptor, accessible across different functions for network operations

// Function declarations
void handleSIGINT(int signalNumber);
void resetClientCounters();
void incrementClientCounter(int *currentCount);
int calculateServerPort(int clientCount);
void initiateServerRequest(const char* serverIP, int serverPort);
void processServerResponse();
void downloadFile(const char *fileName, int socketDescriptor);
void validateDirectory(const char* directoryPath);

int main(int argc, char *argv[]) {
    // It will verify the right number of command-line args
    if (argc < 2 || argc > 3) {
        fprintf(stderr, "Usage: %s <server IP> [--reset]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Handle the '--reset' command-line argument to reset the client counter
    if (argc == 3 && strcmp(argv[2], "--reset") == 0) {
        resetClientCounters();
        return 0;
    }

    // Setup a signal handler for SIGINT to allow for graceful termination
    signal(SIGINT, handleSIGINT);

    // Variable to store the number of clients that have connected to the server
    int clientCount;
    incrementClientCounter(&clientCount);

    // Extract the server IP from command-line arguments and determine the server port based on the client count
    const char* serverIP = argv[1];
    int serverPort = calculateServerPort(clientCount);

    // Initiate a request to the server with the determined IP and port
    initiateServerRequest(serverIP, serverPort);
    return 0;
}

void handleSIGINT(int signalNumber) {
    // Handle SIGINT (Ctrl+C) to ensure that the socket is closed properly
    printf("\nInterrupt signal received. Exiting...\n");
    if (globalSocket != -1) {
        close(globalSocket);
    }
    exit(0);
}

void resetClientCounters() {
    // Open the client count file for writing to reset its value
    FILE *file = fopen(COUNTER_FILE_PATH, "w");
    if (!file) {
        perror("Failed to open client count file for reset");
        exit(EXIT_FAILURE);
    }
    fprintf(file, "0"); // Reset counter to zero
    fclose(file);
    printf("Client counter has been reset.\n");
}

void incrementClientCounter(int *count) {
    // Attempt to open the client count file for reading and writing
    FILE *file = fopen(COUNTER_FILE_PATH, "r+");
    if (!file) {
        // If opening fails, attempt to create the file
        file = fopen(COUNTER_FILE_PATH, "w+");
        if (!file) {
            perror("Failed to access client count file");
            exit(EXIT_FAILURE);
        }
    }
    // Read the current count, increment it, and write it back
    if (fscanf(file, "%d", count) != 1) {
        *count = 0; // Default to zero if the file is empty or unreadable
    }
    (*count)++;
    fseek(file, 0, SEEK_SET);
    fprintf(file, "%d", *count);
    fclose(file);
}

int calculateServerPort(int clientCount) {
    // Determine the appropriate server port based on the number of clients
    const int basePort = 6969;
    if (clientCount <= 3) return basePort; // Main server for first 3 clients
    else if (clientCount <= 6) return basePort + 1; // First mirror for next 3 clients
    else if (clientCount <= 9) return basePort + 2; // Second mirror for next 3 clients
    else return basePort + ((clientCount - 1) % 3); // Use round-robin distribution for any additional clients
}

void initiateServerRequest(const char* serverIP, int serverPort) {
    // Setup and connect to the server using TCP
    struct sockaddr_in serverAddr;
    globalSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (globalSocket == -1) {
        perror("Failed to create socket");
        exit(EXIT_FAILURE);
    }

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = inet_addr(serverIP);
    serverAddr.sin_port = htons(serverPort);

    if (connect(globalSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("Connection to server failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to %s on port %d. Type your commands below.\n", serverIP, serverPort);

    // Main loop to handle user input and send commands to the server
    char command[BUFFER_SIZE];
    while (1) {
        printf("Enter command ('dirlis -a', 'dirlist -t', 'w2fn <filename>', 'w24fz <size1> <size2>', 'w24ft <extensions>', 'w24fdb <date>', 'w24fda <date>'):- ");
        fgets(command, BUFFER_SIZE, stdin);
        command[strcspn(command, "\n")] = 0; // Remove newline character

        if (strncmp(command, "w24fz", 5) == 0 || strncmp(command, "w24ft", 5) == 0 || 
            strncmp(command, "w24fdb", 6) == 0 || strncmp(command, "w24fda", 6) == 0) {
            send(globalSocket, command, strlen(command), 0);
            downloadFile("temp.tar.gz", globalSocket); // Download file from the server
            continue;
        }

        send(globalSocket, command, strlen(command), 0);

        if (strcmp(command, "quitc") == 0) {
            break; // Exit the loop if 'quitc' command is given
        }

        processServerResponse(); // Handle non-file responses from the server
    }

    // Close the socket when done
    if (globalSocket != -1) {
        close(globalSocket);
    }
    printf("Connection closed. Exiting client...\n");
}

void processServerResponse() {
    // Receive and display the server response
    char serverReply[BUFFER_SIZE] = {0};
    ssize_t bytesRead = recv(globalSocket, serverReply, BUFFER_SIZE - 1, 0);
    if (bytesRead > 0) {
        serverReply[bytesRead] = '\0'; // Ensure the response is null-terminated
        printf("Server response:\n%s\n", serverReply);
    } else {
        printf("No response from server or connection error.\n");
    }
}

void downloadFile(const char *fileName, int socketDescriptor) {
    // Ensure the directory exists where the file will be saved
    validateDirectory("/home/patel489/w24project");

    // Create the full path for the file
    char fullPath[1024];
    snprintf(fullPath, sizeof(fullPath), "/home/patel489/w24project/%s", fileName);

    // Receive the file size first
    int fileSize;
    recv(socketDescriptor, &fileSize, sizeof(fileSize), 0);

    // Open the file in order to write
    FILE *file = fopen(fullPath, "wb");
    if (!file) {
        perror("Failed to create file on disk");
        return;
    }

    // Receive the file in chunks and write to the file
    int totalReceived = 0, bytesReceived;
    char buffer[BUFFER_SIZE];
    while (totalReceived < fileSize) {
        bytesReceived = recv(socketDescriptor, buffer, BUFFER_SIZE, 0);
        if (bytesReceived > 0) {
            fwrite(buffer, 1, bytesReceived, file);
            totalReceived += bytesReceived;
        } else if (bytesReceived == 0) {
            break; // Connection closed by server
        } else {
            perror("File receive error");
            break;
        }
    }

    // Close the file and report success
    fclose(file);
    printf("File downloaded successfully: %s\n", fullPath);
}

void validateDirectory(const char* directoryPath) {
    // It will check whether the directory exists or not. If not, it will create one
    struct stat st = {0};

    if (stat(directoryPath, &st) == -1) {
        mkdir(directoryPath, 0700); // Creates the directory with full permissions for the owner only
    }
}
