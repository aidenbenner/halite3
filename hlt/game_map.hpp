#pragma once

#include "types.hpp"
#include "map_cell.hpp"
#include "player.hpp"

#include <cassert>
#include <vector>
#include <algorithm>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <unordered_set>
#include <set>
#include <deque>
#include <string>
#include <queue>

using namespace std;
namespace hlt {

struct pairhash {
public:
  template <typename T, typename U>
  std::size_t operator()(const std::pair<T, U> &x) const
  {
    return std::hash<T>()(x.first) + std::hash<U>()(x.second) * 255;
  }
};

    struct TimePos {
        int turn;
        Position p;

        friend bool operator<(const TimePos& a, const TimePos& b) {
            if (a.turn ==  b.turn) {
                return a.p < b.p;
            }
            return a.turn < b.turn;
        }
    };

    struct GameMap {
        int width;
        int height;
        std::vector<std::vector<MapCell>> cells;

        std::set<TimePos> planned_route;
        // std::map<Position, int> future_halite;

        std::map<Position, set<int>> hal_mp;

        bool checkIfPlanned(int future_turns, Position p) {
            return planned_route.count(TimePos{future_turns, p});
        }

        int hal_at(Position p, int turn) {
            if (turn == 0) {
                return at(p)->halite;
            }

            if (!hal_mp.count(p)) {
                hal_mp[p] = std::set<int>();
            }
            int uses = distance(hal_mp[p].begin(), hal_mp[p].lower_bound(turn));
            return at(p)->halite * pow(0.8, uses);
        }

        void mine_hal(Position p, int turn) {
            if (!hal_mp.count(p)) {
                hal_mp[p] = std::set<int>();
            }
            hal_mp[p].insert(turn);
        }

        void addPlanned(int future_turns, VC<Position> p) {
            for (int i = 0; i<(int)p.size(); i++) {
                planned_route.insert(TimePos{i, p[i]});
            }
        }

        void addPlanned(int future_turns, Position p) {
            planned_route.insert(TimePos{future_turns, p});
        }

        MapCell* at(const Position& position) {
            Position normalized = normalize(position);
            return &cells[normalized.y][normalized.x];
        }


        int at(Position p, int turn) {
            assert(false);
            return 0;
        }

        MapCell* at(int x, int y) {
            return at(Position{x,y});
        }

        MapCell* at(const Entity& entity) {
            return at(entity.position);
        }

        MapCell* at(const Entity* entity) {
            return at(entity->position);
        }

        MapCell* at(const std::shared_ptr<Entity>& entity) {
            return at(entity->position);
        }

        Direction getDirectDiff(Position a, Position b) {
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
            assert(false);
            return Direction::STILL;
        }


        BFSR BFS(Position source) {
            // dfs out of source to the entire map
            set<Position> visited;

            int def = 1e8;
            vector<vector<int>> dist(width,vector<int>(height,def));
            vector<vector<Position>> parent(width,vector<Position>(height, {-1, -1}));

            dist[source.x][source.y] = 0;

            vector<Position> frontier;
            vector<Position> next;
            next.push_back(source);
            while(!next.empty()) {
                frontier = next;
                next.clear();
                while(!frontier.empty()) {
                    auto p = frontier.back();
                    frontier.pop_back();
                    if (visited.count(p)) {
                        continue;
                    }
                    visited.insert(p);

                    for (auto d : ALL_CARDINALS) {
                        next.push_back(normalize(p.directional_offset(d)));
                    }

                    for (auto d : ALL_CARDINALS) {
                        auto f = normalize(p.directional_offset(d));
                        // if (parent[f.x][f.y].x == -1) continue;

                        int c = at(f)->cost() + dist[f.x][f.y];
                        if (c <= dist[p.x][p.y]) {
                            dist[p.x][p.y] = c;
                            parent[p.x][p.y] = f;
                        }
                    }
                }
            }
            return BFSR{dist, parent};
        }

        vector<Position> traceBackPath(VVP parents, Position start, Position dest) {
            Position curr = dest;
            VC<Position> path;

            while (curr != start) {
                curr = parents[curr.x][curr.y];
                path.push_back(curr);
            }
            std::reverse(path.begin(), path.end());

            return path;
        }

