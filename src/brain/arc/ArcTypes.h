#pragma once

#include <vector>
#include <array>
#include <cstdint>
#include <string>
#include <optional>
#include <algorithm>

namespace yuki {
namespace brain {
namespace arc {

using Color = uint8_t;
using Grid = std::vector<std::vector<Color>>;
using Shape = std::pair<size_t, size_t>;

constexpr Color COLOR_BLACK = 0;
constexpr Color COLOR_BLUE = 1;
constexpr Color COLOR_RED = 2;
constexpr Color COLOR_GREEN = 3;
constexpr Color COLOR_YELLOW = 4;
constexpr Color COLOR_GRAY = 5;
constexpr Color COLOR_PINK = 6;
constexpr Color COLOR_ORANGE = 7;
constexpr Color COLOR_TEAL = 8;
constexpr Color COLOR_MAROON = 9;
constexpr size_t NUM_COLORS = 10;

inline Shape getShape(const Grid& grid) {
    if (grid.empty()) return {0, 0};
    return {grid.size(), grid[0].size()};
}

inline Grid makeGrid(size_t h, size_t w, Color fill = COLOR_BLACK) {
    return Grid(h, std::vector<Color>(w, fill));
}

inline Grid copyGrid(const Grid& grid) {
    return grid;
}

inline bool sameShape(const Grid& a, const Grid& b) {
    return getShape(a) == getShape(b);
}

inline bool gridsEqual(const Grid& a, const Grid& b) {
    if (!sameShape(a, b)) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        for (size_t j = 0; j < a[i].size(); ++j) {
            if (a[i][j] != b[i][j]) return false;
        }
    }
    return true;
}

inline Grid rotate90(const Grid& grid) {
    auto [h, w] = getShape(grid);
    Grid result = makeGrid(w, h);
    for (size_t i = 0; i < h; ++i) {
        for (size_t j = 0; j < w; ++j) {
            result[j][h - 1 - i] = grid[i][j];
        }
    }
    return result;
}

inline Grid rotate180(const Grid& grid) {
    return rotate90(rotate90(grid));
}

inline Grid rotate270(const Grid& grid) {
    return rotate90(rotate180(grid));
}

inline Grid flipHorizontal(const Grid& grid) {
    auto [h, w] = getShape(grid);
    Grid result = makeGrid(h, w);
    for (size_t i = 0; i < h; ++i) {
        for (size_t j = 0; j < w; ++j) {
            result[i][j] = grid[i][w - 1 - j];
        }
    }
    return result;
}

inline Grid flipVertical(const Grid& grid) {
    auto [h, w] = getShape(grid);
    Grid result = makeGrid(h, w);
    for (size_t i = 0; i < h; ++i) {
        for (size_t j = 0; j < w; ++j) {
            result[i][j] = grid[h - 1 - i][j];
        }
    }
    return result;
}

inline Grid cropGrid(const Grid& grid, size_t r, size_t c, size_t h, size_t w) {
    auto [gh, gw] = getShape(grid);
    if (r + h > gh || c + w > gw) return {};
    Grid result = makeGrid(h, w);
    for (size_t i = 0; i < h; ++i) {
        for (size_t j = 0; j < w; ++j) {
            result[i][j] = grid[r + i][c + j];
        }
    }
    return result;
}

inline std::vector<Grid> findObjects(const Grid& grid, Color background = COLOR_BLACK) {
    auto [h, w] = getShape(grid);
    std::vector<std::vector<bool>> visited(h, std::vector<bool>(w, false));
    std::vector<Grid> objects;
    
    for (size_t i = 0; i < h; ++i) {
        for (size_t j = 0; j < w; ++j) {
            if (visited[i][j] || grid[i][j] == background) continue;
            
            Color color = grid[i][j];
            std::vector<std::pair<size_t, size_t>> pixels;
            std::vector<std::pair<size_t, size_t>> stack = {{i, j}};
            visited[i][j] = true;
            
            while (!stack.empty()) {
                auto [r, c] = stack.back();
                stack.pop_back();
                pixels.push_back({r, c});
                
                const int dr[4] = {-1, 1, 0, 0};
                const int dc[4] = {0, 0, -1, 1};
                for (int d = 0; d < 4; ++d) {
                    int nr = static_cast<int>(r) + dr[d];
                    int nc = static_cast<int>(c) + dc[d];
                    if (nr >= 0 && nr < static_cast<int>(h) && nc >= 0 && nc < static_cast<int>(w) &&
                        !visited[nr][nc] && grid[nr][nc] == color) {
                        visited[nr][nc] = true;
                        stack.push_back({static_cast<size_t>(nr), static_cast<size_t>(nc)});
                    }
                }
            }
            
            if (!pixels.empty()) {
                size_t min_r = h, min_c = w, max_r = 0, max_c = 0;
                for (auto [r, c] : pixels) {
                    min_r = std::min(min_r, r);
                    min_c = std::min(min_c, c);
                    max_r = std::max(max_r, r);
                    max_c = std::max(max_c, c);
                }
                Grid obj = makeGrid(max_r - min_r + 1, max_c - min_c + 1, background);
                for (auto [r, c] : pixels) {
                    obj[r - min_r][c - min_c] = color;
                }
                objects.push_back(std::move(obj));
            }
        }
    }
    return objects;
}

inline std::string gridToString(const Grid& grid) {
    std::string result;
    for (const auto& row : grid) {
        for (Color c : row) {
            result += char('0' + c);
            result += ' ';
        }
        if (!row.empty()) result.pop_back();
        result += '\n';
    }
    return result;
}

inline Grid stringToGrid(const std::string& str) {
    Grid result;
    std::string line;
    for (char c : str) {
        if (c == '\n') {
            if (!line.empty()) {
                std::vector<Color> row;
                for (char ch : line) {
                    if (ch >= '0' && ch <= '9') {
                        row.push_back(static_cast<Color>(ch - '0'));
                    }
                }
                if (!row.empty()) result.push_back(row);
                line.clear();
            }
        } else if (c != ' ') {
            line += c;
        }
    }
    return result;
}

} // namespace arc
} // namespace brain
} // namespace yuki