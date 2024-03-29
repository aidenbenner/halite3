#include "game_map.hpp"
#include "input.hpp"
#include "game.hpp"
#include "utils.hpp"
#include "metrics.hpp"
#include <memory>

using namespace std;
using namespace hlt;

Position GameMap::closest_dropoff(Position pos, Game *g) {
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

Position GameMap::closest_enemy_dropoff(Position pos, Game *g) {
    if (closestEnemyDropMp.count(pos)) return closestEnemyDropMp[pos];

    Position out;
    int outdist = 10000;
    for (auto p : g->players) {
        if (p->id == game->me->id) {
            continue;
        }
        Position currcurrp = p->shipyard->position;
        int m = g->game_map->calculate_distance(pos, currcurrp);

        for (auto d : p->dropoffs) {
            auto currp = d.second->position;
            int currd = g->game_map->calculate_distance(pos,currp);

            if (currd < m) {
                m = currd;
                currcurrp = currp;
            }
        }

        if (m < outdist) {
            outdist = m;
            out = currcurrp;
        }
    }
    return closestEnemyDropMp[pos] = out;
}


// Gets the closest ship... in case of a tie uses the ship with least halite.
Ship * GameMap::get_closest_ship(Position pos, const vector<shared_ptr<Player>> &p, const vector<Ship*> &ignore) {
    int dist = 10000000;
    int curr_hal = 10000;
    Ship* soj = nullptr;
    for (auto player : p) {
        for (auto s : player->ships) {
            int curr =  calculate_distance(pos, s.second->position);
            if (curr < dist || (curr == dist && s.second->halite < curr_hal)) {
                bool should_ignore = false;
                for (auto ig : ignore) {
                    if (ig == s.second.get()) should_ignore = true;
                }
                curr_hal = s.second->halite;
                if (should_ignore) continue;
                dist = curr;
                soj = s.second.get();
            }
        }
    }
    return soj;
}

static map<Position, Ship*> closestFriendlyMemo;
static map<Position, Ship*> closestEnemyMemo;
Ship* GameMap::closestEnemyShip(hlt::Position pos) {
    if (closestFriendlyMemo.count(pos)) return closestFriendlyMemo[pos];
    return closestFriendlyMemo[pos] = get_closest_ship(pos, game->getEnemies(), {});
}

Ship* GameMap::closestFriendlyShip(hlt::Position pos) {
    if (closestEnemyMemo.count(pos)) return closestEnemyMemo[pos];
    return closestEnemyMemo[pos] = get_closest_ship(pos, {game->me}, {});
}


void hlt::GameMap::_update() {
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            cells[y][x].ship.reset();
        }
    }

    avgAroundPointMemo.clear();
    enemies_around_point_memo.clear();
    friends_around_point_memo.clear();
    likelyInspiredMemo.clear();
    closestFriendlyMemo.clear();
    closestEnemyMemo.clear();

    closestEnemyDropMp.clear();
    closestDropMp.clear();
    hal_mp.clear();
    planned_route.clear();
    set_route.clear();
    inspiredMemo.clear();
    inspiredCountMemo.clear();

    int update_count;
    hlt::get_sstream() >> update_count;

    for (int i = 0; i < update_count; ++i) {
        int x;
        int y;
        int halite;
        hlt::get_sstream() >> x >> y >> halite;
        cells[y][x].halite = halite;
    }


    hal_dist.clear();
    for (int i = 0; i < width; i++) {
        for (int k = 0; k < height; k++) {
            hal_dist.push_back(at(i, k)->halite);
        }
    }
    std::sort(hal_dist.begin(), hal_dist.end());


}

std::unique_ptr<hlt::GameMap> hlt::GameMap::_generate() {
    std::unique_ptr<hlt::GameMap> map = std::make_unique<GameMap>();

    hlt::get_sstream() >> map->width >> map->height;

    map->cells.resize((size_t)map->height);
    for (int y = 0; y < map->height; ++y) {
        auto in = hlt::get_sstream();

        map->cells[y].reserve((size_t)map->width);
        for (int x = 0; x < map->width; ++x) {
            hlt::Halite halite;
            in >> halite;

            map->cells[y].push_back(MapCell(x, y, halite));
        }
    }

    return map;
}

bool GameMap::checkSet(int future_turns, Position p) {
    return set_route.count(TimePos{future_turns, p});
}

void GameMap::addSet(int future_turns, Position p, Ship *s) {
    set_route[TimePos{future_turns, p}] = s;
    addPlanned(1, p);
}

Ship* GameMap::getSet(int turns, Position p) {
    return set_route[TimePos{turns, p}];
}

bool GameMap::checkIfPlanned(int future_turns, Position p) {
    return planned_route.count(TimePos{future_turns, p});
}

void GameMap::clearPlanned() {
    planned_route.clear();
}

