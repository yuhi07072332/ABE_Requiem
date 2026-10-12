#include <map>
#include <algorithm>
#include <limits>
#include <utility>
#include <print>

#include "genk.hpp"

/// @brief 度数が2番目まで最大なkeyを返す
std::pair<int, int> max2(const std::map<int, int>& data) {
    int first_key = 0, second_key = 0;

    for (auto [key, value] : data) {
        if (value > data.at(first_key)) {
            second_key = first_key;
            first_key = key;
        } else if (value > data.at(second_key)) {
            second_key = key;
        }
    }

    return {first_key, second_key};
}

std::map<int, int> calc_frequency(const std::vector<double>& heights,
                                  int width) {
    std::map<int, int> freq;

    for (double height : heights) {
        if (std::isnan(height)) continue;
        int lower = static_cast<int>(height / width) * width;
        if (auto it = freq.find(lower); it != freq.end()) it->second++;
        else freq.try_emplace(lower, 1);
    }

    return freq;
}

std::vector<Point> find_ground(const MapData& data) {
    constexpr int N = 2;
    constexpr int CHUNK_WIDTH = 16;
    constexpr int CHUNK_HEIGHT = 16;

    auto freq = calc_frequency(data.grid);
    auto freq_max2 = max2(freq);

    // これ以下の高さを地面の候補とする
    double ground_candidate_height =
        std::max(freq_max2.first, freq_max2.second);
    std::println("地面基準: {}", ground_candidate_height);

    std::vector<Point> ground_points;

    auto process_chunk = [&](int i_begin, int j_begin) -> void {
        // チャンク内高さが一番低い点を探す
        double min_height = std::numeric_limits<double>::max();
        for (int i = i_begin; i < i_begin + CHUNK_HEIGHT; ++i) {
            for (int j = j_begin; j < j_begin + CHUNK_WIDTH; ++j) {
                if (data.is_missing(i, j)) continue;
                if (data[i, j] < min_height) min_height = data[i, j];
            }
        }

        if (min_height > ground_candidate_height) return;

        for (int i = i_begin; i < i_begin + CHUNK_HEIGHT; ++i) {
            for (int j = j_begin; j < j_begin + CHUNK_WIDTH; ++j) {
                Point p = data.point_at(i, j);
                if (p.y < min_height + N) ground_points.push_back(p);
            }
        }
    };

    int chunks_rows = data.rows / CHUNK_HEIGHT;
    int chunks_cols = data.cols / CHUNK_WIDTH;

    for (int r = 0; r < chunks_rows; ++r) {
        for (int c = 0; c < chunks_cols; ++c) {
            int i_begin = r * CHUNK_HEIGHT;
            int j_begin = c * CHUNK_WIDTH;

            process_chunk(i_begin, j_begin);
        }
    }

    return ground_points;
}
