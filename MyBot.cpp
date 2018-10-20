#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"

#include <random>
#include <string>
#include <unordered_set>
#include <map>
#include <ctime>

using namespace std;
using namespace hlt;

enum ShipState {
    GATHERING,
    RETURNING,
    SUPER_RETURN,
};

int main(int argc, char* argv[]) {
    unsigned int rng_seed;
    if (argc > 1) {
        rng_seed = static_cast<unsigned int>(stoul(argv[1]));
    } else {
        rng_seed = static_cast<unsigned int>(time(nullptr));
    }
    mt19937 rng(rng_seed);

    Game game;
    // At this point "game" variable is populated with initial map data.
    // This is a good place to do computationally expensive start-up pre-processing.
    // As soon as you call "ready" function below, the 2 second per turn timer will start.
    game.ready("adbv8");

    log::log("Successfully created bot! My Player ID is " + to_string(game.my_id) + ". Bot rng seed is " + to_string(rng_seed) + ".");

    map<EntityId, ShipState> stateMp;
    for (;;) {
        game.update_frame();
        shared_ptr<Player> me = game.me;
        unique_ptr<GameMap>& game_map = game.game_map;

        vector<Command> command_queue;

        int remaining_turns = constants::MAX_TURNS - game.turn_number;

        vector<vector<int>> proposed;
        for (int i = 0; i<game_map->width; i++) {
            proposed.push_back(vector<int>());
            for (int k = 0; k<game_map->height; k++) {
                proposed[i].push_back(0);
            }
        }

        // ships that can't move are highest priority
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            if (!game_map->canMove(ship)) {
                ship->log("can't move");
                proposed[ship->position.x][ship->position.y] = 1;
            }
            else {
                ship->log("can move");
            }
        }


        unordered_set<Position> claimed;
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            EntityId id = ship->id;
            ship->log("hit");

            if (!game_map->canMove(ship)) {
               command_queue.push_back(ship->stay_still());
                continue;
            }

            if (!stateMp.count(id)) {
                stateMp[id] = GATHERING;
            }
            if (ship->halite >= constants::MAX_HALITE * 0.91) {
                stateMp[id] = RETURNING;
            }
            if (ship->position == me->shipyard->position) {
                stateMp[id] = GATHERING;
            }
            if (remaining_turns < game_map->calculate_distance(ship->position, me->shipyard->position) + 5) {
                stateMp[id] = SUPER_RETURN;
            }

            ShipState state = stateMp[id];

            Direction move = Direction::STILL;
            vector<Direction> options;
            if (state == GATHERING) {
               ship->log("gathering");

                if (game_map->at(ship)->halite < constants::MAX_HALITE / 10) {
                    auto mdest = Position(0,0);
                    int cost = 1e9;

                    int r = 32;
                    for (int i = 0; i<2 * r; i++) {
                        for (int k = 0; k<2 * r; k++) {
                            auto dest = Position(ship->position.x - r + i, ship->position.y - r + k);
                            if (claimed.count(dest)) continue;
                            int c = game_map->costfn(ship.get(), me->shipyard->position, dest);
                            // ship->log("test");
                            // ship->log(to_string(c) + " " + to_string(i) + " " + to_string(k));
                            if (c < cost) {
                                cost = c;
                                mdest = dest;
                            }
                        }
                    }

                    // Position dest = game_map->largestInArea(ship->position, 6);
                    claimed.insert(mdest);
                    //claimed.insert(mdest.directional_offset(Direction::NORTH));
                    //claimed.insert(mdest.directional_offset(Direction::EAST));
                    //claimed.insert(mdest.directional_offset(Direction::SOUTH));
                    move = game_map->naive_navigate(ship, mdest);

                } else {
                    move = Direction::STILL;
                }
            }
            else if (state == SUPER_RETURN) {
                ship->log("super returning");
                move = game_map->naive_navigate(ship, me->shipyard->position);
            }
            else {
                ship->log("returning");
                move = game_map->naive_navigate(ship, me->shipyard->position);
            }
            ship->log("calculating proposed");
            auto pos = ship->position;
            pos = game_map->normalize(pos.directional_offset(move));
            bool super_ignore = state == SUPER_RETURN && pos == me->shipyard->position;
            if (proposed[pos.x][pos.y] && !super_ignore) {
                ship->log("detected incoming collision ");
                if (!proposed[ship->position.x][ship->position.y]) {
                    move = Direction::STILL;
                }
                else {
                    for (int i = 0; i<4; i++) {
                        auto dir = ALL_CARDINALS[i];
                        pos = ship->position.directional_offset(dir);
                        pos = game_map->normalize(pos);
                        if (!proposed[pos.x][pos.y]) {
                            move = dir;
                            break;
                        }
                        ship->log("Could not find escape :(");
                        move = ALL_CARDINALS[rng() % 4];
                    }
                }
            }
            ship->log("found proposed");
            pos = game_map->normalize(ship->position.directional_offset(move));
            proposed[pos.x][pos.y] = 1;
            command_queue.push_back(ship->move(move));
        }

        log::log("Command queue ", command_queue.size());

        auto yardpos = me->shipyard->position;
        if (
            game.turn_number <= constants::MAX_TURNS / 2 &&
            me->halite >= constants::SHIP_COST &&
            !game_map->at(me->shipyard)->is_occupied() &&
            !proposed[yardpos.x][yardpos.y])
        {
            command_queue.push_back(me->shipyard->spawn());
        }

        if (!game.end_turn(command_queue)) {
            break;
        }
    }

    return 0;
}
