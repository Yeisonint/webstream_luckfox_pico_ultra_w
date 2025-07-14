#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

#include <iostream>

typedef websocketpp::server<websocketpp::config::asio> server;

void on_message(server* s, websocketpp::connection_hdl hdl, server::message_ptr msg) {
    std::cout << "Mensaje recibido: " << msg->get_payload() << std::endl;

    // Responder al cliente con el mismo mensaje
    s->send(hdl, "Echo: " + msg->get_payload(), msg->get_opcode());
}

int main() {
    server echo_server;

    try {
        echo_server.set_message_handler(std::bind(&on_message, &echo_server, std::placeholders::_1, std::placeholders::_2));
        echo_server.init_asio();
        echo_server.set_reuse_addr(true);
        echo_server.listen(websocketpp::lib::asio::ip::tcp::v4(), 9002);
        echo_server.start_accept();
        echo_server.run();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}