        VC<Position> random_walk(int starting_halite, Position start, Position dest) {
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

        VC<Position> wait_adjust(int starting_halite, VC<Position> walk, int turn) {
            int curr_hal = starting_halite;
            VC<Position> adjusted;
            for (auto a : walk) {
                while (hal_at(a,turn) >= get_halite_percentile(0.50) || curr_hal < 0.1 * hal_at(a, turn)) {
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

        bool path_conflicts(int starting_halite, VC<Position> path) {
            int turn = 0;
            for (int i = 0; i<(int)path.size(); i++) {
                if (checkIfPlanned(turn, path[i]))
                    return true;
                turn++;
            }
            return false;
        }

        vector<Direction> dirsFrompath(VC<Position> p) {
            if ((int)p.size() == 0 || (int)p.size() == 1) return vector<Direction>(1, Direction::STILL);
            log::log("Get other ", p[0]);
            return {getDirectDiff(p[0], p[1])};
        }

        int getPathCost(VC<Position> p) {
            // get sum of walk
            int out = 0;
            for (int i = 1; i<(int)p.size(); i++) {
                if (p[i] == p[i-1]) {
                    out += hal_at(p[i-1], i-1) * 0.25;
                }
                else {
                    out -= hal_at(p[i-1], i-1) * 0.1;
                }
            }
            return out / p.size();
        }

        vector<Direction> plan_gather_path(int starting_halite, Position start, Position dest) {
           // out of 10 random walks take the one that maximizes halite^2 and doesn't conflict
           auto walks = VC<VC<Position>>();
           auto allowed_walks = VC<VC<Position>>();
           VC<Position> chosen_walk;
           int mcost = -100;
           for (int i = 0; i<500; i++) {
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

        vector<Direction> plan_min_cost_route(VVP parents, int starting_halite, Position start, Position dest) {
            VC<Position> path = traceBackPath(parents, start, dest);

            int pind = 0;
            auto curr = start;
            int curr_h = starting_halite;
            int time = 0;
            int wturns = 0;

            auto max_halite_pos = start;
            while (curr != dest) {
            //    log::log(curr, dest, checkIfPlanned(time, curr), curr_h, at(curr)->cost());
                if (at(curr)->halite > at(max_halite_pos)->halite) {
                    max_halite_pos = curr;
                }
                // Assuming halite stays constant
                if (!checkIfPlanned(time, curr) && curr_h > at(curr)->cost()) {
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
                time += 1;
            }
            // we had to wait a turn
            if (wturns > 0 && max_halite_pos == start) {
                return vector<Direction>(1, Direction::STILL);
            }

            time = 0;
            for (auto p : path) {
                addPlanned(time, p);
                time++;
            }
            return minCostOptions(parents, start, dest);
        }

        vector<Direction> minCostOptions(VVP pos, Position start, Position dest) {
            if (start == dest) {
                return vector<Direction>(1, Direction::STILL);
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
            opts.insert(opts.end(), others.begin(), others.end());
            return opts;
        }

        int calculate_distance(const Position& source, const Position& target) {
            const auto& normalized_source = normalize(source);
            const auto& normalized_target = normalize(target);

            const int dx = std::abs(normalized_source.x - normalized_target.x);
            const int dy = std::abs(normalized_source.y - normalized_target.y);

            const int toroidal_dx = std::min(dx, width - dx);
            const int toroidal_dy = std::min(dy, height - dy);

            return toroidal_dx + toroidal_dy;
        }

        Position normalize(const Position& position) {
            const int x = ((position.x % width) + width) % width;
            const int y = ((position.y % height) + height) % height;
            return { x, y };
        }


        bool canMove(std::shared_ptr<Ship> ship) {
            return at(ship)->halite * 0.1 <= ship->halite;
        }

        std::vector<Direction> get_unsafe_moves(const Position& source, const Position& destination) {
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

        bool is_in_range_of_enemy(Position p, PlayerId pl) {
            if (at(p)->occupied_by_not(pl)) return true;
            for (int i = 0; i<4; i++) {
                auto po = p.directional_offset(ALL_CARDINALS[i]);
                if (at(po)->occupied_by_not(pl)) return true;
            }
            return false;
        }

        std::vector<int> hal_dist;
        int get_halite_percentile(double percentile) {
            int out = hal_dist[percentile * hal_dist.size()];
            return out;
        }

        int get_total_halite() {
            int sum = 0;
            for (int i = 0; i<width; i++) {
                for (int k= 0; k<height; k++) {
                    sum += at(i,k)->halite;
                }
            }
            return sum;
        }

        inline double costfn(Ship *s, int to_cost, int home_cost, Position shipyard, Position dest, PlayerId pid, bool is_1v1) {
            if (dest == shipyard) return 10000000;
            // if (at(dest)->occupied_by_not(pid)) return 100000000;

            int turns_to = calculate_distance(s->position, dest);
            int turns_back = calculate_distance(dest, shipyard);
            int turns = turns_to + turns_back;

            int halite = at(dest)->halite;
            if (turns_to <= 4) {
                if (is_inspired(dest, pid)) {
                    halite *= 3;
                }
            }
            if (halite < get_halite_percentile(0.5)) {
                return 10000;
            }

            //to_cost = 0;
            //int avg_hal = avg_around_point(dest, 1);
            double out = (halite - to_cost - home_cost) / (double)turns;
            return -out;
        }

        map<Position, bool> inspiredMemo;
        bool is_inspired(Position p, PlayerId id) {
            if (inspiredMemo.count(p)) return inspiredMemo[p];
            int radius = constants::INSPIRATION_RADIUS * 2;
            int enemies = 0;
            for (int i = 0; i<radius * 2; i++) {
                for (int k = 0; k<radius * 2; k++) {
                    auto c = Position {p.x - radius + i, p.y - radius + k};
                    if (this->calculate_distance(c, p) <= constants::INSPIRATION_RADIUS) {
                        if (at(c)->occupied_by_not(id)) {
                            enemies++;
                        }
                    }
                }
            }
            return inspiredMemo[p] = constants::INSPIRATION_SHIP_COUNT >= 2;
        }

        int sum_around_point(Position p, int r) {
            int sum = 0;
            for (int i = 0; i<2 * r; i++) {
                for (int k = 0; k< 2 * r; k++) {
                    auto end = Position(p.x - r + i, p.y -r + k);
                    if (calculate_distance(p, end) <= r) {
                        auto z = normalize(end);
                        sum += at(z)->halite;
                    }
                }
            }
            return sum;
        }

        float avg_around_point(Position p, int r) {
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
            return sum / (float)count;
        }

        void _update();
        static std::unique_ptr<GameMap> _generate();
    };
}
