#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <thread>
#include <set>
#include <mutex>
#include <atomic>
#include <chrono>
#include <string>
#include <iostream>
#include "pwm_controller.hpp"

typedef websocketpp::server<websocketpp::config::asio> ws_server;
typedef websocketpp::connection_hdl connection_hdl;

class WebSocketServer
{
private:
  ws_server server;
  std::set<connection_hdl, std::owner_less<connection_hdl>> clients;
  std::mutex clients_mutex;
  std::thread server_thread;
  std::thread watchdog_thread;

  std::atomic<bool> running_watchdog{true};
  std::chrono::steady_clock::time_point last_command_time;
  std::mutex time_mutex;

  PWMController pwm_right_reverse;
  PWMController pwm_left_reverse;
  PWMController pwm_left_forward;
  PWMController pwm_right_forward;

public:
  WebSocketServer()
      : pwm_right_reverse("pwmchip6", "pwm0"),
        pwm_left_reverse("pwmchip11", "pwm0"),
        pwm_left_forward("pwmchip2", "pwm0"),
        pwm_right_forward("pwmchip0", "pwm0")
  {
    constexpr int PWM_PERIOD_NS = 1000000;
    constexpr const char *POLARITY = "normal";

    pwm_right_reverse.init(PWM_PERIOD_NS, POLARITY);
    pwm_left_reverse.init(PWM_PERIOD_NS, POLARITY);
    pwm_left_forward.init(PWM_PERIOD_NS, POLARITY);
    pwm_right_forward.init(PWM_PERIOD_NS, POLARITY);

    server.init_asio();
    server.clear_access_channels(websocketpp::log::alevel::all);
    server.clear_error_channels(websocketpp::log::elevel::all);

    server.set_open_handler([this](connection_hdl hdl)
                            {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.insert(hdl); });

    server.set_close_handler([this](connection_hdl hdl)
                             {
            std::lock_guard<std::mutex> lock(clients_mutex);
            clients.erase(hdl); });

    server.set_message_handler([this](connection_hdl, ws_server::message_ptr msg)
                               { handle_command(msg->get_payload()); });

    start_watchdog();
  }

  ~WebSocketServer()
  {
    server.stop_listening();
    server.stop();

    running_watchdog = false;
    if (watchdog_thread.joinable())
      watchdog_thread.join();
    if (server_thread.joinable())
      server_thread.join();
  }

  void start(uint16_t port = 9000)
  {
    server.set_reuse_addr(true);
    server.listen(websocketpp::lib::asio::ip::tcp::v4(), port);
    server.start_accept();

    server_thread = std::thread([this]()
                                { server.run(); });
  }

  void send_frame(const uint8_t *data, size_t size, uint64_t pts)
  {
    std::lock_guard<std::mutex> lock(clients_mutex);
    for (auto &hdl : clients)
    {
      websocketpp::lib::error_code ec;
      server.send(hdl, data, size, websocketpp::frame::opcode::binary, ec);
    }
  }

private:
  void handle_command(const std::string &input)
  {
    std::cout << "\nCMD: " << input << std::endl;

    std::string cmd;
    int percent = 50;

    size_t colon_pos = input.find(':');
    if (colon_pos != std::string::npos)
    {
      cmd = input.substr(0, colon_pos);
      try
      {
        percent = std::stoi(input.substr(colon_pos + 1));
        if (percent < 0)
          percent = 0;
        if (percent > 100)
          percent = 100;
      }
      catch (...)
      {
        percent = 50;
      }
    }
    else
    {
      cmd = input;
    }

    constexpr int PWM_PERIOD_NS = 1000000;
    int duty = (PWM_PERIOD_NS * percent) / 100;

    if (cmd == "forward"){
      pwm_right_forward.set_duty_cycle(duty*0.75);
      pwm_left_forward.set_duty_cycle(duty);
    }else if (cmd == "backward"){
      pwm_right_reverse.set_duty_cycle(duty*0.75);
      pwm_left_reverse.set_duty_cycle(duty);
    }else if (cmd == "left"){
      pwm_right_forward.set_duty_cycle(duty*0.75);
      pwm_left_reverse.set_duty_cycle(duty);
    }else if (cmd == "right"){
      pwm_left_forward.set_duty_cycle(duty);
      pwm_right_reverse.set_duty_cycle(duty*0.75);
    }else if (cmd == "stop"){
      pwm_right_forward.set_duty_cycle(0);
      pwm_left_forward.set_duty_cycle(0);
      pwm_right_reverse.set_duty_cycle(0);
      pwm_left_reverse.set_duty_cycle(0);
    }

    {
      std::lock_guard<std::mutex> lock(time_mutex);
      last_command_time = std::chrono::steady_clock::now();
    }
  }

  void start_watchdog()
  {
    last_command_time = std::chrono::steady_clock::now();

    watchdog_thread = std::thread([this](){
      constexpr auto TIMEOUT = std::chrono::milliseconds(1000);

      while (running_watchdog) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));

        auto now = std::chrono::steady_clock::now();
        std::chrono::steady_clock::time_point last;

        {
          std::lock_guard<std::mutex> lock(time_mutex);
          last = last_command_time;
        }

        if (now - last > TIMEOUT) {
          pwm_right_forward.set_duty_cycle(0);
          pwm_left_forward.set_duty_cycle(0);
          pwm_right_reverse.set_duty_cycle(0);
          pwm_left_reverse.set_duty_cycle(0);
        }
      } 
    });
  }
};
