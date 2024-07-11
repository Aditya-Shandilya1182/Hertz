#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <set>
#include <queue>
#include <thread>
#include <mutex>
#include <memory>
#include <cstring>

#include "html_parser.h"

// Unix
#ifdef __unix__
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

// Windows
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

std::mutex QueueLock;
std::queue<int> event_queue;

std::mutex DictionaryLock; 

std::mutex CoutLock;

class WebsiteHandler {
public:
    WebsiteHandler() {
        init_dictionary();
    }

    void load(const std::string& filename) {
        page[filename] = readFile(filename);
    }

    std::string get_page(const std::string& filename, int request_type, const std::string& input, const std::string& text) {
        if(!file_exists(filename)) {
            std::cerr << filename << " could not be opened! Check if the path is correct\n";
            return "";
        }
        std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=ISO-8859-4 \r\n\r\n" + page[filename];
        if (request_type == 1) {
            add_dictionary(text);
        } else if (request_type == 0 && !input.empty()) {
            if (check_dictionary(input)) {
                response.replace(response.find("<!Rvalue>"), 9, input + " is found in your Dictionary");
            } else {
                response.replace(response.find("<!Rvalue>"), 9, input + " is NOT found in your Dictionary");
            }
        }
        return response;
    }

    void add_dictionary(const std::string& word) {
    if (word.empty()) return;

    std::string actual_word = word.substr(5); 

    std::lock_guard<std::mutex> lock(DictionaryLock);
    if (dictionary.insert(actual_word).second) {
        std::ofstream fDictionary("dictionary.txt", std::ios::app);
        if (!fDictionary) {
            std::cerr << "Failed to open file\n";
            return;
        }
        fDictionary << actual_word << "\n";
    }
}


    bool check_dictionary(const std::string& word) {
        std::lock_guard<std::mutex> lock(DictionaryLock);
        return dictionary.count(word);
    }

    void init_dictionary() {
        std::lock_guard<std::mutex> lock(DictionaryLock);
        std::ifstream fp("dictionary.txt");
        if (!fp) {
            std::ofstream created("dictionary.txt");
            return;
        }
        std::string word;
        while (std::getline(fp, word)) {
            dictionary.insert(word);
        }
    }

private:
    std::set<std::string> dictionary; // Current dictionary set
    std::map<std::string, std::string> page; // "page name" -> page_contents

    std::string readFile(const std::string& filename) {
        std::ifstream file(filename);
        if (!file) {
            return "";
        }
        std::ostringstream ss;
        ss << file.rdbuf();
        return ss.str();
    }

    bool file_exists(const std::string& filename) {
        std::ifstream file(filename);
        return file.good();
    }
};

WebsiteHandler website;

class Server {
public:
    Server(uint32_t internet_address, uint16_t port_number = 80, int thread_count = 10)
        : THREAD_COUNT(thread_count), server_up(false) {
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = (internet_address == 0) ? INADDR_ANY : internet_address;
        address.sin_port = htons(port_number);
        memset(address.sin_zero, '\0', sizeof address.sin_zero);
        if (initialize_socket() && bind_address() && start_listen()) {
            server_up = true;
        }
    }

    ~Server() {
        shutdown(file_descriptor, 2);
    }

    void start() {
        if (!server_up) {
            std::cerr << "Server failed to start!\n";
            return;
        }

        for (int i = 0; i < THREAD_COUNT; ++i) {
            threads.emplace_back(&Server::connection_thread, this);
        }

        while (true) {
            int socket_num = accept_connection();
            std::lock_guard<std::mutex> lock(QueueLock);
            event_queue.push(socket_num);
        }
    }

private:
    int file_descriptor;
    int sizeof_address;
    int THREAD_COUNT;
    sockaddr_in address;
    bool server_up;
    std::vector<std::thread> threads;

    bool initialize_socket() {
#ifdef _WIN32
        WSADATA data;
        if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
            std::cerr << "WSAStartup failed\n";
            return false;
        }
#endif
        file_descriptor = socket(AF_INET, SOCK_STREAM, 0);
        if (file_descriptor < 0) {
            std::cerr << "ERROR: Invalid Socket\n";
            return false;
        }
        return true;
    }

    bool bind_address() {
        if (bind(file_descriptor, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            std::cerr << "ERROR: Couldn't bind\n";
            return false;
        }
        return true;
    }

    bool start_listen(int backlog = 100000) {
        if (listen(file_descriptor, backlog) < 0) {
            std::cerr << "ERROR: Couldn't listen\n";
            return false;
        }
        return true;
    }

    int accept_connection() {
        sockaddr_in address; // Make sure sockaddr_in is defined at the top of the file
        socklen_t sizeof_address = sizeof(address);
        int connection_value = accept(file_descriptor, reinterpret_cast<sockaddr*>(&address), &sizeof_address);
        if (connection_value < 0) {
            std::cerr << "Server failed to accept incoming connection\n";
            return -1;
        }
        return connection_value;
    }


    void connection_thread() {
        while (true) {
            int socket_num;
            {
                std::lock_guard<std::mutex> lock(QueueLock);
                if (event_queue.empty()) {
                    continue;
                }
                socket_num = event_queue.front();
                event_queue.pop();
            }

            char buffer[1000] = {0};
            int buffer_length = recv(socket_num, buffer, sizeof(buffer), 0);
            if (buffer_length < 0) {
                std::cerr << "ERROR: Receiving Failure\n";
                continue;
            }
            html_parser request(buffer, buffer_length);

            std::string message = website.get_page("main.html", request.get_request_type(), request.get_input("name"), request.get_text());
            int send_value = send(socket_num, message.c_str(), message.length(), 0);
            if (send_value < 0) {
                std::cerr << "ERROR: Sending Failure\n";
            }
#ifdef _unix
            close(socket_num);
#endif
#ifdef _WIN32
            closesocket(socket_num);
#endif
        }
    }
};

int main() {
    website.load("main.html");
    Server basic_server(0, 8080, 10);
    basic_server.start();
    return 0;
}