void GameMap::addPlanned(int future_turns, VC<Position> p) {
    for (int i = 0; i<(int)p.size(); i++) {
        planned_route.insert(TimePos{i, p[i]});
    }
    for (int i = 0; i<(int)p.size() - 1; i++) {
        if (p[i] == p[i + 1]) {
            mine_hal(p[i], i);
        }
    }
}

void GameMap::addPlanned(int future_turns, Position p) {
    planned_route.insert(TimePos{future_turns, p});
}

int GameMap::hal_at(Position p, int turn) {
    if (turn == 0) {
        return at(p)->halite;
    }

    if (!hal_mp.count(p)) {
        hal_mp[p] = std::set<int>();
    }
    int uses = distance(hal_mp[p].begin(), hal_mp[p].lower_bound(turn));
    return at(p)->halite * pow(0.75, uses);
}

void GameMap::mine_hal(Position p, int turn) {
    if (!hal_mp.count(p)) {
        hal_mp[p] = std::set<int>();
    }
    hal_mp[p].insert(turn);
}


MapCell* GameMap::at(const Position& position) {
    Position normalized = normalize(position);
    return &cells[normalized.y][normalized.x];
}


int GameMap::at(Position p, int turn) {
    assert(false);
    return 0;
}

MapCell* GameMap::at(int x, int y) {
    return at(Position{x,y});
}

MapCell* GameMap::at(const Entity& entity) {
    return at(entity.position);
}

MapCell* GameMap::at(const Entity* entity) {
    return at(entity->position);
}

MapCell* GameMap::at(const std::shared_ptr<Entity>& entity) {
    return at(entity->position);
}

Direction GameMap::getDirectDiff(Position a, Position b) {
    a = normalize(a);
    b = normalize(b);
    if (a == b) {
        return Direction::STILL;
    }
    for (auto d : ALL_CARDINALS) {
        if (normalize(a.directional_offset(d)) == b) {
            return d;
        }
    }

    log::log("Assertion failed");
    log::log(a);
    log::log(b);
    assert(false);
    return Direction::STILL;
}

RandomWalkResult GameMap::get_best_random_walk(int starting_halite, Position start, Position dest, Order& order, double time_bank) {
    Direction best_move = Direction::STILL;
    double best_cost = -1000000;
    int best_turns = 1;
    if (calculate_distance(start,dest) == 0) {
        order.add_dir_priority(Direction::STILL, 1);
        return {Direction::STILL, (double)at(dest)->halite * 0.25, 1};
    }

    auto closest_enemy = closestEnemyShip(dest);
    int turns_to_enemy = calculate_distance(closest_enemy->position, dest);

    vector<Position> best_path;
    Timer timer;
    timer.start();
    int itrs = min(5000, (int)pow(calculate_distance(start, dest), 3) * 100);
    int i = 0;

    map<Direction, double> costMp;
    if (time_bank == -1) {
        itrs = 50;
        time_bank = 0;
    }

    while (i < itrs + 1) {
        if (time_bank != 0 && timer.elapsed() > time_bank) {
            log::log("breaking cause time bank");
            log::log(timer.elapsed(), time_bank);
            break;
        }
        i++;

        int curr_halite = starting_halite;
        double turns = 1;

        auto curr = start;
        Direction first_move = Direction::STILL;

        int cost = 0;
        int curr_square_hal = at(curr)->halite;
        vector<Position> path;
        bool did_break = false;
        while (dest != curr) {
            //int enemies = enemies_around_point(curr, 5);
            //int friends = friends_around_point(curr, 5);

            /*
            if (is_inspired(curr, constants::PID)) {
                if (game->players.size() == 2 && turns < 7) {
                    curr_square_hal += curr_square_hal * (friends - enemies) / 4.0;
                }
                else if (game->players.size() == 4) {
                    curr_square_hal += curr_square_hal * (friends) / 9.0;
                }
            }*/

            path.push_back(curr);
            auto move = get_random_dir_towards(curr, dest);

            /*
            if (at(curr)->occupied_by_not(constants::PID)) {
                int enemy_hal = at(curr)->ship->halite;
                if (should_collide(curr, order.ship)) {
                    if (game->players.size() == 4) {
                        curr_halite += enemy_hal / 2;
                    }
                    else {
                        curr_halite += enemy_hal;
                    }
                }
            }*/

            if (curr_halite < curr_square_hal * 0.1 || move == Direction::STILL) {
                move = Direction::STILL;
                if ((is_inspired(curr, constants::PID) || likely_inspired(curr, turns))) {
                    // log::log("is inspired");
                    curr_halite += ceil(2 * curr_square_hal * 0.25);
                }
                curr_halite += ceil(curr_square_hal * 0.25);
                curr_square_hal -= ceil(0.25 * curr_square_hal);
            }
            else {
                curr_halite -= floor(curr_square_hal * 0.1);
                curr = normalize(curr.directional_offset(move));
                cost += floor(curr_square_hal * 0.1);
                curr_square_hal = at(curr)->halite;
            }
            if (turns == 1) {
                first_move = move;
                if (!costMp.count(first_move)) costMp[first_move] = -10;
            }
            turns += 1;
            if (curr_halite >= 900) {
                did_break = true;
                break;
            }
        }
        if (at(curr)->occupied_by_not(constants::PID)) {
            int enemy_hal = at(curr)->ship->halite;
            if (game->players.size() == 4) {
                curr_halite += enemy_hal;
            }
            else {
                curr_halite += enemy_hal;
            }
        }
        int dest_halite = at(dest)->halite * 0.25;
        int remaining_hal = at(dest)->halite * 0.75;
        turns++;
        if (is_inspired(dest, constants::PID) || likely_inspired(dest, turns)) {
            dest_halite *= 3;
        }
        if (did_break) {
            turns--;
            dest_halite = 0;
        }

        if (abs(turns - turns_to_enemy) <= 1) {
        }

        //auto drop = closest_dropoff(curr, game);
        //int turns_back = calculate_distance(curr, drop);
        //turns += turns_back;
        double c = (dest_halite + curr_halite - starting_halite) / turns;

        for (int i = 1; i<=6; i++) {
            if (did_break) break;
            dest_halite += 0.25 * remaining_hal;
            if (is_inspired(dest, constants::PID) || likely_inspired(dest, turns + i)) {
                dest_halite += 0.5 * remaining_hal;
            }
            remaining_hal *= 0.75;
            c = fmax(c, (dest_halite + curr_halite - starting_halite) / (turns + i));
        }

        //log::log(i, calculate_distance(start, dest), curr_halite, turns, c, first_move);
        if (!costMp.count(first_move)) costMp[first_move] = c;
        costMp[first_move] = fmax(costMp[first_move], c);
        if (c > best_cost) {
            best_path = path;
            best_cost = c;
            best_move = first_move;
            best_turns = turns;
        }
    }

    for (auto d : costMp) {
        if (best_cost == 0) best_cost = 1;
        order.add_dir_priority(d.first, 100 * pow(1e4, 1.0 - ((double)d.second / (double)best_cost)));
        //log::log("Walks", order.ship->id, d.first, d.second, 1.0 - ((double)d.second / (double)best_cost));
        //log::log("Best cost", best_cost);
    }

    return {best_move, best_cost, best_turns, best_path};
}

