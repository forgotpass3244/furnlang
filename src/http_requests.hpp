#ifndef CROSS_PLATFORM_HTTP_CLIENT_H
#define CROSS_PLATFORM_HTTP_CLIENT_H

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <vector>
#include <any>
#include <stdexcept>
#include <unordered_map>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#endif

namespace xhttp
{

    using HttpResult = std::vector<std::pair<std::string, std::any>>;

    class HttpClient
    {
    public:
        static HttpResult Get(const std::string &host, const std::string &path = "/")
        {
#ifdef _WIN32
            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
            {
                throw std::runtime_error("WSAStartup failed");
            }
#endif

            struct addrinfo hints{}, *res;
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;

            if (getaddrinfo(host.c_str(), "80", &hints, &res) != 0)
            {
#ifdef _WIN32
                WSACleanup();
#endif
                throw std::runtime_error("Failed to resolve host");
            }

            int sockfd;
#ifdef _WIN32
            sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (sockfd == INVALID_SOCKET)
            {
                freeaddrinfo(res);
                WSACleanup();
                throw std::runtime_error("Socket creation failed");
            }
#else
            sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (sockfd < 0)
            {
                freeaddrinfo(res);
                throw std::runtime_error("Socket creation failed");
            }
#endif

            if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0)
            {
#ifdef _WIN32
                closesocket(sockfd);
                WSACleanup();
#else
                close(sockfd);
#endif
                freeaddrinfo(res);
                throw std::runtime_error("Connection failed");
            }

            freeaddrinfo(res);

            std::ostringstream request;
            request << "GET " << path << " HTTP/1.1\r\n"
                    << "Host: " << host << "\r\n"
                    << "Connection: close\r\n\r\n";

            std::string reqStr = request.str();
            send(sockfd, reqStr.c_str(), static_cast<int>(reqStr.size()), 0);

            char buffer[4096];
            std::ostringstream response_stream;

            int received;
            while ((received = recv(sockfd, buffer, sizeof(buffer) - 1, 0)) > 0)
            {
                buffer[received] = '\0';
                response_stream << buffer;
            }

#ifdef _WIN32
            closesocket(sockfd);
            WSACleanup();
#else
            close(sockfd);
#endif

            return ParseResponse(response_stream.str());
        }

    private:
        static HttpResult ParseResponse(const std::string &response)
        {
            HttpResult result;
            std::istringstream stream(response);
            std::string line;

            // Parse status line
            if (!std::getline(stream, line))
            {
                throw std::runtime_error("Invalid HTTP response (empty)");
            }

            std::istringstream status_line(line);
            std::string http_version;
            int status_code = 0;
            status_line >> http_version >> status_code;
            result.emplace_back("Status", status_code);

            // Parse headers
            std::unordered_map<std::string, std::any> headers;

            while (std::getline(stream, line) && line != "\r" && !line.empty())
            {
                size_t colon = line.find(':');
                if (colon != std::string::npos)
                {
                    std::string key = line.substr(0, colon);
                    std::string value = line.substr(colon + 1);
                    value.erase(0, value.find_first_not_of(" \t\r\n"));
                    value.erase(value.find_last_not_of(" \t\r\n") + 1);
                    headers[key] = value; // unordered_map insert
                }
            }
            result.emplace_back("Headers", headers);

            // Parse body
            std::ostringstream body_stream;
            while (std::getline(stream, line))
            {
                body_stream << line << "\n";
            }
            result.emplace_back("Body", body_stream.str());

            return result;
        }
    };

} // namespace xhttp

#endif // CROSS_PLATFORM_HTTP_CLIENT_H