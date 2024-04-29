#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define SERVER_PORT 6969
#define BUFFER_SIZE 1024
#define TEMP_DIRECTORY "/home/patel489/server_temp"

// Function prototypes, describing the actions and parameters
int findFileInDirectory(const char* directoryPath, const char* targetFilename, char* resultInfo, size_t maxInfoLength);
void listDirectoryContents(int socket, const char* sortFlag);
int sortByModificationTime(const struct dirent **a, const struct dirent **b);
void searchByFileSizeAndArchive(int socket, long minSize, long maxSize);
void searchByFileExtensionAndArchive(int socket, char fileTypes[][10], int fileTypeCount);
void searchByDateBeforeAndArchive(int socket, char* dateString);
void searchByDateAfterAndArchive(int socket, char* dateString);
void archiveFilesAndSend(int socket, char* archivePath, int operationResult);
void ensureDirectoryExists(const char* path);

// Main server process that listens and accepts client connections
int main() {
    ensureDirectoryExists(TEMP_DIRECTORY);  // Ensure the temporary directory exists

    int serverSocket, clientSocket, clientStructSize;
    struct sockaddr_in serverAddr, clientAddr;

    // Create TCP socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        printf("Socket creation Unsuccessful\n");
        return 1;
    }

    // Setup server address
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    // Bind socket to the server address
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind failed. Error");
        return 1;
    }

    // Start listening for client connections
    listen(serverSocket, 3);
    printf("Server listening on port %d...\n", SERVER_PORT);
    clientStructSize = sizeof(struct sockaddr_in);

    // Continuously accept client connections and handle them in child processes
    while ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, (socklen_t*)&clientStructSize))) {
        pid_t processID = fork();

        if (processID == 0) { // Child process handles client requests
            crequest(clientSocket);
            exit(0);
        } else if (processID > 0) { // Parent process goes back to listening
            close(clientSocket);
        } else { // Fork failed
            perror("fork failed");
        }
    }

    if (clientSocket < 0) {
        perror("Sorry! Cannot Accept");
        return 1;
    }

    return 0;
}

// Function to handle client requests
void crequest(int socket) {
    char commandBuffer[BUFFER_SIZE];

    while (1) {
        memset(commandBuffer, 0, BUFFER_SIZE);
        ssize_t bytesRead = recv(socket, commandBuffer, BUFFER_SIZE - 1, 0);
        commandBuffer[bytesRead] = '\0'; // Ensure the command is NULL-terminated

        if (bytesRead <= 0 || strcmp(commandBuffer, "quitc") == 0) {
            break; // Exit loop if client disconnects or sends quit command
        }

        // Process command by removing any newline characters
        commandBuffer[strcspn(commandBuffer, "\n")] = 0;
        commandBuffer[strcspn(commandBuffer, "\r")] = 0;

        // Handle different commands for various operations
        if (strncmp(commandBuffer, "w24fn ", 6) == 0 && strlen(commandBuffer) > 6) {
            char* filename = commandBuffer + 6;
            char fileInfo[BUFFER_SIZE] = {0};
            if (!findFileInDirectory("/home/patel489", filename, fileInfo, sizeof(fileInfo))) {
                char* msg = "File is not present\n";
                send(socket, msg, strlen(msg), 0);
            } else {
                send(socket, fileInfo, strlen(fileInfo), 0);
            }
        } else if (strcmp(commandBuffer, "dirlist -a") == 0) {
            listDirectoryContents(socket, "-a");
        } else if (strcmp(commandBuffer, "dirlist -t") == 0) {
            listDirectoryContents(socket, "-t");
        } else if (strncmp(commandBuffer, "w24fz ", 6) == 0 && strlen(commandBuffer) > 6) {
            long size1, size2;
            sscanf(commandBuffer + 6, "%ld %ld", &size1, &size2);
            searchByFileSizeAndArchive(socket, size1, size2);
        } else if (strncmp(commandBuffer, "w24ft ", 6) == 0 && strlen(commandBuffer) > 6) {
            char fileTypes[3][10];
            int count = sscanf(commandBuffer + 6, "%s %s %s", fileTypes[0], fileTypes[1], fileTypes[2]);
            searchByFileExtensionAndArchive(socket, fileTypes, count);
        } else if (strncmp(commandBuffer, "w24fdb ", 7) == 0 && strlen(commandBuffer) > 7) {
            char* dateString = commandBuffer + 7;
            searchByDateBeforeAndArchive(socket, dateString);
        } else if (strncmp(commandBuffer, "w24fda ", 7) == 0 && strlen(commandBuffer) > 7) {
            char* dateString = commandBuffer + 7;
            searchByDateAfterAndArchive(socket, dateString);
        } else {
            char* msg = "Invalid command or syntax error\n";
            send(socket, msg, strlen(msg), 0);
        }
    }
    close(socket);
}

