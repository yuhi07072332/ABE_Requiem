#include <map>
#include <utility>

#include "genk.hpp"
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

