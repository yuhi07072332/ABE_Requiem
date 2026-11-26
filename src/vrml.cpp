#include <cassert>
#include <map>
#include <ostream>
#include <string_view>
#include <print>
#include <vector>
#include <cstddef>
#include <ranges>
#include <array>
#include <format>

#include <third_party/CDT/CDT.hpp>

#include "genk.hpp"
#include "formatter.hpp"

namespace views = std::views;

/// 点群をElevationGridで出力
void write_elevation_grid(std::ostream& os, const MapData& data) {
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

static Point v2d_to_Point(CDT::V2d<double> v, const MapData& data) {
    return Point {
        .x = v.x,
        .y = data.at(v.y - data.z, v.x - data.x),
        .z = v.y
    };
}

struct IndexedFaceSet {
    std::string diffuse_color;
    std::vector<Point> vertices;
    std::vector<int64_t> faces;

    IndexedFaceSet(std::string diffuse_color)
        : diffuse_color(std::move(diffuse_color)) {}

    void write_to(std::ostream& os) {
        constexpr std::string_view FMT1 = R"(
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

        std::print(os, FMT1, diffuse_color);

        for (Point p : vertices) {
            std::println(os, "{} {} {}", p.x, p.y, p.z);
        }

        std::print(os, FMT2, diffuse_color);

        for (auto [i, f] : faces | views::enumerate) {
            std::print(os, "{},", f);
            if (i % 50 == 0) std::print(os, "\n");
        }

        std::print(os, FMT3, diffuse_color);
    }
};

/// @brief 点群に対しドロネーの三角形分割を実行し、IndexFacedSetとして出力
/// @returns 三角形の面数
int write_delaunay_triangulation(std::ostream& os,
                                 const std::vector<Coord>& coords,
                                 const MapData& data,
                                 std::string_view diffuse_color) {
    CDT::Triangulation<double> cdt;

    auto valid_coords = coords
        | views::filter([&](Coord c){ return !data.is_missing(c); });

    cdt.insertVertices(valid_coords.begin(),
                       valid_coords.end(),
                       [&](Coord c){ return data.get_x(c); },
                       [&](Coord c) { return data.get_z(c); });
    cdt.eraseSuperTriangle();

    IndexedFaceSet ifs = IndexedFaceSet(std::string(diffuse_color));
    for (CDT::V2d<double> v : cdt.vertices) {
        ifs.vertices.push_back(v2d_to_Point(v, data));
    }

    for (const auto& tri : cdt.triangles) {
        ifs.faces.push_back(tri.vertices[2]);
        ifs.faces.push_back(tri.vertices[1]);
        ifs.faces.push_back(tri.vertices[0]);
        ifs.faces.push_back(-1);
    }

    ifs.write_to(os);

    return cdt.triangles.size();
}

/// 制限付きドロネー三角形分割を使って建物の上面をIndexedFaceSetに追加
static void add_building_top(IndexedFaceSet& ifs,
                             const MapData& data,
                             const Building& building) {
    auto valid_coords = building.coords
        | views::filter([&](Coord c){ return !data.is_missing(c); });

    std::map<Coord, std::size_t> outline_indices;
    for (auto [i, c] : valid_coords | views::enumerate) {
        if (data.type_at(c) != Type::BuildingOutline
            || data.is_missing(c)) continue;
        outline_indices.try_emplace(c, i);
    }

    CDT::Triangulation<double> cdt;
    cdt.insertVertices(valid_coords.begin(),
                       valid_coords.end(),
                       [&](Coord c){ return data.get_x(c); },
                       [&](Coord c){ return data.get_z(c); });

    CDT::EdgeVec edges;
    for (const auto& path : building.outline_paths) {
        for (std::size_t n = path.size(), i = 0; i + 1 < n; ++i) {
            const Coord c1 = path[i];
            const Coord c2 = path[i + 1];
            edges.emplace_back(
                outline_indices.at(c1),
                outline_indices.at(c2)
            );
        }
    }

    cdt.insertEdges(edges);
    cdt.eraseOuterTrianglesAndHoles();

    const std::size_t offset = ifs.vertices.size();

    for (CDT::V2d<double> v : cdt.vertices) {
        ifs.vertices.push_back(v2d_to_Point(v, data));
    }

    for (const auto& tri : cdt.triangles) {
        ifs.faces.push_back(tri.vertices[2] + offset);
        ifs.faces.push_back(tri.vertices[1] + offset);
        ifs.faces.push_back(tri.vertices[0] + offset);
        ifs.faces.push_back(-1);
    }
}

/// 建物の壁をIndexedFaceSetに追加
static void add_building_wall(IndexedFaceSet& ifs,
                              const MapData& data,
                              const Building& building) {
    auto is_building_type = [&](Coord c) {
        return data.contains(c)
            && (data.type_at(c) == Type::Building 
            || data.type_at(c) == Type::BuildingOutline);
    };

    for (const auto& outline : building.outline_paths
            | views::take(building.outline_paths.size() - 1)) {
        for (Coord c : outline) {
            const int base = ifs.vertices.size();

            auto add_face = [&](std::array<int, 4> indices) {
                for (int& i : indices) i += base;
                ifs.faces.append_range(indices);
                ifs.faces.push_back(-1);
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

            ifs.vertices.append_range(coord_vertices);

            if (!is_building_type(c + Coord{-1, 0})) 
                add_face({0, 4, 5, 1});
            if (!is_building_type(c + Coord{0, 1})) 
                add_face({1, 5, 6, 2});
            if (!is_building_type(c + Coord{1, 0})) 
                add_face({2, 6, 7, 3});
            if (!is_building_type(c + Coord{0, -1})) 
                add_face({3, 7, 4, 0});
        }
    }
}

struct RGB {
    double r, g, b;
};

RGB hsv_to_rgb(double h, double s, double v) {
    h = std::fmod(h, 360.0);
    if (h < 0.0) h += 360.0;

    const double c = v * s;
    const double x = c * (1.0 - std::abs(std::fmod(h / 60.0, 2.0) - 1.0));
    const double m = v - c;

    RGB rgb;

    if      (h < 60)  rgb = {c, x, 0};
    else if (h < 120) rgb = {x, c, 0};
    else if (h < 180) rgb = {0, c, x};
    else if (h < 240) rgb = {0, x, c};
    else if (h < 300) rgb = {x, 0, c};
    else              rgb = {c, 0, x};

    return {rgb.r + m, rgb.g + m, rgb.b + m};
}


/// @brief 全てのVRML化を行う
///
/// @param out メインとなるwrlを出力するためのostream
/// @param out_ground 地面部分を出力するためのostream
/// @param out_building 建物部分を出力するためのostream
/// @param out_ground_filename out_groundに対応ファイル名
/// @param out_building_filename out_buildingに対応ファイル名
/// @param colorful_buildings 建物それぞれについて色分けするか
void export_vrml(const MapData& data,
                std::ostream& out,
                std::ostream& out_ground,
                std::string_view out_ground_filename,
                std::ostream& out_building,
                std::string_view out_building_filename,
                bool colorful_buildings) {
    auto building_color = [&](std::size_t index) -> std::string {
        constexpr double GOLDEN_ANGLE = 137.50776405003785;

        const double hue = std::fmod(index * GOLDEN_ANGLE, 360.0);

        constexpr double saturation = 0.65;
        constexpr double value = 0.90;

        const RGB rgb = hsv_to_rgb(hue, saturation, value);

        return std::format("{:.4f} {:.4f} {:.4f}",
                           rgb.r, rgb.g, rgb.b);
    };


    constexpr std::string_view MAIN_VRML = R"({}
Inline {{
    url "{}"
}}

Inline {{
    url "{}"
}}
)";

    std::vector<Coord> ground_coords;

    for (int i = 0; i < data.rows; ++i) { 
        for (int j = 0; j < data.cols; ++j) {
            if (data.is_missing(i, j)) continue;
            switch (data.type_at(i, j)) {
                case Type::Ground: ground_coords.emplace_back(i, j); break;
                default: break;
            }
        }
    }

    std::print(out,
               MAIN_VRML,
               VRML_HEADER,
               out_ground_filename,
               out_building_filename);
    
    std::print(out_ground, VRML_HEADER);
    if (colorful_buildings) {
        write_delaunay_triangulation(out_ground, ground_coords, data, "0.3 0.3 0.3");
    } else {
        write_delaunay_triangulation(out_ground, ground_coords, data, "0.35 0.43 0.67");
    }
    std::println("地面出力完了");

    std::print(out_building, VRML_HEADER);

    if (colorful_buildings) {
        for (const auto& [i, building] 
            : data.buildings | std::views::enumerate) {
            IndexedFaceSet building_ifs = IndexedFaceSet(building_color(i));
            add_building_wall(building_ifs, data, building);
            add_building_top(building_ifs, data, building);
            building_ifs.write_to(out_building);
        }
    } else {
        IndexedFaceSet building_ifs = IndexedFaceSet("0.96 0.47 0.38");
        for (const auto& building : data.buildings) {
            add_building_wall(building_ifs, data, building);
            add_building_top(building_ifs, data, building);
        }
        building_ifs.write_to(out_building);
    }

    std::println("建物出力完了");

}
