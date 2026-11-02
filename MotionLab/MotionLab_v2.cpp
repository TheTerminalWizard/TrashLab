// videogen_advanced.cpp - Multiple effects with dynamic dimensions
// Compile: clang++ -std=c++17 -O2 videogen_advanced.cpp -o videogen

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cmath>
#include <cstring>
#include <iomanip>
#include <filesystem>
#include <random>
#include <map>

// Default dimensions (may be overridden by --width and --height)
int WIDTH  = 48;
int HEIGHT = 48;

struct RGB { uint8_t r, g, b; };

RGB colour_to_rgb(const std::string& name) {
    if (name == "black")  return {0,0,0};
    if (name == "white")  return {255,255,255};
    if (name == "red")    return {255,0,0};
    if (name == "green")  return {0,255,0};
    if (name == "blue")   return {0,0,255};
    if (name == "yellow") return {255,255,0};
    if (name == "orange") return {255,165,0};
    if (name == "purple") return {128,0,128};
    if (name == "cyan")   return {0,255,255};
    if (name == "magenta")return {255,0,255};
    return {255,0,0};
}

RGB lerp_rgb(const RGB& a, const RGB& b, float t) {
    return {
        static_cast<uint8_t>(a.r*(1-t) + b.r*t + 0.5f),
        static_cast<uint8_t>(a.g*(1-t) + b.g*t + 0.5f),
        static_cast<uint8_t>(a.b*(1-t) + b.b*t + 0.5f)
    };
}

// ----------------------------------------------------------------------
// 44×44 mask helpers (unchanged, always 44×44)
using Mask44 = bool[44][44];
struct PointF { double x, y; };

