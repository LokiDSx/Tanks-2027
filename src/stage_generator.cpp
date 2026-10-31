#include "stage_generator.h"
#include "appconfig.h"

#include <ctime>
#include <queue>
#include <random>
#include <utility>

namespace
{
    using Grid = std::vector<std::vector<char>>;
    using Mask = std::vector<std::vector<bool>>;
    using Field = std::vector<std::vector<bool>>;

    struct Cell
    {
        int r, c;
    };

    unsigned sessionSeed()
    {
        static unsigned seed = static_cast<unsigned>(std::time(nullptr)) ^ 0xA5F15C0Du;
        return seed;
    }

    int ri(std::mt19937 &rng, int lo, int hi)
    {
        return lo + static_cast<int>(rng() % static_cast<unsigned>(hi - lo + 1));
    }

    bool roll(std::mt19937 &rng, double probability)
    {
        return static_cast<double>(rng()) / static_cast<double>(rng.max()) < probability;
    }

    void mark(Grid &grid, Mask &forced, int c, int r, char ch)
    {
        if (r < 0 || r >= (int)grid.size() || c < 0 || c >= (int)grid[0].size())
            return;
        grid[r][c] = ch;
        forced[r][c] = true;
    }

    void applyProtectedAreas(Grid &grid, Mask &forced, bool with_eagle)
    {
        const int w = AppConfig::map_size.w;
        const int h = AppConfig::map_size.h;
        const int half_w = w / 2;
        const int tw = AppConfig::tile_size.w;
        const int th = AppConfig::tile_size.h;
        const int eagle_c = w / 2 - 1;
        const int eagle_r = h - 2;

        if (with_eagle)
        {
            // Eagle nest: brick wall on the left (eagle_c - 1) and on top, eagle
            // cells stay empty. The right half of the nest is created by mirroring.
            mark(grid, forced, eagle_c - 1, eagle_r - 1, '#');
            mark(grid, forced, eagle_c - 1, eagle_r,     '#');
            mark(grid, forced, eagle_c - 1, eagle_r + 1, '#');
            mark(grid, forced, eagle_c,     eagle_r - 1, '#');
            mark(grid, forced, eagle_c,     eagle_r,     '.');
            mark(grid, forced, eagle_c,     eagle_r + 1, '.');
        }

        // Player 1 spawn area (the mirrored area covers the player 2 spawn).
        const int p1_c = AppConfig::player_starting_point[0].x / tw;
        const int p1_r = AppConfig::player_starting_point[0].y / th;
        for (int r = p1_r - 1; r <= p1_r + 1; r++)
            for (int c = p1_c - 2; c <= p1_c + 2; c++)
                if (c < half_w)
                    mark(grid, forced, c, r, '.');

        // Enemy spawn areas; only the left half is marked here, mirroring
        // takes care of the middle and the right enemy spawn.
        for (const auto &point : AppConfig::enemy_starting_point)
        {
            const int c0 = point.x / tw;
            const int r0 = point.y / th;
            if (c0 >= half_w)
                continue;
            for (int r = r0; r <= r0 + 2; r++)
                for (int c = c0 - 1; c <= c0 + 1; c++)
                    if (c >= 0 && c < half_w)
                        mark(grid, forced, c, r, '.');
        }
    }

    void placeRect(std::mt19937 &rng, Grid &grid, const Mask &forced, char ch, bool only_empty)
    {
        const int w = (int)grid[0].size();
        const int h = (int)grid.size();
        const int half_w = w / 2;

        const int rw = ri(rng, 1, 2) * 2;
        const int rh = ri(rng, 1, 2) * 2;
        int x = ri(rng, 0, (half_w - 1) / 2) * 2;
        int y = ri(rng, 0, (h - 1) / 2) * 2;

        if (x + rw > half_w)
            x = half_w - rw;
        if (x < 0)
            x = 0;
        if (y + rh > h)
            y = h - rh;

        for (int r = y; r < y + rh; r++)
            for (int c = x; c < x + rw; c++)
            {
                if (c >= half_w)
                    break;
                if (forced[r][c])
                    continue;
                if (only_empty && grid[r][c] != '.')
                    continue;
                grid[r][c] = ch;
            }
    }

