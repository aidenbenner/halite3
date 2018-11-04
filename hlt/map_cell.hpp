#pragma once

#include <cmath>
#include "types.hpp"
#include "position.hpp"
#include "ship.hpp"
#include "dropoff.hpp"

namespace hlt {
    struct MapCell {
        Position position;
        Halite halite;
        std::shared_ptr<Ship> ship;
        std::shared_ptr<Entity> structure;

        MapCell(int x, int y, Halite halite) :
            position(x, y),
            halite(halite)
        {}

        bool is_empty() const {
            return !ship && !structure;
        }

        int cost() {
            return floor(1.0 / constants::MOVE_COST_RATIO * halite);
        }

        int gain() {
            return floor(1.0 / constants::EXTRACT_RATIO * halite);
        }

        bool is_occupied(PlayerId p) const {
            if (is_occupied()) {
                return p == ship->owner;
            }
            return false;
        }

        bool occupied_by_not(PlayerId p) {
            return !is_occupied(p) && is_occupied();
        }

        bool is_occupied() const {
            return static_cast<bool>(ship);
        }

        bool has_structure() const {
            return static_cast<bool>(structure);
        }

        void mark_unsafe(std::shared_ptr<Ship>& ship) {
            this->ship = ship;
        }
    };
}
