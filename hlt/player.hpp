#pragma once

#include "types.hpp"
#include "shipyard.hpp"
#include "ship.hpp"
#include "dropoff.hpp"

#include <memory>
#include <unordered_map>
#include <set>

namespace hlt {
    struct Player {
        PlayerId id;
        std::shared_ptr<Shipyard> shipyard;
        Halite halite;
        set<Position> collisions;

        bool recent_collision(Position p) {
            return collisions.count(p);
        }

        std::set<EntityId> profitable_ships;
        std::map<EntityId, std::shared_ptr<Ship>> ships;
        std::map<EntityId, std::shared_ptr<Dropoff>> dropoffs;

        Player(PlayerId player_id, int shipyard_x, int shipyard_y) :
            id(player_id),
            shipyard(std::make_shared<Shipyard>(player_id, shipyard_x, shipyard_y)),
            halite(0)
        {}

        void _update(int num_ships, int num_dropoffs, Halite halite);
        static std::shared_ptr<Player> _generate();
    };
}
