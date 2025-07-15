#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <thread>
#include <set>
#include <mutex>

typedef websocketpp::server<websocketpp::config::asio> ws_server;
typedef websocketpp::connection_hdl connection_hdl;

class WebSocketServer {
private:
    ws_server server;
    std::set<connection_hdl, std::owner_less<connection_hdl>> clients;
    std::mutex clients_mutex;
    std::thread server_thread;

public:
    WebSocketServer() {
        server.init_asio();
        
        server.clear_access_channels(websocketpp::log::alevel::all);
        server.clear_error_channels(websocketpp::log::elevel::all);

        server.set_open_handler([this](connection_hdl hdl) {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.insert(hdl);
        });

        server.set_close_handler([this](connection_hdl hdl) {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.erase(hdl);
        });

        server.set_message_handler([](connection_hdl, ws_server::message_ptr) {
            // Opcional: manejar mensajes del cliente
        });
    }

    void start(uint16_t port = 9000) {
        server.set_reuse_addr(true);
        server.listen(websocketpp::lib::asio::ip::tcp::v4(), port);
        server.start_accept();

        server_thread = std::thread([this]() {
            server.run();
        });
    }

    void send_frame(const uint8_t* data, size_t size, uint64_t pts) {
        std::lock_guard<std::mutex> lock(clients_mutex);
        for (auto& hdl : clients) {
            websocketpp::lib::error_code ec;
            server.send(hdl, data, size, websocketpp::frame::opcode::binary, ec);
        }
    }

    ~WebSocketServer() {
        server.stop_listening();
        server.stop();
        if (server_thread.joinable()) server_thread.join();
    }
};
