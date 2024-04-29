#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define SERVER_PORT 6970
#define BUFFER_SIZE 1024
#define TEMP_DIRECTORY "/home/patel489/server_temp_mirror1"

// Function prototypes with descriptive names and purpose
int findFileInDirectory(const char* directoryPath, const char* targetFilename, char* fileInfo, size_t fileInfoSize);
void listDirectoryContents(int socket, const char* sortFlag);
int sortByModificationTime(const struct dirent **a, const struct dirent **b);
void searchByFileSizeAndArchive(int socket, long minSize, long maxSize);
void searchByFileExtensionAndArchive(int socket, char extensions[][10], int extensionCount);
void searchByDateBeforeAndArchive(int socket, char* dateStr);
void searchByDateAfterAndArchive(int socket, char* dateStr);
void archiveFilesAndSend(int socket, char* archivePath, int operationResult);
void ensureDirectoryExists(const char* path);

int main() {
    // Ensure that the necessary directory for server operations exists
    ensureDirectoryExists(TEMP_DIRECTORY);

    int serverSocket, clientSocket, addrSize;
    struct sockaddr_in serverAddr, clientAddr;

    // Formation of a socket to communicate
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket == -1) {
        printf("Could not create socket\n");
        return 1;
    }

    // Set up server address structure
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(SERVER_PORT);

    // Bind the socket to the server address
    if (bind(serverSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        perror("bind failed. Error");
        return 1;
    }

    // Start listening for client connections
    listen(serverSocket, 3);
    printf("Mirror1 Server listening on port %d...\n", SERVER_PORT);
    addrSize = sizeof(struct sockaddr_in);

    // Accept client connections and handle them in separate processes
    while ((clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddr, (socklen_t*)&addrSize))) {
        pid_t pid = fork();

        if (pid == 0) { // Child process
            crequest(clientSocket);
            exit(0);
        } else if (pid > 0) { // Parent process
            close(clientSocket);
        } else { // Error in fork
            perror("fork failed");
        }
    }

    if (clientSocket < 0) {
        perror("accept failed");
        return 1;
    }

    return 0;
}

void crequest(int socket) {
    // Function to handle client requests
    char commandBuffer[BUFFER_SIZE];
    while (1) {
        memset(commandBuffer, 0, BUFFER_SIZE);
        ssize_t bytesRead = recv(socket, commandBuffer, BUFFER_SIZE - 1, 0);
        commandBuffer[bytesRead] = '\0'; // Ensure the string is NULL-terminated

        if (bytesRead <= 0 || strcmp(commandBuffer, "quitc") == 0) {
            break;
        }

        commandBuffer[strcspn(commandBuffer, "\n")] = 0;
        commandBuffer[strcspn(commandBuffer, "\r")] = 0;

        // Process commands based on their prefix and act accordingly
        if (strncmp(commandBuffer, "w24fdb ", 7) == 0 && strlen(commandBuffer) > 7) {
            char* dateStr = commandBuffer + 7;
            searchByDateBeforeAndArchive(socket, dateStr);
        } else if (strncmp(commandBuffer, "w24ft ", 6) == 0 && strlen(commandBuffer) > 6) {
            char fileTypes[3][10];
            int fileTypeCount = sscanf(commandBuffer + 6, "%s %s %s", fileTypes[0], fileTypes[1], fileTypes[2]);
            searchByFileExtensionAndArchive(socket, fileTypes, fileTypeCount);
        } else if (strncmp(commandBuffer, "w24fn ", 6) == 0 && strlen(commandBuffer) > 6) {
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
        } else if (strncmp(commandBuffer, "w24fda ", 7) == 0 && strlen(commandBuffer) > 7) {
            char* dateStr = commandBuffer + 7;
            searchByDateAfterAndArchive(socket, dateStr);
        } else {
            char* msg = "Invalid command\n";
            send(socket, msg, strlen(msg), 0);
        }
    }
    close(socket);
}

void listDirectoryContents(int socket, const char* sortFlag) {
    // Lists the directory contents sorted either alphabetically or by modification time
    DIR *dir;
    struct dirent **namelist;
    int n;
    char buffer[BUFFER_SIZE] = {0};
    char path[1024] = "/home/patel489";

    int dirFilter(const struct dirent *entry) {
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

int sortByModificationTime(const struct dirent **a, const struct dirent **b) {
    // Comparator for sorting directory entries by modification time
    struct stat sa, sb;
    char pathA[1024], pathB[1024];
    snprintf(pathA, sizeof(pathA), "/home/patel489/%s", (*a)->d_name);
    snprintf(pathB, sizeof(pathB), "/home/patel489/%s", (*b)->d_name);
    stat(pathA, &sa);
    stat(pathB, &sb);
    return (sa.st_mtime > sb.st_mtime) - (sa.st_mtime < sb.st_mtime);
}

int findFileInDirectory(const char* directoryPath, const char* targetFilename, char* fileInfo, size_t fileInfoSize) {
    // Searches for a file within a directory and its subdirectories
    DIR* dir;
    struct dirent* entry;
    char path[1024];
    struct stat fileInfoStat;

    if (!(dir = opendir(directoryPath))) {
        return 0;  // Failed to open directory
    }

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            continue;  // Skip hidden files and directories
        }

        snprintf(path, sizeof(path), "%s/%s", directoryPath, entry->d_name);

        if (stat(path, &fileInfoStat) != 0) continue; // Skip if stat fails

        if (S_ISDIR(fileInfoStat.st_mode)) {  // Recurse into subdirectories
            if (findFileInDirectory(path, targetFilename, fileInfo, fileInfoSize)) {
                closedir(dir);
                return 1;  // File found in subdirectory
            }
        } else if (strcmp(entry->d_name, targetFilename) == 0) {
            // File found, retrieve metadata
            char timeBuffer[100];
            strftime(timeBuffer, sizeof(timeBuffer), "%Y-%m-%d %H:%M:%S", localtime(&fileInfoStat.st_mtime));
            snprintf(fileInfo, fileInfoSize, "%s, Size: %ld bytes, Modified: %s, Permissions: %o",
                     entry->d_name, fileInfoStat.st_size, timeBuffer, fileInfoStat.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO));
            closedir(dir);
            return 1;  // File found
        }
    }
    closedir(dir);
    return 0;  // File not found
}

