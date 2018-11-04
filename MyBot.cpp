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
    bool collision = false;
    bool built_dropoff = false;

    game.ready("adbv12");
    log::log("Successfully created bot! My Player ID is " + to_string(game.my_id) + ". Bot rng seed is " + to_string(rng_seed) + ".");

    map<EntityId, ShipState> stateMp;
    for (;;) {
        game.update_frame();
        shared_ptr<Player> me = game.me;
        unique_ptr<GameMap>& game_map = game.game_map;

        vector<Command> command_queue;
        unordered_set<Ship*> assigned;

        int remaining_turns = constants::MAX_TURNS - game.turn_number;

        vector<vector<int>> proposed(game_map->width, vector<int>(game_map->height));

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
            if (ship->halite >= constants::MAX_HALITE * 0.80) {
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
                else if (game_map->at(ship)->halite > constants::MAX_HALITE / 10) {
                    proposed[ship->position.x][ship->position.y] = 1;
                    command_queue.push_back(ship->stay_still());
                    assigned.insert(ship.get());
                }
            }
        }


        VVI home_cost_map = game_map->BFS(me->shipyard->position);
        map<Position, VVI> ship_to_dist;
        ship_to_dist[me->shipyard->position] = home_cost_map;
        for (auto s : me->ships) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
        }
        for (auto s : me->dropoffs) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
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

                VVI& dist = ship_to_dist[ship_iterator.second->position];

                if (game_map->at(ship)->halite < constants::MAX_HALITE / 10) {
                    auto mdest = Position(0,0);
                    double cost = 1e9;

                    for (int i = 0; i<game_map->width; i++) for (int k = 0; k<game_map->width; k++) {
                            auto dest = Position(i, k);
                            if (claimed.count(dest)) continue;

                            // TODO(abenner) dropoffs
                            int cost_to = dist[dest.x][dest.y];
                            int cost_from = home_cost_map[dest.x][dest.y];
                            // log::log("Cost to ", cost_to);

                            double c = game_map->costfn(ship.get(), cost_to, cost_from, closest_dropoff(ship.get(), &game), dest);
                            // log::log('cost', c);
                            if (c < cost) {
                                cost = c;
                                mdest = dest;
                            }
                        }

                    claimed.insert(mdest);
                    move = game_map->naive_navigate(ship, mdest);
                } else {
                    move = Direction::STILL;
                }
            }
            else if (state == SUPER_RETURN) {
                ship->log("super returning");
                move = game_map->naive_navigate(ship, closest_dropoff(ship.get(), &game));
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

            //bool enemy_collision = game_map->at(pos)->occupied_by_not(me->id);
            bool enemy_collision = game_map->is_in_range_of_enemy(pos,me->id);
            // zone of influence
            int dist_from_base = game_map->calculate_distance(pos, closest_dropoff(ship.get(), &game));
            if (collision && enemy_collision && dist_from_base >= game.game_map->width / 2) {
                ship->log("enemy collision detected... avoiding");
                move = Direction::STILL;
            }
            pos = game_map->normalize(ship->position.directional_offset(move));

            proposed[pos.x][pos.y] = 1;
            command_queue.push_back(ship->move(move));
        }

        log::log("Command queue ", command_queue.size());

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
