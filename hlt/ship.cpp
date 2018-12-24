#include "ship.hpp"
#include "input.hpp"
#include "direction.hpp"
#include "game_map.hpp"
#include "game.hpp"

using namespace hlt;

std::shared_ptr<hlt::Ship> hlt::Ship::_generate(hlt::PlayerId player_id) {
    hlt::EntityId ship_id;
    int x;
    int y;
    hlt::Halite halite;
    hlt::get_sstream() >> ship_id >> x >> y >> halite;

    return std::make_shared<hlt::Ship>(player_id, ship_id, x, y, halite);
}

vector<Direction> Ship::GetBannedDirs(GameMap *game_map, EnemyResponse type, Game& g) {
    auto dirs = GetAllowedDirs(game_map, type, g);

    vector<Direction> out;
    for (auto dir : ALL_DIRS) {
        bool found = false;
        for (auto d : dirs) {
            if (dir == d) {
                found = true;
            }
        }
        if (!found) {
            out.push_back(dir);
        }
    }

    return out;
}

vector<Direction> Ship::GetAllowedDirs(GameMap *game_map, EnemyResponse type, Game &g) {
    // Check if we can move
    if (halite < game_map->at(this)->cost()) {
        return vector<Direction>(1, Direction::STILL);
    }

    bool stuck = is_stuck();

    // Check other dirs
    vector<Direction> out;
    for (auto d : ALL_DIRS) {
        auto pos = this->position.directional_offset(d);
        // Ship* enemy_on_square = game_map->enemy_in_range(pos, constants::PID, true);
        Ship* enemy = game_map->enemy_in_range(pos, constants::PID);

        if (!stuck && enemy != nullptr) {
            switch(type) {
                case AVOID:
                    // Don't add
                    break;
                case TOLERATE:
                    if (d == Direction::STILL || enemy == nullptr) {
                        out.push_back(d);
                    }
                    break;
                case IGNORE:
                    out.push_back(d);
                    break;
                case SMART:
                    // in a 5 move radius who has more ships?
                    Ship *s = game_map->get_closest_ship(position, g.players, {this, enemy});
                    int enemies = game_map->enemies_around_point(position, 3);
                    int friends = game_map->enemies_around_point(position, 3);
                    if (s->owner == constants::PID) {
                        if (friends >= enemies) {
                            out.push_back(d);
                        }
                    }
                    break;
            }
        }
        else {
            out.push_back(d);
        }
    }
    return out;
}