    void mirrorLeftHalf(Grid &grid, Mask &forced)
    {
        const int w = (int)grid[0].size();
        for (size_t r = 0; r < grid.size(); r++)
            for (int c = w / 2; c < w; c++)
            {
                grid[r][c] = grid[r][w - 1 - c];
                forced[r][c] = forced[r][w - 1 - c];
            }
    }

    // Limit the number of stone cells on the left half (mirroring doubles
    // it), randomly downgrading the excess to brick.
    void capStoneCells(std::mt19937 &rng, Grid &grid, int max_left_cells)
    {
        const int w = (int)grid[0].size();
        const int h = (int)grid.size();
        const int half_w = w / 2;

        std::vector<Cell> cells;
        for (int r = 0; r < h; r++)
            for (int c = 0; c < half_w; c++)
                if (grid[r][c] == '@')
                    cells.push_back({r, c});

        if ((int)cells.size() <= max_left_cells)
            return;

        for (int i = (int)cells.size() - 1; i >= max_left_cells; i--)
        {
            std::swap(cells[i], cells[ri(rng, 0, i)]);
            grid[cells[i].r][cells[i].c] = '#';
        }
    }

    // Limit the number of field cells on the left half (mirroring doubles it),
    // randomly switching off the excess. Keeps large water bodies in check.
    void capField(std::mt19937 &rng, Field &f, int max_left_cells)
    {
        const int w = (int)f[0].size();
        const int h = (int)f.size();
        const int half_w = w / 2;

        std::vector<Cell> cells;
        for (int r = 0; r < h; r++)
            for (int c = 0; c < half_w; c++)
                if (f[r][c])
                    cells.push_back({r, c});

        if ((int)cells.size() <= max_left_cells)
            return;

        // Partial Fisher-Yates: shuffle the cells, then switch off the excess
        for (int i = (int)cells.size() - 1; i >= max_left_cells; i--)
        {
            std::swap(cells[i], cells[ri(rng, 0, i)]);
            f[cells[i].r][cells[i].c] = false;
        }

        // Restore the mirror symmetry
        for (int r = 0; r < h; r++)
            for (int c = half_w; c < w; c++)
                f[r][c] = f[r][w - 1 - c];
    }

    Field buildField(std::mt19937 &rng, double density, int iterations, int rule_threshold)
    {
        const int w = AppConfig::map_size.w;
        const int h = AppConfig::map_size.h;
        const int half_w = w / 2;

        Field f(h, std::vector<bool>(w, false));
        for (int r = 0; r < h; r++)
            for (int c = 0; c < half_w; c++)
                f[r][c] = roll(rng, density);
        for (int r = 0; r < h; r++)
            for (int c = half_w; c < w; c++)
                f[r][c] = f[r][w - 1 - c];

        for (int it = 0; it < iterations; it++)
        {
            Field next = f;
            for (int r = 0; r < h; r++)
                for (int c = 0; c < w; c++)
                {
                    int count = 0;
                    for (int dr = -1; dr <= 1; dr++)
                        for (int dc = -1; dc <= 1; dc++)
                        {
                            if (dr == 0 && dc == 0)
                                continue;
                            const int rr = r + dr;
                            const int cc = c + dc;
                            if (rr < 0 || rr >= h || cc < 0 || cc >= w)
                                continue;
                            if (f[rr][cc])
                                count++;
                        }
                    next[r][c] = count >= rule_threshold;
                }
            f = next;
        }
        return f;
    }

    void renderField(const Field &f, Grid &grid, const Mask &forced, char ch)
    {
        for (size_t r = 0; r < grid.size(); r++)
            for (size_t c = 0; c < grid[r].size(); c++)
                if (f[r][c] && !forced[r][c] && grid[r][c] == '.')
                    grid[r][c] = ch;
    }

