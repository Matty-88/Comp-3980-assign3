#include <arpa/inet.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 256
#define PORT 12345

void handle_signal(int sig);
void process_client(int client_fd);
void custom_error(const char *message) __attribute__((noreturn));
void process_message(const char *filter, char *message);
void apply_upper_filter(char *message);
void apply_lower_filter(char *message);
void apply_none_filter(const char *message);

void process_message(const char *filter, char *message)
{
    // Apply the specified filter
    if(strcmp(filter, "upper") == 0)
    {
        apply_upper_filter(message);
    }
    else if(strcmp(filter, "lower") == 0)
    {
        apply_lower_filter(message);
    }
    else if(strcmp(filter, "none") == 0)
    {
        apply_none_filter(message);
    }
    else
    {
        fprintf(stderr, "Unknown filter type: %s\n. Please use 'upper'/'lower'/'none'", filter);
        return;
    }
}

// Function to convert message to uppercase
void apply_upper_filter(char *message)
{
    for(int i = 0; message[i] != '\0'; i++)
    {
        message[i] = (char)toupper((unsigned char)message[i]);
    }
}

// Function to convert message to lowercase
void apply_lower_filter(char *message)
{
    for(int i = 0; message[i] != '\0'; i++)
    {
        message[i] = (char)tolower((unsigned char)message[i]);
    }
}

// Function that leaves message unchanged
void apply_none_filter(const char *message)
{
    (void)message;
}

int main(void)
{
    int server_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;
    socklen_t          client_len;
    pid_t              child_pid;
    int                opt = 1;

    // Set up signal handling for graceful termination
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if(sigaction(SIGINT, &sa, NULL) == -1)
    {
        perror("sigaction failed");
        exit(EXIT_FAILURE);
    }

    // Create TCP socket
    // NOLINTNEXTLINE(android-cloexec-socket)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Allow socket address reuse
    if(setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1)
    {
        perror("Setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Bind the socket to an IP and port
    // bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family      = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);    // Bind to any available interface
    server_addr.sin_port        = htons(PORT);

    if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // Start listening
    if(listen(server_fd, SOMAXCONN) == -1)
    {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port %d...\n", PORT);

    // Main loop to accept and handle client connections

    while(1)
    {
        int client_fd;
        client_len = sizeof(client_addr);
        client_fd  = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if(client_fd == -1)
        {
            if(errno == EINTR)
            {
                // Interrupted by signal, check exit_flag
                printf("\nServer shutting down.\n");
                break;
            }

            perror("Accept failed");
            continue;
        }

        // Fork a new process to handle the client
        child_pid = fork();
        if(child_pid == -1)
        {
            perror("Fork failed");
            close(client_fd);
            continue;
        }
        if(child_pid == 0)
        {
            // Child process
            close(server_fd);
            process_client(client_fd);
            close(client_fd);
            exit(EXIT_SUCCESS);
        }
        else
        {
            close(client_fd);
        }
    }

    close(server_fd);
    printf("\nServer shutting down.\n");
    return 0;
}

void handle_signal(int sig)
{
    (void)sig;
}

void process_client(int client_fd)
{
    char        buffer[BUFFER_SIZE];
    ssize_t     num_bytes;
    char       *colon_pos;
    size_t      message_len;
    const char *filter;
    char       *message;

    num_bytes = read(client_fd, buffer, BUFFER_SIZE - 1);
    if(num_bytes <= 0)
    {
        perror("Read from client failed");
        return;
    }

    buffer[num_bytes] = '\0';    // Null-terminate the string

    colon_pos = strchr(buffer, ':');
    if(!colon_pos)
    {
        fprintf(stderr, "Invalid format from client.\n");
        return;
    }

    *colon_pos = '\0';
    filter     = buffer;
    message    = colon_pos + 1;

    process_message(filter, message);
    printf("Processed Message: %s\n", message);

    message_len = strlen(message);
    if(write(client_fd, message, message_len) != (ssize_t)message_len)
    {
        perror("Write to client has failed");
    }
}

void custom_error(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}
