#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"
#include "hlt/utils.hpp"
#include "hlt/hungarian.hpp"
#include "hlt/game_map.hpp"

#include <random>
#include <vector>
#include <string>
#include <unordered_set>
#include <map>
#include <ctime>

using namespace std;
using namespace hlt;
using namespace constants;

// TODO
// - EVADING
// - Don't take bad collisions
// - If enemy ship has less halite avoid
// - Mark enemy ships that can't move

bool constants::INSPIRATION_ENABLED = true;
bool constants::DROPOFFS_ENABLED = true;
bool constants::IS_DEBUG = false;
int constants::PID = 0;

typedef VC<Position> Path;

enum ShipState {
    GATHERING,
    RETURNING,
    SUPER_RETURN,
};

enum OrderType {
    GATHER,
    RETURN,
    FORCED_STILL,
};


const bool COLLIDE_IN_1v1 = false;

struct Order {
    int priority;
    int type;

    VC<double> dir_weights;
    map<Direction, double> nextCosts;

    void add_dir_priority(Direction d, double c) {
        nextCosts[d] = c;
    }

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


    void setAllCosts(double d) {
        add_dir_priority(Direction::STILL, d);
        add_dir_priority(Direction::NORTH, d);
        add_dir_priority(Direction::SOUTH, d);
        add_dir_priority(Direction::EAST, d);
        add_dir_priority(Direction::WEST, d);
    }

    Order() {}

