/*
 * KidDraw — 儿童电子画板
 * Built with SDL2 (pixel-buffer only). All geometry rendered via
 * hand-written Bresenham line & midpoint-circle algorithms.
 *
 * Compile:  cmake -B build && cmake --build build       (Linux / macOS)
 *           cmake -B build -DCMAKE_TOOLCHAIN_FILE=...    (VS + vcpkg)
 */

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

// ============================================================================
//  1. Constants
// ============================================================================
static constexpr int WIN_W       = 1024;
static constexpr int WIN_H       = 768;
static constexpr int TB_H        = 110;       // toolbar height  (px)
static constexpr int CANVAS_TOP  = TB_H;
static constexpr int CANVAS_W    = WIN_W;
static constexpr int CANVAS_H    = WIN_H - TB_H;

// ============================================================================
//  2. Types
// ============================================================================
struct Color { Uint8 r, g, b, a; };

static const Color PALETTE[] = {       // 10 bright kid-friendly colours
    {255, 60,  60,  255},  // Red
    {255, 140, 30,  255},  // Orange
    {255, 220, 30,  255},  // Yellow
    { 50, 200, 60,  255},  // Green
    { 40, 160, 255, 255},  // Blue
    {140, 60,  220, 255},  // Purple
    {255, 120, 200, 255},  // Pink
    {139, 90,  43,  255},  // Brown
    { 30, 30,  30,  255},  // Black
    {255, 255, 255, 255},  // White
};
static constexpr int N_COLORS = sizeof(PALETTE) / sizeof(PALETTE[0]);

enum StampKind { STAMP_NONE = -1, STAMP_STAR = 0, STAMP_HEART, STAMP_SMILEY };

struct Stroke {
    std::vector<SDL_Point> pts;
    Color color;
    int   radius;      // brush radius in px
};

// ============================================================================
//  3. Pixel-level drawing primitives  (Bresenham line / midpoint circle)
// ============================================================================

