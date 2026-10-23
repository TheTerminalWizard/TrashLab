#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <iomanip>
#include <filesystem>
#include <cmath>
#include <cstring>
#include <algorithm>

const int WIDTH  = 48;
const int HEIGHT = 48;

// ----------------------------------------------------------------------
// Simple RGB structure
struct RGB { uint8_t r, g, b; };

// Map colour name to RGB
RGB colour_to_rgb(const std::string& name) {
    if (name == "black")  return {0,   0,   0};
    if (name == "white")  return {255, 255, 255};
    if (name == "red")    return {255, 0,   0};
    if (name == "green")  return {0,   255, 0};
    if (name == "blue")   return {0,   0,   255};
    if (name == "yellow") return {255, 255, 0};
    return {0, 0, 0};
}

// ----------------------------------------------------------------------
// 44×44 binary mask type
using Mask44 = bool[44][44];

// ----------------------------------------------------------------------
// Point with floating coordinates for polygon generation
struct PointF { double x, y; };

// Generate vertices for a regular N‑gon
static void polygon_vertices(int N, PointF* verts, int& count) {
    count = N;
    const double cx = 21.5, cy = 21.5, r = 22.0;
    for (int i = 0; i < N; ++i) {
        double angle = 2.0 * M_PI * i / N - M_PI_2;  // start from top
        verts[i] = {cx + r * std::cos(angle), cy + r * std::sin(angle)};
    }
}

// Generate vertices for a star with N points (inner radius = 0.4 * outer)
static void star_vertices(int N, PointF* verts, int& count) {
    count = 2 * N;
    const double cx = 21.5, cy = 21.5;
    const double R = 22.0, r = 0.4 * R;
    for (int i = 0; i < N; ++i) {
        double angle_outer = 2.0 * M_PI * i / N - M_PI_2;
        double angle_inner = 2.0 * M_PI * (i + 0.5) / N - M_PI_2;
        verts[2 * i    ] = {cx + R * std::cos(angle_outer), cy + R * std::sin(angle_outer)};
        verts[2 * i + 1] = {cx + r * std::cos(angle_inner), cy + r * std::sin(angle_inner)};
    }
}

// Generate vertices for a spike (N sharp triangles, inner radius = 0)
static void spike_vertices(int N, PointF* verts, int& count) {
    count = 2 * N;
    const double cx = 21.5, cy = 21.5;
    const double R = 22.0;
    for (int i = 0; i < N; ++i) {
        double angle = 2.0 * M_PI * i / N - M_PI_2;
        verts[2 * i    ] = {cx + R * std::cos(angle), cy + R * std::sin(angle)};
        verts[2 * i + 1] = {cx, cy};   // centre point
    }
}

// Point‑in‑polygon (ray casting) – works for arbitrary simple polygon
static bool point_in_polygon(const PointF* verts, int n, double px, double py) {
    bool inside = false;
    for (int i = 0, j = n - 1; i < n; j = i++) {
        double xi = verts[i].x, yi = verts[i].y;
        double xj = verts[j].x, yj = verts[j].y;
        if (((yi > py) != (yj > py)) &&
            (px < (xj - xi) * (py - yi) / (yj - yi) + xi))
            inside = !inside;
    }
    return inside;
}