// Lists contents of a directory sorted alphabetically or by modification time
void listDirectoryContents(int socket, const char* sortFlag) {
    DIR *dir;
    struct dirent **namelist;
    int n;
    char buffer[BUFFER_SIZE] = {0};
    char path[1024] = "/home/patel489"; // Path to the directory to list

    int dirFilter(const struct dirent *entry) {
        // Filter to exclude current and parent directory entries
        return (entry->d_type == DT_DIR && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0);
    }

    n = scandir(path, &namelist, dirFilter, (strcmp(sortFlag, "-a") == 0) ? alphasort : sortByModificationTime);
    if (n < 0) {
        perror("scandir");
        send(socket, "Failed to open directory.\n", 26, 0);
    } else {
        for (int i = 0; i < n; i++) {
            strcat(buffer, namelist[i]->d_name);
            strcat(buffer, "\n");
            free(namelist[i]);
        }
        free(namelist);
        send(socket, buffer, strlen(buffer), 0);
    }
}

// Compares two directory entries by modification time for sorting
int sortByModificationTime(const struct dirent **a, const struct dirent **b) {
    struct stat sa, sb;
    char pathA[1024], pathB[1024];
    snprintf(pathA, sizeof(pathA), "/home/patel489/%s", (*a)->d_name);
    snprintf(pathB, sizeof(pathB), "/home/patel489/%s", (*b)->d_name);
    stat(pathA, &sa);
    stat(pathB, &sb);
    return (sa.st_ctime > sb.st_ctime) - (sa.st_ctime < sb.st_ctime);
}

// Recursively searches for a file within a directory and subdirectories
int findFileInDirectory(const char* directoryPath, const char* targetFilename, char* resultInfo, size_t maxInfoLength) {
    DIR* dir;
    struct dirent* entry;
    char path[1024];
    struct stat fileInfo;

    if (!(dir = opendir(directoryPath))) {
        return 0;  // Unable to open directory
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;  // Skip hidden files and directories
        }

        snprintf(path, sizeof(path), "%s/%s", directoryPath, entry->d_name);

        if (stat(path, &fileInfo) != 0) continue; // Skip if unable to get file info

        if (S_ISDIR(fileInfo.st_mode)) {  // If directory, recurse into it
            if (findFileInDirectory(path, targetFilename, resultInfo, maxInfoLength)) {
                closedir(dir);
                return 1;  // File found in subdirectory
            }
        } else if (strcmp(entry->d_name, targetFilename) == 0) {
            // When target file is found, format file information
            char timeBuffer[100];
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localtime(&fileInfo.st_mtime));
            snprintf(resultInfo, maxInfoLength, "%s, Size: %ld bytes, Modified: %s, Permissions: %o",
                     entry->d_name, fileInfo.st_size, timeBuffer, fileInfo.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
            closedir(dir);
            return 1;
        }
    }
    closedir(dir);
    return 0;  // File not found after checking all files
}

// Searches for files within a specific size range, archives them, and sends the archive to the client
void searchByFileSizeAndArchive(int socket, long minSize, long maxSize) {
    char findCommand[1024];
    char archivePath[1024];
    sprintf(archivePath, "%s/temp.tar.gz", TEMP_DIRECTORY);

    // Construct the find command to locate files within the specified size range and exclude hidden files
    sprintf(findCommand, "find /home/patel489 -maxdepth 2 -type f -size +%ldc -size -%ldc ! -name '.*' -print0 | tar -czvf %s --null -T - > /dev/null 2>&1", minSize, maxSize, archivePath);
    int result = system(findCommand);
    archiveFilesAndSend(socket, archivePath, result);
}