    bool inEagleArea(int r, int c, int w, int h)
    {
        return c >= w / 2 - 1 && c < w / 2 + 1 && r >= h - 2;
    }

    bool passableForPath(char ch)
    {
        // Bricks are destructible, so tanks can always shoot their way through.
        return ch == '.' || ch == '%' || ch == '-' || ch == '#';
    }

    bool passableForTank(char ch)
    {
        return ch == '.' || ch == '%' || ch == '-';
    }

    bool validate(const Grid &grid, bool with_eagle)
    {
        const int w = (int)grid[0].size();
        const int h = (int)grid.size();
        const int tw = AppConfig::tile_size.w;
        const int th = AppConfig::tile_size.h;
        const Cell start = {AppConfig::player_starting_point[0].y / th, AppConfig::player_starting_point[0].x / tw};
        const Cell player2 = {AppConfig::player_starting_point[1].y / th, AppConfig::player_starting_point[1].x / tw};

        const int dr[4] = {1, -1, 0, 0};
        const int dc[4] = {0, 0, 1, -1};

        auto openForPath = [&](int r, int c)
        {
            if (r < 0 || r >= h || c < 0 || c >= w)
                return false;
            if (with_eagle && inEagleArea(r, c, w, h))
                return false;
            return passableForPath(grid[r][c]);
        };

        // Flood fill from the player spawn through everything destructible.
        if (!openForPath(start.r, start.c))
            return false;

        std::vector<std::vector<bool>> vis(h, std::vector<bool>(w, false));
        std::queue<Cell> q;
        vis[start.r][start.c] = true;
        q.push(start);
        while (!q.empty())
        {
            const Cell cur = q.front();
            q.pop();
            for (int i = 0; i < 4; i++)
            {
                const int r = cur.r + dr[i];
                const int c = cur.c + dc[i];
                if (openForPath(r, c) && !vis[r][c])
                {
                    vis[r][c] = true;
                    q.push({r, c});
                }
            }
        }

        // Every enemy spawn and the second player spawn must be reachable.
        for (const auto &point : AppConfig::enemy_starting_point)
        {
            const Cell e = {point.y / th, point.x / tw};
            if (!vis[e.r][e.c])
                return false;
        }
        if (!vis[player2.r][player2.c])
            return false;

        // The eagle must be reachable through destructible blocks.
        if (with_eagle)
        {
            const int eagle_c = w / 2 - 1;
            const int eagle_r = h - 2;
            bool eagle_reachable = false;
            for (int c = eagle_c - 1; c <= eagle_c + 2 && !eagle_reachable; c++)
                if (c >= 0 && c < w && vis[eagle_r - 1][c])
                    eagle_reachable = true;
            for (int r = eagle_r; r < h && !eagle_reachable; r++)
            {
                if (eagle_c - 1 >= 0 && vis[r][eagle_c - 1])
                    eagle_reachable = true;
                if (eagle_c + 2 < w && vis[r][eagle_c + 2])
                    eagle_reachable = true;
            }
            if (!eagle_reachable)
                return false;
        }

        // The player must have enough open space to actually drive around.
        if (!passableForTank(grid[start.r][start.c]))
            return false;

        int open_cells = 0;
        std::vector<std::vector<bool>> tank_vis(h, std::vector<bool>(w, false));
        std::queue<Cell> tq;
        tank_vis[start.r][start.c] = true;
        tq.push(start);
        while (!tq.empty())
        {
            const Cell cur = tq.front();
            tq.pop();
            open_cells++;
            for (int i = 0; i < 4; i++)
            {
                const int r = cur.r + dr[i];
                const int c = cur.c + dc[i];
                if (r >= 0 && r < h && c >= 0 && c < w && !tank_vis[r][c] && passableForTank(grid[r][c]))
                {
                    tank_vis[r][c] = true;
                    tq.push({r, c});
                }
            }
        }

        // The drivable region must touch at least one destructible brick,
        // otherwise the player could be sealed in for good by stone or water.
        bool escapable = false;
        for (int r = 0; r < h && !escapable; r++)
            for (int c = 0; c < w && !escapable; c++)
            {
                if (!tank_vis[r][c])
                    continue;
                for (int i = 0; i < 4 && !escapable; i++)
                {
                    const int rr = r + dr[i];
                    const int cc = c + dc[i];
                    if (rr >= 0 && rr < h && cc >= 0 && cc < w && grid[rr][cc] == '#')
                        escapable = true;
                }
            }
        if (!escapable)
            return false;

        return open_cells >= 20;
    }