Direction GameMap::get_random_dir_towards(Position start, Position end) {
    auto moves = get_unsafe_moves(start, end);
    if (moves.size() == 0) {
        moves.push_back(moves[0]);
        moves.push_back(Direction::STILL);
        if (moves[0] == Direction::NORTH || moves[0] == Direction::SOUTH) {
            moves.push_back(Direction::WEST);
            moves.push_back(Direction::EAST);
        }
        else {
            moves.push_back(Direction::NORTH);
            moves.push_back(Direction::SOUTH);
        }
    }
    moves.push_back(Direction::STILL);
    //if (rand() % 10) {
    //    return ALL_CARDINALS[rand() % 4];
    //}
    return moves[rand() % moves.size()];
}


static bool visited[64][64];
/*
BFSR GameMap::BFSToDrop(Position source) {
    for (int i = 0; i<64; i++) {
        for (int k = 0; k<64; k++) {
            visited[i][k] = false;
        }
    }

    int def = 1e8;

    vector<vector<int>> dist(width,vector<int>(height, def));
    vector<vector<Position>> parent(width,vector<Position>(height, {-1, -1}));
    vector<vector<int>> turns(width, vector<int>(height, 1e9));

    dist[source.x][source.y] = 0;
    turns[source.x][source.y] = 1;

    vector<pair<int, Position>> edge_pq;
    vector<Position> frontier;
    vector<Position> next;
    next.reserve(200);
    frontier.reserve(200);
    next.push_back(source);
    while(!next.empty()) {
        std::swap(next, frontier);
        next.clear();
        while(!frontier.empty()) {
            auto p = frontier.back();
            frontier.pop_back();

            if (visited[p.x][p.y]) {
                continue;
            }

            visited[p.x][p.y] = true;

            for (auto d : ALL_CARDINALS) {
                auto f = normalize(p.directional_offset(d));
                if (!visited[f.x][f.y]) {
                    next.push_back(f);
                    continue;
                }
            }
        }
    }
    return BFSR{dist, parent, turns};
}*/