static void polygon_vertices(int N, PointF* verts, int& count) {
    count = N;
    const double cx=21.5, cy=21.5, r=22.0;
    for(int i=0;i<N;++i) {
        double a=2*M_PI*i/N - M_PI_2;
        verts[i]={cx+r*cos(a), cy+r*sin(a)};
    }
}
static void star_vertices(int N, PointF* verts, int& count) {
    count=2*N;
    const double cx=21.5, cy=21.5, R=22.0, r=0.4*R;
    for(int i=0;i<N;++i) {
        double a1=2*M_PI*i/N - M_PI_2;
        double a2=2*M_PI*(i+0.5)/N - M_PI_2;
        verts[2*i]={cx+R*cos(a1), cy+R*sin(a1)};
        verts[2*i+1]={cx+r*cos(a2), cy+r*sin(a2)};
    }
}
static void spike_vertices(int N, PointF* verts, int& count) {
    count=2*N;
    const double cx=21.5, cy=21.5, R=22.0;
    for(int i=0;i<N;++i) {
        double a=2*M_PI*i/N - M_PI_2;
        verts[2*i]={cx+R*cos(a), cy+R*sin(a)};
        verts[2*i+1]={cx, cy};
    }
}
static bool point_in_polygon(const PointF* verts, int n, double px, double py) {
    bool inside=false;
    for(int i=0,j=n-1;i<n;j=i++) {
        double xi=verts[i].x, yi=verts[i].y;
        double xj=verts[j].x, yj=verts[j].y;
        if(((yi>py)!=(yj>py)) && (px<(xj-xi)*(py-yi)/(yj-yi)+xi))
            inside=!inside;
    }
    return inside;
}
static void generate_mask(const std::string& shape, float param, Mask44& mask) {
    for(int i=0;i<44;++i) for(int j=0;j<44;++j) mask[i][j]=false;
    if(shape=="circle") {
        const double cx=21.5,cy=21.5,r2=22.0*22.0;
        for(int i=0;i<44;++i) for(int j=0;j<44;++j) {
            double dx=j-cx, dy=i-cy;
            mask[i][j]=(dx*dx+dy*dy<=r2);
        }
    } else if(shape=="square") {
        for(int i=0;i<44;++i) for(int j=0;j<44;++j) mask[i][j]=true;
    } else if(shape=="triangle") {
        for(int i=0;i<44;++i) for(int j=0;j<44;++j) {
            double x=j, y=i;
            double x1=21.5,y1=0, x2=0,y2=43, x3=43,y3=43;
            double denom=(y2-y3)*(x1-x3)+(x3-x2)*(y1-y3);
            double a=((y2-y3)*(x-x3)+(x3-x2)*(y-y3))/denom;
            double b=((y3-y1)*(x-x3)+(x1-x3)*(y-y3))/denom;
            double c=1-a-b;
            mask[i][j]=(a>=0 && b>=0 && c>=0);
        }
    } else if(shape=="heart") {
        for(int i=0;i<44;++i) for(int j=0;j<44;++j) {
            double fx=(j-21.5)/21.5, fy=-(i-21.5)/21.5;
            double v=(fx*fx+fy*fy-1.0);
            v=v*v*v - fx*fx*fy*fy*fy;
            mask[i][j]=(v<=0.0);
        }
    } else if(shape=="polygon") {
        int N=std::max(3,(int)param);
        PointF verts[100]; int cnt;
        polygon_vertices(N, verts, cnt);
        for(int i=0;i<44;++i) for(int j=0;j<44;++j)
            mask[i][j]=point_in_polygon(verts, cnt, j+0.0, i+0.0);
    } else if(shape=="star") {
        int N=std::max(2,(int)param);
        PointF verts[200]; int cnt;
        star_vertices(N, verts, cnt);
        for(int i=0;i<44;++i) for(int j=0;j<44;++j)
            mask[i][j]=point_in_polygon(verts, cnt, j+0.0, i+0.0);
    } else if(shape=="ellipse") {
        double r=param;
        double a2=22.0*22.0, b2=(22.0*r)*(22.0*r);
        for(int i=0;i<44;++i) for(int j=0;j<44;++j) {
            double dx=j-21.5, dy=i-21.5;
            mask[i][j]=((dx*dx)/a2 + (dy*dy)/b2 <=1.0);
        }
    } else if(shape=="rounded_square") {
        double r=param, half=22.0, cr=r*half;
        for(int i=0;i<44;++i) for(int j=0;j<44;++j) {
            double dx=std::abs(j-21.5), dy=std::abs(i-21.5);
            if(dx<=half-cr && dy<=half-cr) mask[i][j]=true;
            else if(dx>half-cr && dy>half-cr) {
                double cx=dx-(half-cr), cy=dy-(half-cr);
                mask[i][j]=(cx*cx+cy*cy<=cr*cr);
            } else mask[i][j]=true;
        }
    } else if(shape=="spike") {
        int N=std::max(2,(int)param);
        PointF verts[200]; int cnt;
        spike_vertices(N, verts, cnt);
        for(int i=0;i<44;++i) for(int j=0;j<44;++j)
            mask[i][j]=point_in_polygon(verts, cnt, j+0.0, i+0.0);
    } else {
        generate_mask("circle",0,mask);
    }
}

// ----------------------------------------------------------------------
// Drawing functions (now take flat buffer uint8_t* and dimensions)

// Draw 44x44 mask centered, cleared background
void draw_shape_centered(const Mask44& mask, const RGB& col,
                         uint8_t* fb, int w, int h) {
    std::memset(fb, 0, w * h * 3);
    int startX = w/2 - 22;
    int startY = h/2 - 22;
    for (int y = 0; y < 44; ++y) {
        for (int x = 0; x < 44; ++x) {
            if (mask[y][x]) {
                int px = startX + x;
                int py = startY + y;
                if (px >= 0 && px < w && py >= 0 && py < h) {
                    int idx = (py * w + px) * 3;
                    fb[idx]   = col.r;
                    fb[idx+1] = col.g;
                    fb[idx+2] = col.b;
                }
            }
        }
    }
}

// Draw a filled circle (pixel‑based, for circle effect)
void draw_circle(uint8_t* fb, int w, int h, int cx, int cy, int rad, const RGB& col) {
    for(int y=0; y<h; ++y) {
        for(int x=0; x<w; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            if (dx*dx + dy*dy <= rad*rad) {
                int idx = (y * w + x) * 3;
                fb[idx]   = col.r;
                fb[idx+1] = col.g;
                fb[idx+2] = col.b;
            }
        }
    }
}