/* --- Bresenham line ---------------------------------------------------- */
static void draw_line(SDL_Renderer* r, int x0, int y0, int x1, int y1) {
    int dx  = abs(x1 - x0);
    int dy  = -abs(y1 - y0);
    int sx  = x0 < x1 ? 1 : -1;
    int sy  = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (;;) {
        SDL_RenderDrawPoint(r, x0, y0);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

/* --- Midpoint circle outline ------------------------------------------- */
static void draw_circle_outline(SDL_Renderer* r, int cx, int cy, int rad) {
    int x = 0, y = rad, d = 1 - rad;
    while (x <= y) {
        SDL_RenderDrawPoint(r, cx + x, cy + y);
        SDL_RenderDrawPoint(r, cx - x, cy + y);
        SDL_RenderDrawPoint(r, cx + x, cy - y);
        SDL_RenderDrawPoint(r, cx - x, cy - y);
        SDL_RenderDrawPoint(r, cx + y, cy + x);
        SDL_RenderDrawPoint(r, cx - y, cy + x);
        SDL_RenderDrawPoint(r, cx + y, cy - x);
        SDL_RenderDrawPoint(r, cx - y, cy - x);
        if (d < 0) d += 2 * x + 3;
        else       { d += 2 * (x - y) + 5; --y; }
        ++x;
    }
}

/* --- Midpoint filled circle  (scan-line fill via symmetry) ------------- */
static void draw_filled_circle(SDL_Renderer* r, int cx, int cy, int rad) {
    if (rad <= 0) { SDL_RenderDrawPoint(r, cx, cy); return; }
    int x = 0, y = rad, d = 1 - rad;
    while (x <= y) {
        // horizontal spans for each y-level
        SDL_RenderDrawLine(r, cx - x, cy - y, cx + x, cy - y);
        SDL_RenderDrawLine(r, cx - x, cy + y, cx + x, cy + y);
        SDL_RenderDrawLine(r, cx - y, cy - x, cx + y, cy - x);
        SDL_RenderDrawLine(r, cx - y, cy + x, cx + y, cy + x);
        if (d < 0) d += 2 * x + 3;
        else       { d += 2 * (x - y) + 5; --y; }
        ++x;
    }
}

/* --- Rounded rect helpers (filled / outline) --------------------------- */
static void fill_rounded_rect(SDL_Renderer* r, SDL_Rect rc, int cr) {
    // central rectangle
    SDL_Rect inner = { rc.x + cr, rc.y, rc.w - 2 * cr, rc.h };
    SDL_RenderFillRect(r, &inner);
    // left / right vertical strips
    SDL_Rect left  = { rc.x, rc.y + cr, cr, rc.h - 2 * cr };
    SDL_Rect right = { rc.x + rc.w - cr, rc.y + cr, cr, rc.h - 2 * cr };
    SDL_RenderFillRect(r, &left);
    SDL_RenderFillRect(r, &right);
    // 4 filled quarter-circles at corners
    draw_filled_circle(r, rc.x + cr,          rc.y + cr,          cr);
    draw_filled_circle(r, rc.x + rc.w - cr,   rc.y + cr,          cr);
    draw_filled_circle(r, rc.x + cr,          rc.y + rc.h - cr,   cr);
    draw_filled_circle(r, rc.x + rc.w - cr,   rc.y + rc.h - cr,   cr);
}

static void draw_rounded_rect(SDL_Renderer* r, SDL_Rect rc, int cr) {
    // 4 edges
    SDL_RenderDrawLine(r, rc.x + cr, rc.y, rc.x + rc.w - cr, rc.y);
    SDL_RenderDrawLine(r, rc.x + cr, rc.y + rc.h, rc.x + rc.w - cr, rc.y + rc.h);
    SDL_RenderDrawLine(r, rc.x, rc.y + cr, rc.x, rc.y + rc.h - cr);
    SDL_RenderDrawLine(r, rc.x + rc.w, rc.y + cr, rc.x + rc.w, rc.y + rc.h - cr);
    // 4 quarter-arcs
    draw_circle_outline(r, rc.x + cr,          rc.y + cr,          cr);
    draw_circle_outline(r, rc.x + rc.w - cr,   rc.y + cr,          cr);
    draw_circle_outline(r, rc.x + cr,          rc.y + rc.h - cr,   cr);
    draw_circle_outline(r, rc.x + rc.w - cr,   rc.y + rc.h - cr,   cr);
}

// ============================================================================
//  4. Stamp pattern generators  (returns relative-point vectors)
// ============================================================================

static std::vector<SDL_Point> gen_star(int size) {
    std::vector<SDL_Point> pts;
    double outerR = size * 0.5;
    double innerR = outerR * 0.38;
    for (int i = 0; i < 10; ++i) {
        double r   = (i & 1) ? innerR : outerR;
        double ang = (i * 36.0 - 90.0) * M_PI / 180.0;
        pts.push_back({ (int)(r * cos(ang)), (int)(r * sin(ang)) });
    }
    return pts;
}

static std::vector<SDL_Point> gen_heart(int size) {
    std::vector<SDL_Point> pts;
    double s = size / 32.0;
    for (int i = 0; i <= 40; ++i) {
        double t = i * 2.0 * M_PI / 40.0;
        double x =  16.0 * pow(sin(t), 3);
        double y = -13.0 * cos(t) + 5.0 * cos(2*t)
                   + 2.0 * cos(3*t) + cos(4*t);
        pts.push_back({ (int)(x * s), (int)(y * s) });
    }
    return pts;
}

static void draw_smiley_stamp(SDL_Renderer* r, int cx, int cy, int size) {
    int rad = size / 2;
    // face outline
    draw_circle_outline(r, cx, cy, rad - 1);
    draw_circle_outline(r, cx, cy, rad);
    // eyes
    int eyeR = rad / 5;
    if (eyeR < 2) eyeR = 2;
    int ex = rad / 3, ey = rad / 4;
    draw_filled_circle(r, cx - ex, cy - ey, eyeR);
    draw_filled_circle(r, cx + ex, cy - ey, eyeR);
    // smile arc (approximate via Bresenham segments)
    for (int i = 0; i < 20; ++i) {
        double t1 = (20.0 + i * 140.0 / 20.0)       * M_PI / 180.0;
        double t2 = (20.0 + (i+1) * 140.0 / 20.0)   * M_PI / 180.0;
        draw_line(r,
            cx + (int)(rad * 0.6 * cos(t1)), cy + (int)(rad * 0.5 * sin(t1)),
            cx + (int)(rad * 0.6 * cos(t2)), cy + (int)(rad * 0.5 * sin(t2)));
    }
}

// ============================================================================
//  5. Canvas — stroke management + render-to-texture
// ============================================================================

class Canvas {
public:
    Canvas(SDL_Renderer* renderer, int w, int h)
        : ren_(renderer), w_(w), h_(h)
    {
        tex_ = SDL_CreateTexture(ren_, SDL_PIXELFORMAT_RGBA32,
                                 SDL_TEXTUREACCESS_TARGET, w_, h_);
        SDL_SetTextureBlendMode(tex_, SDL_BLENDMODE_BLEND);
        reset();
    }
    ~Canvas() { if (tex_) SDL_DestroyTexture(tex_); }

    // -- stroke recording --
    void begin_stroke(int canvas_x, int canvas_y, int radius, Color col) {
        cur_.pts.clear();
        cur_.pts.push_back({ canvas_x, canvas_y });
        cur_.radius = radius;
        cur_.color  = col;
    }
    void extend_stroke(int canvas_x, int canvas_y) {
        cur_.pts.push_back({ canvas_x, canvas_y });
    }
    void end_stroke() {
        if (cur_.pts.empty()) return;
        strokes_.push_back(cur_);
        bake(cur_);
        cur_.pts.clear();
    }

    // -- stamp --
    void place_stamp(StampKind kind, int cx, int cy, int size, Color col) {
        bake_stamp(kind, cx, cy, size, col);
    }

    // -- undo / clear --
    void undo() {
        if (strokes_.empty()) return;
        strokes_.pop_back();
        rebuild();
    }
    void reset() {
        strokes_.clear();
        cur_.pts.clear();
        SDL_SetRenderTarget(ren_, tex_);
        SDL_SetRenderDrawColor(ren_, 255, 255, 255, 255);
        SDL_RenderClear(ren_);
        SDL_SetRenderTarget(ren_, nullptr);
    }

    // -- save --
    bool save_bmp(const char* path) const {
        SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(
            0, w_, h_, 32, SDL_PIXELFORMAT_RGBA32);
        if (!s) return false;
        SDL_SetRenderTarget(ren_, tex_);
        SDL_RenderReadPixels(ren_, nullptr, SDL_PIXELFORMAT_RGBA32,
                             s->pixels, s->pitch);
        SDL_SetRenderTarget(ren_, nullptr);
        int rc = SDL_SaveBMP(s, path);
        SDL_FreeSurface(s);
        return rc == 0;
    }

    // -- per-frame --
    void present() const {
        SDL_Rect dst = { 0, CANVAS_TOP, w_, h_ };
        SDL_RenderCopy(ren_, tex_, nullptr, &dst);
    }
    void render_current() const {
        if (cur_.pts.empty()) return;
        set_color(cur_.color);
        render_thick_points(cur_.pts, cur_.radius, CANVAS_TOP);
    }

    int width()  const { return w_; }
    int height() const { return h_; }

    size_t stroke_count() const { return strokes_.size(); }

private:
    void set_color(Color c) const {
        SDL_SetRenderDrawColor(ren_, c.r, c.g, c.b, c.a);
    }

    void render_thick_points(const std::vector<SDL_Point>& pts, int rad, int y_offset = 0) const {
        for (size_t i = 0; i < pts.size(); ++i) {
            draw_filled_circle(ren_, pts[i].x, pts[i].y + y_offset, rad);
            if (i > 0) {
                // interpolate between consecutive distant points
                int dx   = pts[i].x - pts[i - 1].x;
                int dy   = pts[i].y - pts[i - 1].y;
                float d  = sqrtf((float)(dx*dx + dy*dy));
                int   n  = (d < 0.001f) ? 0 : (int)(d / (rad * 0.7f));
                for (int s = 1; s < n; ++s) {
                    float t = (float)s / n;
                    int ix  = pts[i - 1].x + (int)(dx * t);
                    int iy  = pts[i - 1].y + (int)(dy * t);
                    draw_filled_circle(ren_, ix, iy + y_offset, rad);
                }
            }
        }
    }

    void bake(const Stroke& s) {
        SDL_SetRenderTarget(ren_, tex_);
        set_color(s.color);
        render_thick_points(s.pts, s.radius);
        SDL_SetRenderTarget(ren_, nullptr);
    }

    void bake_stamp(StampKind kind, int cx, int cy, int size, Color col) {
        SDL_SetRenderTarget(ren_, tex_);
        set_color(col);

        switch (kind) {
        case STAMP_SMILEY:
            draw_smiley_stamp(ren_, cx, cy, size);
            break;
        case STAMP_STAR:
        case STAMP_HEART: {
            std::vector<SDL_Point> rel = (kind == STAMP_STAR)
                                         ? gen_star(size)
                                         : gen_heart(size);
            for (size_t i = 0; i < rel.size(); ++i) {
                size_t j = (i + 1) % rel.size();
                draw_line(ren_, cx + rel[i].x, cy + rel[i].y,
                                cx + rel[j].x, cy + rel[j].y);
            }
            break;
        }
        default: break;
        }

        SDL_SetRenderTarget(ren_, nullptr);
    }

    void rebuild() {
        SDL_SetRenderTarget(ren_, tex_);
        SDL_SetRenderDrawColor(ren_, 255, 255, 255, 255);
        SDL_RenderClear(ren_);
        for (auto& s : strokes_) {
            set_color(s.color);
            render_thick_points(s.pts, s.radius);
        }
        SDL_SetRenderTarget(ren_, nullptr);
    }

    SDL_Renderer* ren_;
    SDL_Texture*  tex_;
    int           w_, h_;
    std::vector<Stroke> strokes_;
    Stroke        cur_;     // in-progress stroke
};

// ============================================================================
//  6. Toolbar — layout + rendering  (all primitives self-drawn)
// ============================================================================

struct ToolBtn {
    SDL_Rect rect;
    int      id;
};

enum BtnID : int {
    // colours  (0..9)
    BTN_COLOR0 = 0, BTN_COLOR1, BTN_COLOR2, BTN_COLOR3, BTN_COLOR4,
    BTN_COLOR5,    BTN_COLOR6, BTN_COLOR7, BTN_COLOR8, BTN_COLOR9,
    // brush sizes
    BTN_SIZE_S = 20, BTN_SIZE_M, BTN_SIZE_L,
    // tools
    BTN_ERASER = 30, BTN_UNDO, BTN_CLEAR, BTN_SAVE,
    // stamps
    BTN_STAMP_STAR = 40, BTN_STAMP_HEART, BTN_STAMP_SMILEY,
    BTN_NONE = -1
};

static std::vector<ToolBtn> tbtns;   // built in main()

static void build_toolbar() {
    tbtns.clear();
    // --- row 1: colour circles  (y = 12, r = 16, gap = 56) ---
    int yc = 30, cr = 16, gap = 56;
    for (int i = 0; i < N_COLORS; ++i) {
        int xc = 40 + i * gap;
        SDL_Rect r = { xc - cr - 4, yc - cr - 4, (cr + 4) * 2, (cr + 4) * 2 };
        tbtns.push_back({ r, BTN_COLOR0 + i });
    }
    // brush-size circles (S, M, L) to the right of colours
    int sx = 40 + N_COLORS * gap + 20;
    for (int i = 0; i < 3; ++i) {
        int sz[] = { 4, 7, 10 };
        int rr   = sz[i] + 6;
        SDL_Rect r = { sx + i * 44 - rr, yc - rr, rr * 2, rr * 2 };
        tbtns.push_back({ r, BTN_SIZE_S + i });
    }

    // --- row 2: tool buttons  (y = 62, h = 34) ---
    int by = 62, bh = 34, bw = 76, bgap = 6;
    struct { int id; } tools[] = {
        { BTN_ERASER }, { BTN_UNDO }, { BTN_CLEAR },
        { BTN_SAVE },   { BTN_STAMP_STAR }, { BTN_STAMP_HEART },
        { BTN_STAMP_SMILEY }
    };
    int nt = sizeof(tools)/sizeof(tools[0]);
    int startX = 16;
    for (int i = 0; i < nt; ++i) {
        SDL_Rect r = { startX + i * (bw + bgap), by, bw, bh };
        tbtns.push_back({ r, tools[i].id });
    }
}

static int hit_test(int mx, int my) {
    for (auto& b : tbtns)
        if (mx >= b.rect.x && mx < b.rect.x + b.rect.w &&
            my >= b.rect.y && my < b.rect.y + b.rect.h)
            return b.id;
    return BTN_NONE;
}

static void render_toolbar(SDL_Renderer* r, int selectedColorIdx,
                           int brushSize, bool eraserMode, StampKind stampPending)
{
    // background
    SDL_SetRenderDrawColor(r, 235, 235, 240, 255);
    SDL_Rect tbbg = { 0, 0, WIN_W, TB_H };
    SDL_RenderFillRect(r, &tbbg);
    SDL_SetRenderDrawColor(r, 180, 180, 190, 255);
    SDL_RenderDrawLine(r, 0, TB_H - 1, WIN_W, TB_H - 1);

    int yc = 30, cr = 16, gap = 56;

    // -- colour circles --
    for (int i = 0; i < N_COLORS; ++i) {
        int xc  = 40 + i * gap;
        auto& c = PALETTE[i];
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        draw_filled_circle(r, xc, yc, cr);
        // outline
        if (i == selectedColorIdx) {
            SDL_SetRenderDrawColor(r, 50, 50, 50, 255);
        } else {
            SDL_SetRenderDrawColor(r, 160, 160, 170, 255);
        }
        draw_circle_outline(r, xc, yc, cr);
        if (i == selectedColorIdx) {
            draw_circle_outline(r, xc, yc, cr + 1);
        }
    }

    // -- brush-size indicators --
    int sx = 40 + N_COLORS * gap + 20;
    int radii[] = { 4, 7, 10 };
    for (int i = 0; i < 3; ++i) {
        int rad = radii[i];
        SDL_SetRenderDrawColor(r, 80, 80, 80, 255);
        draw_filled_circle(r, sx + i * 44, yc, rad);
        if (rad == brushSize) {
            SDL_SetRenderDrawColor(r, 50, 50, 50, 255);
            draw_circle_outline(r, sx + i * 44, yc, rad + 2);
        }
    }

    // -- tool buttons --
    for (auto& b : tbtns) {
        if (b.id < BTN_SIZE_S) continue;            // skip colour / size circles
        SDL_Rect& rc = b.rect;
        // button background
        bool active = false;
        if (b.id == BTN_ERASER && eraserMode) active = true;
        if (b.id >= BTN_STAMP_STAR &&
            (int)(b.id - BTN_STAMP_STAR) == (int)stampPending) active = true;
        if (active) {
            SDL_SetRenderDrawColor(r, 180, 200, 255, 255);
        } else {
            SDL_SetRenderDrawColor(r, 210, 210, 220, 255);
        }
        fill_rounded_rect(r, rc, 6);
        SDL_SetRenderDrawColor(r, 140, 140, 155, 255);
        draw_rounded_rect(r, rc, 6);

        // -- icons inside button (drawn with primitives) --
        SDL_SetRenderDrawColor(r, 60, 60, 60, 255);
        int ix = rc.x + rc.w / 2;
        int iy = rc.y + rc.h / 2;

        switch (b.id) {
        case BTN_ERASER: {
            // small rectangle + diagonal
            SDL_Rect er = { ix - 8, iy - 6, 16, 12 };
            SDL_RenderFillRect(r, &er);
            SDL_SetRenderDrawColor(r, 230, 230, 240, 255);
            SDL_RenderDrawLine(r, ix - 6, iy - 4, ix + 4, iy + 4);
            SDL_RenderDrawLine(r, ix - 6, iy + 4, ix + 4, iy - 4);
            break;
        }
        case BTN_UNDO:
            // curved left arrow
            draw_line(r, ix + 6, iy - 5, ix - 6, iy);
            draw_line(r, ix + 6, iy + 5, ix - 6, iy);
            draw_line(r, ix - 6, iy,     ix - 3, iy - 5);
            break;
        case BTN_CLEAR:
            // X
            draw_line(r, ix - 7, iy - 7, ix + 7, iy + 7);
            draw_line(r, ix + 7, iy - 7, ix - 7, iy + 7);
            break;
        case BTN_SAVE: {
            // floppy / box with arrow
            SDL_Rect fr = { ix - 6, iy - 4, 12, 10 };
            SDL_RenderDrawRect(r, &fr);
            draw_line(r, ix, iy + 6, ix, iy + 0);
            draw_line(r, ix - 3, iy + 3, ix, iy);
            draw_line(r, ix + 3, iy + 3, ix, iy);
            break;
        }
        case BTN_STAMP_STAR: {
            auto spts = gen_star(16);
            SDL_SetRenderDrawColor(r, 255, 180, 30, 255);
            for (size_t i = 0; i < spts.size(); ++i) {
                size_t j = (i + 1) % spts.size();
                draw_line(r, ix + spts[i].x / 2, iy + spts[i].y / 2,
                             ix + spts[j].x / 2, iy + spts[j].y / 2);
            }
            break;
        }
        case BTN_STAMP_HEART: {
            auto hpts = gen_heart(16);
            SDL_SetRenderDrawColor(r, 255, 60, 80, 255);
            for (size_t i = 0; i < hpts.size(); ++i) {
                size_t j = (i + 1) % hpts.size();
                draw_line(r, ix + hpts[i].x / 2, iy + hpts[i].y / 2,
                             ix + hpts[j].x / 2, iy + hpts[j].y / 2);
            }
            break;
        }
        case BTN_STAMP_SMILEY:
            SDL_SetRenderDrawColor(r, 255, 200, 50, 255);
            draw_filled_circle(r, ix, iy, 6);
            SDL_SetRenderDrawColor(r, 60, 60, 60, 255);
            draw_circle_outline(r, ix, iy, 6);
            SDL_RenderDrawPoint(r, ix - 2, iy - 2);
            SDL_RenderDrawPoint(r, ix + 2, iy - 2);
            // smile
            SDL_RenderDrawPoint(r, ix - 2, iy + 2);
            SDL_RenderDrawPoint(r, ix - 1, iy + 3);
            SDL_RenderDrawPoint(r, ix + 0, iy + 4);
            SDL_RenderDrawPoint(r, ix + 1, iy + 3);
            SDL_RenderDrawPoint(r, ix + 2, iy + 2);
            break;
        }
    }

    // -- current-colour indicator (big filled circle, top-right) --
    int ind_x = WIN_W - 60, ind_y = yc;
    {
        const Color& c = PALETTE[selectedColorIdx];
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
        draw_filled_circle(r, ind_x, ind_y, 18);
        SDL_SetRenderDrawColor(r, 60, 60, 60, 255);
        draw_circle_outline(r, ind_x, ind_y, 18);
        draw_circle_outline(r, ind_x, ind_y, 19);
    }
}

// ============================================================================
//  7. main()
// ============================================================================

int main(int /*argc*/, char* /*argv*/[]) {
    // ---- init SDL ----
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window* win = SDL_CreateWindow(
        "KidDraw - 儿童电子画板",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WIN_W, WIN_H, SDL_WINDOW_SHOWN);
    if (!win) { fprintf(stderr, "Window: %s\n", SDL_GetError()); return 1; }

    SDL_Renderer* ren = SDL_CreateRenderer(
        win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_TARGETTEXTURE);
    if (!ren) { fprintf(stderr, "Renderer: %s\n", SDL_GetError()); return 1; }

    // ---- state ----
    Canvas canvas(ren, CANVAS_W, CANVAS_H);

    int       selColorIdx = 0;
    int       brushSz     = 4;         // radius
    bool      eraserMode  = false;
    StampKind stampPend   = STAMP_NONE;
    bool      drawing     = false;

    build_toolbar();

    // ---- main loop ----
    bool running = true;
    SDL_Event ev;

    while (running) {
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_MOUSEBUTTONDOWN: {
                int mx = ev.button.x, my = ev.button.y;
                if (my < CANVAS_TOP) {
                    // toolbar hit
                    int bid = hit_test(mx, my);
                    if (bid >= BTN_COLOR0 && bid <= BTN_COLOR9) {
                        selColorIdx = bid - BTN_COLOR0;
                        eraserMode  = false;
                        stampPend   = STAMP_NONE;
                    } else if (bid >= BTN_SIZE_S && bid <= BTN_SIZE_L) {
                        int radii[] = { 4, 7, 10 };
                        brushSz     = radii[bid - BTN_SIZE_S];
                    } else if (bid == BTN_ERASER) {
                        eraserMode  = !eraserMode;
                        stampPend   = STAMP_NONE;
                    } else if (bid == BTN_UNDO) {
                        canvas.undo();
                    } else if (bid == BTN_CLEAR) {
                        canvas.reset();
                    } else if (bid == BTN_SAVE) {
                        char fname[64];
                        snprintf(fname, sizeof(fname),
                                 "drawing_%ld.bmp", (long)time(nullptr));
                        canvas.save_bmp(fname);
                        // brief flash feedback — just print for now
                        printf("[saved] %s\n", fname);
                    } else if (bid == BTN_STAMP_STAR) {
                        stampPend  = STAMP_STAR;
                        eraserMode = false;
                    } else if (bid == BTN_STAMP_HEART) {
                        stampPend  = STAMP_HEART;
                        eraserMode = false;
                    } else if (bid == BTN_STAMP_SMILEY) {
                        stampPend  = STAMP_SMILEY;
                        eraserMode = false;
                    }
                } else {
                    // canvas click
                    int cx = mx, cy = my - CANVAS_TOP;
                    if (stampPend != STAMP_NONE) {
                        canvas.place_stamp(stampPend, cx, cy, 32,
                                          PALETTE[selColorIdx]);
                        stampPend = STAMP_NONE;
                    } else {
                        Color c = eraserMode
                                  ? Color{255, 255, 255, 255}
                                  : PALETTE[selColorIdx];
                        int rad = eraserMode ? 12 : brushSz;
                        canvas.begin_stroke(cx, cy, rad, c);
                        drawing = true;
                    }
                }
                break;
            }

            case SDL_MOUSEMOTION:
                if (drawing && ev.button.button == SDL_BUTTON_LEFT) {
                    int cx = ev.motion.x;
                    int cy = ev.motion.y - CANVAS_TOP;
                    canvas.extend_stroke(cx, cy);
                }
                break;

            case SDL_MOUSEBUTTONUP:
                if (drawing) {
                    canvas.end_stroke();
                    drawing = false;
                }
                break;
            }
        }

        // -- render --
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderClear(ren);

        render_toolbar(ren, selColorIdx, brushSz, eraserMode, stampPend);
        canvas.present();
        canvas.render_current();

        SDL_RenderPresent(ren);
    }

    // ---- cleanup ----
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
