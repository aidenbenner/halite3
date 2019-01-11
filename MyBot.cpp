#include "hlt/game.hpp"
#include "hlt/constants.hpp"
#include "hlt/log.hpp"
#include "hlt/utils.hpp"
#include "hlt/hungarian.hpp"
#include "hlt/game_map.hpp"
#include "hlt/metrics.hpp"

#include <random>
#include <vector>
#include <string>
#include <unordered_set>
#include <map>
#include <ctime>

using namespace std;
using namespace hlt;
using namespace constants;

double getTime() {
    timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec + tv.tv_usec * 1e-6;
}

int dirToInt(Direction d) {
    switch(d) {
        case Direction::NORTH:
            return 0;
        case Direction::EAST:
            return 1;
        case Direction::SOUTH:
            return 2;
        case Direction::WEST:
            return 3;
        case Direction::STILL:
            return 4;
    }
    assert(false);
    return -1;
}


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

enum OrderType {
    GATHER,
    RETURN,
    FORCED_STILL,
    DROPOFF,
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

    game.ready("adbv121");
    log::log("Successfully created bot! My Player ID is " + to_string(game.my_id) + ". Bot rng seed is " + to_string(rng_seed) + ".");
    constants::PID = game.my_id;
    Metrics::init(&game);

    Timer turnTimer;
    for (;;) {
        game.update_frame();
        turnTimer.start();

        shared_ptr<Player> me = game.me;
        shared_ptr<Player> opponent = game.players[0];
        int drop_save_amount = 0;

        if (opponent->id == me->id)
            opponent = game.players[1];

        if (built_ship_last) {
            last_ship_count += 1;
        }
        if ((int) last_ship_count > (int) me->ships.size()) {
            has_collided = true;
        }
        log::log("Has collided ", has_collided);
        last_ship_count = me->ships.size();

        int remaining_turns = constants::MAX_TURNS - game.turn_number;
        unique_ptr<GameMap> &game_map = game.game_map;
        game_map->initGame(&game);
        vector<Command> command_queue;
        unordered_set<Ship *> assigned;

        // Add shipyard to dropoffs
        me->dropoffs[-3000] = make_shared<Dropoff>(me->id, -3000, me->shipyard->position.x, me->shipyard->position.y);

        map<EntityId, Order> ordersMap;

        // ROLE ASSIGNMENT
        multiset<Position> claimed;

        int num_gathering_ships = 0;

        // ships returning to each drop
        map<Position, vector<int>> dropReturnCount;
        map<Position, int> dropReturnCoeff;
        for (auto s : me->ships) {
            auto drop = game_map->closest_dropoff(s.second->position, &game);
            auto dirs = game_map->get_unsafe_moves(s.second->position, drop);
            if (!dropReturnCoeff.count(drop)) dropReturnCoeff[drop] = 0;
            if (!dropReturnCount.count(drop)) dropReturnCount[drop] = vector<int>(6, 0);
            if (dirs.size() == 0 || dirs[0] == Direction::STILL) {
                continue;
            }
            auto mdir = dirs.at(0);
            int amount = dropReturnCount[drop].at(dirToInt(dirs.at(0)));
            for (auto d : dirs) {
                if (amount < dropReturnCount[drop][dirToInt(d)]) {
                    amount = dropReturnCount[drop][dirToInt(d)];
                    mdir = d;
                }
            }
            dropReturnCount[drop][dirToInt(mdir)] += 1;
            dropReturnCoeff[drop] = *std::max_element(dropReturnCount[drop].begin(), dropReturnCount[drop].end());
        }

        int return_cash = 0;
        for (const auto &ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            EntityId id = ship->id;

            if (!stateMp.count(id)) {
                stateMp[id] = GATHERING;
            }
            if (stateMp[id] == ShipState::DROPOFF) {
                stateMp[id] = GATHERING;
            }
            if (ship->halite >= constants::MAX_HALITE * 0.90) {
                stateMp[id] = RETURNING;
                return_cash += ship->halite;
            }
            if (ship->halite == 0) {
                stateMp[id] = GATHERING;
                num_gathering_ships++;
            }
            auto drop = game_map->closest_dropoff(ship->position, &game);
            auto dirs = game_map->get_unsafe_moves(ship->position, drop);

            int turns_to_return = 0;
            if (dirs.size() > 1) {
                turns_to_return = max(dropReturnCount[drop][dirToInt(dirs[0])], dropReturnCount[drop][dirToInt(dirs[1])]);
            }
            if (remaining_turns <
                game_map->calculate_distance(ship->position, drop) + 1 + turns_to_return) {
                stateMp[id] = SUPER_RETURN;
            }
            ship->state = stateMp[id];
        }

        double halite_per_ship_turn = Metrics::getHalPerShipEma();

        log::log("Before BFS", turnTimer.elapsed());
        map<Position, BFSR> ship_to_dist;
        map<Position, BFSR> &greedy_bfs = ship_to_dist;

        ship_to_dist.clear();
        for (auto s : me->ships) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
        }
        for (auto s : me->dropoffs) {
            ship_to_dist[s.second->position] = game_map->BFS(s.second->position);
        }