// Draw a rotated 44x44 mask at the given center point and angle
void draw_rotated_shape(const Mask44& mask, const RGB& col,
                        uint8_t* fb, int w, int h,
                        int cx, int cy, float angle_deg) {
    float rad = angle_deg * M_PI / 180.0f;
    float cos_a = std::cos(rad);
    float sin_a = std::sin(rad);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int dx = x - cx;
            int dy = y - cy;
            // inverse rotation to find mask coordinate
            float src_x =  dx * cos_a + dy * sin_a;
            float src_y = -dx * sin_a + dy * cos_a;
            int mx = static_cast<int>(src_x + 21.5f);
            int my = static_cast<int>(src_y + 21.5f);
            if (mx >= 0 && mx < 44 && my >= 0 && my < 44 && mask[my][mx]) {
                int idx = (y * w + x) * 3;
                fb[idx]   = col.r;
                fb[idx+1] = col.g;
                fb[idx+2] = col.b;
            }
        }
    }
}

// Moving shape (linear interpolation of position)
void draw_moving_shape(const Mask44& mask, const RGB& col,
                       uint8_t* fb, int w, int h,
                       int sx, int sy, int ex, int ey, float t) {
    std::memset(fb, 0, w * h * 3);
    int cx = static_cast<int>(sx + (ex - sx) * t);
    int cy = static_cast<int>(sy + (ey - sy) * t);
    for (int y = 0; y < 44; ++y) {
        for (int x = 0; x < 44; ++x) {
            if (mask[y][x]) {
                int px = cx - 22 + x;
                int py = cy - 22 + y;
                if (px >= 0 && px < w && py >= 0 && py < h) {
                    int idx = (py * w + px) * 3;
                    fb[idx]   = col.r;
                    fb[idx+1] = col.g;
                    fb[idx+2] = col.b;
                }
            }
        }
    }
}

// Scaled shape (scale factor 0..1 relative to 44)
void draw_scaled_shape(const Mask44& mask, const RGB& col,
                       uint8_t* fb, int w, int h, float scale) {
    std::memset(fb, 0, w * h * 3);
    int newSize = static_cast<int>(44 * scale);
    if (newSize < 1) newSize = 1;
    int startX = w/2 - newSize/2;
    int startY = h/2 - newSize/2;
    for (int y = 0; y < newSize && y < h; ++y) {
        for (int x = 0; x < newSize && x < w; ++x) {
            int origX = static_cast<int>(x / scale);
            int origY = static_cast<int>(y / scale);
            if (origX < 44 && origY < 44 && mask[origY][origX]) {
                int px = startX + x;
                int py = startY + y;
                if (px >= 0 && px < w && py >= 0 && py < h) {
                    int idx = (py * w + px) * 3;
                    fb[idx]   = col.r;
                    fb[idx+1] = col.g;
                    fb[idx+2] = col.b;
                }
            }
        }
    }
}

// Shake effect: apply random offset to an already drawn frame
void apply_shake(uint8_t* fb, int w, int h, int amp, std::mt19937& rng) {
    std::vector<uint8_t> temp(fb, fb + w * h * 3);
    std::memset(fb, 0, w * h * 3);
    int dx = std::uniform_int_distribution<int>(-amp, amp)(rng);
    int dy = std::uniform_int_distribution<int>(-amp, amp)(rng);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int nx = x + dx;
            int ny = y + dy;
            if (nx >= 0 && nx < w && ny >= 0 && ny < h) {
                int srcIdx = (ny * w + nx) * 3;
                int dstIdx = (y * w + x) * 3;
                fb[dstIdx]   = temp[srcIdx];
                fb[dstIdx+1] = temp[srcIdx+1];
                fb[dstIdx+2] = temp[srcIdx+2];
            }
        }
    }
}

