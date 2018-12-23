#pragma once

#include "game_map.hpp"
#include "player.hpp"
#include "types.hpp"

#include <vector>
#include <iostream>

using namespace std;
typedef vector<vector<int>> VVI;

namespace hlt {
    struct Game {
        int turn_number;
        PlayerId my_id;
        std::vector<std::shared_ptr<Player>> players;
        std::shared_ptr<Player> me;
        std::unique_ptr<GameMap> game_map;

        std::vector<std::shared_ptr<Player>> getEnemies() {
            std::vector<std::shared_ptr<Player>> enemies;
            for (auto p : players) {
                if (p->id == me->id) continue;
                enemies.push_back(p);
            }
            return enemies;
        }

        std::shared_ptr<Player> getClosestOpponentByScore() {
            int mdiff = 1e9;
            std::shared_ptr<Player> player = me;
            for (auto p : getEnemies()) {
                int diff = abs(p->halite - me->halite);
                if (diff < mdiff) {
                    mdiff = diff;
                    player = p;
                }
            }
            return player;
        }

        Game();
        void ready(const std::string& name);
        void update_frame();
        bool end_turn(const std::vector<Command>& commands);
    };
}
