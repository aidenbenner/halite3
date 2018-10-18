#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"

#include <random>
#include <string>
#include <map>
#include <ctime>

using namespace std;
using namespace hlt;

enum ShipState {
    GATHERING,
    RETURNING,
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
    int version = 0;
    game.ready("adb" + version);

    log::log("Successfully created bot! My Player ID is " + to_string(game.my_id) + ". Bot rng seed is " + to_string(rng_seed) + ".");

    map<EntityId, ShipState> stateMp;
    for (;;) {
        game.update_frame();
        shared_ptr<Player> me = game.me;
        unique_ptr<GameMap>& game_map = game.game_map;

        vector<Command> command_queue;

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

        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            EntityId id = ship->id;

            if (!game_map->canMove(ship)) {
               command_queue.push_back(ship->stay_still());
                continue;
            }

            if (!stateMp.count(id)) {
                stateMp[id] = GATHERING;
            }
            if (ship->is_full()) {
                stateMp[id] = RETURNING;
            }
            if (ship->position == me->shipyard->position) {
                stateMp[id] = GATHERING;
            }
            ShipState state = stateMp[id];

            Direction move = Direction::STILL;
            if (state == GATHERING) {
               ship->log("gathering");
                if (game_map->at(ship)->halite < constants::MAX_HALITE / 10) {
                    Direction random_direction = ALL_CARDINALS[rng() % 4];


                    int mhalite = game_map->at(ship->position)->halite;
                    move = Direction::STILL;

                    auto dir = Direction::STILL;
                    for (int i = 0; i<4; i++) {
                        dir = ALL_CARDINALS[i];
                        int hal = game_map->at(ship->position.directional_offset(dir))->halite;
                        if (hal > mhalite) {
                            move = dir;
                        }
                    }
                    // move = random_direction;
                } else {
                    move = Direction::STILL;
                }
            }
            else {
                ship->log("returning");
                move = game_map->naive_navigate(ship, me->shipyard->position);
            }

            auto pos = ship->position;
            pos = game_map->normalize(pos.directional_offset(move));
            if (proposed[pos.x][pos.y]) {
                ship->log("detected incoming collision ");
                for (int i = 0; i<4; i++) {
                    auto dir = ALL_CARDINALS[i];
                    pos = ship->position.directional_offset(dir);
                    pos = game_map->normalize(pos);
                    ship->log(to_string(pos.x) + " " + to_string(pos.y));
                    ship->log(to_string((char)dir));
                    if (!proposed[pos.x][pos.y]) {
                        move = dir;
                        break;
                    }
                    ship->log("Could not find escape :(");
                    move = ALL_CARDINALS[rng() % 4];
                }
            }
            pos = game_map->normalize(ship->position.directional_offset(move));

            proposed[pos.x][pos.y] = 1;

            command_queue.push_back(ship->move(move));
        }

        auto yardpos = me->shipyard->position;
        if (
            game.turn_number <= 200 &&
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
