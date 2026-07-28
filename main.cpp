#include <iostream>
#include <string>
#include <cstring>
#include <sstream>
#include <vector>
#include <iomanip>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <fcntl.h>

#define TIMEOUT_SECONDS 5

std::vector<uint8_t> hex_to_bytes(const std::string &hex)
{
    std::vector<uint8_t> bytes;
    for (size_t i = 0; i < hex.length(); i += 2)
    {
        std::string byteStr = hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(strtol(byteStr.c_str(), nullptr, 16));
        bytes.push_back(byte);
    }
    return bytes;
}

std::string bytes_to_hex(const std::vector<uint8_t> &bytes)
{
    std::ostringstream oss;
    for (auto byte : bytes)
    {
        oss << std::hex << std::setfill('0') << std::setw(2) << (int)byte;
    }
    return oss.str();
}

void display_help()
{
    std::string help = "Usage:\n"
                       "--source | -s     Set source IP. (optional)\n"
                       "--dest   | -d     Set destination IP. (required)\n"
                       "--port   | -p     Set the destination port. Default is 27015.\n"
                       "--help   | -h     Shows this help\n";
    std::cout << help;
}

bool is_equal(const char *key, char const *value)
{
    if ((strcmp(key, value) == 0))
        return true;
    return false;
}

bool is_number(const char *value)
{
    bool flag = false;
    for (int i = 0; value[i] != '\0'; i++)
    {
        if (isdigit(value[i]) != 0)
        {
            flag = true;
        }
        else
        {
            return false;
        }
    }
    return flag;
}

int main(int argc, char *argv[])
{
    // const char *src_ip = "192.168.31.167";
    // const char *dst_ip = "192.168.31.153";
    // const int dst_port = 27015;
    char *src_ip = nullptr;
    char *dst_ip = nullptr;
    int dst_port = 27015;

    if (argc == 1)
    {
        std::cout << "No arguments were provided." << std::endl;
        display_help();
        return 2;
    }

    // TODO: Add more options
    // FIXME: I'm sure that there are unexpected behavior with the current implementation so try to refactor.
    for (int i = 1; i < argc; i += 2)
    {
        if (is_equal("--help", argv[i]) || is_equal("-h", argv[i]))
        {
            display_help();
            return 2;
        }
        else if ((is_equal("--source", argv[i]) || is_equal("-s", argv[i])) && i + 1 < argc)
        {
            src_ip = argv[i + 1];
        }
        else if ((is_equal("--dest", argv[i]) || is_equal("-d", argv[i])) && i + 1 < argc)
        {
            dst_ip = argv[i + 1];
        }
        else if ((is_equal("--port", argv[i]) || is_equal("-p", argv[i])) && i + 1 < argc)
        {
            if (!is_number(argv[i + 1]))
            {
                std::cout << "Invalid port value. It should only contain digits." << std::endl;
                display_help();
                return 3;
            }

            int port = std::atoi(argv[i + 1]);
            if (port > 65535 || port <= 0)
            {
                std::cout << "Invalid port value. Port range is 1-65535." << std::endl;
                display_help();
                return 3;
            }
            dst_port = port;
        }
        else
        {
            std::cout << "Invalid arguments." << std::endl;
            display_help();
            return 2;
        }
    }

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0)
    {
        perror("Socket creation failed");
        return 1;
    }

    // Optionally bind to source IP if provided
    if (src_ip != nullptr)
    {
        sockaddr_in src_addr{};
        src_addr.sin_family = AF_INET;
        src_addr.sin_port = htons(0); // Let OS pick a source port
        if (inet_pton(AF_INET, src_ip, &src_addr.sin_addr) <= 0)
        {
            perror("Invalid source IP");
            close(sockfd);
            return 1;
        }

        if (bind(sockfd, (sockaddr *)&src_addr, sizeof(src_addr)) < 0)
        {
            perror("Binding to source IP failed");
            close(sockfd);
            return 1;
        }
    }

    // Set receive timeout
    struct timeval timeout;
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    sockaddr_in dest_addr{};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(dst_port);
    inet_pton(AF_INET, dst_ip, &dest_addr.sin_addr);

    std::string hex_data = "ffffffff54536f7572636520456e67696e6520517565727900";
    std::vector<uint8_t> data = hex_to_bytes(hex_data);

    if (sendto(sockfd, data.data(), data.size(), 0, (sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
    {
        perror("Failed to send first query.");
        close(sockfd);
        return 1;
    }

    uint8_t buffer[4096];
    sockaddr_in sender_addr{};
    socklen_t sender_len = sizeof(sender_addr);
    ssize_t received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr *)&sender_addr, &sender_len);

    if (received < 0)
    {
        perror("Failed to receive challenge number from server.");
        close(sockfd);
        return 1;
    }

    std::vector<uint8_t> response(buffer, buffer + received);
    std::string partial_hex = bytes_to_hex(response).substr(10); // skip first 5 bytes

    // Append and resend
    hex_data += partial_hex;
    data = hex_to_bytes(hex_data);

    if (sendto(sockfd, data.data(), data.size(), 0, (sockaddr *)&dest_addr, sizeof(dest_addr)) < 0)
    {
        perror("Failed to send challenge data to server.");
        close(sockfd);
        return 1;
    }

    received = recvfrom(sockfd, buffer, sizeof(buffer), 0, (sockaddr *)&sender_addr, &sender_len);

    if (received < 0)
    {
        perror("Failed to receive server response.");
        close(sockfd);
        return 1;
    }

    // std::cout << "Final response (" << received << " bytes):\n";
    // for (ssize_t i = 0; i < received; ++i)
    // {
    //     std::printf("%02x ", buffer[i]);
    // }
    // std::cout << std::endl;

    std::cout << "Final response (" << received << " bytes):\n";
    for (ssize_t i = 0; i < received; i += 16)
    {
        std::printf("%04zx  ", i);
        for (ssize_t j = 0; j < 16; ++j)
        {
            if (i + j < received)
            {
                std::printf("%02x ", buffer[i + j]);
            }
            else
            {
                std::printf("   ");
            }
        }

        std::printf(" ");
        for (ssize_t j = 0; j < 16; ++j)
        {
            if (i + j < received)
            {
                char c = static_cast<char>(buffer[i + j]);
                std::printf("%c", std::isprint(static_cast<unsigned char>(c)) ? c : '.');
            }
        }
        std::printf("\n");
    }

    close(sockfd);
    return 0;
}
