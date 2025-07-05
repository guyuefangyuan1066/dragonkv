// g++ -std=c++11 -o shortconn_client shortconn_client.cpp -lws2_32
//./shortconn_client
#include <iostream>
#include <string>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

// 单次短连接请求
std::string send_command(const std::string& cmd) {
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2,2), &wsaData);
    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "socket failed\n";
        WSACleanup();
        return "";
    }
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(50000);
    serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
    if (connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "connect failed\n";
        closesocket(sock);
        WSACleanup();
        return "";
    }
    send(sock, cmd.c_str(), cmd.size(), 0);
    char buffer[1024] = {0};
    int len = recv(sock, buffer, sizeof(buffer)-1, 0);
    std::string resp;
    if (len > 0) resp.assign(buffer, len);
    closesocket(sock);
    WSACleanup();
    return resp;
}

int main() {
    std::cout << "Short-connection test client for DragonKV\n";
    // PUT
    std::string putCmd = "*3\r\n$3\r\nPUT\r\n$3\r\nfoo\r\n$3\r\nbar\r\n";
    std::string putResp = send_command(putCmd);
    std::cout << "PUT response: " << putResp << std::endl;
    // GET
    std::string getCmd = "*2\r\n$3\r\nGET\r\n$3\r\nfoo\r\n";
    std::string getResp = send_command(getCmd);
    std::cout << "GET response: " << getResp << std::endl;
    // REMOVE
    std::string removeCmd = "*2\r\n$6\r\nREMOVE\r\n$3\r\nfoo\r\n";
    std::string removeResp = send_command(removeCmd);
    std::cout << "REMOVE response: " << removeResp << std::endl;
    // GET again
    std::string getResp2 = send_command(getCmd);
    std::cout << "GET after REMOVE: " << getResp2 << std::endl;
    return 0;
}