    std::vector<std::string> fallbackStage(bool with_water_stone)
    {
        // Known good symmetric layout (classic stage 1) used if generation
        // somehow fails to produce a valid map.
        std::vector<std::string> lines = {
            "..........................",
            "..........................",
            "..##..##..##..##..##..##..",
            "..##..##..##..##..##..##..",
            "..##..##..##..##..##..##..",
            "..##..##..##..##..##..##..",
            "..##..##..##@@##..##..##..",
            "..##..##..##@@##..##..##..",
            "..##..##..##..##..##..##..",
            "..##..##..........##..##..",
            "..##..##..........##..##..",
            "..........##..##..........",
            "..........##..##..........",
            "##..####..........####..##",
            "@@..####..........####..@@",
            "..........##..##..........",
            "..........######..........",
            "..##..##..######..##..##..",
            "..##..##..##..##..##..##..",
            "..##..##..##..##..##..##..",
            "..##..##..##..##..##..##..",
            "..##..##..........##..##..",
            "..##..##..........##..##..",
            "..##..##...####...##..##..",
            "...........#..#...........",
            "...........#..#..........."};

        if (!with_water_stone)
            for (auto &line : lines)
                for (char &ch : line)
                    if (ch == '@')
                        ch = '#';

        return lines;
    }
}

std::vector<std::string> StageGenerator::generate(int stage_number, bool with_eagle, bool with_water_stone)
{
    const int w = AppConfig::map_size.w;
    const int h = AppConfig::map_size.h;

    std::mt19937 rng(sessionSeed() + static_cast<unsigned>(stage_number) * 2654435761u);

    for (int attempt = 0; attempt < 40; attempt++)
    {
        Grid grid(h, std::vector<char>(w, '.'));
        Mask forced(h, std::vector<bool>(w, false));

        applyProtectedAreas(grid, forced, with_eagle);

        const int brick_rects = 10 + ri(rng, 0, 8);
        for (int i = 0; i < brick_rects; i++)
            placeRect(rng, grid, forced, '#', false);

        if (with_water_stone)
        {
            const int stone_rects = 1 + ri(rng, 0, 1);
            for (int i = 0; i < stone_rects; i++)
                placeRect(rng, grid, forced, '@', false);
            // At most 16 stone cells on the left half (32 mirrored ~= 5% of
            // the map); the excess is downgraded to brick.
            capStoneCells(rng, grid, 16);
        }

        if (ri(rng, 0, 2) == 0)
            placeRect(rng, grid, forced, '-', true);

        mirrorLeftHalf(grid, forced);

        if (with_water_stone)
        {
            // Water is capped so a map can never become a swamp: at most
            // 36 cells on the left half (72 mirrored ~= 11% of the map).
            Field water = buildField(rng, 0.25, 2, 4);
            capField(rng, water, 36);
            renderField(water, grid, forced, '~');
        }

        Field bush = buildField(rng, 0.32, 2, 4);
        renderField(bush, grid, forced, '%');

        if (validate(grid, with_eagle))
        {
            std::vector<std::string> lines;
            lines.reserve(h);
            for (int r = 0; r < h; r++)
                lines.emplace_back(grid[r].begin(), grid[r].end());
            return lines;
        }
    }

    return fallbackStage(with_water_stone);
}