    Order(int prioritiy, int type, Ship* ship, Position planned_dest) : priority(prioritiy), type(type), ship(ship), planned_dest(planned_dest) {
        int def = 5;
        setAllCosts(def);
    }
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
    bool one_ship = false;

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
        else if (std::string(argv[i]) == "--oneship") {
            log::log("Debug mode enabled");
            one_ship = true;
        }
        else {

        }
    }

    log::log("DROPOFFS_ENABLED", constants::DROPOFFS_ENABLED);
    log::log("INSPIRATION_ENABLED", constants::INSPIRATION_ENABLED);
    log::log("DEBUG_ENABLED", constants::IS_DEBUG);

    Game game;

    HungarianAlgorithm hungarianAlgorithm;

    // INIT
    bool is_1v1 = game.players.size() == 2;
    bool save_for_drop = false;
    int last_ship_count = 0;

    bool has_collided = false;
    bool built_ship_last = false;

    map<EntityId, int> hal_mined_per_ship;
    map<EntityId, ShipState> stateMp;
    map<EntityId, int> stuckMap;

    game.ready("adbv64");
    log::log("Successfully created bot! My Player ID is " + to_string(game.my_id) + ". Bot rng seed is " + to_string(rng_seed) + ".");
    constants::PID = game.my_id;

    Timer turnTimer;
    // General stuck -> Repeat same turn 3 times, make a random move.

    for (;;) {
        game.update_frame();
        turnTimer.start();

        shared_ptr<Player> me = game.me;
        shared_ptr<Player> opponent = game.players[0];

        if (opponent->id == me->id)
            opponent = game.players[1];

        if (built_ship_last) {
            last_ship_count += 1;
        }
        if ((int)last_ship_count > (int)me->ships.size()) {
            has_collided = true;
        }
        last_ship_count = me->ships.size();

        int remaining_turns = constants::MAX_TURNS - game.turn_number;
        unique_ptr<GameMap> &game_map = game.game_map;
        vector<Command> command_queue;
        unordered_set<Ship *> assigned;

        // Add shipyard to dropoffs
        me->dropoffs[-3000] = make_shared<Dropoff>(me->id, -3000, me->shipyard->position.x, me->shipyard->position.y);

        map<EntityId, Order> ordersMap;

        // ROLE ASSIGNMENT
        multiset<Position> claimed;

        for (const auto &ship_iterator : me->ships) {
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
            if (remaining_turns < game_map->calculate_distance(ship->position, game_map->closest_dropoff(ship->position, &game))
                                  + 0.3 * me->ships.size()) {
                stateMp[id] = SUPER_RETURN;
            }
        }

        // TODO make this an ordertype with a costfn to evalute when it's worth to gather vs build a dropoff
        set<EntityId> given_order;
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

                int dist = game_map->calculate_distance(ship->position, game_map->closest_dropoff(ship->position, &game));
                float avg_halite = game_map->avg_around_point(ship->position, 5);

                if (game_map->at(ship)->halite > 4000 - ship->halite) {
                    curr_avg_halite = 9999999;
                    best_dropoff = ship.get();
                }
                if (remaining_turns > 100
                    && dist > game_map->width / 3
                    && avg_halite > 180
                    && curr_dropoffs < expected_dropoffs) {
                    log::flog(log::Log{game.turn_number - 1, ship->position.x, ship->position.y, "could drop", "#00FFFF"});
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
                    me->dropoffs[(int) -ship->id] = std::make_shared<Dropoff>(me->id, -ship->id, ship->position.x, ship->position.y);

                    given_order.insert(ship->id);
                    command_queue.push_back(ship->make_dropoff());
                    assigned.insert(ship);
                    game_map->closestDropMp.clear();
                    me->halite -= 4000;
                } else {
                    auto ship = best_dropoff;
                    me->dropoffs[(int) -ship->id] = std::make_shared<Dropoff>(me->id, -ship->id, ship->position.x,
                                                                              ship->position.y);
                    save_for_drop = true;
                }
            }
            // END DROPOFFS
        }

        log::log("Before BFS", turnTimer.elapsed());
        map<Position, BFSR> ship_to_dist;
        map<Position, BFSR> greedy_bfs;
        ship_to_dist.clear();
        for (auto s : me->ships) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
            greedy_bfs[s.second->position] = ship_to_dist[s.second->position]; // game_map->BFS(s.second->position, true);
        }
        for (auto s : me->dropoffs) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
        }

        log::log("After BFS", turnTimer.elapsed());
        set<EntityId> added;
        for (const auto &ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            if (assigned.count(ship.get())) continue;
            auto state = stateMp[ship->id];

            auto mdest = game_map->closest_dropoff(ship->position, &game);
            vector<Direction> options;
            VVP &pars = ship_to_dist[ship->position].parent;
            options = game_map->minCostOptions(pars, ship->position, mdest);

            if (state == RETURNING) {
                log::flog(log::Log{game.turn_number - 1, ship->position.x, ship->position.y, "returning", "#0000FF"});
                int hal_on_square = game_map->at(ship->position)->halite;
                if (hal_on_square >= game_map->get_mine_threshold()) {
                    if (hal_on_square * 0.25 + ship->halite < 1000) {
                        auto stay_opts = vector<Direction>{1, Direction::STILL};
                        stay_opts.insert(stay_opts.end(), options.begin(), options.end());

                        Order o{0, RETURNING, ship.get(), mdest};
                        o.setAllCosts(1e5);
                        o.add_dir_priority(Direction::STILL, 1);
                        ordersMap[ship->id] = o;
                        continue;
                    }
                }
                // options = game_map->minCostOptions(pars)// game_map->plan_min_cost_route(pars, ship->halite, ship->position, mdest);
            }
            Order o{0, RETURNING, ship.get(), mdest};
            o.setAllCosts(1e9);
            o.add_dir_priority(Direction::STILL, 1e4);

            int cost = 10;
            for (auto c : options) {
                o.add_dir_priority(c, cost);
                cost *= 10;
            }

            ordersMap[ship->id] = o;
        }
        log::log("After costs filled ", turnTimer.elapsed());

        // Fill ship costs
        struct Cost {
            Position dest;
            Ship *s;
        };

        map<double, VC<Cost>> costs;
        map<ShipPos, double> costMp;
        vector<Order> gather_orders;

        log::log("Start gathering");

        // Gather w/ cost heuristic
        VC<VC<double>> costMatrix;
        costMatrix.reserve(me->ships.size());
        map<int, Ship*> asnMp;
        set<Ship*> frozen;

        int shipn = 0;
        for (auto s : me->ships) {
            shared_ptr<Ship> ship = s.second;
            if (assigned.count(ship.get())) continue;
            if (added.count(ship->id)) continue;

            EntityId id = ship->id;
            ShipState state = stateMp[id];
            if (state != GATHERING) {
                continue;
            }

            asnMp[costMatrix.size()] = ship.get();
            costMatrix.push_back(vector<double>(game_map->width * game_map->width, 1e9));
            vector<Direction> options;
            VVI &dist = greedy_bfs[ship->position].dist;
            VVI &turns = greedy_bfs[ship->position].turns;

            // Take top ships.size() + 1 costs
            multimap<double, Position> candidate_squares;

            for (int i = 0; i < game_map->width; i++){
                for (int k = 0; k < game_map->width; k++) {
                    auto dest = Position(i, k);
                    auto drop = game_map->closest_dropoff(dest, &game);
                    VVI &dropoff_dist = ship_to_dist[drop].dist;
                    int net_cost_to = dist[dest.x][dest.y];
                    int cost_from = dropoff_dist[dest.x][dest.y];
                    int extra_turns = turns[dest.x][dest.y];

                    double c = game_map->costfn(ship.get(), net_cost_to, cost_from, drop, dest, me->id, is_1v1, extra_turns);
                    // costMatrix.back()[i * game_map->width + k] = c + 1000000;
                    if (!costs.count(c)) costs[c] = VC<Cost>();

                    candidate_squares.insert(make_pair(c, dest));
                    /*
                    if (shipn == 0) {
                        stringstream ss;

                        int colc = max(16, min(255, -(int)c));
                        ss << "#" << std::hex << colc << "0000";
                        string color = ss.str();
                        std::transform(color.begin(), color.end(), color.begin(), ::toupper);
                        log::flog(log::Log{game.turn_number - 1, dest.x, dest.y,
                                           "cost - " + color + " - " + to_string(c), color});
                    }*/
                    auto sp = ShipPos{ship->id, dest};
                    costMp[sp] = c;
                    costs[c].PB(Cost{dest, ship.get()});
                }
            }

            size_t count = 0;
            for (auto square : candidate_squares) {
                if (count > me->ships.size() + 20) {
                    break;
                }
                auto cost = game_map->get_best_random_walk(ship->halite, ship->position, square.second);
                int turns_back = game_map->calculate_distance(ship->position, game_map->closest_dropoff(ship->position, &game));
                double c = cost.cost / (turns_back + cost.turns);
                costMatrix.back()[game_map->width * square.second.x + square.second.y] = 10000 - 50 * c;
                log::flog(log::Log{game.turn_number - 1, square.second.x, square.second.y, "cost - " + std::to_string(c), "#DD00DD"});
                //log::log(cost);
                count += 1;
            }

            if (shipn == 0) {
                //log::flog(log::Log{game.turn_number - 1, ship->position.x, ship->position.y, "cost - " + std::to_string(ship->id), "#DD00DD"});
            }
            shipn++;
        }

        vector<int> assgn(me->ships.size(), 0);
        if (costMatrix.size() != 0) {
            log::log(costMatrix.size(), costMatrix[0].size());
            hungarianAlgorithm.Solve(costMatrix, assgn);
            for (auto i : asnMp) {
                log::log("Ship ", i.second->id);
                auto ship = i.second;
                int x = assgn[i.first] / game_map->width;
                int y = assgn[i.first] % game_map->width;
                log::log(assgn[i.first] / game_map->width);
                log::log(assgn[i.first] % game_map->width);


                auto mdest = Position{x,y};
                log::flog(log::Log{game.turn_number - 1, x, y, "gather - " + std::to_string(ship->id), "#FF0000"});
                log::flog(log::Log{game.turn_number - 1, ship->position.x, ship->position.y,
                                   "Going to - " + to_string(mdest.x) + " " + to_string(mdest.y), "#FFFFFF"});

                vector<Direction> options;
                options = game_map->minCostOptions(greedy_bfs[ship->position].parent, ship->position, mdest);
                auto bestdir = game_map->get_best_random_walk(ship->halite, ship->position, mdest).bestdir;

                added.insert(ship->id);
                claimed.insert(mdest);

                Order o{10, GATHERING, ship, mdest};
                o.setAllCosts(1e8);

                // log::log(bestdir);
                /*if (game_map->at(ship->position)->halite >= game_map->get_mine_threshold()) {
                // o.add_dir_priority(Direction::STILL, 1);
                }
                else {
                // o.add_dir_priority(Direction::STILL, 1e4);
                }*/

                int cost = 10;
                for (auto d : options) {
                    o.add_dir_priority(d, cost);
                    cost *= 10;
                }
                // o.add_dir_priority(Direction::STILL, 1e5);
                o.add_dir_priority(bestdir, 1);

                ordersMap[ship->id] = o;
            }
        }

        for (const auto &ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            if (!game_map->canMove(ship)) {
                game_map->addSet(1, ship->position);

                Order o{10, FORCED_STILL, ship.get(), ship->position};
                log::flog(log::Log{game.turn_number - 1, ship->position.x, ship->position.y, "frozen - " + std::to_string(ship->id), "#F0F0F0"});
                o.setAllCosts(1e9);
                o.add_dir_priority(Direction::STILL, 1);

                ordersMap[ship->id] = o;
                assigned.insert(ship.get());
            }
        }
        log::log("After Assignment ", turnTimer.elapsed());

        VC<Order> orders;
        for (auto s : ordersMap) {
            orders.push_back(s.second);
        }

        sort(orders.begin(), orders.end());

        // Create map of banned positions.
        map<Position, bool> is_in_range_of_enemy;
        for (int i = 0; i<game_map->width; i++) {
            for (int k = 0; k<game_map->height; k++) {
                is_in_range_of_enemy[Position{i,k}] = game_map->is_in_range_of_enemy(Position{i,k}, me->id);
            }
        }

        map<Position, int> bijectToPos;
        map<int, Position> indToPos;
        int ind = 0;
        for (auto s : me->ships) {
            for (auto p : game_map->get_surrounding_pos(s.second->position)) {
                if (!bijectToPos.count(p)) {
                    bijectToPos[p] = ind;
                    indToPos[ind] = p;
                    ind++;
                }
            }
        }

        log::log("Starting resolve phase");
        vector<vector<double>> directionCostMatrix(me->ships.size(), vector<double>(ind, 1e10));
        int i = 0;
        for (auto s : me->ships) {
            auto ship = s.second;
            auto state = stateMp[ship->id];

            auto response = EnemyResponse::SMART;
            if (!is_1v1) {
                if (has_collided) {
                    response = AVOID;
                }
                else {
                    response = TOLERATE;
                }
            }
            else {
                response = SMART;
            }
            if (state == RETURNING) {
                response = AVOID;
            }
            if (game_map->calculate_distance(ship->position, game_map->closest_dropoff(ship->position, &game)) <= 3 || state == SUPER_RETURN) {
                response = IGNORE;
            }

            for (auto d : ship->GetBannedDirs(game_map.get(), response, game)) {
                ordersMap[ship->id].add_dir_priority(d, 1e9);
            }


            auto drop = game_map->closest_dropoff(ship->position, &game);
            if (game_map->calculate_distance(ship->position, drop) <= 1 && state == SUPER_RETURN) {
                // Don't include
                command_queue.push_back(ship->move(game_map->get_unsafe_moves(ship->position, drop)[0]));
                given_order.insert(ship->id);
                i++;
                continue;
            }

            for (auto d : ALL_DIRS) {
                Position p = game_map->normalize(ship->position.directional_offset(d));
                int ind = bijectToPos[p];

                if (game_map->closest_dropoff(ship->position, &game) == p) {
                    if (d == Direction::STILL) {
                        ordersMap[ship->id].add_dir_priority(d, 1e9);
                    }
                }

                directionCostMatrix[i][ind] = ordersMap[ship->id].nextCosts[d];
            }
            i++;
        }
        log::log("After Dir Assignment ", turnTimer.elapsed());

        log::log("hit");
        vector<int> dirShipMapping(me->ships.size());
        if (me->ships.size() > 0) {
            hungarianAlgorithm.Solve(directionCostMatrix, dirShipMapping);
        }

        i = 0;
        for (auto s : me->ships) {
            auto ship = s.second;
            if (given_order.count(ship->id)) {
                i++;
                continue;
            }
            int ind = dirShipMapping[i];
            Position move = indToPos[ind];
            auto dir = game_map->getDirectDiff(game_map->normalize(ship->position), move);

            auto state = stateMp[ship->id];
            if (state == SUPER_RETURN) {
                auto drop = game_map->closest_dropoff(ship->position, &game);
                if (game_map->calculate_distance(ship->position, drop) <= 1) {
                    continue;
                }
            }

            game_map->addSet(1, game_map->normalize(ship->position.directional_offset(dir)));
            command_queue.push_back(ship->move(dir));
            i++;
        }

        log::log("Starting logging phase");

        // ---------------------------------------------------------------------------
        // ---------------------------------------------------------------------------
        // Logging - Ship spawning
        // ---------------------------------------------------------------------------
        // ---------------------------------------------------------------------------

        auto shipyard_pos = me->shipyard->position;
        int save_to = 1000;
        if (save_for_drop) {
            save_to += constants::DROPOFF_COST;
        }

        int ship_target = 10;
        bool should_spawn;
        if (is_1v1) {
            ship_target = max(10, (int)opponent->ships.size());
            if (one_ship) {
                should_spawn = me->ships.size() < 1;
            }
            else {
                should_spawn = remaining_turns > 220 || (int) me->ships.size() < ship_target;
            }
        }
        else {
            int num_ships = 0;
            for (auto p : game.players) {
                if (p->id == me->id) {
                    continue;
                }
                num_ships += p->ships.size();
            }
            ship_target = max(10, num_ships / 3);
            should_spawn = remaining_turns > 300 || (int) me->ships.size() < ship_target;
        }

        built_ship_last = false;
        if (me->halite >= save_to &&
            !game_map->checkSet(1, shipyard_pos) && should_spawn)
        {
            if (remaining_turns > 60) {
                command_queue.push_back(me->shipyard->spawn());
                built_ship_last = true;
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
