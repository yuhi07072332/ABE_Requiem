#include <ostream>
#include <string_view>
#include <print>
#include <vector>
#include <cstddef>

#include "third_party/delaunator/delaunator.hpp"

#include "genk.hpp"

/// 点群をElevationGridで出力
void write_elevation_grid(const MapData& data, std::ostream& os) {
    std::println(os, VRML_HEADER);

    constexpr std::string_view FMT1 = R"(
Shape {{
   appearance Appearance {{
       material Material {{
           diffuseColor 0.812 0.664 0.988
       }}
   }}
   geometry ElevationGrid {{
       xDimension {0}
       zDimension {1}
       xSpacing 1
       zSpacing 1
       solid TRUE
       ccw TRUE
       height [
)";

    constexpr std::string_view FMT2 = R"(
       ]
   }}
}}
)";

    std::print(os, FMT1, data.cols, data.rows);

    int count = 0;
    for (int z = 0; z < data.rows; ++z) {
        for (int x = 0; x < data.cols; ++x) {
            if (data.is_missing(z, x)) std::print(os, "100, ");
            else std::print(os, "{}, ", data[z, x]);
            if (count++ == 50) {
                count = 0;
                std::println(os);
            }
        }
    }

    std::print(os, FMT2);
}

/// @brief 点群に対しドロネーの三角形分割を実行し、IndexFacedSetとして出力
/// @returns 三角形の面数
int write_delaunay_triangulation(const std::vector<Point>& pts,
                                 std::ostream& os) {
    constexpr std::string_view FMT1 = R"(
Shape {{
    appearance Appearance {{
        material Material {{
            diffuseColor 0.3 0.5 0.7
        }}
    }}
    geometry IndexedFaceSet {{
        coord Coordinate {{
            point [
)";

    constexpr std::string_view FMT2 = R"(
        ]
        }}
        coordIndex [
)";

    constexpr std::string_view FMT3 = R"(
        ]
    }}
}}
)";

    std::vector<double> coords;
    coords.reserve(pts.size() * 2);
    for (const Point& p : pts) {
        coords.push_back(p.x);
        coords.push_back(p.z);
    }

    delaunator::Delaunator d(coords);

    std::print(os, FMT1);

    for (Point p : pts) {
        std::println(os, "{} {} {},", p.x, p.y, p.z);
    }

    std::print(os, FMT2);

    for (std::size_t i = 0; i < d.triangles.size(); i += 3) {
        std::println(os,
                     "{}, {}, {}, -1",
                     d.triangles[i],
                     d.triangles[i + 1],
                     d.triangles[i + 2]);
    }

    std::print(os, FMT3);
    return d.triangles.size() / 3;
}
