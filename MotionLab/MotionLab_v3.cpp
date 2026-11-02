// videogen_advanced.cpp - Multiple effects, any aspect ratio, flexible colours
// Compile: g++ -std=c++17 -O2 videogen_advanced.cpp -o videogen
//      or: clang++ -std=c++17 -O2 videogen_advanced.cpp -o videogen
//
// NEW in this version
//   * Aspect ratio:  --ratio 9:16 | 3:4 | 1:1 | 9:20 | 16:9 | ...   (also 9x16, 9/16)
//                    combine with --size N (long side, default 48), or --width / --height
//   * Colours:       names (Red, blue, Yellow ...), hex (#FFFF00, #0FD, #FF000080, FFFFFF)
//                    or "r,g,b" (255,128,0). Case-insensitive.
//   * Background:    --bg <colour>   (default black). Colour alpha is blended over it.
//   * Shapes now scale to the canvas, so they fit any ratio.

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <iomanip>
#include <filesystem>
#include <random>
#include <map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_PI_2
#define M_PI_2 1.57079632679489661923
#endif

// Canvas size (resolved in main from --ratio / --size / --width / --height)
int WIDTH  = 48;
int HEIGHT = 48;

struct RGB  { uint8_t r, g, b; };
struct RGBA { uint8_t r, g, b, a; };

RGB BG = {0, 0, 0};   // background colour (--bg)

// ----------------------------------------------------------------------
// Colour parsing
static std::string to_lower(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Accepts: names, #RGB, #RGBA, #RRGGBB, #RRGGBBAA (with or without '#', or 0x), "r,g,b"
static bool parse_color(const std::string& input, RGBA& out) {
    static const std::map<std::string, RGBA> names = {
        {"black",{0,0,0,255}},     {"white",{255,255,255,255}}, {"red",{255,0,0,255}},
        {"green",{0,255,0,255}},   {"blue",{0,0,255,255}},      {"yellow",{255,255,0,255}},
        {"orange",{255,165,0,255}},{"purple",{128,0,128,255}},  {"cyan",{0,255,255,255}},
        {"magenta",{255,0,255,255}},{"gray",{128,128,128,255}},{"grey",{128,128,128,255}},
        {"pink",{255,192,203,255}},{"brown",{139,69,19,255}},   {"lime",{50,205,50,255}},
        {"navy",{0,0,128,255}},    {"teal",{0,128,128,255}},    {"gold",{255,215,0,255}},
        {"silver",{192,192,192,255}},{"maroon",{128,0,0,255}}
    };
    std::string s = to_lower(input);
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c){ return std::isspace(c); }), s.end());
    if (s.empty()) return false;

    auto it = names.find(s);
    if (it != names.end()) { out = it->second; return true; }

    // "r,g,b"
    if (s.find(',') != std::string::npos) {
        int r, g, b; char extra;
        if (std::sscanf(s.c_str(), "%d,%d,%d%c", &r, &g, &b, &extra) == 3 &&
            r >= 0 && r <= 255 && g >= 0 && g <= 255 && b >= 0 && b <= 255) {
            out = {(uint8_t)r, (uint8_t)g, (uint8_t)b, 255};
            return true;
        }
        return false;
    }

    // hex
    if (s[0] == '#') s.erase(0, 1);
    else if (s.size() > 2 && s[0] == '0' && s[1] == 'x') s.erase(0, 2);
    for (char c : s) if (hex_val(c) < 0) return false;

    int v[8];
    size_t n = s.size();
    if (n == 3 || n == 4) {                       // short form: each digit doubled
        for (size_t i = 0; i < n; ++i) v[i] = hex_val(s[i]) * 17;
        out = {(uint8_t)v[0], (uint8_t)v[1], (uint8_t)v[2], (uint8_t)(n == 4 ? v[3] : 255)};
        return true;
    }
    if (n == 6 || n == 8) {
        for (size_t i = 0; i < n / 2; ++i) v[i] = hex_val(s[2*i]) * 16 + hex_val(s[2*i+1]);
        out = {(uint8_t)v[0], (uint8_t)v[1], (uint8_t)v[2], (uint8_t)(n == 8 ? v[3] : 255)};
        return true;
    }
    return false;
}

