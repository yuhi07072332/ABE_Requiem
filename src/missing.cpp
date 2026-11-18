#include <array>
#include <queue>
#include <vector>
#include <algorithm>

#include "genk.hpp"


/// @brief 欠損点補完
/// @details
/// * 線形探索で有効なデータに隣接している欠損点をキューに入れる
/// * 以降は幅優先探索で欠損点領域の外側から
///   8近傍の有効なデータの中央値を取って欠損点を補完する
/// * 補完した欠損点の8近傍を調べ、欠損点でキューに入っていないものを入れる
void fill_missing(MapData& data) {

    constexpr std::array<Coord, 8> dirs = {
        Coord {-1, -1}, Coord {-1, 0}, Coord {-1, 1},
        Coord {0, -1},                 Coord {0, 1},
        Coord {1, -1}, Coord {1, 0}, Coord {1, 1},
    };

    std::queue<Coord> missing_queue;

    std::vector<std::vector<bool>> queued(
        data.rows,
        std::vector<bool>(data.cols, false)
    );


    // 欠損点の座標をキューに入れる
    for (int i = 0; i < data.rows; ++i) {
        for (int j = 0; j < data.cols; ++j) {
            Coord current {i, j};

            if (!data.is_missing(current)) continue;

            for (Coord dir : dirs) {
                Coord neighbor = current + dir;
                
                if (neighbor.i < 0 || neighbor.i >= data.rows) continue;
                if (neighbor.j < 0 || neighbor.j >= data.cols) continue;

                if (!data.is_missing(neighbor)) {
                    if (!queued.at(current.i).at(current.j)) {
                        missing_queue.push(current);
                        queued.at(current.i).at(current.j) = true;
                    }
                    break;
                }
            }
        }
    }


    // 幅優先探索で欠損点を補完
    while (!missing_queue.empty()) {
        Coord current = missing_queue.front();
        missing_queue.pop();

        std::vector<double> heights;

        for (Coord dir : dirs) {
            Coord neighbor = current + dir;

            if (neighbor.i < 0 || neighbor.i >= data.rows) continue;
            if (neighbor.j < 0 || neighbor.j >= data.cols) continue;

            if (!data.is_missing(neighbor)) {
                heights.push_back(data.at(neighbor));
            }
        }

        std::sort(heights.begin(), heights.end());
        if (heights.empty()) continue;

        double median = heights.at(heights.size() / 2);    //偶数個の場合は中央2値のうち大きい方を使用
        data.at(current) = median;

        
        // 補完した欠損点の8近傍を調べ、欠損点でキューに入っていないものを入れる
        for (Coord dir : dirs) {
            Coord neighbor = current + dir;

            if (neighbor.i < 0 || neighbor.i >= data.rows) continue;
            if (neighbor.j < 0 || neighbor.j >= data.cols) continue;

            if (!data.is_missing(neighbor)) continue;
            if (queued.at(neighbor.i).at(neighbor.j)) continue;

            missing_queue.push(neighbor);
            queued.at(neighbor.i).at(neighbor.j) = true;
        }
    }
}