// ----------------------------------------------------------------------
// Fire effect
class FireEffect {
    struct Particle { float x,y,vx,vy,life; };
    std::vector<Particle> parts;
    std::mt19937 rng;
public:
    FireEffect(int cnt, int w, int h) {
        rng.seed(123);
        parts.resize(cnt);
        for(auto& p: parts) {
            p.x = std::uniform_real_distribution<float>(0, static_cast<float>(w))(rng);
            p.y = std::uniform_real_distribution<float>(static_cast<float>(h-10), static_cast<float>(h))(rng);
            p.vx = std::uniform_real_distribution<float>(-10,10)(rng);
            p.vy = std::uniform_real_distribution<float>(-60,-120)(rng);
            p.life = std::uniform_real_distribution<float>(0.6,1.2)(rng);
        }
    }
    void update(float dt, int w, int h) {
        for(auto& p: parts) {
            p.x += p.vx*dt;
            p.y += p.vy*dt;
            p.life -= dt*1.5f;
            if(p.life<=0 || p.y<0) {
                p.x = std::uniform_real_distribution<float>(0, static_cast<float>(w))(rng);
                p.y = h - std::uniform_real_distribution<float>(0,15)(rng);
                p.vx = std::uniform_real_distribution<float>(-10,10)(rng);
                p.vy = std::uniform_real_distribution<float>(-60,-120)(rng);
                p.life = 0.8f;
            }
        }
    }
    void draw(uint8_t* fb, int w, int h) {
        for(auto& p: parts) {
            if(p.life<=0) continue;
            int x = static_cast<int>(p.x);
            int y = static_cast<int>(p.y);
            if(x>=0 && x<w && y>=0 && y<h) {
                uint8_t r = 200 + static_cast<uint8_t>(55*(1.0f-p.life));
                uint8_t g = static_cast<uint8_t>(80*(1.0f-p.life));
                int idx = (y * w + x) * 3;
                fb[idx]   = r;
                fb[idx+1] = g;
                fb[idx+2] = 0;
            }
        }
    }
};

// ----------------------------------------------------------------------
// Explosion effect
class ExplosionEffect {
    struct Particle { float x,y,vx,vy,life; };
    std::vector<Particle> parts;
    std::mt19937 rng;
public:
    ExplosionEffect(int cx, int cy, int radius, int count, float duration) {
        rng.seed(123);
        parts.resize(count);
        for(auto& p: parts) {
            float angle = std::uniform_real_distribution<float>(0, 2*M_PI)(rng);
            float speed = std::uniform_real_distribution<float>(50, 150)(rng);
            p.x = cx + std::uniform_real_distribution<float>(-radius, radius)(rng);
            p.y = cy + std::uniform_real_distribution<float>(-radius, radius)(rng);
            p.vx = std::cos(angle) * speed;
            p.vy = std::sin(angle) * speed;
            p.life = std::uniform_real_distribution<float>(0.2, duration)(rng);
        }
    }
    void update(float dt) {
        for(auto& p: parts) {
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.life -= dt;
        }
    }
    void draw(uint8_t* fb, int w, int h) {
        for(auto& p: parts) {
            if(p.life<=0) continue;
            int x = static_cast<int>(p.x);
            int y = static_cast<int>(p.y);
            if(x>=0 && x<w && y>=0 && y<h) {
                float t = p.life;
                uint8_t r = 255;
                uint8_t g = static_cast<uint8_t>(100 * t);
                int idx = (y * w + x) * 3;
                fb[idx]   = r;
                fb[idx+1] = g;
                fb[idx+2] = 0;
            }
        }
    }
};

// ----------------------------------------------------------------------
// Rainbow HSV to RGB
RGB hsv_to_rgb(float h, float s=1.0f, float v=1.0f) {
    float r,g,b;
    int i = static_cast<int>(h*6);
    float f = h*6 - i;
    float p = v*(1-s);
    float q = v*(1-f*s);
    float t = v*(1-(1-f)*s);
    switch(i%6) {
        case 0: r=v; g=t; b=p; break;
        case 1: r=q; g=v; b=p; break;
        case 2: r=p; g=v; b=t; break;
        case 3: r=p; g=q; b=v; break;
        case 4: r=t; g=p; b=v; break;
        default: r=v; g=p; b=q; break;
    }
    return {static_cast<uint8_t>(r*255), static_cast<uint8_t>(g*255), static_cast<uint8_t>(b*255)};
}