// Blend a colour with alpha over the background
static RGB flatten(const RGBA& c, const RGB& bg) {
    float a = c.a / 255.0f;
    return {
        static_cast<uint8_t>(c.r * a + bg.r * (1 - a) + 0.5f),
        static_cast<uint8_t>(c.g * a + bg.g * (1 - a) + 0.5f),
        static_cast<uint8_t>(c.b * a + bg.b * (1 - a) + 0.5f)
    };
}

RGB lerp_rgb(const RGB& a, const RGB& b, float t) {
    return {
        static_cast<uint8_t>(a.r*(1-t) + b.r*t + 0.5f),
        static_cast<uint8_t>(a.g*(1-t) + b.g*t + 0.5f),
        static_cast<uint8_t>(a.b*(1-t) + b.b*t + 0.5f)
    };
}

// ----------------------------------------------------------------------
// Aspect-ratio parsing: "9:16", "9x16", "9/16", "1.85:1"
static bool parse_ratio(const std::string& s, double& rw, double& rh) {
    size_t p = s.find_first_of(":xX/");
    if (p == std::string::npos || p == 0 || p + 1 >= s.size()) return false;
    try {
        size_t i1 = 0, i2 = 0;
        std::string a = s.substr(0, p), b = s.substr(p + 1);
        rw = std::stod(a, &i1);
        rh = std::stod(b, &i2);
        if (i1 != a.size() || i2 != b.size()) return false;
    } catch (...) { return false; }
    return rw > 0 && rh > 0;
}

// ----------------------------------------------------------------------
// Square mask of any size M x M (replaces the fixed 44x44 mask)
struct Mask {
    int M = 44;
    std::vector<char> d;
    void init(int m) { M = m; d.assign(static_cast<size_t>(m) * m, 0); }
    bool at(int y, int x) const { return d[static_cast<size_t>(y) * M + x] != 0; }
    void set(int y, int x, bool v) { d[static_cast<size_t>(y) * M + x] = v ? 1 : 0; }
};

struct PointF { double x, y; };

static void polygon_vertices(int M, int N, std::vector<PointF>& v) {
    v.resize(N);
    const double c = (M - 1) / 2.0, r = M / 2.0;
    for (int i = 0; i < N; ++i) {
        double a = 2 * M_PI * i / N - M_PI_2;
        v[i] = {c + r * std::cos(a), c + r * std::sin(a)};
    }
}
static void star_vertices(int M, int N, std::vector<PointF>& v) {
    v.resize(2 * N);
    const double c = (M - 1) / 2.0, R = M / 2.0, r = 0.4 * R;
    for (int i = 0; i < N; ++i) {
        double a1 = 2 * M_PI * i / N - M_PI_2;
        double a2 = 2 * M_PI * (i + 0.5) / N - M_PI_2;
        v[2*i]   = {c + R * std::cos(a1), c + R * std::sin(a1)};
        v[2*i+1] = {c + r * std::cos(a2), c + r * std::sin(a2)};
    }
}
static void spike_vertices(int M, int N, std::vector<PointF>& v) {
    v.resize(2 * N);
    const double c = (M - 1) / 2.0, R = M / 2.0;
    for (int i = 0; i < N; ++i) {
        double a = 2 * M_PI * i / N - M_PI_2;
        v[2*i]   = {c + R * std::cos(a), c + R * std::sin(a)};
        v[2*i+1] = {c, c};
    }
}
static bool point_in_polygon(const std::vector<PointF>& v, double px, double py) {
    bool inside = false;
    int n = static_cast<int>(v.size());
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = v[i].x, yi = v[i].y, xj = v[j].x, yj = v[j].y;
        if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
            inside = !inside;
    }
    return inside;
}

