#include "player.hpp"
#include "input.hpp"

void hlt::Player::_update(int num_ships, int num_dropoffs, Halite halite) {
    this->halite = halite;

    map<EntityId, shared_ptr<Ship>> nextShips;
    for (int i = 0; i < num_ships; ++i) {
        std::shared_ptr<hlt::Ship> ship = hlt::Ship::_generate(id);
        if (ships.count(ship->id)) {
            nextShips[ship->id] = ships[ship->id];
            nextShips[ship->id]->_update(ship->halite, ship->position);
        }
        else {
            nextShips[ship->id] = ship;
        }

        if (nextShips[ship->id]->lifetime_hal > 1000) {
            profitable_ships.insert(ship->id);
        }
    }
    ships = nextShips;

    dropoffs.clear();
    for (int i = 0; i < num_dropoffs; ++i) {
        std::shared_ptr<hlt::Dropoff> dropoff = hlt::Dropoff::_generate(id);
        dropoffs[dropoff->id] = dropoff;
    }
}


std::shared_ptr<hlt::Player> hlt::Player::_generate() {
    PlayerId player_id;
    int shipyard_x;
    int shipyard_y;
    hlt::get_sstream() >> player_id >> shipyard_x >> shipyard_y;

    return std::make_shared<hlt::Player>(player_id, shipyard_x, shipyard_y);
}
