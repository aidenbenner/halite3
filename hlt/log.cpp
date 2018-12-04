#include "log.hpp"
#include "constants.hpp"

#include <fstream>
#include <iostream>
#include <vector>
#include <chrono>

static std::ofstream log_file;
static std::ofstream flog_file;
static std::vector<std::string> log_buffer;
static std::vector<std::string> flog_buffer(1, "[");
static bool has_opened = false;
static bool has_atexit = false;


void dump_buffer_at_exit() {
    if (has_opened) {
        return;
    }

    auto now_in_nanos = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    std::string filename = "bot-unknown-" + std::to_string(now_in_nanos) + ".log";
    std::ofstream file(filename, std::ios::trunc | std::ios::out);
    for (const std::string& message : log_buffer) {
        file << message << std::endl;
    }
}

void hlt::log::open(int bot_id) {
    if (has_opened) {
        hlt::log::log("Error: log: tried to open(" + std::to_string(bot_id) + ") but we have already opened before.");
        exit(1);
    }

    has_opened = true;
    std::string filename = "bot-" + std::to_string(bot_id) + ".log";
    std::string ffilename = "bot-" + std::to_string(bot_id) + ".flog";
    log_file.open(filename, std::ios::trunc | std::ios::out);
    flog_file.open(ffilename, std::ios::trunc | std::ios::out);

    for (const std::string& message : log_buffer) {
        log_file << message << std::endl;
    }
    for (const std::string& message : flog_buffer) {
        flog_file << message << std::endl;
    }
    log_buffer.clear();
    flog_buffer.clear();
}



void hlt::log::log(const std::string& message) {
    if (constants::IS_DEBUG) {
        if (has_opened) {
            log_file << message << std::endl;
        } else {
            if (!has_atexit) {
                has_atexit = true;
                atexit(dump_buffer_at_exit);
            }
            log_buffer.push_back(message);
        }
    }
}

void hlt::log::flog(const std::string& message) {
    if (constants::IS_DEBUG) {
        if (has_opened) {
            flog_file << message << std::endl;
        } else {
            if (!has_atexit) {
                has_atexit = true;
                atexit(dump_buffer_at_exit);
            }
            flog_buffer.push_back(message);
        }
    }
}

void hlt::log::flog(Log f) {
    if (constants::IS_DEBUG) {
        flog(f.get_json() + ",");
    }
}
