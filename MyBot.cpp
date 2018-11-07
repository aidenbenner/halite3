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
    CHOOSEN_DROP,
};

enum OrderType {
    GATHER,
    RETURN,
};

map<Position, Position> closestDropMp;
Position closest_dropoff(Position pos, Game *g) {
    if (closestDropMp.count(pos)) return closestDropMp[pos];
    auto p = g->me->shipyard->position;
    int m = g->game_map->calculate_distance(pos,p);

    for (auto d : g->me->dropoffs) {
        auto currp = d.second->position;
        int currd = g->game_map->calculate_distance(pos,currp);

        if (currd < m) {
            m = currd;
            p = currp;
        }
    }
    return closestDropMp[pos] = p;
}

Ship* getClosestShip(Position pos, Player &p, GameMap &g) {
    int dist = 10000000;
    Ship* soj = nullptr;
    for (auto s : p.ships) {
        int curr =  g.calculate_distance(pos, s.second->position);
        if (curr < dist) {
            dist = curr;
            soj = s.second.get();
        }
    }
    return soj;
}

const int DROPOFF_THRESH = 8141;
Position getBestDropoff(Game &g) {
    Position out = Position(-1, -1);
    int maxz = -1;
    for (int i = 0; i<g.game_map->width; i++) {
        for (int k = 0; k<g.game_map->height; k++) {
            auto curr = Position(i, k);
            int c = g.game_map->sum_around_point(curr, 5);
            if (c < DROPOFF_THRESH) continue;
            int dist = g.game_map->calculate_distance(curr, closest_dropoff(curr, &g));
            if (dist > 21) {
                Ship* close = getClosestShip(curr, *g.me.get(), *g.game_map.get());
                auto ship_pos = g.me->shipyard->position;
                int dist_to_yard = g.game_map->calculate_distance(ship_pos, curr);
                if (close != nullptr) {
                    ship_pos = close->position;
                }
                int d_to_ship = g.game_map->calculate_distance(ship_pos, curr);

                bool use = true;
                for (auto other_player : g.players) {
                    int dist_to_other = g.game_map->calculate_distance(curr, other_player->shipyard->position);
                    if (dist_to_yard > dist_to_other + 5)
                        use = false;
                }

                if (!use)
                    continue;


               if ((close == nullptr || d_to_ship < 30)) {
                    if (c > maxz) {
                        maxz = c;
                        out = curr;
                    }
                }
            }
        }
    }
    return out;
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

    game.ready("adbv23");

    map<EntityId, ShipState> stateMp;
    vector<int> halite_at_turn;
    log::log("Successfully created bot! My Player ID is " + to_string(game.my_id) + ". Bot rng seed is " + to_string(rng_seed) + ".");
    //const int FIRST_DROP = 100;
    // int ships_per_drop = 25;
    int last_drop_off = 0;
    Position chosen_drop = getBestDropoff(game);
    Ship *lastShip = nullptr;
    for (;;) {
        last_drop_off++;
        game.update_frame();
        closestDropMp.clear();
        shared_ptr<Player> me = game.me;
        shared_ptr<Player> opponent = game.players[0];
        if (opponent->id == me->id)
            opponent = game.players[1];

        unique_ptr<GameMap>& game_map = game.game_map;


        vector<Command> command_queue;
        unordered_set<Ship*> assigned;
        me->dropoffs[-3000] = make_shared<Dropoff>(me->id, -3000, me->shipyard->position.x, me->shipyard->position.y);

        int remaining_turns = constants::MAX_TURNS - game.turn_number;

        vector<vector<int>> proposed(game_map->width, vector<int>(game_map->height));
        map<EntityId , vector<Direction>> optionsMap;



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

        // CHOSEN DROP
        int num_ships = me->ships.size();
        int num_drops = me->dropoffs.size();
        int expected_dropoffs = num_ships / 10;
        bool save_for_drop = false;
        if (num_drops < expected_dropoffs && remaining_turns > 60) {
            log::log("hit");
            Ship* chosen_ship = getClosestShip(chosen_drop, *me.get(), *game_map);
            if (lastShip != nullptr && chosen_ship != nullptr) {
                if (chosen_ship->id != lastShip->id) {
                    chosen_drop = getBestDropoff(game);
                    chosen_ship = getClosestShip(chosen_drop, *me.get(), *game_map);
                }
            }
            lastShip = chosen_ship;

            if (chosen_ship != nullptr)
                log::log(chosen_ship->id);

            for (auto s : me->ships) {
                if (s.second->id != chosen_ship->id) {
                    if (stateMp[s.second->id] == CHOOSEN_DROP) {
                        stateMp[s.second->id] = GATHERING;
                    }
                }
            }

            if (game_map->at(chosen_drop)->structure != nullptr) {
                if (chosen_ship != nullptr && !assigned.count(chosen_ship)) {
                    stateMp[chosen_ship->id] = CHOOSEN_DROP;
                    if (chosen_ship->position == chosen_drop) {
                        save_for_drop = true;
                        if (me->halite > 4000) {
                            last_drop_off = game.turn_number + 1;
                            me->dropoffs[-chosen_ship->id] = std::make_shared<Dropoff>(me->id, -chosen_ship->id, chosen_drop.x, chosen_drop.y);
                            command_queue.push_back(chosen_ship->make_dropoff());
                            assigned.insert(chosen_ship);
                            me->halite -= 4000;
                        }
                    }
                    else {
                        optionsMap[chosen_ship->id] = game_map->get_unsafe_moves(chosen_ship->position, chosen_drop);
                    }
                    if (me->halite > 4000) {
                        me->dropoffs[-chosen_ship->id] = std::make_shared<Dropoff>(me->id, -chosen_ship->id, chosen_drop.x, chosen_drop.y);
                    }
                    // me->dropoffs[(int)-chosen_ship->id] = std::make_shared<Dropoff>(me->id, -chosen_ship->id, chosen_drop.x, chosen_drop.y);
                    closestDropMp.clear();
                }
            }
        }
        else {
            chosen_drop = getBestDropoff(game);
            last_drop_off = game.turn_number + 1;
        }

        // priority to gathering
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            EntityId id = ship->id;
            if (stateMp[id] == CHOOSEN_DROP) continue;

            if (!stateMp.count(id)) {
                stateMp[id] = GATHERING;
            }
            if (ship->halite >= constants::MAX_HALITE * 0.75) {
                if (game.turn_number > constants::MAX_TURNS * 0.25) {
                    if (ship->halite >= constants::MAX_HALITE * 0.95) {
                        stateMp[id] = RETURNING;
                    }
                }
                else {
                    stateMp[id] = RETURNING;
                }
            }
            if (ship->halite == 0) {
                stateMp[id] = GATHERING;
            }
            if (remaining_turns < game_map->calculate_distance(ship->position, closest_dropoff(ship->position, &game))
            + game_map->width / 6 + 0.3 * me->ships.size()) {
                stateMp[id] = SUPER_RETURN;
            }
        }

        // Gathering
        // DROPOFF + GATHERING CONDITIONS
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            EntityId id = ship->id;
            if (stateMp[id] == CHOOSEN_DROP) continue;
            if (assigned.count(ship.get())) continue;
            ShipState state = stateMp[id];
            if (state == GATHERING || state == RETURNING) {
                // dropoff condition
                /*
                int dist = game_map->calculate_distance(ship->position, closest_dropoff(ship->position, &game));
                float sum_halite = game_map->sum_around_point(ship->position, 5);
                if (remaining_turns > 100
                    && dist > 20
                    && sum_halite > 7141) {
                    if (me->halite >= constants::DROPOFF_COST) {
                        me->dropoffs[(int)-ship->id] = std::make_shared<Dropoff>(me->id, -ship->id, ship->position.x, ship->position.y);
                        command_queue.push_back(ship->make_dropoff());
                        assigned.insert(ship.get());
                        closestDropMp.clear();
                        me->halite -= 4000;
                        continue;
                    }
                }*/
                // TODO(abenner) median
                if (game_map->at(ship)->halite > game_map->get_halite_percentile(0.50)) {
                    if (state == RETURNING && ship->halite >= 899) {
                        continue;
                    }
                    proposed[ship->position.x][ship->position.y] = 1;
                    command_queue.push_back(ship->stay_still());
                    assigned.insert(ship.get());
                }
            }
        }

        map<Position, BFSR> ship_to_dist;
        ship_to_dist.clear();
        for (auto s : me->ships) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
        }
        for (auto s : me->dropoffs) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
        }

        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            if (stateMp[ship->id] == CHOOSEN_DROP) continue;
            if (assigned.count(ship.get())) continue;

            auto mdest = closest_dropoff(ship->position, &game);
            vector<Direction> options;
            VVP& pars = ship_to_dist[ship->position].parent;
            options = game_map->minCostOptions(pars, ship->position, mdest);
            optionsMap[ship->id] = options;
        }

        // Fill ship costs
        struct Cost {
            Position dest;
            Ship* s;
        };
        map<double, VC<Cost>> costs;
        for (auto s : me->ships) {
            shared_ptr<Ship> ship = s.second;
            if (stateMp[ship->id] == CHOOSEN_DROP) continue;
            if (assigned.count(ship.get())) continue;

            EntityId id = ship->id;
            ShipState state = stateMp[id];
            if (state != GATHERING) {
                continue;
            }

            vector<Direction> options;
            VVI& dist = ship_to_dist[ship->position].dist;
            for (int i = 0; i<game_map->width; i++) for (int k = 0; k<game_map->width; k++) {
                    auto dest = Position(i, k);
                    auto drop = closest_dropoff(dest, &game);
                    VVI& dropoff_dist = ship_to_dist[drop].dist;
                    int cost_to = dist[dest.x][dest.y];
                    int cost_from = dropoff_dist[dest.x][dest.y];
                    double c = game_map->costfn(ship.get(), cost_to, cost_from, drop, dest, me->id, is_1v1);
                    if (!costs.count(c)) costs[c] = VC<Cost>();
                    costs[c].PB(Cost {dest, ship.get()});
                }
        }

        unordered_set<Position> claimed;
        set<EntityId> added;
        auto cost_itr = costs.begin();
        while(cost_itr != costs.end()) {
            for (auto cost : cost_itr->second) {
                if (added.count(cost.s->id)) continue;
                if (claimed.count(cost.dest)) continue;

                auto mdest = cost.dest;
                added.insert(cost.s->id);
                claimed.insert(mdest);

                vector<Direction> options;
                options = game_map->get_unsafe_moves(cost.s->position, mdest);
                optionsMap[cost.s->id] = options;
            }
            cost_itr++;
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
                int dist_from_base = game_map->calculate_distance(pos, closest_dropoff(ship->position, &game));
                bool enemy_collision = collision && game_map->is_in_range_of_enemy(pos, me->id) && dist_from_base > 1;
                if (is_1v1) {
                    enemy_collision = game_map->at(pos)->occupied_by_not(me->id);
                }

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
                            enemy_collision = collision && game_map->is_in_range_of_enemy(pos, me->id) && dist_from_base > 1;
                            if (is_1v1) {
                                enemy_collision = game_map->at(pos)->occupied_by_not(me->id);
                            }
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

        auto yardpos = me->shipyard->position;
        int save_to = 1000;
        if (save_for_drop) {
            save_to += constants::DROPOFF_COST;
        }

        int ship_target = 10;
        if (is_1v1) {
            ship_target = max(10, (int)opponent->ships.size());
        }
        if (
            remaining_turns > 80 &&
            me->halite >= save_to &&
            !proposed[yardpos.x][yardpos.y])
        {
            if (remaining_turns > 220 || (int)me->ships.size() < ship_target) {
                command_queue.push_back(me->shipyard->spawn());
            }
        }

        if (remaining_turns == 1) {
            int i = 0;
            for (auto a : halite_at_turn) {
                i++;
                log::log(to_string(i) + "," + to_string(a));
            }
        }

        if (!game.end_turn(command_queue)) {

            break;
        }
    }

    return 0;
}
