#include <exception>
#include <ostream>
#include <string_view>
#include <print>
#include <utility>
#include <vector>
#include <cstddef>
#include <ranges>
#include <array>

#include "third_party/delaunator/delaunator.hpp"

#include "genk.hpp"

namespace views = std::views;

/// 点群をElevationGridで出力
void write_elevation_grid(std::ostream& os, const MapData& data) {
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
            else std::print(os, "{}, ", data.at(z, x));
            if (count++ == 50) {
                count = 0;
                std::println(os);
            }
        }
    }

    std::print(os, FMT2);
}


constexpr std::string_view INDEXEDFACESET_FMT1 = R"(
Shape {{
    appearance Appearance {{
        material Material {{
            diffuseColor {0}
        }}
    }}
    geometry IndexedFaceSet {{
        coord Coordinate {{
            point [
)";

constexpr std::string_view INDEXEDFACESET_FMT2 = R"(
        ]
        }}
        coordIndex [
)";

constexpr std::string_view INDEXEDFACESET_FMT3 = R"(
        ]
    }}
}}
)";

/// @brief 点群に対しドロネーの三角形分割を実行し、IndexFacedSetとして出力
/// @returns 三角形の面数
int write_delaunay_triangulation(std::ostream& os,
                                 const std::vector<Coord>& coords,
                                 const MapData& data,
                                 std::string_view diffuse_color) {
    std::vector<double> coords_vec;
    coords_vec.reserve(coords.size() * 2);
    for (Coord c : coords) {
        Point p = data.point_at(c);
        coords_vec.push_back(p.x);
        coords_vec.push_back(p.z);
    }

    if (coords.size() < 2) {
        throw std::logic_error("write_delaunay_triangulation(): coordは最低2以上必要");
    }

    delaunator::Delaunator d(coords_vec);

    std::print(os, INDEXEDFACESET_FMT1, diffuse_color);

    for (Coord c : coords) {
        Point p = data.point_at(c);
        std::println(os, "{} {} {},", p.x, p.y, p.z);
    }

    std::print(os, INDEXEDFACESET_FMT2);

    for (std::size_t i = 0; i < d.triangles.size(); i += 3) {
        std::println(os,
                     "{}, {}, {}, -1",
                     d.triangles.at(i),
                     d.triangles.at(i + 1),
                     d.triangles.at(i + 2));
    }

    std::print(os, INDEXEDFACESET_FMT3);
    return d.triangles.size() / 3;

}


/// @brief 建物をIndexFacedSetとして出力
///
/// @returns 面数
int write_building(std::ostream& os,
                    const MapData& data,
                    const Building& building,
                    std::string_view diffuse_color) {
    auto is_building = [&](Coord c) {
        return data.contains(c)
            && (data.type_at(c) == Type::Building 
            || data.type_at(c) == Type::BuildingOutline);
    };

    std::vector<Point> vertices;
    std::vector<std::array<int, 4>> faces; 

    // 壁
    for (const auto& outline : building.outline_paths) {
        for (Coord c : outline) {
            const int base = vertices.size();

            auto add_face = [&](std::array<int, 4> indices) {
                for (int& i : indices) i += base;
                faces.push_back(indices);
            };

            auto [x, y, z] = data.point_at(c);

            std::array<Point, 8> coord_vertices{{
                {x - 0.5, -30, z - 0.5},
                {x + 0.5, -30, z - 0.5},
                {x + 0.5, -30, z + 0.5},
                {x - 0.5, -30, z + 0.5},

                {x - 0.5, y, z - 0.5},
                {x + 0.5, y, z - 0.5},
                {x + 0.5, y, z + 0.5},
                {x - 0.5, y, z + 0.5},
            }};

            vertices.append_range(coord_vertices);

            if (!is_building(c + Coord{-1, 0})) 
                add_face({0, 4, 5, 1});
            if (!is_building(c + Coord{0, 1})) 
                add_face({1, 5, 6, 2});
            if (!is_building(c + Coord{1, 0})) 
                add_face({2, 6, 7, 3});
            if (!is_building(c + Coord{0, -1})) 
                add_face({3, 7, 4, 0});
        }
    }

    std::print(os, INDEXEDFACESET_FMT1, diffuse_color);

    for (auto p : vertices) 
        std::println(os, "{} {} {},", p.x, p.y, p.z);

    std::print(os, INDEXEDFACESET_FMT2);

    for (auto face : faces) 
        std::println(os, "{}, {}, {}, {} -1", face[0], face[1], face[2], face[3]);

    std::print(os, INDEXEDFACESET_FMT3);

    // 屋上
    if (building.coords.size() >= 2) {
        try {
            write_delaunay_triangulation(
                os,
                std::vector<Coord>(std::from_range, building.coords),
                data,
                diffuse_color
            );
        } catch (std::exception& e) {

        }
    }

    return faces.size();
}
