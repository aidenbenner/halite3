#pragma once

#include "entity.hpp"
#include "constants.hpp"
#include "command.hpp"

#include <memory>

using namespace std;
namespace hlt {
    class GameMap;
    struct Game;


    struct Ship : Entity {
        Halite halite;
        ShipState state;

        int last_hal = 0;
        int lifetime_hal = 0;

        Ship(PlayerId player_id, EntityId ship_id, int x, int y, Halite halite) :
            Entity(player_id, ship_id, x, y),
            halite(halite)
        {}

        vector<Direction> GetBannedDirs(GameMap *game_map, EnemyResponse type, Game& g);
        vector<Direction> GetAllowedDirs(GameMap *game_map, EnemyResponse type, Game &g);

        bool is_full() const {
            return halite >= constants::MAX_HALITE;
        }

        Command make_dropoff() const {
            return hlt::command::transform_ship_into_dropoff_site(id);
        }

        Command move(Direction direction) const {
            return hlt::command::move(id, direction);
        }

        Command stay_still() const {
            return hlt::command::move(id, Direction::STILL);
        }

        void log(std::string s) {
            log::log("Ship #" + std::to_string(id) + ": " + s);
        }

        static std::shared_ptr<Ship> _generate(PlayerId player_id);

        void _update(int halite, Position position) {
            this->halite = halite;
            this->position = position;

            if (halite == 0 && last_hal > 100) {
                lifetime_hal += last_hal;
            }
            last_hal = this->halite;
        }
    };


    struct Order {
        int priority;
        int type;

        map<Direction, double> nextCosts;

        void add_dir_priority(Direction d, double c) {
            nextCosts[d] = c;
        }

        Ship *ship;
        Position planned_dest;

        bool operator<(const Order& b) {
            return priority < b.priority;
        }

        Position post() {
            return planned_dest;
        }

        Ship* use() {
            return ship;
        }


        void setAllCosts(double d) {
            add_dir_priority(Direction::STILL, d);
            add_dir_priority(Direction::NORTH, d);
            add_dir_priority(Direction::SOUTH, d);
            add_dir_priority(Direction::EAST, d);
            add_dir_priority(Direction::WEST, d);
        }

        Order() {}

        Order(int prioritiy, int type, Ship* ship, Position planned_dest) : priority(prioritiy), type(type), ship(ship), planned_dest(planned_dest) {
            int def = 5;
            setAllCosts(def);
        }
    };



}
