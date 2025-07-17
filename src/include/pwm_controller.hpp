#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>

class PWMController {
public:
    PWMController(const std::string& chip, const std::string& channel)
        : pwm_chip(chip), pwm_channel(channel) {
        base_path = "/sys/class/pwm/" + pwm_chip + "/" + pwm_channel;
    }

    ~PWMController() {
        disable();
        unexport();
    }

    bool init(int period_ns, const std::string& polarity) {
        if (!export_pwm()) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        if (!write(base_path + "/period", std::to_string(period_ns))) return false;
        if (!write(base_path + "/polarity", polarity)) return false;
        if (!enable()) return false;

        return true;
    }

    bool set_duty_cycle(int duty_ns) {
        return write(base_path + "/duty_cycle", std::to_string(duty_ns));
    }

    void disable() {
        write(base_path + "/enable", "0");
    }

private:
    std::string pwm_chip;
    std::string pwm_channel;
    std::string base_path;

    bool write(const std::string& path, const std::string& value) {
        std::ofstream file(path);
        if (!file) {
            // std::cerr << "Failed to open: " << path << std::endl;
            return false;
        }
        file << value;
        return true;
    }

    bool export_pwm() {
        return write("/sys/class/pwm/" + pwm_chip + "/export", "0");
    }

    void unexport() {
        write("/sys/class/pwm/" + pwm_chip + "/unexport", "0");
    }

    bool enable() {
        return write(base_path + "/enable", "1");
    }
};
