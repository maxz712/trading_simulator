#include <iostream>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netinet/in.h>

#define WELL_KNOWN_PORT 8080

void* handle_client(void*arg);

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);

    // Create socket file descriptor (IPv4, TCP)
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Reuse the address and port to avoid "Address already in use" error
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT,
                   &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the well-known port
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;  // Accept connections from any IP address
    address.sin_port = htons(WELL_KNOWN_PORT);

    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    // Start listening for incoming connections
    if (listen(server_fd, 10) < 0) {  // Maximum of 10 pending connections
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    std::cout << "Server listening on port " << WELL_KNOWN_PORT << std::endl;

    while (true) {
        // Accept a new client connection
        if ((client_socket = accept(server_fd, (struct sockaddr*)&address,
                                    &addrlen)) < 0) {
            perror("Accept failed");
            continue;  // Continue accepting other connections
        }

        std::cout << "New client connected" << std::endl;

        // Create a new thread to handle the client
        pthread_t thread_id;
        int* pclient = new int;
        *pclient = client_socket;  // Pass client socket to the thread

        if (pthread_create(&thread_id, nullptr, handleClient, pclient) != 0) {
            perror("Failed to create thread");
            delete pclient;
            continue;
        }

        // Detach the thread to handle its own cleanup
        pthread_detach(thread_id);
    }

    // Close the server socket (unreachable code in this example)
    close(server_fd);
    return 0;
}

void* handleClient(void* arg) {
    int client_socket = *((int*)arg);
    delete (int*)arg;  // Free the allocated memory
    char buffer[1024] = {0};

    // Create a new socket for the assigned port
    int new_socket;
    struct sockaddr_in new_address;
    socklen_t addrlen = sizeof(new_address);

    if ((new_socket = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        close(client_socket);
        pthread_exit(nullptr);
    }

    // Bind to any available port (ephemeral port)
    new_address.sin_family = AF_INET;
    new_address.sin_addr.s_addr = INADDR_ANY;
    new_address.sin_port = 0;  // 0 lets the OS assign an available port

    if (bind(new_socket, (struct sockaddr*)&new_address, sizeof(new_address)) < 0) {
        perror("Bind failed");
        close(client_socket);
        close(new_socket);
        pthread_exit(nullptr);
    }

    // Retrieve the assigned port number
    if (getsockname(new_socket, (struct sockaddr*)&new_address, &addrlen) < 0) {
        perror("getsockname failed");
        close(client_socket);
        close(new_socket);
        pthread_exit(nullptr);
    }

    int assigned_port = ntohs(new_address.sin_port);
    std::cout << "Assigned port " << assigned_port << " to client" << std::endl;

    // Send the assigned port number to the client
    std::string port_str = std::to_string(assigned_port);
    if (send(client_socket, port_str.c_str(), port_str.length(), 0) < 0) {
        perror("Failed to send assigned port to client");
        close(client_socket);
        close(new_socket);
        pthread_exit(nullptr);
    }

    // Close the initial connection
    close(client_socket);

    // Start listening on the new port
    if (listen(new_socket, 1) < 0) {
        perror("Listen on new port failed");
        close(new_socket);
        pthread_exit(nullptr);
    }

    // Accept the client's connection on the new port
    int final_socket;
    if ((final_socket = accept(new_socket, (struct sockaddr*)&new_address,
                               &addrlen)) < 0) {
        perror("Accept on new port failed");
        close(new_socket);
        pthread_exit(nullptr);
    }

    std::cout << "Established new connection with client on port " << assigned_port << std::endl;

    // Communicate with the client
    std::string welcome_msg = "Welcome! Connection established on new port.";
    send(final_socket, welcome_msg.c_str(), welcome_msg.length(), 0);

    // Receive messages from the client
    int valread;
    while ((valread = read(final_socket, buffer, sizeof(buffer) - 1)) > 0) {
        buffer[valread] = '\0';  // Null-terminate the received data
        std::cout << "Client: " << buffer << std::endl;
        memset(buffer, 0, sizeof(buffer));  // Clear the buffer
    }

    // Clean up
    close(final_socket);
    close(new_socket);
    pthread_exit(nullptr);
}