// ----------------------------------------------------------------------
// Fill a 44×44 mask according to the shape and its parameter
// Supported shapes: circle, square, triangle, heart,
// polygon N, star N, ellipse ratio, rounded_square r, spike N
static void generate_mask(const std::string& shape, float param, Mask44& mask) {
    // Clear mask
    for (int i = 0; i < 44; ++i)
        for (int j = 0; j < 44; ++j)
            mask[i][j] = false;

    if (shape == "circle") {
        const double cx = 21.5, cy = 21.5, r2 = 22.0 * 22.0;
        for (int i = 0; i < 44; ++i)
            for (int j = 0; j < 44; ++j) {
                double dx = j - cx, dy = i - cy;
                mask[i][j] = (dx*dx + dy*dy <= r2);
            }
    }
    else if (shape == "square") {
        for (int i = 0; i < 44; ++i)
            for (int j = 0; j < 44; ++j)
                mask[i][j] = true;
    }
    else if (shape == "triangle") {
        // Isosceles triangle (44×44) pointing up
        for (int i = 0; i < 44; ++i) {
            for (int j = 0; j < 44; ++j) {
                // vertices (0,43) bottom-left, (43,43) bottom-right, (21.5,0) top
                double x = j, y = i;
                double x1 = 21.5, y1 = 0.0;
                double x2 = 0.0,  y2 = 43.0;
                double x3 = 43.0, y3 = 43.0;
                double denom = (y2 - y3)*(x1 - x3) + (x3 - x2)*(y1 - y3);
                double a = ((y2 - y3)*(x - x3) + (x3 - x2)*(y - y3)) / denom;
                double b = ((y3 - y1)*(x - x3) + (x1 - x3)*(y - y3)) / denom;
                double c = 1.0 - a - b;
                mask[i][j] = (a >= 0 && b >= 0 && c >= 0);
            }
        }
    }
    else if (shape == "heart") {
        // Parametric heart: (x² + y² - 1)³ - x² y³ <= 0, scaled to 44
        for (int i = 0; i < 44; ++i) {
            for (int j = 0; j < 44; ++j) {
                double fx = (j - 21.5) / 21.5;
                double fy = -(i - 21.5) / 21.5;  // flip vertically
                double val = (fx*fx + fy*fy - 1.0);
                val = val * val * val - fx*fx * fy*fy*fy;
                mask[i][j] = (val <= 0.0);
            }
        }
    }
    else if (shape == "polygon") {
        int N = static_cast<int>(param);
        if (N < 3) N = 3;
        PointF verts[100];
        int count = 0;
        polygon_vertices(N, verts, count);
        for (int i = 0; i < 44; ++i)
            for (int j = 0; j < 44; ++j)
                mask[i][j] = point_in_polygon(verts, count, j + 0.0, i + 0.0);
    }
    else if (shape == "star") {
        int N = static_cast<int>(param);
        if (N < 2) N = 2;
        PointF verts[200];
        int count = 0;
        star_vertices(N, verts, count);
        for (int i = 0; i < 44; ++i)
            for (int j = 0; j < 44; ++j)
                mask[i][j] = point_in_polygon(verts, count, j + 0.0, i + 0.0);
    }
    else if (shape == "ellipse") {
        double ratio = param;   // 0.0 .. 1.0 typically
        double a2 = 22.0 * 22.0;
        double b2 = (22.0 * ratio) * (22.0 * ratio);
        for (int i = 0; i < 44; ++i)
            for (int j = 0; j < 44; ++j) {
                double dx = j - 21.5, dy = i - 21.5;
                mask[i][j] = ( (dx*dx)/a2 + (dy*dy)/b2 <= 1.0 );
            }
    }
    else if (shape == "rounded_square") {
        double r = param;   // 0..1 fraction of half-size
        double half = 22.0, corner_rad = r * half;
        for (int i = 0; i < 44; ++i)
            for (int j = 0; j < 44; ++j) {
                double dx = std::abs(j - 21.5);
                double dy = std::abs(i - 21.5);
                if (dx <= half - corner_rad && dy <= half - corner_rad)
                    mask[i][j] = true;
                else if (dx > half - corner_rad && dy > half - corner_rad) {
                    double cx = dx - (half - corner_rad);
                    double cy = dy - (half - corner_rad);
                    mask[i][j] = (cx*cx + cy*cy <= corner_rad*corner_rad);
                }
                else
                    mask[i][j] = true;   // straight side
            }
    }
    else if (shape == "spike") {
        int N = static_cast<int>(param);
        if (N < 2) N = 2;
        PointF verts[200];
        int count = 0;
        spike_vertices(N, verts, count);
        for (int i = 0; i < 44; ++i)
            for (int j = 0; j < 44; ++j)
                mask[i][j] = point_in_polygon(verts, count, j + 0.0, i + 0.0);
    }
    // Unknown shape → fallback to circle
    else {
        generate_mask("circle", 0.0f, mask);
    }
}

