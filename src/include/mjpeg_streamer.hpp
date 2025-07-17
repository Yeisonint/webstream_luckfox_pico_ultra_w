#pragma once
#include <thread>
#include <vector>
#include <mutex>
#include <netinet/in.h>
#include <unistd.h>
#include <sstream>
#include <set>

class MJPEGStreamer {
public:
    MJPEGStreamer(int port = 8080) : port_(port), running_(true) {
        server_thread_ = std::thread(&MJPEGStreamer::run_server, this);
    }

    ~MJPEGStreamer() {
        running_ = false;
        close(server_fd_);
        if (server_thread_.joinable())
            server_thread_.join();
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (int fd : clients_)
            close(fd);
    }

    void push_frame(const uint8_t* data, size_t size) {
        std::lock_guard<std::mutex> lock(frame_mutex_);
        frame_.assign(data, data + size);
    }

private:
    int port_;
    int server_fd_;
    std::thread server_thread_;
    std::vector<uint8_t> frame_;
    std::mutex frame_mutex_;
    bool running_;

    std::set<int> clients_;
    std::mutex clients_mutex_;

    void run_server() {
        server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port_);
        addr.sin_addr.s_addr = INADDR_ANY;

        bind(server_fd_, (sockaddr*)&addr, sizeof(addr));
        listen(server_fd_, 5);

        while (running_) {
            int client_fd = accept(server_fd_, nullptr, nullptr);
            if (client_fd < 0) continue;

            {
                std::lock_guard<std::mutex> lock(clients_mutex_);
                clients_.insert(client_fd);
            }

            std::thread(&MJPEGStreamer::handle_client, this, client_fd).detach();
        }
    }

    void handle_client(int client_fd) {
        std::string header =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n\r\n";
        if (send(client_fd, header.c_str(), header.size(), 0) < 0) {
            close_client(client_fd);
            return;
        }

        while (running_) {
            std::vector<uint8_t> local_frame;
            {
                std::lock_guard<std::mutex> lock(frame_mutex_);
                local_frame = frame_;
            }

            if (!local_frame.empty()) {
                std::ostringstream part;
                part << "--frame\r\n"
                     << "Content-Type: image/jpeg\r\n"
                     << "Content-Length: " << local_frame.size() << "\r\n\r\n";

                if (send(client_fd, part.str().c_str(), part.str().size(), 0) < 0 ||
                    send(client_fd, local_frame.data(), local_frame.size(), 0) < 0 ||
                    send(client_fd, "\r\n", 2, 0) < 0) {
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(33));
        }

        close_client(client_fd);
    }

    void close_client(int client_fd) {
        close(client_fd);
        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.erase(client_fd);
    }
};
