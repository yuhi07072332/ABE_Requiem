#include <format>
#include <string_view>

#include "genk.hpp"

namespace std {

template <>
struct formatter<Coord> : formatter<std::string_view> {
    template <typename FormatContext>
    auto format(Coord c,FormatContext& ctx) const {
        return format_to(ctx.out(), "({}, {})", c.i, c.j);
    }
};

}   // namespace std