// ----------------------------------------------------------------------
// Draw a shape (given by its 44×44 mask) onto a 48×48 RGB buffer
// Shape is placed at x=2..45, y=2..45
static void draw_shape(const Mask44& mask, const RGB& colour,
                       uint8_t framebuffer[HEIGHT][WIDTH][3]) {
    // clear to black
    for (int y = 0; y < HEIGHT; ++y)
        for (int x = 0; x < WIDTH; ++x)
            framebuffer[y][x][0] = framebuffer[y][x][1] = framebuffer[y][x][2] = 0;

    for (int y = 2; y <= 45; ++y)
        for (int x = 2; x <= 45; ++x)
            if (mask[y-2][x-2]) {
                framebuffer[y][x][0] = colour.r;
                framebuffer[y][x][1] = colour.g;
                framebuffer[y][x][2] = colour.b;
            }
}

// ----------------------------------------------------------------------
// Parse a shape specification from a token list.
// Returns the shape name (base) and fills param if parametric.
// Advances the token index accordingly.
static bool parse_shape_token(const std::string& token, std::string& shape, float& param, bool& has_param) {
    static const std::string parametric[] = {"polygon", "star", "ellipse", "rounded_square", "spike"};
    shape = token;
    has_param = false;
    for (auto& s : parametric) {
        if (shape == s) {
            has_param = true;
            break;
        }
    }
    return true;
}

