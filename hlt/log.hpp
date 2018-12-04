#pragma once

#include <string>
#include <sstream>
#include "cJSON/cJSON.h"

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


        void flog(const std::string& msg);

        struct Log {
            int turn;
            int x;
            int y;
            std::string msg;
            std::string color;

            Log(int turn_number, int x, int y, std::string msg, std::string color) {
                turn = turn_number;
                this->x = x;
                this->y = y;
                this->msg = msg;
                this->color = color;
            }

            std::string get_json() {
                cJSON *obj = cJSON_CreateObject();
                cJSON_AddNumberToObject(obj, "t", this->turn);
                cJSON_AddNumberToObject(obj, "x", this->x);
                cJSON_AddNumberToObject(obj, "y", this->y);
                cJSON_AddStringToObject(obj, "msg", this->msg.c_str());
                if (this->color != "") {
                    cJSON_AddStringToObject(obj, "color", this->color.c_str());
                }

                std::string out = cJSON_Print(obj);
                delete obj;
                return out;
            }
        };

        void flog(Log f);
    }
}
