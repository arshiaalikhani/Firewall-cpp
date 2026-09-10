#include <iostream>
#include <string>
#include <cstring>
#include <winsock2.h>      // هسته اصلی سوکت در ویندوز
#include <ws2tcpip.h>      // برای توابعی مثل inet_ntoa
#include <windows.h>       // توابع پایه ویندوز
#pragma comment(lib, "ws2_32.lib")

int main()
{
    WSADATA wsaData;
    int sockfd;
    struct sockaddr_in server_addr, client_addr;
    int client_len = sizeof(client_addr);  // در ویندوز از int استفاده می‌شود
    char buffer[1024];
     // ------------------------------------------------
     // 1. Initialize Winsock (Windows)
    // ------------------------------------------------
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }
    // ------------------------------------------------
    //      CREATE UDP SOCKET
    // ------------------------------------------------
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == INVALID_SOCKET){ //in windows error check with invalid_socket
            std::cerr << "socket creation failed: "<<WSAGetLastError()<<std::endl;
        WSACleanup();
        return 1;
    }
    // ------------------------------------------------
    //          3. Configuring the server address structure
    //------------------------------------------------
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(53);
    //------------------------------------------
    //  4. Binding the socket to an address and port
    //------------------------------------------
    if(bind(sockfd,(struct sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR){
        std::cerr<<"bind filed"<<WSAGetLastError()<<std::endl;
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }
    std::cout<<"UDP server started on port 53. Waiting for data..."<<std::endl;
    //------------------------------------------------
    //  5. Data reception loop
    //------------------------------------------------


}
