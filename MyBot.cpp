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

Position closest_dropoff(Ship* s, Game *g) {
    auto p = g->me->shipyard->position;
    int m = g->game_map->calculate_distance(s->position,p);
    for (auto d : g->me->dropoffs) {
        auto currp = d.second->position;
        int currd = g->game_map->calculate_distance(s->position,currp);

        if (currd < m) {
            m = currd;
            p = currp;
        }
    }
    return p;
}

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
    bool is_1v1 = game.players.size() == 2;
    bool built_dropoff = false;
    game.ready("adbv10");

    log::log("Successfully created bot! My Player ID is " + to_string(game.my_id) + ". Bot rng seed is " + to_string(rng_seed) + ".");

    map<EntityId, ShipState> stateMp;
    for (;;) {
        game.update_frame();
        shared_ptr<Player> me = game.me;
        unique_ptr<GameMap>& game_map = game.game_map;

        vector<Command> command_queue;
        unordered_set<Ship*> assigned;

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
                command_queue.push_back(ship->stay_still());
                assigned.insert(ship.get());
            }
            else {
                ship->log("can move");
            }
        }

        // priority to gathering
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            EntityId id = ship->id;

            if (!stateMp.count(id)) {
                stateMp[id] = GATHERING;
            }
            if (ship->halite >= constants::MAX_HALITE * 0.91) {
                stateMp[id] = RETURNING;
            }
            if (ship->halite == 0) {
                stateMp[id] = GATHERING;
            }
            if (remaining_turns < game_map->calculate_distance(ship->position, me->shipyard->position) + 5) {
                stateMp[id] = SUPER_RETURN;
            }
        }

        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            EntityId id = ship->id;
            if (assigned.count(ship.get())) continue;
            ShipState state = stateMp[id];
            if (state == GATHERING) {
                if (game.turn_number > constants::MAX_TURNS / 2
                    && game_map->at(ship)->halite > constants::MAX_HALITE / 2
                    && me->halite >= constants::SHIP_COST + constants::DROPOFF_COST
                    && !built_dropoff && is_1v1) {
                    command_queue.push_back(ship->make_dropoff());
                    assigned.insert(ship.get());
                    built_dropoff = true;
                }
                else if (game_map->at(ship)->halite > constants::MAX_HALITE / 10) {
                    proposed[ship->position.x][ship->position.y] = 1;
                    command_queue.push_back(ship->stay_still());
                    assigned.insert(ship.get());
                }
            }
        }

        unordered_set<Position> claimed;
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            if (assigned.count(ship.get())) continue;

            EntityId id = ship->id;
            ship->log("hit");

            if (!game_map->canMove(ship)) {
                command_queue.push_back(ship->stay_still());
                assigned.insert(ship.get());
                continue;
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
                    game_map->clear_fast_mincostnav();
                    for (int i = 0; i<2 * r; i++) {
                        for (int k = 0; k<2 * r; k++) {
                            auto dest = Position(ship->position.x - r + i, ship->position.y - r + k);
                            if (claimed.count(dest)) continue;
                            // not optimal closest dropoff
                            int c = game_map->costfn(ship.get(), closest_dropoff(ship.get(), &game), dest);
                            if (c < cost) {
                                cost = c;
                                mdest = dest;
                            }
                        }
                    }

                    // Position dest = game_map->largestInArea(ship->position, 6);
                    claimed.insert(mdest);
                    move = game_map->naive_navigate(ship, mdest);

                } else {
                    move = Direction::STILL;
                }
            }
            else if (state == SUPER_RETURN) {
                ship->log("super returning");
                if (ship->halite < 200 && is_1v1) {
                    int p = 0;
                    if (game.players[0]->id == me->id) {
                        p++;
                    }
                    move = game_map->naive_navigate(ship, game.players[p]->shipyard->position);
                }
                else {
                    move = game_map->naive_navigate(ship, closest_dropoff(ship.get(), &game));
                }
            }
            else {
                ship->log("returning");
                move = game_map->naive_navigate(ship, closest_dropoff(ship.get(), &game));
            }

            ship->log("calculating proposed");
            auto pos = ship->position;
            pos = game_map->normalize(pos.directional_offset(move));
            bool super_ignore = state == SUPER_RETURN && pos == me->shipyard->position;
            for (auto d : me->dropoffs) {
                super_ignore = super_ignore || (state == SUPER_RETURN && pos == d.second->position);
            }

            if (proposed[pos.x][pos.y] && !super_ignore) {
                ship->log("detected incoming collision ");
                if (!proposed[ship->position.x][ship->position.y] &&
                        ship->position != me->shipyard->position) {
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

/*
map<int, int> ship_map;
map<int, pair<int, int>> last_pos;
vector<Command> copyCat(Game &game) {
    shared_ptr<Player> me = game.me;
    unique_ptr<GameMap>& game_map = game.game_map;

    vector<Command> out;
    Player *cat = me.get();
    for (auto p : game.players) {
        if (p->id != me->id)
            cat = p.get();
    }

    set<int> assigned;
    for (auto s : cat->ships) {
        if (!ship_map.count(s.first)) {
            if (s.second->position == cat->shipyard->position) {
                out.push_back(me->shipyard->spawn());
                continue;
            }
            else {
                ship_map[s.first] = game_map->at(me->shipyard->position)->ship->id;
            }
        }
        Position last = Position(last_pos[s.first].first, last_pos[s.first].second);
        auto curr = s.second->position;
        auto our_ship = me->ships[ship_map[s.first]];
        assigned.insert(our_ship->id);

        if (last == curr) {
            out.push_back(our_ship->stay_still());
        }

        for (int i = 0; i<4; i++) {
            auto d = ALL_CARDINALS[i];
            if (last.directional_offset(d) == curr) {
                if (d == Direction::EAST)
                    d = Direction::WEST;
                else if (d == Direction::WEST)
                    d = Direction::EAST;
                out.push_back(our_ship->move(d));
            }
        }
        log::log("hit");
    }

    for (auto s : me->ships) {
        if (assigned.count(s.first)) {
            continue;
        }
        else {
            out.push_back(s.second->make_dropoff());
        }
    }

    last_pos.clear();
    for (auto s : cat->ships) {
        last_pos[s.first] = make_pair(s.second->position.x, s.second->position.y);
    }
    return out;
}
*/