// param <= 0 means "use a sensible default" for the parametric shapes
static void generate_mask(const std::string& shape, float param, int M, Mask& mask) {
    mask.init(M);
    const double c = (M - 1) / 2.0, half = M / 2.0;

    if (shape == "square") {
        for (int i = 0; i < M; ++i) for (int j = 0; j < M; ++j) mask.set(i, j, true);
    } else if (shape == "triangle") {
        double x1 = c, y1 = 0, x2 = 0, y2 = M - 1, x3 = M - 1, y3 = M - 1;
        double denom = (y2-y3)*(x1-x3) + (x3-x2)*(y1-y3);
        for (int i = 0; i < M; ++i) for (int j = 0; j < M; ++j) {
            double a = ((y2-y3)*(j-x3) + (x3-x2)*(i-y3)) / denom;
            double b = ((y3-y1)*(j-x3) + (x1-x3)*(i-y3)) / denom;
            mask.set(i, j, a >= 0 && b >= 0 && (1 - a - b) >= 0);
        }
    } else if (shape == "heart") {
        for (int i = 0; i < M; ++i) for (int j = 0; j < M; ++j) {
            double fx = (j - c) / c, fy = -(i - c) / c;
            double v = fx*fx + fy*fy - 1.0;
            v = v*v*v - fx*fx*fy*fy*fy;
            mask.set(i, j, v <= 0.0);
        }
    } else if (shape == "polygon") {
        int N = param > 0 ? std::max(3, (int)param) : 5;
        std::vector<PointF> v; polygon_vertices(M, N, v);
        for (int i = 0; i < M; ++i) for (int j = 0; j < M; ++j)
            mask.set(i, j, point_in_polygon(v, j, i));
    } else if (shape == "star") {
        int N = param > 0 ? std::max(2, (int)param) : 5;
        std::vector<PointF> v; star_vertices(M, N, v);
        for (int i = 0; i < M; ++i) for (int j = 0; j < M; ++j)
            mask.set(i, j, point_in_polygon(v, j, i));
    } else if (shape == "spike") {
        int N = param > 0 ? std::max(2, (int)param) : 8;
        std::vector<PointF> v; spike_vertices(M, N, v);
        for (int i = 0; i < M; ++i) for (int j = 0; j < M; ++j)
            mask.set(i, j, point_in_polygon(v, j, i));
    } else if (shape == "ellipse") {
        double r = param > 0 ? param : 0.6;
        double a2 = half * half, b2 = (half * r) * (half * r);
        for (int i = 0; i < M; ++i) for (int j = 0; j < M; ++j) {
            double dx = j - c, dy = i - c;
            mask.set(i, j, (dx*dx)/a2 + (dy*dy)/b2 <= 1.0);
        }
    } else if (shape == "rounded_square") {
        double r = param > 0 ? param : 0.3, cr = r * half;
        for (int i = 0; i < M; ++i) for (int j = 0; j < M; ++j) {
            double dx = std::abs(j - c), dy = std::abs(i - c);
            if (dx > half - cr && dy > half - cr) {
                double ex = dx - (half - cr), ey = dy - (half - cr);
                mask.set(i, j, ex*ex + ey*ey <= cr*cr);
            } else mask.set(i, j, true);
        }
    } else {   // "circle" and unknown shapes
        for (int i = 0; i < M; ++i) for (int j = 0; j < M; ++j) {
            double dx = j - c, dy = i - c;
            mask.set(i, j, dx*dx + dy*dy <= half*half);
        }
    }
}

// ----------------------------------------------------------------------
// Drawing helpers (flat RGB buffer)
static inline void put_px(uint8_t* fb, int w, int x, int y, const RGB& c) {
    int idx = (y * w + x) * 3;
    fb[idx] = c.r; fb[idx+1] = c.g; fb[idx+2] = c.b;
}

void clear_fb(uint8_t* fb, int w, int h) {
    for (int i = 0; i < w * h; ++i) { fb[i*3] = BG.r; fb[i*3+1] = BG.g; fb[i*3+2] = BG.b; }
}

// Mask centred on the canvas
void draw_shape_centered(const Mask& mask, const RGB& col, uint8_t* fb, int w, int h) {
    clear_fb(fb, w, h);
    int sx = (w - mask.M) / 2, sy = (h - mask.M) / 2;
    for (int y = 0; y < mask.M; ++y)
        for (int x = 0; x < mask.M; ++x)
            if (mask.at(y, x)) {
                int px = sx + x, py = sy + y;
                if (px >= 0 && px < w && py >= 0 && py < h) put_px(fb, w, px, py, col);
            }
}