BFSR GameMap::BFS(Position source, bool collide, int starting_hal) {
    // dfs out of source to the entire map
    for (int i = 0; i<64; i++) {
        for (int k = 0; k<64; k++) {
            visited[i][k] = false;
        }
    }

    int def = 1e8;

    vector<vector<int>> dist(width,vector<int>(height, def));
    vector<vector<Position>> parent(width,vector<Position>(height, {-1, -1}));
    vector<vector<int>> turns(width, vector<int>(height, 1e9));

    dist[source.x][source.y] = 0;
    turns[source.x][source.y] = 1;

    vector<Position> frontier;
    vector<Position> next;
    next.reserve(200);
    frontier.reserve(200);
    next.push_back(source);
    while(!next.empty()) {
        std::swap(next, frontier);
        next.clear();
        while(!frontier.empty()) {
            auto p = frontier.back();
            frontier.pop_back();

            if (visited[p.x][p.y]) {
                continue;
            }

            visited[p.x][p.y] = true;
            if (collide && at(p)->occupied_by_enemy()) {
                continue;
            }

            for (auto d : ALL_CARDINALS) {
                auto f = normalize(p.directional_offset(d));
                if (!visited[f.x][f.y]) {
                    next.push_back(f);
                    continue;
                }
                int c = at(f)->cost() + dist[f.x][f.y];
                if (c <= dist[p.x][p.y]) {
                    dist[p.x][p.y] = c;
                    parent[p.x][p.y] = f;
                }
            }
        }
    }
    return BFSR{dist, parent, turns};
}

vector<Position> GameMap::traceBackPath(VVP parents, Position start, Position dest) {
    Position curr = dest;
    VC<Position> path(1, dest);

    while (curr != start) {
        curr = parents[curr.x][curr.y];
        path.push_back(curr);
        if (curr == Position{-1, -1}) {
            return VC<Position>{start, curr};
        }
    }
    std::reverse(path.begin(), path.end());

    return path;
}

void GameMap::random_walk(VC<Position> &walk, int length, int seed) {
    if (length <= 0) return;
    auto next = walk.back().directional_offset(ALL_CARDINALS[(seed + rand() % 3) % 4]);
    bool found = true;
    int max_itr = 20;
    int i = 0;
    while (found && i < max_itr) {
        i++;
        found = false;
        for (int i = 0; i<(int)walk.size(); i++) {
            if (next == walk[i]) {
                found = true;
            }
        }
    }
    walk.push_back(normalize(next));
    random_walk(walk, length - 1, seed);
}

VC<Position> GameMap::random_walk(int starting_halite, Position start, Position dest) {
    start = normalize(start);
    dest = normalize(dest);
    vector<Position> path;
    path.push_back(start);
    if (start == dest) {
        return path;
    }

    auto moves = get_unsafe_moves(start, dest);
    auto dir = moves[rand() % moves.size()];

    auto p2 = random_walk(starting_halite, start.directional_offset(dir), dest);
    path.insert(path.end(), p2.begin(), p2.end());
    return path;
}

VC<Position> GameMap::wait_adjust(int starting_halite, VC<Position> walk, int turn) {
    int curr_hal = starting_halite;
    VC<Position> adjusted;
    for (auto a : walk) {
        while (hal_at(a,turn) >= get_mine_threshold() || curr_hal < 0.1 * hal_at(a, turn)) {
            if (curr_hal > 900) break;
            curr_hal += ceil(0.25 * hal_at(a, turn));
            turn++;
            mine_hal(a, turn);
            adjusted.push_back(a);
        }
        curr_hal -= 0.1 * hal_at(a, turn);
        adjusted.push_back(a);
        turn++;
    }
    return adjusted;
}

bool GameMap::path_conflicts(int starting_halite, VC<Position> path) {
    int turn = 0;
    for (int i = 0; i<(int)path.size(); i++) {
        if (checkIfPlanned(turn, path[i]))
            return true;
        turn++;
    }
    return false;
}

vector<Direction> GameMap::dirsFrompath(VC<Position> p) {
    if ((int)p.size() == 0 || (int)p.size() == 1) return vector<Direction>(1, Direction::STILL);
    log::log("Get other ", p[0]);
    return {getDirectDiff(p[0], p[1])};
}

int GameMap::getPathCost(VC<Position> p) {
    // get sum of walk
    int out = 0;
    for (int i = 1; i<(int)p.size(); i++) {
        int hal = at(p[i])->halite;
        out += hal;
    }
    return out;
}

vector<Position> GameMap::hc_plan_gather_path(int starting_halite, Position start, Position end, vector<Position> starting_path) {
    auto walks = VC<VC<Position>>();
    auto allowed_walks = VC<VC<Position>>();
    VC<Position> chosen_walk = starting_path;
    double mcost = getPathCost(starting_path) / (double)max(1, (int)starting_path.size());
    for (int i = 0; i<50; i++) {
        vector<Position> walk = random_walk(starting_halite, start, end);
        walks.push_back(wait_adjust(starting_halite, walk, 0));
        // log::log(i, walk.size());
        double cost = getPathCost(walks[i]) / (double)max(1, (int)walk.size());
        // double cost = getPathCost(walks[i]);
        if (cost > mcost) {
            // log::log(cost);
            mcost = cost;
            chosen_walk = walks[i];
        }
    }
    addPlanned(0, chosen_walk);
    return chosen_walk;
}