// ----------------------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0]
                  << " \"<shape> <colour1> [<colour2> ...] <duration>\"\n"
                  << "   or: \"<shape1> <colour1> to <shape2> <colour2> <duration>\"\n";
        return 1;
    }

    std::string cmd(argv[1]);
    std::istringstream iss(cmd);
    std::vector<std::string> tokens;
    {
        std::string token;
        while (iss >> token) tokens.push_back(token);
    }
    if (tokens.empty()) return 1;

    // Detect "to" keyword
    size_t to_pos = std::string::npos;
    for (size_t i = 0; i < tokens.size(); ++i)
        if (tokens[i] == "to") { to_pos = i; break; }

    // ------------------------------------------------------------------
    // Case 1: two different shapes (with "to")
    if (to_pos != std::string::npos) {
        // expected: tokens[0..k-1] shape1 colour1, tokens[k]="to", tokens[k+1..m-2] shape2 colour2, tokens[m-1] duration
        if (to_pos < 2 || to_pos > tokens.size() - 3) {
            std::cerr << "Invalid 'to' command format.\n";
            return 1;
        }

        // Parse shape1 (first token(s) before colour1)
        size_t idx = 0;
        std::string shape1_name;
        float shape1_param = 0;
        bool shape1_has_param;
        // The colour1 is the last token before "to"
        std::string colour1_name = tokens[to_pos - 1];
        // Shape1 consists of tokens from 0 to to_pos-2
        // Actually shape name + optional parameter, then colour.
        // We'll assume shape1 is exactly one token (name) or two tokens (name + param)
        // So check if to_pos-2 is a number -> then shape1 has parameter.
        if (to_pos >= 3 && (isdigit(tokens[to_pos-2][0]) || tokens[to_pos-2][0] == '.' || tokens[to_pos-2][0] == '-')) {
            // parametric: tokens[to_pos-3] name, tokens[to_pos-2] param, tokens[to_pos-1] colour
            shape1_name = tokens[to_pos-3];
            shape1_param = std::stof(tokens[to_pos-2]);
            shape1_has_param = true;
        } else {
            // non-parametric: tokens[to_pos-2] name, tokens[to_pos-1] colour
            shape1_name = tokens[to_pos-2];
            shape1_has_param = false;
        }

        // Parse shape2 (tokens after "to" until colour2, then duration)
        size_t idx2 = to_pos + 1;
        std::string shape2_name;
        float shape2_param = 0;
        bool shape2_has_param;
        // colour2 is the second last token (before duration)
        std::string colour2_name = tokens[tokens.size() - 2];
        // shape2 may be one or two tokens before colour2
        if (tokens.size() - to_pos >= 4 && (isdigit(tokens[tokens.size()-3][0]) || tokens[tokens.size()-3][0] == '.')) {
            shape2_name = tokens[tokens.size()-4];
            shape2_param = std::stof(tokens[tokens.size()-3]);
            shape2_has_param = true;
        } else {
            shape2_name = tokens[tokens.size()-3];
            shape2_has_param = false;
        }

        // Duration
        std::string dur_token = tokens.back();
        if (dur_token.empty() || dur_token.back() != 's') {
            std::cerr << "Duration must end with 's'.\n";
            return 1;
        }
        int seconds = std::stoi(dur_token.substr(0, dur_token.size()-1));
        if (seconds <= 0) {
            std::cerr << "Duration must be positive.\n";
            return 1;
        }

        RGB c1 = colour_to_rgb(colour1_name);
        RGB c2 = colour_to_rgb(colour2_name);

        int total_frames = seconds * 10;   // 10 fps
        if (total_frames < 1) total_frames = 1;

        // Generate masks for both shapes (cached statically)
        static Mask44 mask1, mask2;
        static std::string cached_shape1, cached_shape2;
        static float cached_param1, cached_param2;
        if (shape1_name != cached_shape1 || shape1_param != cached_param1) {
            generate_mask(shape1_name, shape1_has_param ? shape1_param : 0.0f, mask1);
            cached_shape1 = shape1_name;
            cached_param1 = shape1_param;
        }
        if (shape2_name != cached_shape2 || shape2_param != cached_param2) {
            generate_mask(shape2_name, shape2_has_param ? shape2_param : 0.0f, mask2);
            cached_shape2 = shape2_name;
            cached_param2 = shape2_param;
        }

        // Create output directory
        try { std::filesystem::create_directory("video_output"); }
        catch (...) { system("mkdir -p video_output"); }

        // Buffers
        uint8_t img1[HEIGHT][WIDTH][3];
        uint8_t img2[HEIGHT][WIDTH][3];
        uint8_t framebuffer[HEIGHT][WIDTH][3];

        for (int f = 0; f < total_frames; ++f) {
            float t = (total_frames == 1) ? 0.0f : static_cast<float>(f) / (total_frames - 1);

            draw_shape(mask1, c1, img1);
            draw_shape(mask2, c2, img2);

            // Blend images
            for (int y = 0; y < HEIGHT; ++y)
                for (int x = 0; x < WIDTH; ++x)
                    for (int c = 0; c < 3; ++c) {
                        float v = (1.0f - t) * img1[y][x][c] + t * img2[y][x][c];
                        framebuffer[y][x][c] = static_cast<uint8_t>(v + 0.5f);
                    }

            // Write PPM
            std::ostringstream fname;
            fname << "video_output/frame" << std::setfill('0') << std::setw(3) << f << ".ppm";
            std::ofstream out(fname.str(), std::ios::binary);
            if (!out) { std::cerr << "Cannot write " << fname.str() << "\n"; return 1; }
            out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
            for (int y = 0; y < HEIGHT; ++y)
                for (int x = 0; x < WIDTH; ++x)
                    for (int c = 0; c < 3; ++c)
                        out.put(static_cast<char>(framebuffer[y][x][c]));
            out.close();
        }
        std::cout << "Saved " << total_frames << " frames in video_output/\n";
    }
    // ------------------------------------------------------------------
    // Case 2: single shape, multiple colours (no "to")
    else {
        // tokens: shape (maybe + param) then colour1 colour2 ... colourN duration
        if (tokens.size() < 4) {   // at least shape, colour, duration (if only one colour, still need 3)
            std::cerr << "Not enough arguments.\n";
            return 1;
        }

        // Parse shape (first one or two tokens)
        size_t idx = 0;
        std::string shape_name = tokens[idx++];
        float shape_param = 0;
        bool has_param = false;
        static const std::string param_shapes[] = {"polygon", "star", "ellipse", "rounded_square", "spike"};
        for (auto& s : param_shapes) {
            if (shape_name == s) {
                has_param = true;
                if (idx < tokens.size()) {
                    shape_param = std::stof(tokens[idx++]);
                }
                break;
            }
        }

        // Collect colours: tokens from idx until the last token (duration)
        std::vector<RGB> colour_list;
        while (idx < tokens.size() - 1) {
            colour_list.push_back(colour_to_rgb(tokens[idx++]));
        }
        if (colour_list.empty()) {
            std::cerr << "At least one colour required.\n";
            return 1;
        }

        // Duration
        std::string dur_token = tokens.back();
        if (dur_token.empty() || dur_token.back() != 's') {
            std::cerr << "Duration must end with 's'.\n";
            return 1;
        }
        int seconds = std::stoi(dur_token.substr(0, dur_token.size()-1));
        if (seconds <= 0) {
            std::cerr << "Duration must be positive.\n";
            return 1;
        }

        int total_frames = seconds * 10;
        if (total_frames < 1) total_frames = 1;
        int N = colour_list.size();

        // Generate mask for the shape (cached)
        static Mask44 shape_mask;
        static std::string cached_shape;
        static float cached_param;
        if (shape_name != cached_shape || shape_param != cached_param) {
            generate_mask(shape_name, has_param ? shape_param : 0.0f, shape_mask);
            cached_shape = shape_name;
            cached_param = shape_param;
        }

        // Create output folder
        try { std::filesystem::create_directory("video_output"); }
        catch (...) { system("mkdir -p video_output"); }

        uint8_t framebuffer[HEIGHT][WIDTH][3];

        for (int f = 0; f < total_frames; ++f) {
            float global_t = (total_frames == 1) ? 0.0f : static_cast<float>(f) / (total_frames - 1);
            // Colour interpolation
            RGB cur_colour;
            if (N == 1) {
                cur_colour = colour_list[0];
            } else {
                float pos = global_t * (N - 1);
                int seg = static_cast<int>(pos);
                if (seg >= N - 1) seg = N - 2;
                float local_t = pos - seg;
                if (local_t < 0) local_t = 0;
                if (local_t > 1) local_t = 1;
                cur_colour.r = static_cast<uint8_t>((1.0f - local_t) * colour_list[seg].r + local_t * colour_list[seg+1].r + 0.5f);
                cur_colour.g = static_cast<uint8_t>((1.0f - local_t) * colour_list[seg].g + local_t * colour_list[seg+1].g + 0.5f);
                cur_colour.b = static_cast<uint8_t>((1.0f - local_t) * colour_list[seg].b + local_t * colour_list[seg+1].b + 0.5f);
            }

            // Draw shape with current colour
            draw_shape(shape_mask, cur_colour, framebuffer);

            // Save PPM
            std::ostringstream fname;
            fname << "video_output/frame" << std::setfill('0') << std::setw(3) << f << ".ppm";
            std::ofstream out(fname.str(), std::ios::binary);
            if (!out) { std::cerr << "Cannot write " << fname.str() << "\n"; return 1; }
            out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
            for (int y = 0; y < HEIGHT; ++y)
                for (int x = 0; x < WIDTH; ++x)
                    for (int c = 0; c < 3; ++c)
                        out.put(static_cast<char>(framebuffer[y][x][c]));
            out.close();
        }
        std::cout << "Saved " << total_frames << " frames in video_output/\n";
    }

    return 0;
}

/*
  Example commands:
  ./videogen "polygon 6 yellow to star 8 red 5s"
  ./videogen "ellipse 0.6 white to circle blue 3s"
  ./videogen "rounded_square 0.4 red green blue yellow 6s"

  Convert frames to GIF:
  ffmpeg -framerate 10 -i video_output/frame%03d.ppm -c:v gif output.gif
*/