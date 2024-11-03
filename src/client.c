#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>    // For bzero on some systems
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#define BUFFER_SIZE 256
#define PORT 12345
#define SERVER_IP "127.0.0.1"

void custom_error(const char *message) __attribute__((noreturn));
void parse_args(int argc, char **argv, const char **filter, char **input_message);

int main(int argc, char **argv)
{
    int                client_fd;
    struct sockaddr_in server_addr;
    const char        *filter        = NULL;
    char              *input_message = NULL;
    char               buffer[BUFFER_SIZE];
    ssize_t            num_bytes;
    size_t             message_len;

    // Parse command-line arguments
    parse_args(argc, argv, &filter, &input_message);

    // Create a TCP socket
    // NOLINTNEXTLINE(android-cloexec-socket)
    client_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(client_fd == -1)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set up the server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port   = htons(PORT);
    if(inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0)
    {
        custom_error("Invalid server IP address");
    }

    // Connect to the server
    if(connect(client_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Connection to server failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // Prepare the message in the format "filter:message"
    message_len = strlen(filter) + strlen(input_message) + 2;    // +2 for ':' and '\0'
    if(message_len > BUFFER_SIZE)
    {
        custom_error("Message is too long");
    }
    snprintf(buffer, BUFFER_SIZE, "%s:%s", filter, input_message);

    // Send the message to the server
    if(write(client_fd, buffer, message_len - 1) == -1)
    {    // Exclude the null terminator
        perror("Write to server failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    // Read the response from the server
    num_bytes = read(client_fd, buffer, BUFFER_SIZE - 1);
    if(num_bytes <= 0)
    {
        perror("Read from server failed");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    buffer[num_bytes] = '\0';    // Null-terminate the string
    printf("Received from server: %s\n", buffer);

    // Clean up
    close(client_fd);
    return 0;
}

void parse_args(int argc, char **argv, const char **filter, char **input_message)
{
    int opt;
    opterr = 0;

    if(argc < 3)
    {
        fprintf(stderr, "Usage: %s -u|-l|-n \"message\"\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    while((opt = getopt(argc, argv, "uln")) != -1)
    {
        switch(opt)
        {
            case 'u':
                *filter = "upper";
                break;
            case 'l':
                *filter = "lower";
                break;
            case 'n':
                *filter = "none";
                break;
            default:
                fprintf(stderr, "Usage: %s -u|-l|-n \"message\"\n", argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if(*filter == NULL)
    {
        fprintf(stderr, "Missing filter argument\n");
        exit(EXIT_FAILURE);
    }

    if(optind < argc)
    {
        *input_message = argv[optind];
    }
    else
    {
        fprintf(stderr, "Missing message argument\n");
        exit(EXIT_FAILURE);
    }

    if(optind + 1 < argc)
    {
        fprintf(stderr, "Too many arguments\n");
        exit(EXIT_FAILURE);
    }
}

void custom_error(const char *message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}