vector<Position> GameMap::hc_plan_gather_path(int starting_halite, Position start, vector<Position> starting_path) {
    auto walks = VC<VC<Position>>();
    auto allowed_walks = VC<VC<Position>>();
    VC<Position> chosen_walk = starting_path;
    int mcost = getPathCost(starting_path);
    for (int i = 0; i<50; i++) {
        vector<Position> walk = {start};
        int turns = rand() % 30 + 5;
        random_walk(walk, turns, rand());
        walks.push_back(wait_adjust(starting_halite, walk, 0));
        double cost = getPathCost(walks[i]) / (double)turns;
        if (cost > mcost) {
            // log::log(cost);
            mcost = cost;
            chosen_walk = walks[i];
        }
    }
    addPlanned(0, chosen_walk);
    return chosen_walk;
}

vector<Direction> GameMap::plan_gather_path(int starting_halite, Position start, Position dest) {
    // out of 10 random walks take the one that maximizes halite^2 and doesn't conflict
    auto walks = VC<VC<Position>>();
    auto allowed_walks = VC<VC<Position>>();
    VC<Position> chosen_walk;
    int mcost = -100;
    for (int i = 0; i<5000; i++) {
        walks.push_back(wait_adjust(starting_halite, random_walk(starting_halite, start, dest), 0));
        int cost = getPathCost(walks[i]);

        if (cost > mcost && !path_conflicts(starting_halite, walks[i])) {
            mcost = cost;
            chosen_walk = walks[i];
        }
    }
    if (mcost == -100) {
        return vector<Direction>(1, Direction::STILL);
    }
    addPlanned(0, chosen_walk);
    return dirsFrompath(chosen_walk);
}

vector<Direction> GameMap::plan_min_cost_route(VVP parents, int starting_halite, Position start, Position dest, int time) {
    VC<Position> path = traceBackPath(parents, start, dest);
    if (path.back() == Position{-1, -1}) {
        path = random_walk(starting_halite, start, dest);
    }

    assert(path[0] == start);
    assert(path.back() == dest);

    int pind = 0;
    auto curr = start;
    int curr_h = starting_halite;
    int wturns = 0;

    int tmp_time = time;
    auto max_halite_pos = start;
    while (curr != dest) {
        //  log::log(curr, dest, checkIfPlanned(time, curr), curr_h, at(curr)->cost());
        if (at(curr)->halite > at(max_halite_pos)->halite) {
            max_halite_pos = curr;
        }
        // Assuming halite stays constant
        if (!checkIfPlanned(tmp_time, curr) && curr_h > at(curr)->cost()) {
            curr_h -= at(curr)->cost();
            curr = path[++pind];
        }
        else {
            // we have to wait sometime in the future
            // if so wait at the time from now that gave us the most halite.
            // e.g. the start square
            wturns++;
            curr_h += at(curr)->gain();
            time += 1;
            break;
        }
        tmp_time += 1;
    }

    // we had to wait a turn
    if (wturns > 0) {
        int tmp_time = time;
        curr_h = starting_halite;
        for (int i = 0; i<(int)path.size(); i++) {
            addPlanned(tmp_time, path[i]);
            if (path[i] == max_halite_pos) {
                tmp_time++;
                curr_h += at(curr)->gain();
                plan_min_cost_route(parents, curr_h, max_halite_pos, dest, tmp_time);
                if (max_halite_pos == start) {
                    return vector<Direction>(1, Direction::STILL);
                }
                break;
            }
            tmp_time += 1;
            curr_h -= at(curr)->cost();
        }
        return minCostOptions(parents, start, dest);
    }

    for (auto p : path) {
        addPlanned(time, p);
        time++;
    }
    return minCostOptions(parents, start, dest);
}


int GameMap::getPathLength(VVP &pos, Position start, Position dest) {
    if (start == dest) {
        return 1;
    }
    if (pos[dest.x][dest.y] == Position {-1, -1}) {
        return -1;
    }
    Position curr = dest;
    int length = 0;
    while (curr != start) {
        auto tmp = pos[curr.x][curr.y];
        curr = tmp;
        length++;
    }

    return length;
}

vector<Direction> GameMap::minCostOptions(VVP &pos, Position start, Position dest) {
    if (start == dest) {
        return vector<Direction>(1, Direction::STILL);
    }
    if (pos[dest.x][dest.y] == Position {-1, -1}) {
        //assert(false);
        return get_unsafe_moves(start, dest);
    }
    Position curr = dest;
    Direction move = Direction::STILL;
    while (curr != start) {
        auto tmp = pos[curr.x][curr.y];
        move = getDirectDiff(tmp, curr);
        curr = tmp;
    }

    vector<Direction> opts;
    opts.push_back(move);
    auto others = get_unsafe_moves(start, dest);
    for (auto o : others) {
        bool found = false;
        for (auto p : opts) {
            if (o == p) found = true;
        }
        if (!found) opts.push_back(o);
    }
    return opts;
}