// Searches for files matching specific file extensions, archives them, and sends the archive
void searchByFileExtensionAndArchive(int socket, char fileTypes[][10], int fileTypeCount) {
    char findCommand[1024];
    char archivePath[1024];
    char extensionFilter[512] = "";

    sprintf(archivePath, "%s/temp.tar.gz", TEMP_DIRECTORY);
    strcat(extensionFilter, "\\( ! -name '.*' "); // Start condition group to exclude hidden files

    for (int i = 0; i < fileTypeCount; i++) {
        if (i > 0) strcat(extensionFilter, " -o ");
        strcat(extensionFilter, "-name '*.");
        strcat(extensionFilter, fileTypes[i]);
        strcat(extensionFilter, "'");
    }
    strcat(extensionFilter, " \\)"); // Close condition group

    sprintf(findCommand, "find /home/patel489 -maxdepth 1 -type f %s -print0 | tar -czvf %s --null -T - > /dev/null 2>&1", extensionFilter, archivePath);
    int result = system(findCommand);
    archiveFilesAndSend(socket, archivePath, result);
}

// Searches for files modified before a specified date, archives them, and sends the archive
void searchByDateBeforeAndArchive(int socket, char* dateString) {
    char archivePath[1024];
    sprintf(archivePath, "%s/temp.tar.gz", TEMP_DIRECTORY);

    char findCommand[2048];
    char adjustedDateString[20];
    struct tm tm = {0};
    if (strptime(dateString, "%Y-%m-%d", &tm) != NULL) {
        tm.tm_mday += 1;  // Adjust the day to include all files from the specified day
        strftime(adjustedDateString, sizeof(adjustedDateString), "%Y-%m-%d", &tm);
    } else {
        strncpy(adjustedDateString, dateString, sizeof(adjustedDateString));
    }

    // Construct the find command to locate files modified before the specified date
    sprintf(findCommand, "find /home/patel489 -maxdepth 1 -type f ! -newermt '%s' ! -name '.*' -print0 | tar -czvf %s --null -T - > /dev/null 2>&1", adjustedDateString, archivePath);
    int result = system(findCommand);
    archiveFilesAndSend(socket, archivePath, result);
}

// Searches for files modified after a specified date, archives them, and sends the archive
void searchByDateAfterAndArchive(int socket, char* dateString) {
    char archivePath[1024];
    sprintf(archivePath, "%s/temp.tar.gz", TEMP_DIRECTORY);

    char findCommand[2048];
    sprintf(findCommand, "find /home/patel489 -maxdepth 2 -type f ! -name '.*' -newermt '%s' -print0 | tar -czvf %s --null -T - > /dev/null 2>&1", dateString, archivePath);
    int result = system(findCommand);
    archiveFilesAndSend(socket, archivePath, result);
}

// Sends the archived files to the client, handling the file transfer and error management
void archiveFilesAndSend(int socket, char* archivePath, int operationResult) {
    if (operationResult == 0) {
        struct stat fileInfo;
        if (stat(archivePath, &fileInfo) == 0 && fileInfo.st_size > 0) {
            int fileSize = fileInfo.st_size;
            send(socket, &fileSize, sizeof(fileSize), 0);  // First, send the size of the file

            FILE *file = fopen(archivePath, "rb");
            if (file) {
                char buffer[BUFFER_SIZE];
                int bytesRead;
                while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
                    send(socket, buffer, bytesRead, 0);  // Send the file content in chunks
                }
                fclose(file);
            } else {
                char *msg = "Error opening file.\n";
                send(socket, msg, strlen(msg), 0);
            }
        } else {
            char *msg = "No file found or file created is empty.\n";
            send(socket, msg, strlen(msg), 0);
        }
    } else {
        perror("Failed to create tar file");
        char *msg = "Failed to create tar file.\n";
        send(socket, msg, strlen(msg), 0);
    }
    remove(archivePath);  // Clean up the temporary file
}

// Checks if a directory exists, and creates it if it does not
void ensureDirectoryExists(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0775);  // Creates the directory with read, write, and execute permissions for the owner and group
    }
}
