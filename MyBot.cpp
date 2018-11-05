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
    EXPLORING,
    RETURNING,
    SUPER_RETURN,
};

enum OrderType {
    GATHER,
    RETURN,
};

Position closest_dropoff(Ship* s, Game *g, bool other_player = false) {
    auto p = g->me->shipyard->position;
    int m = g->game_map->calculate_distance(s->position,p);

    if (other_player) {
        for (auto play: g->players) {
            if (play->id != g->me->id) {
                p = g->me->shipyard->position;
                m = g->game_map->calculate_distance(s->position,p);
                for (auto d : play->dropoffs) {
                    auto currp = d.second->position;
                    int currd = g->game_map->calculate_distance(s->position,currp);

                    if (currd < m) {
                        m = currd;
                        p = currp;
                    }
                }
                return p;
            }
        }
    }


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
    bool collision = !is_1v1;
    bool built_dropoff = false;

    game.ready("adbv14");


    map<EntityId, ShipState> stateMp;
    log::log("Successfully created bot! My Player ID is " + to_string(game.my_id) + ". Bot rng seed is " + to_string(rng_seed) + ".");
    for (;;) {
        game.update_frame();
        shared_ptr<Player> me = game.me;
        unique_ptr<GameMap>& game_map = game.game_map;

        vector<Command> command_queue;
        unordered_set<Ship*> assigned;

        int remaining_turns = constants::MAX_TURNS - game.turn_number;

        vector<vector<int>> proposed(game_map->width, vector<int>(game_map->height));
        map<EntityId , vector<Direction>> optionsMap;

        BFSR home_cost_map = game_map->BFS(me->shipyard->position);
        map<Position, BFSR> ship_to_dist;
        ship_to_dist.clear();
        ship_to_dist[me->shipyard->position] = home_cost_map;
        for (auto s : me->ships) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
        }
        for (auto s : me->dropoffs) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
        }

        // ROLE ASSIGNMENT
        // ships that can't move are highest priority
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            if (!game_map->canMove(ship)) {
                proposed[ship->position.x][ship->position.y] = 1;
                command_queue.push_back(ship->stay_still());
                assigned.insert(ship.get());
            }
        }

        // priority to gathering
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            EntityId id = ship->id;

            if (!stateMp.count(id)) {
                stateMp[id] = GATHERING;
            }
            if (game_map->at(ship->position)->halite > 100) {
                stateMp[id] = GATHERING;
            }
            if (ship->halite >= constants::MAX_HALITE * 0.90) {
                stateMp[id] = RETURNING;
            }
            if (ship->halite == 0) {
                stateMp[id] = GATHERING;
            }
            if (remaining_turns < game_map->calculate_distance(ship->position, closest_dropoff(ship.get(), &game))
            + game_map->width / 6) {
                stateMp[id] = SUPER_RETURN;
            }
        }


        // Gathering
        // DROPOFF + GATHERING CONDITIONS
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            EntityId id = ship->id;
            if (assigned.count(ship.get())) continue;
            ShipState state = stateMp[id];
            if (state == GATHERING) {
                // dropoff condition
                if (game.turn_number > constants::MAX_TURNS / 2
                    && game_map->at(ship)->halite > constants::MAX_HALITE / 2
                    && me->halite >= constants::SHIP_COST + constants::DROPOFF_COST
                    && !built_dropoff
                    && is_1v1) {
                    command_queue.push_back(ship->make_dropoff());
                    assigned.insert(ship.get());
                    built_dropoff = true;
                }
                // TODO(abenner) median
                else if (game_map->at(ship)->halite > game_map->get_halite_percentile(0.50)) {
                    proposed[ship->position.x][ship->position.y] = 1;
                    command_queue.push_back(ship->stay_still());
                    assigned.insert(ship.get());
                }
            }
        }

        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            if (assigned.count(ship.get())) continue;

            auto mdest = closest_dropoff(ship.get(), &game);
            vector<Direction> options;
            VVP& pars = ship_to_dist[ship->position].parent;
            options = game_map->minCostOptions(pars, ship->position, mdest);
            optionsMap[ship->id] = options;
        }

        unordered_set<Position> claimed;
        set<EntityId> added;
        for (int i = 0; (size_t)i<me->ships.size(); i++) {
            Ship *next = nullptr;
            auto mdest = Position(0,0);
            double cost = 1e9;

            log::log("hit");
            for (auto s : me->ships) {
                shared_ptr<Ship> ship = s.second;
                if (assigned.count(ship.get())) continue;
                if (added.count(ship->id)) continue;

                EntityId id = ship->id;
                ShipState state = stateMp[id];
                if (state != GATHERING) {
                    continue;
                }

                /*
                auto prntvec = [](VVI &v) {
                    for (auto a : v) {
                        string s = "\t";
                        for (auto c : a) {
                            s += to_string(c);
                            s += "\t";
                        }
                        log::log(s);
                    }
                };*/

                vector<Direction> options;
                VVI& dist = ship_to_dist[ship->position].dist;
                VVI& dropoff_dist = ship_to_dist[closest_dropoff(ship.get(), &game)].dist;
                for (int i = 0; i<game_map->width; i++) for (int k = 0; k<game_map->width; k++) {
                        auto dest = Position(i, k);
                        if (claimed.count(dest)) continue;

                        // TODO(abenner) dropoffs
                        int cost_to = dist[dest.x][dest.y];
                        int cost_from = dropoff_dist[dest.x][dest.y];
                        double c = game_map->costfn(ship.get(), cost_to, cost_from, closest_dropoff(ship.get(), &game), dest, me->id);
                        // log::log('cost', c);
                        if (c < cost) {
                            cost = c;
                            mdest = dest;
                            next = ship.get();
                        }
                    }
            }

            if (next == nullptr) break;
            added.insert(next->id);
            claimed.insert(mdest);

            // VVP& pars = ship_to_dist[next->position].parent;
            vector<Direction> options;
            options = game_map->get_unsafe_moves(next->position, mdest);
            optionsMap[next->id] = options;
        }

        for (const auto& ship_iterator : me->ships) {
            auto ship = ship_iterator.second;
            if (assigned.count(ship.get())) continue;
            vector<Direction> options = optionsMap[ship->id];
            ShipState state = stateMp[ship->id];

            Direction selected_move = Direction::STILL;
            int i = 0;
            if (options.size() == 0)
                options.push_back(Direction::STILL);
            for (auto move : options) {
                auto pos = ship->position;
                pos = game_map->normalize(pos.directional_offset(move));
                bool super_ignore = state == SUPER_RETURN && pos == me->shipyard->position;
                for (auto d : me->dropoffs) {
                    super_ignore = super_ignore || (state == SUPER_RETURN && pos == d.second->position);
                }

                //bool enemy_collision = game_map->at(pos)->occupied_by_not(me->id);
                int dist_from_base = game_map->calculate_distance(pos, closest_dropoff(ship.get(), &game));
                bool enemy_collision = collision && game_map->is_in_range_of_enemy(pos, me->id) && dist_from_base > 5;

                if ((proposed[pos.x][pos.y] || enemy_collision) && !super_ignore) {
                    // try next option
                    ship->log("detected incoming collision ");
                    if ((size_t)i < options.size() - 1) {
                        i++;
                        continue;
                    }
                    // pick something else
                    if (!proposed[ship->position.x][ship->position.y]) {
                        move = Direction::STILL;
                    } else {
                        for (int i = 0; i < 4; i++) {
                            auto dir = ALL_CARDINALS[i];
                            pos = ship->position.directional_offset(dir);
                            pos = game_map->normalize(pos);
                            enemy_collision = (dist_from_base >= 5) && collision && game_map->is_in_range_of_enemy(pos, me->id) && dist_from_base > 5;
                            if (!proposed[pos.x][pos.y] && !enemy_collision) {
                                move = dir;
                                break;
                            }
                            dir = Direction::STILL;
                        }
                        ship->log("Could not find escape :(");
                    }
                }
                else {
                    ship->log("Move okay");
                }

                selected_move = move;
                break;
            }

            auto pos = game_map->normalize(ship->position.directional_offset(selected_move));
            proposed[pos.x][pos.y] = 1;
            command_queue.push_back(ship->move(selected_move));
        }

        int sum = 0;
        for (int i = 0; i < game_map->width; i++) {
            for (int k = 0; k < game_map->width; k++) {
                sum += proposed[i][k];
            }
        }
        log::log(sum, " ", me->ships.size());

        auto yardpos = me->shipyard->position;
        if (
            remaining_turns > 220 &&
            me->halite >= constants::SHIP_COST &&
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