int GameMap::calculate_distance(const Position& source, const Position& target) {
    const auto& normalized_source = normalize(source);
    const auto& normalized_target = normalize(target);

    const int dx = std::abs(normalized_source.x - normalized_target.x);
    const int dy = std::abs(normalized_source.y - normalized_target.y);

    const int toroidal_dx = std::min(dx, width - dx);
    const int toroidal_dy = std::min(dy, height - dy);

    return toroidal_dx + toroidal_dy;
}

Position GameMap::normalize(const Position& position) {
    const int x = ((position.x % width) + width) % width;
    const int y = ((position.y % height) + height) % height;
    return { x, y };
}


bool GameMap::canMove(std::shared_ptr<Ship> ship) {
    return at(ship)->halite * 0.1 <= ship->halite;
}

std::vector<Direction> GameMap::get_unsafe_moves(const Position& source, const Position& destination) {
    const auto& normalized_source = normalize(source);
    const auto& normalized_destination = normalize(destination);

    if (source == destination) {
        return std::vector<Direction>(1, Direction::STILL);
    }

    const int dx = std::abs(normalized_source.x - normalized_destination.x);
    const int dy = std::abs(normalized_source.y - normalized_destination.y);
    const int wrapped_dx = width - dx;
    const int wrapped_dy = height - dy;

    std::vector<Direction> possible_moves;

    if (normalized_source.x < normalized_destination.x) {
        possible_moves.push_back(dx > wrapped_dx ? Direction::WEST : Direction::EAST);
    } else if (normalized_source.x > normalized_destination.x) {
        possible_moves.push_back(dx < wrapped_dx ? Direction::WEST : Direction::EAST);
    }

    if (normalized_source.y < normalized_destination.y) {
        possible_moves.push_back(dy > wrapped_dy ? Direction::NORTH : Direction::SOUTH);
    } else if (normalized_source.y > normalized_destination.y) {
        possible_moves.push_back(dy < wrapped_dy ? Direction::NORTH : Direction::SOUTH);
    }

    if (dy > dx && possible_moves.size() >= 2) {
        auto tmp = possible_moves[0];
        possible_moves[0] = possible_moves[1];
        possible_moves[1] = tmp;
    }

    return possible_moves;
}

bool GameMap::is_in_range_of_enemy(Position p, PlayerId pl, bool on_square) {
    if (at(p)->occupied_by_not(pl)) return true;
    if (on_square) return false;
    for (int i = 0; i<4; i++) {
        auto po = p.directional_offset(ALL_CARDINALS[i]);
        if (at(po)->occupied_by_not(pl)) return true;
    }
    return false;
}

Ship* GameMap::enemy_in_range(Position p, PlayerId pl, bool on_square) {
    if (at(p)->occupied_by_not(pl)) return at(p)->ship.get();
    if (on_square) return nullptr;
    int mhal = 100000;
    Ship* out = nullptr;
    for (int i = 0; i<4; i++) {
        auto po = normalize(p.directional_offset(ALL_CARDINALS[i]));
        if (at(po)->occupied_by_not(pl)) {
            auto ship = at(po)->ship.get();
            if (ship->halite < mhal) {
                mhal = ship->halite;
                out = ship;
            }
        }
    }
    return out;
}

std::vector<int> hal_dist;
int GameMap::get_halite_percentile(double percentile) {
    int out = hal_dist[percentile * hal_dist.size()];
    return out;
}

int GameMap::get_mine_threshold() {
    return max(10, min(99, get_halite_percentile(0.5)));
}

bool GameMap::should_mine(Position p) {
    return at(p)->halite > get_mine_threshold();
}

int GameMap::get_total_halite() {
    int sum = 0;
    for (int i = 0; i<width; i++) {
        for (int k= 0; k<height; k++) {
            sum += at(i,k)->halite;
        }
    }
    return sum;
}

int GameMap::turns_to_enemy_shipyard(Game &g, Position pos) {
    static map<Position, int> dp;
    if (dp.count(pos)) return dp[pos];
    int turns = 100000;
    for (auto p : g.players) {
        if (p == g.me) {
            continue;
        }
        turns = min(turns, calculate_distance(pos, p->shipyard->position));
    }
    return dp[pos] = turns;
}

int GameMap::turns_to_shipyard(Game &g, Position pos) {
    //static map<Position, int> dp;
    //if (dp.count(pos)) return dp[pos];
    return calculate_distance(g.me->shipyard->position, pos);
}


/*
bool GameMap::should_collide(Position position, Ship* ship, Ship* enemy) {
    Ship *s = get_closest_ship(position, game->players, {ship, enemy});
    int enemies = enemies_around_point(position, 3);
    int friends = enemies_around_point(position, 3);
    if (s->owner == constants::PID) {
        if (friends >= enemies) {
            return true;
        }
    }
    return false;
}*/

