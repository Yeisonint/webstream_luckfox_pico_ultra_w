#pragma once
#include <rtc/rtc.hpp>
#include <nlohmann/json.hpp>
#include <memory>
#include <map>
#include <mutex>
#include <iostream>

class WebRTCServer {
public:
    WebRTCServer(uint16_t port, const std::string& certPath, const std::string& keyPath) {
        rtc::WebSocketServer::Configuration ws_config;
        ws_config.tls = rtc::TlsConfig{ certPath, keyPath };

        server = std::make_shared<rtc::WebSocketServer>(ws_config, port, "0.0.0.0");

        server->onClient([this](std::shared_ptr<rtc::WebSocket> client) {
            auto pc = std::make_shared<rtc::PeerConnection>();

            auto dc = pc->createDataChannel("video");
            data_channels[client.get()] = dc;

            pc->onGatheringStateChange([client, pc](rtc::PeerConnection::GatheringState state) {
                if (state == rtc::PeerConnection::GatheringState::Complete) {
                    auto sdp = pc->localDescription();
                    client->send(nlohmann::json{{"sdp", {{"type", sdp.typeString()}, {"sdp", std::string(sdp)}}}}.dump());
                }
            });

            pc->onLocalCandidate([client](rtc::Candidate candidate) {
                client->send(nlohmann::json{{"candidate", {
                    {"candidate", candidate.candidate()},
                    {"sdpMid", candidate.mid()}
                }}}.dump());
            });

            client->onMessage(
                [](const rtc::binary&) {}, // ignorar binarios
                [this, client, pc](const std::string& msg) {
                    auto json = nlohmann::json::parse(msg);
                    if (json.contains("sdp")) {
                        auto sdp = json["sdp"];
                        pc->setRemoteDescription(rtc::Description(sdp["sdp"], sdp["type"]));
                        if (sdp["type"] == "offer") {
                            pc->setLocalDescription();
                        }
                    } else if (json.contains("candidate")) {
                        auto cand = json["candidate"];
                        pc->addRemoteCandidate(rtc::Candidate(cand["candidate"], cand["sdpMid"]));
                    }
                }
            );

            client->onClosed([this, client]() {
                data_channels.erase(client.get());
            });

            peer_connections[client.get()] = pc;
        });

        std::cout << "WebRTC server ready on wss://0.0.0.0:" << port << std::endl;
    }

    void send_frame(const uint8_t* data, size_t size) {
        for (auto& [_, dc] : data_channels) {
            if (dc && dc->isOpen()) {
                dc->send(reinterpret_cast<const std::byte*>(data), size);
            }
        }
    }

private:
    std::shared_ptr<rtc::WebSocketServer> server;
    std::map<void*, std::shared_ptr<rtc::PeerConnection>> peer_connections;
    std::map<void*, std::shared_ptr<rtc::DataChannel>> data_channels;
};
