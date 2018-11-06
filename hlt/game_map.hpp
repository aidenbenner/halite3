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

    struct GameMap {
        int width;
        int height;
        std::vector<std::vector<MapCell>> cells;

        MapCell* at(const Position& position) {
            Position normalized = normalize(position);
            return &cells[normalized.y][normalized.x];
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

            vector<vector<int>> dist(width,vector<int>(height,1e8));
            vector<vector<Position>> parent(width,vector<Position>(height));

            dist[source.x][source.y] = 0;

            vector<Position> frontier;
            vector<Position> next;
            next.push_back(source);
            while(!next.empty()) {
                frontier = std::move(next);
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
                        int c = at(f)->cost() + dist[f.x][f.y];
                        if (c < dist[p.x][p.y]) {
                            dist[p.x][p.y] = c;
                            parent[p.x][p.y] = f;
                        }
                    }
                }
            }
            return BFSR{dist, parent};
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

        void onTurn() {
            ncost.clear();
        }

        map<tuple<int,int,int,int>, int> ncost;
        int naive_cost(int starting_halite, Position a, Position b) {
            a = normalize(a);
            b = normalize(b);
            auto t = make_tuple(a.x, a.y, b.x, b.y);
            if (ncost.count(t)) return ncost[t];
            if (a == b) return 0;
            int cost = at(a)->cost();
            //int gain = at(a)->gain();

            int mhalite = 999999999;
            auto pos = a;
            for (auto d : get_unsafe_moves(a,b)) {
                if (at(a.directional_offset(d))->halite < mhalite) {
                    pos = a.directional_offset(d);
                    mhalite = at(pos)->halite;
                }
            }

            return ncost[t] = cost + naive_cost(starting_halite, pos, b);
        }

        int naive_waits(int starting_halite, Position a, Position b) {
            a = normalize(a);
            b = normalize(b);
            if (a == b) return 0;
            int cost = at(a)->cost();
            int gain = at(a)->gain();
            if (cost > starting_halite) {
                int curr_hal = starting_halite + gain; // - cost * 4 / 5;
                return 1 + naive_waits(curr_hal + gain,a, b);
            }
            if (at(a)->halite > 100) {
                int curr_hal = starting_halite + gain; // - cost * 4 / 5;
                return 1 + naive_waits(curr_hal + gain,a, b);
            }
            return 1 + naive_waits(starting_halite - cost, a.directional_offset(get_unsafe_moves(a,b)[0]), b);
        }

        Direction naive_navigate(std::shared_ptr<Ship> ship, const Position& destination) {
            // get_unsafe_moves normalizes for us
            ship->log("navigating");
            if (ship->position == destination)
                return Direction::STILL;
            for (auto direction : get_unsafe_moves(ship->position, destination)) {
                //Position target_pos = ship->position.directional_offset(direction);
                return direction;
                /*if (!at(target_pos)->is_unsafe()) {
                    at(target_pos)->mark_unsafe(ship);
                    return direction;
                }*/
            }
            return Direction::STILL;
        }

        unordered_map<pair<Position, Position>, pair<int, Direction>, pairhash> mincost;
        std::pair<int, Direction> mincostnav(const Position& start, const Position& dest) {
            mincost.clear();
            return mincostnav2(start,dest);
        }

        std::pair<int, Direction> fast_mincostnav(const Position& start, const Position& dest) {
            return mincostnav2(start,dest);
        }

        void clear_fast_mincostnav() {
            mincost.clear();
        }

        std::pair<int, Direction> mincostnav2(const Position& start, const Position& dest) {
            if (start == dest) {
                return make_pair(0, Direction::STILL);
            }
            auto p = make_pair(start, dest);
            if (mincost.count(p)) {
                return mincost[p];
            }

            auto moves = get_unsafe_moves(start, dest);
            int curr_weight = 666001;
            auto curr_move = Direction::STILL;

            int ccost = at(start)->cost();
            for (auto m : moves) {
                auto curr = mincostnav2(normalize(start.directional_offset(m)), dest);
                if (curr.first < curr_weight) {
                    curr_weight = curr.first;
                    curr_move = curr.second;
                }
            }
            //assert(curr_weight != 666001);
            if (curr_weight == 666001) curr_weight = 0;

           return mincost[p] = std::make_pair(ccost + curr_weight, curr_move);
        }


        bool is_in_range_of_enemy(Position p, PlayerId pl) {
            if (at(p)->occupied_by_not(pl)) return true;
            for (int i = 0; i<4; i++) {
                auto po = p.directional_offset(ALL_CARDINALS[i]);
                if (at(po)->occupied_by_not(pl)) return true;
            }
            return false;
        }

        int get_halite_percentile(double percentile) {
            std::vector<int> hal;
            for (int i = 0; i < width; i++) {
                for (int k = 0; k < height; k++) {
                    hal.push_back(at(i, k)->halite);
                }
            }
            std::sort(hal.begin(), hal.end());

            int out = hal[percentile * hal.size()];
            return out;
        }

        const int TURN_WEIGHT = 30;

        int get_total_halite() {
            int sum = 0;
            for (int i = 0; i<width; i++) {
                for (int k= 0; k<height; k++) {
                    sum += at(i,k)->halite;
                }
            }
            return sum;
        }

        int BLOCK_SIZE = 1;
        inline double costfn(Ship *s, int to_cost, int home_cost, Position shipyard, Position dest, PlayerId pid, bool is_1v1) {
            if (dest == shipyard) return 10000000;

            int turns_to = calculate_distance(s->position, dest);
            int turns_back = calculate_distance(dest, shipyard);
            int turns = turns_to + turns_back;

            int halite = at(dest)->halite;
            if (turns_to < 3) {
                if (is_inspired(dest, pid)) {
                    halite *= 2;
                }
            }


            double out = (halite - to_cost - home_cost) / (double)turns;
            return -out;
        }

        map<Position, bool> inspiredMemo;
        bool is_inspired(Position p, PlayerId id) {
            if (inspiredMemo.count(p)) return inspiredMemo[p];
            int radius = 4;
            int enemies = 0;
            for (int i = 0; i<radius * 2; i++) {
                for (int k = 0; k<radius * 2; k++) {
                    auto c = Position {p.x - radius + i, p.y - radius + k};
                    if (this->calculate_distance(c, p) < radius) {
                        if (at(c)->occupied_by_not(id)) {
                            enemies++;
                        }
                    }
                }
            }
            return inspiredMemo[p] = enemies >= 2;
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


        Position largestInArea(Position p, int r) {
            return largestInArea(p.x, p.y, r);
        }

        Position largestInArea(int x, int y, int r) {
            int curr_max = 0;
            int cx = 0;
            int cy = 0;
            for (int i = 0; i < 2 * r; i++) {
                for (int k = 0; k < 2 * r; k++) {
                    int h = at(x + i - r,y + k - r)->halite;
                    if (h > curr_max) {
                        curr_max = h;
                        cx = x + i - r;
                        cy = y + k - r;
                    }
                }
            }
            return Position {cx, cy};
        }

        void _update();
        static std::unique_ptr<GameMap> _generate();
    };
}