void draw_circle(uint8_t* fb, int w, int h, int cx, int cy, int rad, const RGB& col) {
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int dx = x - cx, dy = y - cy;
            if (dx*dx + dy*dy <= rad*rad) put_px(fb, w, x, y, col);
        }
}

void draw_rotated_shape(const Mask& mask, const RGB& col, uint8_t* fb, int w, int h,
                        float cx, float cy, float angle_deg) {
    float rad = angle_deg * static_cast<float>(M_PI) / 180.0f;
    float ca = std::cos(rad), sa = std::sin(rad);
    float mc = (mask.M - 1) / 2.0f;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            float dx = x - cx, dy = y - cy;
            float sx =  dx * ca + dy * sa;
            float sy = -dx * sa + dy * ca;
            int mx = static_cast<int>(std::lround(sx + mc));
            int my = static_cast<int>(std::lround(sy + mc));
            if (mx >= 0 && mx < mask.M && my >= 0 && my < mask.M && mask.at(my, mx))
                put_px(fb, w, x, y, col);
        }
}

void draw_moving_shape(const Mask& mask, const RGB& col, uint8_t* fb, int w, int h,
                       int sx, int sy, int ex, int ey, float t) {
    clear_fb(fb, w, h);
    int cx = static_cast<int>(sx + (ex - sx) * t);
    int cy = static_cast<int>(sy + (ey - sy) * t);
    for (int y = 0; y < mask.M; ++y)
        for (int x = 0; x < mask.M; ++x)
            if (mask.at(y, x)) {
                int px = cx - mask.M / 2 + x, py = cy - mask.M / 2 + y;
                if (px >= 0 && px < w && py >= 0 && py < h) put_px(fb, w, px, py, col);
            }
}

// scale: 1.0 = full mask size
void draw_scaled_shape(const Mask& mask, const RGB& col, uint8_t* fb, int w, int h, float scale) {
    clear_fb(fb, w, h);
    int newSize = std::max(1, static_cast<int>(mask.M * scale));
    int sx = w / 2 - newSize / 2, sy = h / 2 - newSize / 2;
    for (int y = 0; y < newSize; ++y)
        for (int x = 0; x < newSize; ++x) {
            int ox = std::min(mask.M - 1, x * mask.M / newSize);
            int oy = std::min(mask.M - 1, y * mask.M / newSize);
            if (mask.at(oy, ox)) {
                int px = sx + x, py = sy + y;
                if (px >= 0 && px < w && py >= 0 && py < h) put_px(fb, w, px, py, col);
            }
        }
}

void apply_shake(uint8_t* fb, int w, int h, int amp, std::mt19937& rng) {
    std::vector<uint8_t> temp(fb, fb + w * h * 3);
    clear_fb(fb, w, h);
    int dx = std::uniform_int_distribution<int>(-amp, amp)(rng);
    int dy = std::uniform_int_distribution<int>(-amp, amp)(rng);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                int s = (ny * w + nx) * 3, d = (y * w + x) * 3;
                fb[d] = temp[s]; fb[d+1] = temp[s+1]; fb[d+2] = temp[s+2];
            }
        }
}

// ----------------------------------------------------------------------
// Fire effect (speeds scale with canvas height)
class FireEffect {
    struct Particle { float x, y, vx, vy, life; };
    std::vector<Particle> parts;
    std::mt19937 rng;
    float sc;
    void respawn(Particle& p, int w, int h, float lifeVal) {
        p.x = std::uniform_real_distribution<float>(0, static_cast<float>(w))(rng);
        p.y = h - std::uniform_real_distribution<float>(0, 15 * sc)(rng);
        p.vx = std::uniform_real_distribution<float>(-10 * sc, 10 * sc)(rng);
        p.vy = std::uniform_real_distribution<float>(-120 * sc, -60 * sc)(rng);   // fixed: min <= max
        p.life = lifeVal;
    }
public:
    FireEffect(int cnt, int w, int h) {
        rng.seed(123);
        sc = std::max(1.0f, h / 48.0f);
        parts.resize(cnt);
        for (auto& p : parts) {
            respawn(p, w, h, std::uniform_real_distribution<float>(0.6f, 1.2f)(rng));
            p.y = std::uniform_real_distribution<float>(h - 10 * sc, static_cast<float>(h))(rng);
        }
    }
    void update(float dt, int w, int h) {
        for (auto& p : parts) {
            p.x += p.vx * dt; p.y += p.vy * dt; p.life -= dt * 1.5f;
            if (p.life <= 0 || p.y < 0) respawn(p, w, h, 0.8f);
        }
    }
    void draw(uint8_t* fb, int w, int h) {
        for (auto& p : parts) {
            if (p.life <= 0) continue;
            int x = static_cast<int>(p.x), y = static_cast<int>(p.y);
            if (x >= 0 && x < w && y >= 0 && y < h) {
                uint8_t r = 200 + static_cast<uint8_t>(55 * (1.0f - p.life));
                uint8_t g = static_cast<uint8_t>(80 * (1.0f - p.life));
                put_px(fb, w, x, y, {r, g, 0});
            }
        }
    }
};

