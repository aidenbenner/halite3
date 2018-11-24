#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"
#include "hlt/utils.hpp"

#include <random>
#include <string>
#include <unordered_set>
#include <map>
#include <ctime>

using namespace std;
using namespace hlt;
using namespace constants;

bool constants::INSPIRATION_ENABLED = true;
bool constants::DROPOFFS_ENABLED = true;
bool constants::IS_DEBUG = false;

typedef VC<Position> Path;

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

struct ShipPos {
    int id;
    Position pos;

    bool operator<(const ShipPos &b) const {
        if (id == b.id) {
            return pos < b.pos;
        }
        return id < b.id;
    }
};

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
        char *p;
        int seed = strtol(argv[argc - 1], &p, 10);
        if (*p) {
            rng_seed = static_cast<unsigned int>(time(nullptr));
        }
        else {
            rng_seed = static_cast<unsigned int>(seed);
        }
    } else {
        rng_seed = static_cast<unsigned int>(time(nullptr));
    }

    mt19937 rng(rng_seed);

    for (int i = 0; i < argc; i++) {
        log::log(argv[i]);
        if (std::string(argv[i]) == "--nodrop") {
            log::log("Dropoffs disabled");
            constants::DROPOFFS_ENABLED = false;
        }
        else if (std::string(argv[i]) == "--noinspr") {
            log::log("Inspiration disabled");
            constants::INSPIRATION_ENABLED = false;
        }
        else if (std::string(argv[i]) == "--debug") {
            log::log("Debug mode enabled");
            constants::IS_DEBUG = true;
        }
        else {

        }
    }

    log::log("DROPOFFS_ENABLED", constants::DROPOFFS_ENABLED);
    log::log("INSPIRATION_ENABLED", constants::INSPIRATION_ENABLED);
    log::log("DEBUG_ENABLED", constants::IS_DEBUG);

    Game game;

    // INIT
    bool is_1v1 = game.players.size() == 2;
    bool collision = !is_1v1;
    map<EntityId, ShipState> stateMp;

    bool save_for_drop = false;
    game.ready("adbv39");
    log::log("Successfully created bot! My Player ID is " + to_string(game.my_id) + ". Bot rng seed is " + to_string(rng_seed) + ".");

    // Timers
    Timer turnTimer;
    map<EntityId, Path> lastPath;
    for (;;) {
        game.update_frame();
        turnTimer.start();
        closestDropMp.clear();

        shared_ptr<Player> me = game.me;
        shared_ptr<Player> opponent = game.players[0];

        if (opponent->id == me->id)
            opponent = game.players[1];

        int remaining_turns = constants::MAX_TURNS - game.turn_number;
        unique_ptr<GameMap>& game_map = game.game_map;
        vector<Command> command_queue;
        unordered_set<Ship*> assigned;

        // Add shipyard to dropoffs
        me->dropoffs[-3000] = make_shared<Dropoff>(me->id, -3000, me->shipyard->position.x, me->shipyard->position.y);

        map<EntityId, Order> ordersMap;

        // ROLE ASSIGNMENT
        // ships that can't move have no option here. TODO - should still plan considering the path they want to take
        unordered_set<Position> claimed;
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            if (!game_map->canMove(ship)) {
                game_map->addSet(1, ship->position);
                claimed.insert(ship->position);
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
                if (ship->halite >= constants::MAX_HALITE * 0.92) {
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

        // TODO make this an ordertype with a costfn to evalute when it's worth to gather vs build a dropoff
        if (DROPOFFS_ENABLED) {
            // Gathering
            // DROPOFFs
            // shipyard is a dropoff
            int expected_dropoffs = me->ships.size() / 15 + 1;
            int curr_dropoffs = me->dropoffs.size();

            Ship *best_dropoff = nullptr;
            float curr_avg_halite = 0;
            for (const auto &ship_iterator : me->ships) {
                shared_ptr<Ship> ship = ship_iterator.second;
                if (assigned.count(ship.get())) continue;

                int dist = game_map->calculate_distance(ship->position, closest_dropoff(ship->position, &game));
                float avg_halite = game_map->avg_around_point(ship->position, 5);

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
            if (best_dropoff != nullptr && DROPOFFS_ENABLED) {
                if (me->halite + best_dropoff->halite >= constants::DROPOFF_COST) {
                    auto ship = best_dropoff;
                    me->dropoffs[(int) -ship->id] = std::make_shared<Dropoff>(me->id, -ship->id, ship->position.x,
                                                                              ship->position.y);
                    command_queue.push_back(ship->make_dropoff());
                    assigned.insert(ship);
                    closestDropMp.clear();
                    me->halite -= 4000;
                } else {
                    save_for_drop = true;
                }
            }
            // END DROPOFFS
        }

        map<Position, BFSR> ship_to_dist;
        map<Position, BFSR> greedy_bfs;
        ship_to_dist.clear();
        for (auto s : me->ships) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
            greedy_bfs[s.second->position] = game_map->BFS(s.second->position, true);
        }
        for (auto s : me->dropoffs) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
        }


        set<EntityId> added;
        for (const auto& ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            if (assigned.count(ship.get())) continue;
            auto state = stateMp[ship->id];

            auto mdest = closest_dropoff(ship->position, &game);
            vector<Direction> options;
            VVP& pars = ship_to_dist[ship->position].parent;
            options = game_map->minCostOptions(pars, ship->position, mdest);

            if (state == RETURNING) {
                options = game_map->plan_min_cost_route(pars, ship->halite, ship->position, mdest);
            }
            ordersMap[ship->id] = Order{0, RETURNING, options, ship.get(), mdest};
        }

        // Fill ship costs
        struct Cost {
            Position dest;
            Ship* s;
        };

        map<double, VC<Cost>> costs;
        map<ShipPos, double> costMp;
        vector<Order> gather_orders;

        log::log("Start gathering");

        // Gather w/ cost heuristic
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

            // options = game_map->hc_plan_gather_path(ship->halite, ship->position);
            // gather_orders.push_back(Order{10, GATHERING, options, ship.get(), ship->position});
            // added.insert(ship->id);
            // ordersMap.erase(ship->id);

            for (int i = 0; i<game_map->width; i++) for (int k = 0; k<game_map->width; k++) {
                    auto dest = Position(i, k);
                    auto drop = closest_dropoff(dest, &game);
                    VVI& dropoff_dist = ship_to_dist[drop].dist;
                    int net_cost_to = dist[dest.x][dest.y];
                    int cost_from = dropoff_dist[dest.x][dest.y];

                    double c = game_map->costfn(ship.get(), net_cost_to, cost_from, drop, dest, me->id, is_1v1);
                    if (ship->position == dest) {
                        if (game_map->at(dest)->halite > game_map->get_halite_percentile(0.4)) {
                            // Always wait
                            // These are our next targets
                            claimed.insert(dest);
                            added.insert(ship->id);
                            gather_orders.push_back(Order{5, GATHERING, vector<Direction>(1, Direction::STILL), ship.get(), dest});
                            ordersMap.erase(ship->id);
                            log::log("Ship ", ship->id, " staying at ", dest);
                        }
                    }
                    if (!costs.count(c)) costs[c] = VC<Cost>();

                    auto sp = ShipPos{ship->id, dest};
                    costMp[sp] = c;
                    costs[c].PB(Cost {dest, ship.get()});
                }
        }

        auto cost_itr = costs.begin();
        while(cost_itr != costs.end()) {
            for (auto cost : cost_itr->second) {
                if (added.count(cost.s->id)) continue;
                if (claimed.count(cost.dest)) continue;

                auto mdest = cost.dest;
                vector<Direction> options;
                //options = game_map->plan_gather_path(cost.s->halite, cost.s->position, mdest);
                //if (options.size() == 0)
                //   continue;
                options = game_map->minCostOptions(greedy_bfs[cost.s->position].parent, cost.s->position, mdest);
                //options = game_map->get_unsafe_moves(cost.s->position, mdest);// game_map->dirsFrompath(game_map->traceBackPath(greedy_bfs[cost.s->position].parent, cost.s->position, mdest));
                log::log("Ship ", cost.s->id, " moving to ", mdest);

                added.insert(cost.s->id);
                claimed.insert(mdest);
                gather_orders.push_back(Order{10, GATHERING, options, cost.s, mdest});
                ordersMap.erase(cost.s->id);
                // ordersMap[cost.s->id] = Order(); // Order{10, GATHERING, options, cost.s, mdest};
            }
            cost_itr++;
        }


        /*
        log::log("gather size", gather_orders.size());
        // Gather w/ HC
        Timer optimizeTimer = Timer{"Optimize Timer"};
        optimizeTimer.start();

        map<EntityId, Path> currBestPath;
        for (auto s : me->ships) {
            auto id = s.second->id;
            currBestPath[id] = vector<Position>();
            if (lastPath.count(id)) {
                if (lastPath[id][1] == s.second->position) {
                    currBestPath[id] = vector<Position>(lastPath[id].begin() + 1, lastPath[id].end());
                }
            }
        }

        double time_thresh = 1.8;
        if (IS_DEBUG) {
            time_thresh = 0.1;
        }

        while (optimizeTimer.elapsed() < time_thresh) {
            game_map->clearPlanned();
            for (auto o : gather_orders) {
                auto ship = o.ship;
                if (assigned.count(ship)) continue;
                if (stateMp[ship->id] != GATHERING) continue;
                currBestPath[ship->id] = game_map->hc_plan_gather_path(ship->halite, ship->position, o.planned_dest, currBestPath[ship->id]);
            }
        }

        for (auto s : me->ships) {
            auto ship = s.second;
            if (assigned.count(ship.get())) continue;
            if (stateMp[ship->id] != GATHERING) continue;
            ship->log("Gathering");
            ordersMap[ship->id] = (Order{10, GATHERING, game_map->dirsFrompath(currBestPath[ship->id]),
                                         ship.get(), ship->position});
        }*/

        Timer optimizeTimer = Timer{"Optimize Timer"};
        optimizeTimer.start();

        /*
        double time_thresh = 0.05;
        if (gather_orders.size() > 1) while (turnTimer.elapsed() < time_thresh) {
            // swap 2 random orders
            // log::log(turnTimer.tostring());
            int gsize = gather_orders.size();
            for (int i = 0; i<gsize; i++) {
                for (int k = i+1; k<gsize; k++) {
                    int order_1 = i;
                    int order_2 = k;

                    // log::log(order_1);
                    if (order_1 == order_2) continue;

                    Order& o1 = gather_orders[order_1];
                    Order& o2 = gather_orders[order_2];

                    auto shipPos1 = ShipPos{o1.ship->id, o1.planned_dest};
                    auto shipPos2 = ShipPos{o2.ship->id, o2.planned_dest};

                    auto swapped1 = ShipPos{o1.ship->id, o2.planned_dest};
                    auto swapped2 = ShipPos{o2.ship->id, o1.planned_dest};

                    double startCost = costMp[shipPos1] + costMp[shipPos2];
                    double newCost = costMp[swapped1] + costMp[swapped2];

                    int x = game_map->calculate_distance(o1.ship->position, o1.planned_dest);
                    int y = game_map->calculate_distance(o2.ship->position, o2.planned_dest);
                    startCost = x * x + y * y;
                    x = game_map->calculate_distance(o1.ship->position, o2.planned_dest);
                    y = game_map->calculate_distance(o2.ship->position, o1.planned_dest);
                    newCost = x * x + y * y;

                    if (newCost < startCost) {
                        // swap the orders
                        o1.next_dirs = game_map->minCostOptions(greedy_bfs[o1.ship->position].parent, o1.ship->position, o2.planned_dest);
                        o2.next_dirs = game_map->minCostOptions(greedy_bfs[o2.ship->position].parent, o2.ship->position, o1.planned_dest);
                        auto tmp = o1.planned_dest;
                        o1.planned_dest = o2.planned_dest;
                        o2.planned_dest = tmp;
                        log::log("Cost gain of ", newCost, startCost, o1.ship->id, o2.ship->id);
                    }
                }
            }
        }*/

        VC<Order> orders;
        orders = gather_orders;
        for (auto s : ordersMap) {
            orders.push_back(s.second);
        }

        sort(orders.begin(), orders.end());

        log::log("Starting resolve phase");
        for (const auto& order : orders) {
            auto ship = order.ship;
            if (assigned.count(ship)) continue;
            vector<Direction> options = order.next_dirs;
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

                if (COLLIDE_IN_1v1) {
                    enemy_collision = game_map->at(pos)->occupied_by_not(me->id);
                }

                if ((game_map->checkSet(1, pos) || enemy_collision) && !super_ignore) {
                    // try next option
                    ship->log("detected incoming collision ");
                    if ((size_t)i < options.size() - 1) {
                        i++;
                        continue;
                    }
                    // pick something else
                    if (!game_map->checkSet(1, ship->position) && game_map->at(ship->position)->halite > 0) {
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
                            if (!game_map->checkSet(1, pos) && !enemy_collision) {
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

            auto pos = game_map->normalize(ship->position.directional_offset(selected_move));
            game_map->addSet(1, pos);
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
            !game_map->checkSet(1, shipyard_pos))
        {
            if (remaining_turns > 220 || (int) me->ships.size() < ship_target) {
                if (remaining_turns > 60) {
                    command_queue.push_back(me->shipyard->spawn());
                }
            }
        }

        bool end_turn = !game.end_turn(command_queue);
        log::log(turnTimer.tostring());
        log::log("Score:");
        log::log(me->halite, opponent->halite);
        log::log(me->ships.size(), opponent->ships.size());
        log::log("Hal percentile ", game_map->get_halite_percentile(0.5));
        log::log("Hal percentile ", game_map->get_halite_percentile(0.4));
        if (end_turn) {
            break;
        }
    }

    return 0;
}