bool GameMap::should_collide(Position position, Ship *ship, Ship *enemy) {
    bool is4p = game->players.size() == 4;
    if (enemy == nullptr) {
        enemy = at(position)->ship.get();
    }

    int possible_loss = 0;
    if (ship->position == position) {
        possible_loss = at(position)->halite;
    }

    if (at(position)->has_structure())
        return at(position)->structure->owner == constants::PID;

    if (enemy == nullptr) return true;

    int hal_on_square = at(position)->halite;
    int enemy_hal = hal_on_square + enemy->halite;
    if (ship->halite > 700) return false;
    if (possible_loss + enemy_hal < 300) return false;

    Ship *cenemy = get_closest_ship(position, game->getEnemies(), {ship, enemy});
    Ship *pal = get_closest_ship(position, {game->me}, {ship, enemy});

    if (cenemy == nullptr) return true;
    if (pal == nullptr) return false;

    int collision_hal = cenemy->halite + pal->halite + hal_on_square;

    // check if pal can gather
    int enemies = enemies_around_point(position, 4);
    int friends = friends_around_point(position, 4);
    if (friends >= enemies + 2) return true;

    int paldist = calculate_distance(pal->position, position);
    int enemydist = calculate_distance(cenemy->position, position);
    if (paldist > 2 + enemydist) return false;


    if (pal->halite + collision_hal > 1400) return false;

    if (ship->halite > possible_loss + enemy_hal)
        return false;

    if (friends >= enemies) {
        return true;
    }
    return false;
}

double GameMap::costfn(Ship *s, int to_cost, int home_cost, Position shipyard, Position dest, PlayerId pid, bool is_1v1,
                       int extra_turns, Game &g, double future_ship_val) {
    if (dest == shipyard) return 10000000;

    double bonus = 1;

    int halite = at(dest)->halite;
    int turns_to = calculate_distance(s->position, dest);
    int turns_back = calculate_distance(dest, shipyard);

    int avg_amount = avg_around_point(dest, 3);
    bonus = max(1.0, min(4.0, 3.0 * avg_amount / 150.0));

    int shipyard_bonus = avg_around_point(shipyard, 3);
    if (shipyard_bonus > 150) {
        bonus += 3;
    }

    int enemies = enemies_around_point(dest, 5);
    int friends = friends_around_point(dest, 5);

    int turns = fmax(1.0, turns_to + turns_back);
    if (!is_1v1) {
        turns = turns_to + turns_back;
    }

    if (at(dest)->occupied_by_not(pid)) {
        if (should_collide(dest, s)) {
            if (is_1v1) {
                halite += at(dest)->ship->halite;
            }
            if (!is_1v1) {
                halite += at(dest)->ship->halite;
                halite *= (0.5 + game->turn_number / ((double)constants::MAX_TURNS * 2.0));
            }
        } else {
            if (!is_1v1) {
                return 1000000;
            }
        }
    }

    bool inspired = false;

    if (is_inspired(dest, pid) || likely_inspired(dest, turns_to)) {
        if (is_1v1 && turns_to < 6) {
            bonus += 1 + (1 + friends - enemies);
            inspired = true;
        }
        else if (!is_1v1 && turns_to < 6) {
            bonus += 1 + (1 + friends - enemies);
            inspired = true;
        }
    }

    int curr_hal = s->halite;
    double out = -1000;
    int mined = 0;
    for (int i = 0; i<5; i++) {
        mined += halite * 0.25;
        if (inspired) {
            mined += halite * 0.5;
        }
        halite *= 0.75;
        if (mined + curr_hal > 1000) {
            mined = 1000 - curr_hal;
        }
        int c = max(0, mined - to_cost);
        double cout = (c) / ((double)1 + turns + i);
        out = max(cout, out);
    }

    bonus = fmax(bonus, 1);
    return -bonus * out;
}

// count number of inspired enemies
int GameMap::num_inspired(Position p, PlayerId id) {
    if (!constants::INSPIRATION_ENABLED) return 0;
    if (inspiredCountMemo.count(p)) return inspiredCountMemo[p];
    int radius = constants::INSPIRATION_RADIUS * 2;

    int count = 0;
    for (int i = 0; i<radius * 2; i++) {
        for (int k = 0; k<radius * 2; k++) {
            auto c = Position {p.x - radius + i, p.y - radius + k};
            if (this->calculate_distance(c, p) <= constants::INSPIRATION_RADIUS) {
                if (at(c)->occupied_by_not(id)) {
                    if (is_inspired(c, id, true)) {
                        count += 2 * 0.25 * at(c)->halite;
                    }
                }
                if (is_inspired(p, constants::PID)) {
                    if (at(c)->is_occupied(constants::PID)) {
                        if (is_inspired(c, constants::PID)) {
                            count -= 2 * 0.25 * at(c)->halite;
                        }
                    }
                }
            }
        }
    }
    return inspiredCountMemo[p] = count;
}