// ----------------------------------------------------------------------
// Explosion effect (speeds scale with canvas size)
class ExplosionEffect {
    struct Particle { float x, y, vx, vy, life; };
    std::vector<Particle> parts;
public:
    ExplosionEffect(int cx, int cy, int radius, int count, float duration, float sc) {
        std::mt19937 rng(123);
        parts.resize(count);
        float maxLife = std::max(duration, 0.3f);
        for (auto& p : parts) {
            float angle = std::uniform_real_distribution<float>(0, 2 * static_cast<float>(M_PI))(rng);
            float speed = std::uniform_real_distribution<float>(50 * sc, 150 * sc)(rng);
            p.x = cx + std::uniform_real_distribution<float>(-static_cast<float>(radius), static_cast<float>(radius))(rng);
            p.y = cy + std::uniform_real_distribution<float>(-static_cast<float>(radius), static_cast<float>(radius))(rng);
            p.vx = std::cos(angle) * speed;
            p.vy = std::sin(angle) * speed;
            p.life = std::uniform_real_distribution<float>(0.2f, maxLife)(rng);
        }
    }
    void update(float dt) {
        for (auto& p : parts) { p.x += p.vx * dt; p.y += p.vy * dt; p.life -= dt; }
    }
    void draw(uint8_t* fb, int w, int h) {
        for (auto& p : parts) {
            if (p.life <= 0) continue;
            int x = static_cast<int>(p.x), y = static_cast<int>(p.y);
            if (x >= 0 && x < w && y >= 0 && y < h)
                put_px(fb, w, x, y, {255, static_cast<uint8_t>(std::min(255.0f, 100 * p.life)), 0});
        }
    }
};

// ----------------------------------------------------------------------
RGB hsv_to_rgb(float h, float s = 1.0f, float v = 1.0f) {
    float r, g, b;
    int i = static_cast<int>(h * 6);
    float f = h * 6 - i, p = v * (1 - s), q = v * (1 - f * s), t = v * (1 - (1 - f) * s);
    switch (i % 6) {
        case 0: r=v; g=t; b=p; break;
        case 1: r=q; g=v; b=p; break;
        case 2: r=p; g=v; b=t; break;
        case 3: r=p; g=q; b=v; break;
        case 4: r=t; g=p; b=v; break;
        default: r=v; g=p; b=q; break;
    }
    return {static_cast<uint8_t>(r*255), static_cast<uint8_t>(g*255), static_cast<uint8_t>(b*255)};
}

