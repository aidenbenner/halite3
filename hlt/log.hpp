#pragma once

#include <string>
#include <sstream>

namespace hlt {
    namespace log {
        void open(int bot_id);
        void log(const std::string& message);

        template<typename T>
        std::string logstr(T t) {
            std::stringstream ss;
            ss << t;
            return ss.str();
        }

        template<typename T, typename ... Args>
        std::string logstr(T t, Args... args) {
            return logstr(t) + " - " + logstr(args...);
        }

        template<typename T, typename ... Args>
        void log(T t, Args... args) {
            log(logstr(t, args...));
        }

    }
}