        set<EntityId> given_order;
        if (DROPOFFS_ENABLED) {
            int expected_dropoffs = me->ships.size() / 12 + 1;
            int curr_dropoffs = me->dropoffs.size();

            Position best_drop_location;
            Ship *best_dropoff = nullptr;
            //double best_dropoff_cost = 0;
            float curr_avg_halite = 0;
            for (int x = 0; x<game.game_map->width; x++) {
                for (int y = 0; y<game.game_map->height; y++) {
                    // avg halite around the dropoff
                    Position curr{x, y};
                    if (game_map->at(curr)->has_structure()) continue;
                    int drop_dist = game_map->calculate_distance(curr, game_map->closest_dropoff(curr, &game));

                    int dist_to_enemy_shipyard = 10000;
                    for (auto player : game.getEnemies()) {
                        dist_to_enemy_shipyard = min(dist_to_enemy_shipyard, game_map->calculate_distance(curr, player->shipyard->position));
                    }

                    int dist_to_shipyard = game_map->calculate_distance(me->shipyard->position, curr);

                    if (drop_dist >= 15
                        && curr_dropoffs < expected_dropoffs
                        && remaining_turns > 120) {
                        float avg_halite = game_map->avg_around_point(curr, 4) - sqrt(dist_to_shipyard) / 1000.0;

                        bool tooCloseToEnemy = false;
                        auto enemyDrop = game_map->closest_enemy_dropoff(curr, &game);
                        int enemyDist = game_map->calculate_distance(curr, enemyDrop);
                        if (!is_1v1) {
                            tooCloseToEnemy |= enemyDist <= 6;
                        }

                        if (avg_halite > 180
                            && !tooCloseToEnemy
                            && dist_to_shipyard - dist_to_enemy_shipyard <= 10) {
                            //log::flog(log::Log{game.turn_number - 1, curr.x, curr.y, "could drop" + to_string(avg_halite), "#00FFFF"});
                            auto closest_friend = game_map->closestFriendlyShip(curr);
                            if (closest_friend == nullptr) continue;
                            int friendly_dist = game_map->calculate_distance(closest_friend->position, curr);
                            double cost = game_map->sum_around_point(curr, 4) / (2.0 + friendly_dist);
                            if (curr_avg_halite < cost) {
                                //auto closest_enemy = game_map->closestEnemyShip(curr);
                                //int enemy_dist = game_map->calculate_distance(closest_enemy->position, curr);
                                int enemies = game_map->enemies_around_point(curr, 4);
                                int friends = game_map->friends_around_point(curr, 4);
                                if ((friends - enemies) >= -2) {
                                    if (closest_friend != nullptr) {
                                        curr_avg_halite = cost;
                                        best_dropoff = closest_friend;
                                        best_drop_location = curr;
                                    }
                                }
                            }
                        }
                    }
                }
            }

            for (const auto &ship_iterator : me->ships) {
                if (game_map->at(ship_iterator.second)->halite > 4000 - ship_iterator.second->halite) {
                    curr_avg_halite = 9999999;
                    best_dropoff = ship_iterator.second.get();
                    best_drop_location = ship_iterator.second->position;
                }
            }

            log::log(curr_avg_halite);

            save_for_drop = false;
            if (best_dropoff != nullptr && DROPOFFS_ENABLED) {
                log::flog(log::Log{game.turn_number - 1, best_drop_location.x, best_drop_location.y, "best drop", "#00FF00"});
                int dist = game_map->calculate_distance(best_drop_location, best_dropoff->position);
                stateMp[best_dropoff->id] = ShipState::DROPOFF;

                log::log(best_drop_location);
                log::log("is the best ", best_dropoff->id);
                Order o{10, GATHERING, best_dropoff, best_drop_location};
                log::log(best_dropoff->position);
                auto options = game_map->minCostOptions(ship_to_dist[best_dropoff->position].parent, best_dropoff->position, best_drop_location);

                int cost = 10;
                o.setAllCosts(1e6);
                for (auto dir : options) {
                    o.add_dir_priority(dir, cost);
                    cost *= 10;
                }

                ordersMap[best_dropoff->id] = o;

                save_for_drop = true;
                drop_save_amount = 4000 - best_dropoff->halite + 100 - game_map->at(best_drop_location)->halite;
                if (return_cash * 0.8 + me->halite + game_map->at(best_drop_location)->halite + best_dropoff->halite > 4000) {
                    auto ship = best_dropoff;
                    me->dropoffs[(int) -best_dropoff->id] = std::make_shared<Dropoff>(me->id, -ship->id, ship->position.x,
                                                                              ship->position.y);
                    me->dropoffs[(int) -best_dropoff->id]->is_fake = true;
                }
                if (dist == 0) {
                    if (me->halite + best_dropoff->halite >= constants::DROPOFF_COST) {
                        auto ship = best_dropoff;
                        me->dropoffs[(int) -ship->id] = std::make_shared<Dropoff>(me->id, -ship->id, ship->position.x,
                                                                                  ship->position.y);

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
            }
        }

        log::log("After BFS", turnTimer.elapsed());
        set<EntityId> added;
        for (const auto &ship_iterator : me->ships) {
            shared_ptr<Ship> ship = ship_iterator.second;
            if (assigned.count(ship.get())) continue;
            auto state = stateMp[ship->id];
            auto mdest = game_map->closest_dropoff(ship->position, &game);

            if (state != SUPER_RETURN) {
                Position best_drop = me->shipyard->position;
                int best_cost = 20 + game_map->calculate_distance(me->shipyard->position, ship->position);
                for (auto dropoff : me->dropoffs) {
                    if (dropoff.second->is_fake) continue;
                    if (dropoff.second->position == me->shipyard->position) continue;
                    int dist = game_map->calculate_distance(ship->position, dropoff.second->position);
                    if (dist < best_cost) {
                        best_cost = dist;
                        best_drop = dropoff.second->position;
                    }
                }
                mdest = best_drop;
            }


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

            if (state != ShipState::DROPOFF) {
                Order o{0, RETURNING, ship.get(), mdest};
                o.setAllCosts(1e7);
                o.add_dir_priority(Direction::STILL, 1e4);

                int cost = 1;
                for (auto c : options) {
                    o.add_dir_priority(c, cost);
                    cost *= 10;
                }
                ordersMap[ship->id] = o;
            }
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
        log::log("Ship size", me->ships.size());

        VC<VC<double>> costMatrix;
        costMatrix.reserve(num_gathering_ships);
        map<int, Ship *> asnMp;
        set<Ship *> frozen;


        vector<vector<pair<double, Position>>> candidates;
        int ship_count = 0;
        for (auto s : me->ships) {
            shared_ptr<Ship> ship = s.second;
            if (assigned.count(ship.get())) continue;
            if (added.count(ship->id)) continue;

            EntityId id = ship->id;
            ShipState state = stateMp[id];
            if (state != GATHERING) {
                continue;
            }

            vector<Direction> options;
            VVI &dist = greedy_bfs[ship->position].dist;
            // VVI &turns = greedy_bfs[ship->position].turns;

            // Take top ships.size() + 1 costs
            vector<pair<double, Position>> candidate_squares;
            candidate_squares.reserve(me->ships.size());
            asnMp[ship_count++] = ship.get();

            double futureVal = Metrics::getHalPerShip();
            for (int i = 0; i < game_map->width; i++) {
                for (int k = 0; k < game_map->width; k++) {
                    auto dest = Position(i, k);
                    auto drop = game_map->closest_dropoff(dest, &game);
                    // VVI &dropoff_dist = ship_to_dist[drop].dist;
                    int net_cost_to = dist[dest.x][dest.y];
                    int cost_from = 0; //dropoff_dist[dest.x][dest.y];
                    int extra_turns = 0; //turns[dest.x][dest.y];

                    double c = game_map->costfn(ship.get(), net_cost_to, cost_from, drop, dest, me->id, is_1v1,
                                                extra_turns, game, futureVal);

                    int reps = 1;
                    if (me->recent_collision(dest) && game_map->at(dest)->halite > 500) {
                        reps = 5;
                    }
                    // costMatrix.back()[i * game_map->width + k] = c + 1000000;
                    while(reps--) {
                        candidate_squares.push_back(make_pair(c + 1000000, dest));
                        push_heap(candidate_squares.begin(), candidate_squares.end());
                        if ((int)candidate_squares.size() > (int)me->ships.size()) {
                            pop_heap(candidate_squares.begin(), candidate_squares.end());
                            candidate_squares.pop_back();
                        }
                    }
                }
            }
            /*
            log::log("Walking");
            for (int i = 0; i<(int)candidate_squares.size(); i++) {
                log::log(turnTimer.elapsed());
                auto pos = candidate_squares[i].second;
                Order o;
                auto walk = game_map->get_best_random_walk(ship->halite, ship->position, pos, o, -1);
                candidate_squares[i] = make_pair(100000 - 100 * walk.cost, pos);
            }
            log::log("Done Walking");*/
            candidates.push_back(candidate_squares);
        }

        // Compress states for hungarian assignment
        multimap<Position, int> posToInd;
        map<int, Position> indToPos;
        int pos_counter = 0;
        int ship_counter = 0;
        for (auto shipc : candidates) {
            multiset<Position> cand_map;
            for (auto c : shipc) {
                cand_map.insert(c.second);
            }
            for (auto c : shipc) {
                if (posToInd.count(c.second) < cand_map.count(c.second)) {
                    posToInd.insert(make_pair(c.second, pos_counter));
                    indToPos[pos_counter] = c.second;
                    ++pos_counter;
                }
            }
            ship_counter++;
        }

        ship_counter = 0;
        for (auto shipc : candidates) {
            costMatrix.push_back(vector<double>(pos_counter, 1e9));
            for (auto c : shipc) {
                auto range = posToInd.equal_range(c.second);
                for (auto itr = range.first; itr != range.second; itr++) {
                    costMatrix.at(ship_counter).at(itr->second) = c.first;
                }
            }
            ship_counter++;
        }

        vector<int> assgn(me->ships.size(), 0);
        int count = 0;
        if (costMatrix.size() != 0) {
            double timelim = 1.8;
            if (IS_DEBUG) {
                timelim = 0.1;
            }

            double remaining = (timelim - turnTimer.elapsed()) / (ship_counter - count);
            count++;
            log::log(1.9 - turnTimer.elapsed());
            log::log(costMatrix.size(), costMatrix[0].size());
            log::log("Before hungarian ", turnTimer.elapsed());
            hungarianAlgorithm.Solve(costMatrix, assgn);
            log::log("After hungarian ", turnTimer.elapsed());
            for (auto i : asnMp) {
                log::log("Ship ", i.second->id);
                auto ship = i.second;
                auto mdest = indToPos[assgn[i.first]];

                log::flog(log::Log{game.turn_number - 1, mdest.x, mdest.y, "gather - " + std::to_string(ship->id), "#FF0000"});
                log::flog(log::Log{game.turn_number - 1, ship->position.x, ship->position.y,
                                   "Going to - " + to_string(mdest.x) + " " + to_string(mdest.y), "#FFFFFF"});

                vector<Direction> options;
                options = game_map->minCostOptions(greedy_bfs[ship->position].parent, ship->position, mdest);
                Order o{10, GATHERING, ship, mdest};
                o.setAllCosts(1e8);
                auto walk = game_map->get_best_random_walk(ship->halite, ship->position, mdest, o, fmax(0.001, remaining));
                log::log(ship->id);
                log::log("val of best", walk.cost);
                // game_map->addPlanned(0, walk.walk);
                added.insert(ship->id);
                claimed.insert(mdest);
                o.add_dir_priority(walk.bestdir, 1);
                ordersMap[ship->id] = o;
            }
        }
        log::log("After random walks", turnTimer.elapsed());

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
        indToPos.clear();
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

        log::log("Starting resolve phase", turnTimer.elapsed());
        vector<vector<double>> directionCostMatrix(me->ships.size(), vector<double>(ind, 1e30));
        int i = 0;
        for (auto s : me->ships) {
            auto ship = s.second;
            auto state = stateMp[ship->id];

            auto response = EnemyResponse::SMART;
            if (!is_1v1) {
                if (has_collided) {
                    response = SMART;
                }
                else {
                    response = SMART;
                }
            }
            else {
                response = SMART;
            }
            if (state == RETURNING) {
                response = AVOID;
            }
            if (game_map->calculate_distance(ship->position, game_map->closest_dropoff(ship->position, &game)) <= 1 || state == SUPER_RETURN) {
                response = IGNORE;
            }

            for (auto d : ship->GetBannedDirs(game_map.get(), response, game)) {
                double cost = 1e12;
                if (d == Direction::STILL) {
                    cost = 1e13;
                }
                ordersMap[ship->id].add_dir_priority(d, cost);
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
            auto dir = game_map->getDirectDiff(game_map->normalize(ship->position), game_map->normalize(move));

            auto state = stateMp[ship->id];
            if (state == SUPER_RETURN) {
                auto drop = game_map->closest_dropoff(ship->position, &game);
                if (game_map->calculate_distance(ship->position, drop) <= 1) {
                    continue;
                }
            }

            game_map->addSet(1, game_map->normalize(ship->position.directional_offset(dir)));

            ship->planned_next = ship->position.directional_offset(dir);
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
            save_to += drop_save_amount;
        }

        int ship_target = 10;
        bool should_spawn;

        int profitability_est = halite_per_ship_turn * (remaining_turns - 60);
        log::log("Halite per ship turn, profit est");
        log::log(halite_per_ship_turn, halite_per_ship_turn, profitability_est);

        int total_ships = 1;
        for (auto p : game.players) {
            total_ships += p->ships.size();
        }

        int remaining_hal_per_ship = game_map->get_hal() / total_ships;

        if (is_1v1) {
            ship_target = min(10, (int)opponent->ships.size());
            if (one_ship) {
                should_spawn = me->ships.size() < 1;
            }
            else {
                should_spawn = remaining_hal_per_ship > 500;
                should_spawn |= remaining_turns > 220 || (int) me->ships.size() < ship_target;

                if ((int)me->ships.size() > (int)opponent->ships.size() + 5) {
                    should_spawn = false;
                }
                if ((int)me->ships.size() < (int)opponent->ships.size()) {
                    should_spawn = true;
                }
            }
        }
        else {
            int num_ships = 0;
            int min_ships = 9000;
            int max_ships = 0;
            for (auto p : game.players) {
                if (p->id == me->id) {
                    continue;
                }
                max_ships = max(max_ships, (int)p->ships.size());
                min_ships = min(min_ships, (int)p->ships.size());
                num_ships += p->ships.size();
            }
            ship_target = min(10, num_ships / 3);
            should_spawn = remaining_hal_per_ship > 500;
            //should_spawn = profitability_est > 1100 && remaining_hal_per_ship > 1100;
            should_spawn |= remaining_turns > 300 || (int) me->ships.size() < ship_target;
            if ((int)me->ships.size() < min_ships - 5) {
                should_spawn = true;
            }
            if ((int)me->ships.size() > max_ships + 5) {
                should_spawn = false;
            }
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
        log::log("Score (halite, ships, dropoffs)");
        log::log("Me: ", me->halite, me->ships.size(), me->dropoffs.size());
        for (auto p : game.getEnemies()) {
            log::log("Player " + to_string(p->id) + ":",  p->halite, p->ships.size(), p->dropoffs.size());
            log::log("(Profitable ships) ", p->profitable_ships.size());
        }
        log::log("Hal percentile ", game_map->get_halite_percentile(0.5));
        log::log("Hal percentile ", game_map->get_halite_percentile(0.4));
        log::log("Profitable Ships: ", me->profitable_ships.size());
        log::log("EMA: ", Metrics::getHalPerShip());
        log::log("EMA: ", Metrics::getHalPerShip() / remaining_turns);

        if (end_turn) {
            break;
        }
    }

    return 0;
}
