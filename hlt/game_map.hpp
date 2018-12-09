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

    struct Ship;

    class GameMap {
    public:
        int width;
        int height;
        std::vector<std::vector<MapCell>> cells;

        // Planning for the future: planned = planned + set
        std::set<TimePos> planned_route;
        // std::map<Position, int> future_halite;

        map<Position, int> inspiredCountMemo;

        std::map<Position, set<int>> hal_mp;

        // set_route contains the next turn state.
        std::map<TimePos, Ship*> set_route;

        std::map<Position, Position> closestDropMp;

        Ship* get_closest_ship(Position pos, vector<shared_ptr<Player>> &p);

        Position closest_dropoff(Position pos, Game *g);

        Ship* get_closest_ship(Position pos, Player &p);

        bool checkSet(int future_turns, Position p);

        void addSet(int future_turns, Position p, Ship *s = nullptr);

        Ship* getSet(int turns, Position p);

        bool checkIfPlanned(int future_turns, Position p);

        void clearPlanned();

        void addPlanned(int future_turns, VC<Position> p);

        void addPlanned(int future_turns, Position p);

        int hal_at(Position p, int turn);

        void mine_hal(Position p, int turn);

        MapCell* at(const Position& position);

        int at(Position p, int turn);

        MapCell* at(int x, int y);

        MapCell* at(const Entity& entity);

        MapCell* at(const Entity* entity);

        MapCell* at(const std::shared_ptr<Entity>& entity);

        Direction getDirectDiff(Position a, Position b);

        BFSR BFS(Position source, bool greedy=false, int starting_hal=0);

        vector<Position> traceBackPath(VVP parents, Position start, Position dest);

        void random_walk(VC<Position> &walk, int length, int seed);

        VC<Position> random_walk(int starting_halite, Position start, Position dest);

        VC<Position> wait_adjust(int starting_halite, VC<Position> walk, int turn);

        bool path_conflicts(int starting_halite, VC<Position> path);

        vector<Direction> dirsFrompath(VC<Position> p);

        int getPathCost(VC<Position> p);

        vector<Position> hc_plan_gather_path(int starting_halite, Position start, Position end, vector<Position> starting_path);

        vector<Position> hc_plan_gather_path(int starting_halite, Position start, vector<Position> starting_path);

        vector<Direction> plan_gather_path(int starting_halite, Position start, Position dest);

        RandomWalkResult get_best_random_walk(int starting_halite, Position start, Position dest);

        Direction get_random_dir_towards(Position start, Position end);

        vector<Direction> plan_min_cost_route(VVP parents, int starting_halite, Position start, Position dest, int time = 1);

        vector<Direction> minCostOptions(VVP pos, Position start, Position dest);

        int calculate_distance(const Position& source, const Position& target);

        Position normalize(const Position& position);

        bool canMove(std::shared_ptr<Ship> ship);

        std::vector<Direction> get_unsafe_moves(const Position& source, const Position& destination);

        bool is_in_range_of_enemy(Position p, PlayerId pl, bool on_square=false);

        Ship* enemy_in_range(Position p, PlayerId pl, bool on_square=false);

        std::vector<int> hal_dist;
        int get_halite_percentile(double percentile);

        int get_mine_threshold();

        bool should_mine(Position p);

        int get_total_halite();

        int num_inspired(Position p, PlayerId id);

        double costfn(Ship *s, int to_cost, int home_cost, Position shipyard, Position dest, PlayerId pid, bool is_1v1, int extra_turns);

        map<Position, bool> inspiredMemo;

        bool is_inspired(Position p, PlayerId id, bool enemy=false);

        vector<Position> get_surrounding_pos(Position p, bool inclusive=true);

        int sum_around_point(Position p, int r);

        float avg_around_point(Position p, int r);

        void _update();
        static std::unique_ptr<GameMap> _generate();
    };
}