bool GameMap::likely_inspired(Position p, int turns) {
    return is_inspired(p, constants::PID);
    if (!constants::INSPIRATION_ENABLED) return false;
    turns = min(2, turns / 2);
    if (likelyInspiredMemo.count(make_pair(p, turns))) return likelyInspiredMemo[make_pair(p, turns)];
    int radius = turns + constants::INSPIRATION_RADIUS * 2;
    int enemies = 0;
    for (int i = 0; i<radius * 2; i++) {
        for (int k = 0; k<radius * 2; k++) {
            auto c = Position {p.x - radius + i, p.y - radius + k};
            if (this->calculate_distance(c, p) <= min(2, turns / 2) + constants::INSPIRATION_RADIUS) {
                if (at(c)->occupied_by_not(constants::PID)) {
                    enemies++;
                }
            }
        }
    }

    return likelyInspiredMemo[make_pair(p, turns)] = enemies >= 4;
}

bool GameMap::is_inspired(Position p, PlayerId id, bool enemy) {
    if (!constants::INSPIRATION_ENABLED) return false;
    if (inspiredMemo.count(p)) return inspiredMemo[p];
    int radius = constants::INSPIRATION_RADIUS * 2;
    int enemies = 0;
    for (int i = 0; i<radius * 2; i++) {
        for (int k = 0; k<radius * 2; k++) {
            auto c = Position {p.x - radius + i, p.y - radius + k};
            if (this->calculate_distance(c, p) <= constants::INSPIRATION_RADIUS) {
                if (enemy) {
                    if (at(c)->is_occupied(id)) {
                        enemies++;
                    }
                }
                else {
                    if (at(c)->occupied_by_not(id)) {
                        enemies++;
                    }
                }
            }
        }
    }
    return inspiredMemo[p] = constants::INSPIRATION_SHIP_COUNT <= enemies;
}

vector<Position> GameMap::get_surrounding_pos(Position p, bool inclusive) {
    vector<Position> out;
    if (inclusive) out.push_back(p);
    for (auto a : ALL_CARDINALS) {
        out.push_back(normalize(p.directional_offset(a)));
    }
    return out;
}

int GameMap::sum_around_point(Position p, int r) {
    int sum = 0;
    for (int i = 0; i<2 * r; i++) {
        for (int k = 0; k< 2 * r; k++) {
            auto end = Position(p.x - r + i, p.y -r + k);
            if (calculate_distance(p, end) <= r) {
                auto z = normalize(end);
                sum += at(z)->halite; // 1.0 / (r - calculate_distance(p, end) + 1) * at(z)->halite * at(z)->halite;
            }
        }
    }
    return sum;
}

float GameMap::avg_around_point(Position p, int r) {
    auto pa = make_pair(p, r);
    if (avgAroundPointMemo.count(pa)) return avgAroundPointMemo[pa];
    int sum = 0;
    int count = 0;
    for (int i = 0; i<2 * r; i++) {
        for (int k = 0; k< 2 * r; k++) {
            auto end = Position(p.x - r + i, p.y -r + k);
            if (calculate_distance(p, end) <= r) {
                auto z = normalize(end);
                sum += at(z)->halite;
                count++;
            }
        }
    }
    return avgAroundPointMemo[pa] = sum / (float)count;
}

vector<Position> GameMap::points_around_pos(Position p, int r) {
    vector<Position> out;
    for (int i = 0; i<2 * r; i++) {
        for (int k = 0; k< 2 * r; k++) {
            auto end = Position(p.x - r + i, p.y -r + k);
            if (calculate_distance(p, end) <= r) {
                out.push_back(end);
            }
        }
    }
    return out;
}

int GameMap::enemies_around_point(Position p, int r) {
    if (enemies_around_point_memo.count({p, r})) return enemies_around_point_memo[{p, r}];
    auto points = points_around_pos(p, r);
    return enemies_around_point_memo[{p, r}] = (int)std::count_if(points.begin(), points.end(), [this](Position p) {
        auto ship = this->at(p)->ship.get();
        if (ship == nullptr) return false;
        return ship->owner != constants::PID;// && ship->halite < 900;
    });
}

int GameMap::friends_around_point(Position p, int r) {
    if (friends_around_point_memo.count({p, r})) return friends_around_point_memo[{p, r}];
    auto points = points_around_pos(p, r);
    return friends_around_point_memo[{p, r}] = (int)std::count_if(points.begin(), points.end(), [this](Position p) {
        auto ship = this->at(p)->ship.get();
        if (ship == nullptr) return false;
        return ship->owner == constants::PID;// && ship->halite < 900;
    });
}

void _update();
static std::unique_ptr<GameMap> _generate();

