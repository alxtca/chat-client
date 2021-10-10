#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>

#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>

#pragma comment(lib, "Ws2_32.lib")
#pragma warning(disable : 4996)
// some variables, tb replaced with script
std::string nickname{ "default booster" };
unsigned max_chat_hystory_lines{ 20 };
std::vector<char> message_received(80); //if more than 80 characters are entered to be sendt, it will be splitted in packages and delivered one after another.

//for socket on 64bit system. https://stackoverflow.com/questions/1953639/is-it-safe-to-cast-socket-to-int-under-win64
using socket_t = decltype(socket(0, 0, 0));

void print_vec(const std::vector<std::string>& vec)
{
    for (auto x = vec.rbegin(); x != vec.rend(); ++x) {
        std::cout << *x;
    }
    std::cout << '\n';
}
void receiveMessages(std::vector<std::string>* chat_story, socket_t* newSd, bool* exit, std::mutex & mutex) {
    std::string input_message{};
    std::vector<char> buff;
    while (true) {
        if (*exit == true) break;
        auto bytesRecv = recv(*newSd, message_received.data(), message_received.size(), 0);

        if (message_received[0] == ' ') //server "exit"
        {
            mutex.lock();
            (*chat_story).insert((*chat_story).begin(), "server has disconnected. \n");
            mutex.unlock();
            *exit = true;
            if (chat_story->size() > max_chat_hystory_lines)
            {
                mutex.lock();
                chat_story->pop_back();
                mutex.unlock();
            }
            print_vec(*chat_story);
            break;
        }
        if (bytesRecv > 0) {
            input_message = std::string(message_received.begin(), message_received.begin() + bytesRecv) + "\n"; //message_received.end() = message_received.begin()+bytesRecv
            mutex.lock();
            chat_story->insert(chat_story->begin(), input_message);
            mutex.unlock();
            if (chat_story->size() > max_chat_hystory_lines)
            {
                mutex.lock();
                chat_story->pop_back();
                mutex.unlock();
            }            
            print_vec(*chat_story);
        }
    }
}
void sendMessages(std::vector<std::string>* chat_story, socket_t* clientSd, bool* exit, std::mutex& mutex) {
    std::string client_input;
    while (true)
    {
        std::cout << ">";
        std::getline(std::cin, client_input);
        if (client_input == "exit")
        {
            send(*clientSd, " ", 1, 0);
            *exit = true;
            break;
        }
        client_input.insert(0, nickname+": ");
        send(*clientSd, client_input.data(), client_input.length(), 0);

        mutex.lock();
        chat_story->insert(chat_story->begin(), client_input+"\n");
        mutex.unlock();
        if (chat_story->size() > max_chat_hystory_lines)
        {
            mutex.lock();
            chat_story->pop_back();
            mutex.unlock();
        }
        print_vec(*chat_story);
    }
}
//Client side
int main(int argc, char* argv[])
{
    //connection setup
    WSADATA wsaData;
    int iResult;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    //server address 127.0.0.1
    const char* serverIp = "localhost";
    const int port = 12010;
    //setup a socket and connection tools     
    hostent* host = gethostbyname(serverIp);


    sockaddr_in sendSockAddr;
    memset((char*)&sendSockAddr, '\0', sizeof(sendSockAddr));
    sendSockAddr.sin_family = AF_INET;
    sendSockAddr.sin_addr.s_addr = inet_addr(inet_ntoa(*(in_addr*)*host->h_addr_list));
    sendSockAddr.sin_port = htons(port);

    socket_t clientSd = socket(AF_INET, SOCK_STREAM, 0); //was int clientSd
    //try to connect...
    int status = connect(clientSd, (sockaddr*)&sendSockAddr, sizeof(sendSockAddr));
    if (status == INVALID_SOCKET)
    {
        std::cout << "Error connecting to socket!" << std::endl;
        return -1;
    }
    std::cout << "Connected to the server! \"exit\" to terminate" << std::endl;

    // socket options https://stackoverflow.com/questions/30395258/setting-timeout-to-recv-function
    DWORD timeout = 1 * 1000;
    setsockopt(clientSd, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));

    //main 
    // 
    //some variables and buffers
    std::vector<std::string> chat_story{};
    std::vector<std::string>* chat_ptr{ &chat_story };
    std::mutex chat_story_mutex;
    bool exit = false;
    bool* exit_ptr{ &exit };
    // send/recv
    socket_t* clientSd_ptr{ &clientSd };
    std::thread recv_thread(receiveMessages, chat_ptr, clientSd_ptr, exit_ptr, std::ref(chat_story_mutex));
    std::thread send_thread(sendMessages, chat_ptr, clientSd_ptr, exit_ptr, std::ref(chat_story_mutex));
    recv_thread.join();
    send_thread.join();

    closesocket(clientSd);

    std::cout << "Connection closed" << std::endl;
    return 0;
}