static void print_usage(const char* prog) {
    std::cerr <<
"Usage: " << prog << " --effect <effect> [options] [key=value ...]\n\n"
"Canvas options:\n"
"  --ratio W:H     aspect ratio, e.g. 9:16  3:4  1:1  9:20  16:9  (9x16 and 9/16 also work)\n"
"  --size N        length of the LONG side in pixels when --ratio is used (default 48)\n"
"  --width W / --height H\n"
"                  explicit size. With --ratio, give just one of them and the other is computed.\n"
"  --bg <colour>   background colour (default black)\n\n"
"Colours (any colour argument, e.g. color=... start_color=... --bg ...):\n"
"  names : red blue green yellow black white orange purple cyan magenta gray pink ...\n"
"  hex   : #FFFF00  #0FD  #00FFD4  #FF000080 (last 2 digits = alpha)  FFFFFF\n"
"  rgb   : 255,128,0\n"
"  NOTE  : in a shell put hex values in quotes: color=\"#FF0000\"  --bg \"#000000\"\n\n"
"Effects: circle, fade, move, scale, rainbow, explosion, rotate, fire, shake, pulse\n"
"Common keys: duration=2  shape=<circle|square|triangle|heart|polygon|star|ellipse|rounded_square|spike>\n"
"             param=<N or ratio for polygon/star/spike/ellipse/rounded_square>\n"
"Example: " << prog << " --ratio 9:16 --size 128 --bg \"#101030\" --effect rotate shape=star color=\"#FFD400\"\n";
}

