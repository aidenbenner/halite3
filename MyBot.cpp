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


// KNOBS
const bool COLLIDE_IN_1v1 = false;

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

struct Order {
    int priority;
    int type;
    VC<Direction> next_dirs;
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

    Order() {}
    Order(int prioritiy, int type, VC<Direction> dir, Ship* ship, Position planned_dest) : priority(prioritiy), type(type), next_dirs(dir), ship(ship), planned_dest(planned_dest) { }
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

    // INIT
    bool is_1v1 = game.players.size() == 2;
    bool collision = !is_1v1;

    map<EntityId, ShipState> stateMp;
    vector<int> halite_at_turn;

    bool save_for_drop = false;
    const bool ENABLE_DROPOFFS = true;

    game.ready("adbv29");
    log::log("Successfully created bot! My Player ID is " + to_string(game.my_id) + ". Bot rng seed is " + to_string(rng_seed) + ".");

    for (;;) {
        // INIT
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
        map<EntityId, Order> ordersMap;

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

        // Role assignment
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            EntityId id = ship->id;

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
        // DROPOFFs
        // shipyard is a dropoff
        int expected_dropoffs = me->ships.size() / 15 + 1;
        int curr_dropoffs = me->dropoffs.size();

        Ship* best_dropoff = nullptr;
        float curr_avg_halite = 0;
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            if (assigned.count(ship.get())) continue;

            int dist = game_map->calculate_distance(ship->position, closest_dropoff(ship->position, &game));
            float avg_halite = game_map->avg_around_point(ship->position, 5);

            // min requirements
            //log::log(remaining_turns , dist , avg_halite , expected_dropoffs);
            //log::log(remaining_turns > 100, dist > game_map->width / 3, avg_halite > 100, curr_dropoffs < expected_dropoffs);
            if (game_map->at(ship)->halite > 4000 - ship->halite) {
                curr_avg_halite = 9999999;
                best_dropoff = ship.get();
            }
            if (remaining_turns > 100
                && dist > game_map->width / 3
                && avg_halite > 100
                && curr_dropoffs < expected_dropoffs) {
                if (avg_halite > curr_avg_halite) {
                    curr_avg_halite = avg_halite;
                    best_dropoff = ship.get();
                }
            }
        }
        log::log(curr_avg_halite);

        save_for_drop = false;
        if (best_dropoff != nullptr && ENABLE_DROPOFFS) {
            if (me->halite + best_dropoff->halite >= constants::DROPOFF_COST) {
                auto ship = best_dropoff;
                me->dropoffs[(int)-ship->id] = std::make_shared<Dropoff>(me->id, -ship->id, ship->position.x, ship->position.y);
                command_queue.push_back(ship->make_dropoff());
                assigned.insert(ship);
                closestDropMp.clear();
                me->halite -= 4000;
            }
            else {
                save_for_drop = true;
            }
        }
        // END DROPOFFS

        // GATHERING
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            EntityId id = ship->id;
            if (assigned.count(ship.get())) continue;
            auto state = stateMp[id];
            if (state == GATHERING || state == RETURNING) {
                log::log("HAl percentile", game_map->get_halite_percentile(0.50), game_map->at(ship)->halite);
                if (game_map->at(ship)->halite > game_map->get_halite_percentile(0.50)) {
                    if (state == RETURNING && ship->halite >= 899) {
                        continue;
                    }
                    proposed[ship->position.x][ship->position.y] = 1;
                    command_queue.push_back(ship->stay_still());
                    assigned.insert(ship.get());
                    log::log("Ship is gathering", ship->id);
                }
            }
        }

        map<Position, BFSR> ship_to_dist;
        map<Position, BFSR> greedy_bfs;
        ship_to_dist.clear();
        for (auto s : me->ships) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
        }
        for (auto s : me->dropoffs) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
        }


        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            if (assigned.count(ship.get())) continue;
            auto state = stateMp[ship->id];

            auto mdest = closest_dropoff(ship->position, &game);
            vector<Direction> options;
            VVP& pars = ship_to_dist[ship->position].parent;
            options = game_map->minCostOptions(pars, ship->position, mdest);

            if (state == RETURNING) {
                 // options = game_map->plan_min_cost_route(pars, ship->halite, ship->position, mdest);
            }
            ordersMap[ship->id] = Order{0, RETURNING, options, ship.get(), mdest};
        }

        // Fill ship costs
        struct Cost {
            Position dest;
            Ship* s;
        };

        map<double, VC<Cost>> costs;
        for (auto s : me->ships) {
            shared_ptr<Ship> ship = s.second;
            if (assigned.count(ship.get())) continue;
            log::log("Ship is exploring", ship->id);

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
                    int net_cost_to = dist[dest.x][dest.y];
                    int cost_from = dropoff_dist[dest.x][dest.y];
                    double c = game_map->costfn(ship.get(), net_cost_to, cost_from, drop, dest, me->id, is_1v1);
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
                log::log("hit ", cost_itr->first);

                auto mdest = cost.dest;
                vector<Direction> options;
                //options = game_map->plan_gather_path(cost.s->halite, cost.s->position, mdest);
                //if (options.size() == 0)
                //   continue;
                // game_map->plan_min_cost_route(greedy_bfs[cost.s->position].parent, cost.s->halite, cost.s->position, mdest);
                options = game_map->get_unsafe_moves(cost.s->position, mdest);// game_map->dirsFrompath(game_map->traceBackPath(greedy_bfs[cost.s->position].parent, cost.s->position, mdest));

                added.insert(cost.s->id);
                claimed.insert(mdest);
                ordersMap[cost.s->id] = Order{10, GATHERING, options, cost.s, mdest};
            }
            cost_itr++;
        }

        VC<Order> orders;
        for (auto s : ordersMap) {
            orders.push_back(s.second);
        }

        sort(orders.begin(), orders.end());

        for (const auto& order : orders) {
            auto ship = order.ship;
            if (assigned.count(ship)) continue;
            log::log("hit", order.priority);
            vector<Direction> options = order.next_dirs;
            ShipState state = stateMp[ship->id];
            log::log("shit", order.ship);

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

                if (COLLIDE_IN_1v1) {
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
                    if (!proposed[ship->position.x][ship->position.y] && game_map->at(ship->position)->halite > game_map->get_halite_percentile(0.5)) {
                        move = Direction::STILL;
                    } else {
                        for (int i = 0; i < 4; i++) {
                            auto dir = ALL_CARDINALS[i];
                            pos = ship->position.directional_offset(dir);
                            pos = game_map->normalize(pos);
                            enemy_collision = collision && game_map->is_in_range_of_enemy(pos, me->id) && dist_from_base > 1;
                            if (COLLIDE_IN_1v1) {
                                enemy_collision = game_map->at(pos)->occupied_by_not(me->id);
                            }
                            if (!proposed[pos.x][pos.y] && !enemy_collision) {
                                move = dir;
                                break;
                            }
                            move = Direction::STILL;
                        }
                    }
                }
                selected_move = move;
                break;
            }

            log::log("Assigned");
            auto pos = game_map->normalize(ship->position.directional_offset(selected_move));
            proposed[pos.x][pos.y] = 1;
            command_queue.push_back(ship->move(selected_move));
        }

        // Ship spawning + logging.
        auto shipyard_pos = me->shipyard->position;
        int save_to = 1000;
        if (save_for_drop) {
            save_to += constants::DROPOFF_COST;
        }

        int ship_target = 10;
        if (is_1v1) {
            ship_target = max(10, (int)opponent->ships.size());
        }
        if (me->halite >= save_to &&
            !proposed[shipyard_pos.x][shipyard_pos.y])
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
