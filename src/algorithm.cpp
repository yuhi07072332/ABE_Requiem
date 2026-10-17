#include <map>
#include <algorithm>
#include <limits>
#include <utility>
#include <print>

#include "genk.hpp"

/// @brief 度数が2番目まで最大なkeyを返す
static std::pair<int, int> max2(const std::map<int, int>& data) {
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

/// @brief 高さの度数分布を計算する
/// @returns keyを階級の下限値, valueを度数とするmap
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

/// @brief 地面と判定された点群を返す
/// @details
/// 暫定のアルゴリズム:
/// * 高さの度数が最も高い2つの階級(の下限値)を地面高さの候補とし、高さがそれ
///   より低い階級も地面の候補とする
/// * マップを16 * 16のチャンクに分けて、チャンクごとに：
///     - チャンク内の最も低い高さが地面の候補にあれば、チャンク内でそれを地面
///       の高さの基準とする
///     - 基準よりNメートル高ければ地面以外と判定する
///
/// 可能な問題点:
/// * 斜めの地面を建物に誤判定することがある
/// * 逆に建物が地面に誤判定することがある
/// * チャンク間のつなぎがあまり綺麗じゃないことがある
std::vector<Coord> find_ground(const MapData& data) {
    constexpr int N = 2;
    constexpr int CHUNK_WIDTH = 16;
    constexpr int CHUNK_HEIGHT = 16;

    auto freq = calc_frequency(data.grid);
    auto freq_max2 = max2(freq);

    // これ以下の高さを地面の候補とする
    double ground_candidate_height =
        std::max(freq_max2.first, freq_max2.second);
    std::println("地面基準: {}", ground_candidate_height);

    std::vector<Coord> ground_coords;

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
                double height = data[i, j];
                if (height < min_height + N) ground_coords.emplace_back(i, j);
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

    return ground_coords;
}