// ----------------------------------------------------------------------
int main(int argc, char* argv[]) {
    // ---------- Parse command line ----------
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " [--width W] [--height H] --effect <effect> [param=value ...]\n";
        std::cerr << "Effects: circle, fade, move, scale, rainbow, explosion, rotate, fire, shake, pulse\n";
        return 1;
    }

    std::string effect;
    std::map<std::string, std::string> params;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--width") {
            if (i + 1 < argc) { WIDTH = std::stoi(argv[++i]); }
        } else if (arg == "--height") {
            if (i + 1 < argc) { HEIGHT = std::stoi(argv[++i]); }
        } else if (arg == "--effect") {
            if (i + 1 < argc) { effect = argv[++i]; }
        } else {
            size_t eq = arg.find('=');
            if (eq != std::string::npos) {
                std::string key = arg.substr(0, eq);
                std::string val = arg.substr(eq+1);
                params[key] = val;
            }
        }
    }
    if (WIDTH <= 0 || HEIGHT <= 0) {
        std::cerr << "Width and height must be positive integers.\n";
        return 1;
    }

    // Parameter helpers
    auto getInt = [&](const std::string& key, int def) {
        auto it = params.find(key);
        return it != params.end() ? std::stoi(it->second) : def;
    };
    auto getFloat = [&](const std::string& key, float def) {
        auto it = params.find(key);
        return it != params.end() ? std::stof(it->second) : def;
    };
    auto getString = [&](const std::string& key, const std::string& def) {
        auto it = params.find(key);
        return it != params.end() ? it->second : def;
    };
    auto getColor = [&](const std::string& key, const std::string& def) {
        auto it = params.find(key);
        return colour_to_rgb(it != params.end() ? it->second : def);
    };

    const int fps = 10;
    float duration = getFloat("duration", 2.0f);
    int totalFrames = static_cast<int>(duration * fps);
    if (totalFrames < 1) totalFrames = 1;

    // Create output directory
    std::filesystem::create_directory("video_output");

    // Allocate frame buffer (RGB flat array)
    std::vector<uint8_t> fb(WIDTH * HEIGHT * 3);
    std::mt19937 rng(123);

    // ---------- Effect dispatcher ----------
    if (effect == "circle") {
        int radius = getInt("radius", 20);
        int cx = getInt("cx", WIDTH/2);
        int cy = getInt("cy", HEIGHT/2);
        RGB start = getColor("start_color", "red");
        RGB end   = getColor("end_color", "blue");
        for (int f = 0; f < totalFrames; ++f) {
            float t = (totalFrames == 1) ? 0 : static_cast<float>(f) / (totalFrames-1);
            RGB col = lerp_rgb(start, end, t);
            std::memset(fb.data(), 0, WIDTH * HEIGHT * 3);
            draw_circle(fb.data(), WIDTH, HEIGHT, cx, cy, radius, col);
            std::ostringstream fname;
            fname << "video_output/frame" << std::setfill('0') << std::setw(3) << f << ".ppm";
            std::ofstream out(fname.str(), std::ios::binary);
            out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
            out.write(reinterpret_cast<const char*>(fb.data()), WIDTH * HEIGHT * 3);
        }
    }
    else if (effect == "fade") {
        RGB from = getColor("from_color", "black");
        RGB to   = getColor("to_color", "white");
        for (int f = 0; f < totalFrames; ++f) {
            float t = (totalFrames == 1) ? 0 : static_cast<float>(f) / (totalFrames-1);
            RGB col = lerp_rgb(from, to, t);
            for (int y = 0; y < HEIGHT; ++y) {
                for (int x = 0; x < WIDTH; ++x) {
                    int idx = (y * WIDTH + x) * 3;
                    fb[idx]   = col.r;
                    fb[idx+1] = col.g;
                    fb[idx+2] = col.b;
                }
            }
            std::ostringstream fname;
            fname << "video_output/frame" << std::setfill('0') << std::setw(3) << f << ".ppm";
            std::ofstream out(fname.str(), std::ios::binary);
            out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
            out.write(reinterpret_cast<const char*>(fb.data()), WIDTH * HEIGHT * 3);
        }
    }
    else if (effect == "move") {
        std::string shape = getString("shape", "square");
        RGB col = getColor("color", "red");
        int sx = 0, sy = 0, ex = WIDTH/2, ey = HEIGHT/2;
        std::string sp = getString("start_pos", "0,0");
        std::string ep = getString("end_pos", std::to_string(WIDTH/2) + "," + std::to_string(HEIGHT/2));
        std::sscanf(sp.c_str(), "%d,%d", &sx, &sy);
        std::sscanf(ep.c_str(), "%d,%d", &ex, &ey);
        Mask44 mask;
        generate_mask(shape, 0.0f, mask);
        for (int f = 0; f < totalFrames; ++f) {
            float t = (totalFrames == 1) ? 0 : static_cast<float>(f) / (totalFrames-1);
            draw_moving_shape(mask, col, fb.data(), WIDTH, HEIGHT, sx, sy, ex, ey, t);
            std::ostringstream fname;
            fname << "video_output/frame" << std::setfill('0') << std::setw(3) << f << ".ppm";
            std::ofstream out(fname.str(), std::ios::binary);
            out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
            out.write(reinterpret_cast<const char*>(fb.data()), WIDTH * HEIGHT * 3);
        }
    }
    else if (effect == "scale") {
        std::string shape = getString("shape", "circle");
        RGB col = getColor("color", "blue");
        float minSize = getFloat("min_size", 10);
        float maxSize = getFloat("max_size", 44);
        Mask44 mask;
        generate_mask(shape, 0.0f, mask);
        for (int f = 0; f < totalFrames; ++f) {
            float t = (totalFrames == 1) ? 0 : static_cast<float>(f) / (totalFrames-1);
            float scale = (minSize + (maxSize - minSize) * t) / 44.0f;
            draw_scaled_shape(mask, col, fb.data(), WIDTH, HEIGHT, scale);
            std::ostringstream fname;
            fname << "video_output/frame" << std::setfill('0') << std::setw(3) << f << ".ppm";
            std::ofstream out(fname.str(), std::ios::binary);
            out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
            out.write(reinterpret_cast<const char*>(fb.data()), WIDTH * HEIGHT * 3);
        }
    }
    else if (effect == "rainbow") {
        std::string shape = getString("shape", "heart");
        float speed = getFloat("speed", 1.0f);
        Mask44 mask;
        generate_mask(shape, 0.0f, mask);
        for (int f = 0; f < totalFrames; ++f) {
            float t_sec = static_cast<float>(f) / fps;
            float hue = std::fmod(t_sec * speed, 1.0f);
            RGB col = hsv_to_rgb(hue);
            draw_shape_centered(mask, col, fb.data(), WIDTH, HEIGHT);
            std::ostringstream fname;
            fname << "video_output/frame" << std::setfill('0') << std::setw(3) << f << ".ppm";
            std::ofstream out(fname.str(), std::ios::binary);
            out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
            out.write(reinterpret_cast<const char*>(fb.data()), WIDTH * HEIGHT * 3);
        }
    }
    else if (effect == "explosion") {
        int cx = getInt("cx", WIDTH/2);
        int cy = getInt("cy", HEIGHT/2);
        int radius = getInt("radius", 10);
        int count = getInt("particle_count", 100);
        ExplosionEffect exp(cx, cy, radius, count, duration);
        for (int f = 0; f < totalFrames; ++f) {
            std::memset(fb.data(), 0, WIDTH * HEIGHT * 3);
            exp.update(1.0f / fps);
            exp.draw(fb.data(), WIDTH, HEIGHT);
            std::ostringstream fname;
            fname << "video_output/frame" << std::setfill('0') << std::setw(3) << f << ".ppm";
            std::ofstream out(fname.str(), std::ios::binary);
            out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
            out.write(reinterpret_cast<const char*>(fb.data()), WIDTH * HEIGHT * 3);
        }
    }
    else if (effect == "rotate") {
        std::string shape = getString("shape", "square");
        RGB col = getColor("color", "red");
        float speed = getFloat("speed", 360.0f);
        Mask44 mask;
        generate_mask(shape, 0.0f, mask);
        int cx = WIDTH / 2;
        int cy = HEIGHT / 2;
        for (int f = 0; f < totalFrames; ++f) {
            float angle = speed * (static_cast<float>(f) / fps);
            std::memset(fb.data(), 0, WIDTH * HEIGHT * 3);
            draw_rotated_shape(mask, col, fb.data(), WIDTH, HEIGHT, cx, cy, angle);
            std::ostringstream fname;
            fname << "video_output/frame" << std::setfill('0') << std::setw(3) << f << ".ppm";
            std::ofstream out(fname.str(), std::ios::binary);
            out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
            out.write(reinterpret_cast<const char*>(fb.data()), WIDTH * HEIGHT * 3);
        }
    }
    else if (effect == "fire") {
        int intensity = getInt("intensity", 80);
        FireEffect fire(intensity, WIDTH, HEIGHT);
        for (int f = 0; f < totalFrames; ++f) {
            std::memset(fb.data(), 0, WIDTH * HEIGHT * 3);
            fire.update(1.0f / fps, WIDTH, HEIGHT);
            fire.draw(fb.data(), WIDTH, HEIGHT);
            std::ostringstream fname;
            fname << "video_output/frame" << std::setfill('0') << std::setw(3) << f << ".ppm";
            std::ofstream out(fname.str(), std::ios::binary);
            out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
            out.write(reinterpret_cast<const char*>(fb.data()), WIDTH * HEIGHT * 3);
        }
    }
    else if (effect == "shake") {
        std::string shape = getString("shape", "square");
        RGB col = getColor("color", "red");
        int amplitude = getInt("amplitude", 5);
        Mask44 mask;
        generate_mask(shape, 0.0f, mask);
        for (int f = 0; f < totalFrames; ++f) {
            draw_shape_centered(mask, col, fb.data(), WIDTH, HEIGHT);
            apply_shake(fb.data(), WIDTH, HEIGHT, amplitude, rng);
            std::ostringstream fname;
            fname << "video_output/frame" << std::setfill('0') << std::setw(3) << f << ".ppm";
            std::ofstream out(fname.str(), std::ios::binary);
            out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
            out.write(reinterpret_cast<const char*>(fb.data()), WIDTH * HEIGHT * 3);
        }
    }
    else if (effect == "pulse") {
        std::string shape = getString("shape", "circle");
        RGB c1 = getColor("color1", "red");
        RGB c2 = getColor("color2", "blue");
        float period = getFloat("period", 1.0f);
        Mask44 mask;
        generate_mask(shape, 0.0f, mask);
        for (int f = 0; f < totalFrames; ++f) {
            float t_sec = static_cast<float>(f) / fps;
            float phase = std::fmod(t_sec, period) / period;
            float s = std::sin(phase * 2 * M_PI);
            float scale = 0.7f + 0.3f * (s + 1) / 2;
            RGB col = lerp_rgb(c1, c2, (s + 1) / 2);
            draw_scaled_shape(mask, col, fb.data(), WIDTH, HEIGHT, scale);
            std::ostringstream fname;
            fname << "video_output/frame" << std::setfill('0') << std::setw(3) << f << ".ppm";
            std::ofstream out(fname.str(), std::ios::binary);
            out << "P6\n" << WIDTH << " " << HEIGHT << "\n255\n";
            out.write(reinterpret_cast<const char*>(fb.data()), WIDTH * HEIGHT * 3);
        }
    }
    else {
        std::cerr << "Unknown effect: " << effect << std::endl;
        return 1;
    }

    std::cout << "Saved " << totalFrames << " frames in video_output/\n";
    std::cout << "ffmpeg -framerate " << fps << " -i video_output/frame%03d.ppm -c:v gif output.gif\n";
    return 0;
}