// ----------------------------------------------------------------------
int main(int argc, char* argv[]) {
    try {
        if (argc >= 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
            print_usage(argv[0]); return 0;
        }
        if (argc < 3) { print_usage(argv[0]); return 1; }

        std::string effect, ratioStr, bgStr = "black";
        int reqW = 0, reqH = 0, longSide = 48;
        std::map<std::string, std::string> params;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            auto next = [&](const char* name) -> std::string {
                if (i + 1 >= argc) { std::cerr << "Missing value for " << name << "\n"; std::exit(1); }
                return argv[++i];
            };
            if      (arg == "--width")  reqW = std::stoi(next("--width"));
            else if (arg == "--height") reqH = std::stoi(next("--height"));
            else if (arg == "--effect") effect = next("--effect");
            else if (arg == "--ratio")  ratioStr = next("--ratio");
            else if (arg == "--size")   longSide = std::stoi(next("--size"));
            else if (arg == "--bg")     bgStr = next("--bg");
            else {
                size_t eq = arg.find('=');
                if (eq != std::string::npos) params[arg.substr(0, eq)] = arg.substr(eq + 1);
            }
        }

        // ---- Background colour
        {
            RGBA bg;
            if (!parse_color(bgStr, bg)) { std::cerr << "Invalid --bg colour: " << bgStr << "\n"; return 1; }
            BG = {bg.r, bg.g, bg.b};   // background itself is treated as opaque
        }

        // ---- Resolve canvas size
        if (reqW < 0 || reqH < 0 || longSide <= 0) {
            std::cerr << "Width, height and size must be positive integers.\n"; return 1;
        }
        auto even = [](int v) { v = std::max(v, 2); return v + (v % 2); };   // even sizes keep video encoders happy
        if (!ratioStr.empty()) {
            double rw, rh;
            if (!parse_ratio(ratioStr, rw, rh)) {
                std::cerr << "Invalid ratio '" << ratioStr << "'. Use e.g. 9:16, 3:4, 1:1, 9:20\n"; return 1;
            }
            if (reqW > 0 && reqH > 0) {
                std::cerr << "Note: both --width and --height given, so --ratio is ignored.\n";
                WIDTH = reqW; HEIGHT = reqH;
            } else if (reqW > 0) { WIDTH = reqW; HEIGHT = even((int)std::lround(reqW * rh / rw)); }
            else if (reqH > 0)   { HEIGHT = reqH; WIDTH = even((int)std::lround(reqH * rw / rh)); }
            else if (rw >= rh)   { WIDTH = even(longSide); HEIGHT = even((int)std::lround(longSide * rh / rw)); }
            else                 { HEIGHT = even(longSide); WIDTH = even((int)std::lround(longSide * rw / rh)); }
        } else {
            if (reqW > 0) WIDTH = reqW;
            if (reqH > 0) HEIGHT = reqH;
        }
        if (WIDTH <= 0 || HEIGHT <= 0 || WIDTH > 16384 || HEIGHT > 16384) {
            std::cerr << "Canvas size out of range (1..16384).\n"; return 1;
        }

        // ---- Parameter helpers
        auto getInt = [&](const std::string& k, int def) {
            auto it = params.find(k); return it != params.end() ? std::stoi(it->second) : def; };
        auto getFloat = [&](const std::string& k, float def) {
            auto it = params.find(k); return it != params.end() ? std::stof(it->second) : def; };
        auto getString = [&](const std::string& k, const std::string& def) {
            auto it = params.find(k); return it != params.end() ? it->second : def; };
        auto getColor = [&](const std::string& k, const std::string& def) -> RGB {
            std::string v = getString(k, def);
            RGBA c;
            if (!parse_color(v, c)) {
                std::cerr << "Invalid colour for '" << k << "': " << v
                          << "\n(use a name like red, hex like \"#FF0000\", or r,g,b like 255,0,0)\n";
                std::exit(1);
            }
            return flatten(c, BG);
        };

        const int fps = 10;
        float duration = getFloat("duration", 2.0f);
        int totalFrames = std::max(1, static_cast<int>(duration * fps));

        // Everything scales with the short side of the canvas
        const int minSide = std::min(WIDTH, HEIGHT);
        const float sc = std::max(1.0f, minSide / 48.0f);
        const int S = std::max(8, static_cast<int>(minSide * 44.0 / 48.0 + 0.5));   // shape size (44 px on 48x48)
        const float shapeParam = getFloat("param", 0.0f);

        std::filesystem::create_directory("video_output");
        std::vector<uint8_t> fb(static_cast<size_t>(WIDTH) * HEIGHT * 3);
        std::mt19937 rng(123);

        auto save_frame = [&](int f) {
            std::ostringstream fname;
            fname << "video_output/frame" << std::setfill('0') << std::setw(3) << f << ".ppm";
            std::ofstream out(fname.str(), std::ios::binary);
            out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
            out.write(reinterpret_cast<const char*>(fb.data()), fb.size());
        };
        auto T = [&](int f) { return totalFrames == 1 ? 0.0f : static_cast<float>(f) / (totalFrames - 1); };

        // ---------- Effects ----------
        if (effect == "circle") {
            int radius = getInt("radius", std::max(1, S * 20 / 44));
            int cx = getInt("cx", WIDTH / 2), cy = getInt("cy", HEIGHT / 2);
            RGB start = getColor("start_color", "red"), end = getColor("end_color", "blue");
            for (int f = 0; f < totalFrames; ++f) {
                clear_fb(fb.data(), WIDTH, HEIGHT);
                draw_circle(fb.data(), WIDTH, HEIGHT, cx, cy, radius, lerp_rgb(start, end, T(f)));
                save_frame(f);
            }
        }
        else if (effect == "fade") {
            RGB from = getColor("from_color", "black"), to = getColor("to_color", "white");
            for (int f = 0; f < totalFrames; ++f) {
                RGB col = lerp_rgb(from, to, T(f));
                for (int i = 0; i < WIDTH * HEIGHT; ++i) { fb[i*3] = col.r; fb[i*3+1] = col.g; fb[i*3+2] = col.b; }
                save_frame(f);
            }
        }
        else if (effect == "move") {
            std::string shape = getString("shape", "square");
            RGB col = getColor("color", "red");
            int sx = 0, sy = 0, ex = WIDTH / 2, ey = HEIGHT / 2;
            std::sscanf(getString("start_pos", "0,0").c_str(), "%d,%d", &sx, &sy);
            std::sscanf(getString("end_pos", std::to_string(ex) + "," + std::to_string(ey)).c_str(), "%d,%d", &ex, &ey);
            Mask mask; generate_mask(shape, shapeParam, S, mask);
            for (int f = 0; f < totalFrames; ++f) {
                draw_moving_shape(mask, col, fb.data(), WIDTH, HEIGHT, sx, sy, ex, ey, T(f));
                save_frame(f);
            }
        }
        else if (effect == "scale") {
            std::string shape = getString("shape", "circle");
            RGB col = getColor("color", "blue");
            float minSize = getFloat("min_size", S * 10.0f / 44.0f);   // pixels
            float maxSize = getFloat("max_size", static_cast<float>(S));
            Mask mask; generate_mask(shape, shapeParam, S, mask);
            for (int f = 0; f < totalFrames; ++f) {
                float scale = (minSize + (maxSize - minSize) * T(f)) / S;
                draw_scaled_shape(mask, col, fb.data(), WIDTH, HEIGHT, scale);
                save_frame(f);
            }
        }
        else if (effect == "rainbow") {
            std::string shape = getString("shape", "heart");
            float speed = getFloat("speed", 1.0f);
            Mask mask; generate_mask(shape, shapeParam, S, mask);
            for (int f = 0; f < totalFrames; ++f) {
                float hue = std::fmod(static_cast<float>(f) / fps * speed, 1.0f);
                draw_shape_centered(mask, hsv_to_rgb(hue), fb.data(), WIDTH, HEIGHT);
                save_frame(f);
            }
        }
        else if (effect == "explosion") {
            int cx = getInt("cx", WIDTH / 2), cy = getInt("cy", HEIGHT / 2);
            int radius = getInt("radius", static_cast<int>(10 * sc));
            int count = getInt("particle_count", std::max(100, WIDTH * HEIGHT / 23));
            ExplosionEffect ex(cx, cy, radius, count, duration, sc);
            for (int f = 0; f < totalFrames; ++f) {
                clear_fb(fb.data(), WIDTH, HEIGHT);
                ex.update(1.0f / fps);
                ex.draw(fb.data(), WIDTH, HEIGHT);
                save_frame(f);
            }
        }
        else if (effect == "rotate") {
            std::string shape = getString("shape", "square");
            RGB col = getColor("color", "red");
            float speed = getFloat("speed", 360.0f);
            Mask mask; generate_mask(shape, shapeParam, S, mask);
            for (int f = 0; f < totalFrames; ++f) {
                clear_fb(fb.data(), WIDTH, HEIGHT);
                draw_rotated_shape(mask, col, fb.data(), WIDTH, HEIGHT,
                                   (WIDTH - 1) / 2.0f, (HEIGHT - 1) / 2.0f, speed * f / fps);
                save_frame(f);
            }
        }
        else if (effect == "fire") {
            int intensity = getInt("intensity", std::max(80, WIDTH * HEIGHT / 30));
            FireEffect fire(intensity, WIDTH, HEIGHT);
            for (int f = 0; f < totalFrames; ++f) {
                clear_fb(fb.data(), WIDTH, HEIGHT);
                fire.update(1.0f / fps, WIDTH, HEIGHT);
                fire.draw(fb.data(), WIDTH, HEIGHT);
                save_frame(f);
            }
        }
        else if (effect == "shake") {
            std::string shape = getString("shape", "square");
            RGB col = getColor("color", "red");
            int amplitude = std::max(0, getInt("amplitude", static_cast<int>(5 * sc)));
            Mask mask; generate_mask(shape, shapeParam, S, mask);
            for (int f = 0; f < totalFrames; ++f) {
                draw_shape_centered(mask, col, fb.data(), WIDTH, HEIGHT);
                apply_shake(fb.data(), WIDTH, HEIGHT, amplitude, rng);
                save_frame(f);
            }
        }
        else if (effect == "pulse") {
            std::string shape = getString("shape", "circle");
            RGB c1 = getColor("color1", "red"), c2 = getColor("color2", "blue");
            float period = std::max(0.01f, getFloat("period", 1.0f));
            Mask mask; generate_mask(shape, shapeParam, S, mask);
            for (int f = 0; f < totalFrames; ++f) {
                float phase = std::fmod(static_cast<float>(f) / fps, period) / period;
                float s = std::sin(phase * 2 * static_cast<float>(M_PI));
                float scale = 0.7f + 0.3f * (s + 1) / 2;
                draw_scaled_shape(mask, lerp_rgb(c1, c2, (s + 1) / 2), fb.data(), WIDTH, HEIGHT, scale);
                save_frame(f);
            }
        }
        else {
            std::cerr << "Unknown or missing effect: '" << effect << "'\n";
            print_usage(argv[0]);
            return 1;
        }

        std::cout << "Canvas " << WIDTH << "x" << HEIGHT << " - saved " << totalFrames << " frames in video_output/\n";
        std::cout << "ffmpeg -framerate " << fps << " -i video_output/frame%03d.ppm output.gif\n";
        std::cout << "ffmpeg -framerate " << fps << " -i video_output/frame%03d.ppm -pix_fmt yuv420p output.mp4\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << " (check numeric arguments)\n";
        return 1;
    }
}