void searchByFileSizeAndArchive(int socket, long minSize, long maxSize) {
    // Search for files within a specific size range and archive them
    char findCommand[1024];
    char archivePath[1024];
    sprintf(archivePath, "%s/temp.tar.gz", TEMP_DIRECTORY);

    // Command to find files within size range and create an archive
    sprintf(findCommand, "find /home/patel489 -maxdepth 2 -type f -size +%ldc -size -%ldc ! -name '.*' -print0 | tar -czvf %s --null -T - > /dev/null 2>&1", minSize, maxSize, archivePath);
    int result = system(findCommand);
    archiveFilesAndSend(socket, archivePath, result);
}

void searchByFileExtensionAndArchive(int socket, char extensions[][10], int extensionCount) {
    // Search for files by their extension and archive them
    char findCommand[1024];
    char archivePath[1024];
    char extensionFilter[512] = "";

    sprintf(archivePath, "%s/temp.tar.gz", TEMP_DIRECTORY);
    strcat(extensionFilter, "\\( ! -name '.*' "); // Start group for conditions and exclude hidden files

    for (int i = 0; i < extensionCount; i++) {
        if (i > 0) strcat(extensionFilter, " -o ");
        strcat(extensionFilter, "-name '*.");
        strcat(extensionFilter, extensions[i]);
        strcat(extensionFilter, "'");
    }
    strcat(extensionFilter, " \\)"); // Close group for conditions

    sprintf(findCommand, "find /home/patel489 -maxdepth 1 -type f %s -print0 | tar -czvf %s --null -T - > /dev/null 2>&1", extensionFilter, archivePath);
    int result = system(findCommand);
    archiveFilesAndSend(socket, archivePath, result);
}

void searchByDateBeforeAndArchive(int socket, char* dateString) {
    // Search for files modified before a specific date and archive them
    char archivePath[1024];
    sprintf(archivePath, "%s/temp.tar.gz", TEMP_DIRECTORY);

    char command[2048];
    char adjustedDateStr[20];
    struct tm tm = {0};
    if (strptime(dateString, "%Y-%m-%d", &tm) != NULL) {
        tm.tm_mday += 1;  // Adjust day to include all files from the day
        strftime(adjustedDateStr, sizeof(adjustedDateStr), "%Y-%m-%d", &tm);
    } else {
        strncpy(adjustedDateStr, dateString, sizeof(adjustedDateStr));
    }

    // Find command adjusted to ignore hidden files
    sprintf(command, "find /home/patel489 -maxdepth 1 -type f ! -newermt '%s' ! -name '.*' -print0 | tar -czvf %s --null -T - > /dev/null 2>&1", adjustedDateStr, archivePath);
    int result = system(command);
    archiveFilesAndSend(socket, archivePath, result);
}

void searchByDateAfterAndArchive(int socket, char* dateString) {
    // Search for files modified after a specific date and archive them
    char archivePath[1024];
    sprintf(archivePath, "%s/temp.tar.gz", TEMP_DIRECTORY);

    char command[2048];
    sprintf(command, "find /home/patel489 -maxdepth 2 -type f ! -name '.*' -newermt '%s' -print0 | tar -czvf %s --null -T - > /dev/null 2>&1", dateString, archivePath);
    int result = system(command);
    archiveFilesAndSend(socket, archivePath, result);
}

void archiveFilesAndSend(int socket, char* archivePath, int operationResult) {
    // Send archived files to the client and handle possible errors
    if (operationResult == 0) {
        struct stat fileInfo;
        if (stat(archivePath, &fileInfo) == 0 && fileInfo.st_size > 0) {
            int fileSize = fileInfo.st_size;
            send(socket, &fileSize, sizeof(fileSize), 0);  // Send file size first

            FILE *file = fopen(archivePath, "rb");
            if (file) {
                char buffer[BUFFER_SIZE];
                int bytesRead;
                while ((bytesRead = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
                    send(socket, buffer, bytesRead, 0);
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
    remove(archivePath);
}

void ensureDirectoryExists(const char* path) {
    // Check if the directory exists; if not, create it
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        mkdir(path, 0775);  // Creates the directory with appropriate permissions
    }
}
