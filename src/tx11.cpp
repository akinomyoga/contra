#define CONTRA_TX11_SUPPORT_HIGHDPI
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/Xft/Xft.h>
#include "ansi/render.hpp"
#include "manager.hpp"
#include "pty.hpp"
#include "context.hpp"
#include <memory>

namespace contra::tx11 {
namespace {

  const char* get_x11_event_name(XEvent const& event) {
    switch (event.type) {
    case ButtonPress: return "ButtonPress";
    case ButtonRelease: return "ButtonRelease";
    case CirculateNotify: return "CirculateNotify";
    case CirculateRequest: return "CirculateRequest";
    case ClientMessage: return "ClientMessage";
    case ColormapNotify: return "ColormapNotify";
    case ConfigureNotify: return "ConfigureNotify";
    case ConfigureRequest: return "ConfigureRequest";
    case CreateNotify: return "CreateNotify";
    case DestroyNotify: return "DestroyNotify";
    case EnterNotify: return "EnterNotify";
    case Expose: return "Expose";
    case FocusIn: return "FocusIn";
    case FocusOut: return "FocusOut";
    case GraphicsExpose: return "GraphicsExpose";
    case GravityNotify: return "GravityNotify";
    case KeyPress: return "KeyPress";
    case KeyRelease: return "KeyRelease";
    case KeymapNotify: return "KeymapNotify";
    case LeaveNotify: return "LeaveNotify";
    case MapNotify: return "MapNotify";
    case MapRequest: return "MapRequest";
    case MappingNotify: return "MappingNotify";
    case MotionNotify: return "MotionNotify";
    case NoExpose: return "NoExpose";
    case PropertyNotify: return "PropertyNotify";
    case ReparentNotify: return "ReparentNotify";
    case ResizeRequest: return "ResizeRequest";
    case SelectionClear: return "SelectionClear";
    case SelectionNotify: return "SelectionNotify";
    case SelectionRequest: return "SelectionRequest";
    case UnmapNotify: return "UnmapNotify";
    case VisibilityNotify: return "VisibilityNotify";
    default:
      static char name[16];
      std::sprintf(name, "unknown%d", event.type);
      return name;
    }
  }

  struct x11_color_manager_t {
    Display* m_display;
    Colormap m_cmap;
    std::size_t npixel = 0;
    ansi::color_t color256[256];
    unsigned long color256_to_pixel[256];

    void initialize_colors() {
      std::size_t index = 0;
      XColor xcolor {};
      auto _alloc = [this, &index, &xcolor] (byte r, byte g, byte b) {
        xcolor.red   = r | r << 8;
        xcolor.green = g | g << 8;
        xcolor.blue  = b | b << 8;
        XAllocColor(m_display, m_cmap, &xcolor);
        color256[index] = contra::ansi::rgb(
          (xcolor.red + 128) / 257,
          (xcolor.green + 128) / 257,
          (xcolor.blue + 128) / 257);
        color256_to_pixel[index] = xcolor.pixel;
        index++;
      };
      auto _intensity = [] (int i) -> byte { return i == 0 ? 0 : i * 40 + 55; };

      // 8colors
      for (int i = 0; i < 8; i++)
        _alloc(i & 1 ? 0x80 : 0, i & 2 ? 0x80 : 0, i & 4 ? 0x80 : 0);
      // highlight 8 colors
      for (int i = 0; i < 8; i++)
        _alloc(i & 1 ? 0xFF : 0, i & 2 ? 0xFF : 0, i & 4 ? 0xFF : 0);
      // 6x6x6 colors
      for (int r = 0; r < 6; r++) {
        auto const R = _intensity(r);
        for (int g = 0; g < 6; g++) {
          auto const G = _intensity(g);
          for (int b = 0; b < 6; b++) {
            auto const B = _intensity(b);
            _alloc(R, G, B);
          }
        }
      }
      // 24 grayscale
      for (int k = 0; k < 24; k++) {
        byte const K = k * 10 + 8;
        _alloc(K, K, K);
      }

      mwg_assert(index == 256);
    }
  public:
    x11_color_manager_t(Display* display) {
      this->m_display = display;
      int const screen = DefaultScreen(display);
      this->m_cmap = DefaultColormap(display, screen);
      this->initialize_colors();
    }
    ~x11_color_manager_t() {
      XFreeColors(m_display, m_cmap, &color256_to_pixel[0], std::size(color256_to_pixel), 0);
    }
  private:
    template<typename T>
    static std::pair<T, T> get_minmax3(T a, T b, T c) {
      if (a > b) std::swap(a, b);
      if (b <= c) return {a, c};
      return {std::min(a, c), b};
    }
  public:
    int index(ansi::color_t color) {
      byte r = contra::ansi::rgba2r(color);
      byte g = contra::ansi::rgba2g(color);
      byte b = contra::ansi::rgba2b(color);
      auto [mn, mx] = get_minmax3(r, g, b);
      if (mx - mn < 20) {
        byte k = (r + g + b) / 3;
        k = (k + 2) / 10;
        return k == 0 ? 16 : k == 25 ? 231 : 231 + k;
      } else {
        r = (r - 24) / 40;
        g = (g - 24) / 40;
        b = (b - 24) / 40;
        return 16 + r * 36 + g * 6 + b;
      }
    }
    unsigned long pixel(ansi::color_t color) {
      return color256_to_pixel[index(color)];
    }
  };

  class xft_font_factory {
  public:
    typedef XftFont* font_type;

  private:
    using font_t = ansi::font_t;

    Display* m_display = NULL;
    int m_screen = 0;

    std::string m_fontnames[16];
    int m_font_padding;

  public:
    xft_font_factory(contra::app::context& actx) {
      // 既定ではどのフォントが入っているのか分からないので monospace にする
      m_fontnames[0]  = "monospace";
      m_fontnames[1]  = "monospace";
      m_fontnames[2]  = "monospace";
      m_fontnames[3]  = "monospace";
      m_fontnames[4]  = "monospace";
      m_fontnames[5]  = "monospace";
      m_fontnames[6]  = "monospace";
      m_fontnames[7]  = "monospace";
      m_fontnames[8]  = "monospace";
      m_fontnames[9]  = "monospace";
      m_fontnames[10] = "monospace";
      actx.read("tx11_font_default", m_fontnames[0] );
      actx.read("tx11_font_ansi1"  , m_fontnames[1] );
      actx.read("tx11_font_ansi2"  , m_fontnames[2] );
      actx.read("tx11_font_ansi3"  , m_fontnames[3] );
      actx.read("tx11_font_ansi4"  , m_fontnames[4] );
      actx.read("tx11_font_ansi5"  , m_fontnames[5] );
      actx.read("tx11_font_ansi6"  , m_fontnames[6] );
      actx.read("tx11_font_ansi7"  , m_fontnames[7] );
      actx.read("tx11_font_ansi8"  , m_fontnames[8] );
      actx.read("tx11_font_ansi9"  , m_fontnames[9] );
      actx.read("tx11_font_frak"   , m_fontnames[10]);
      actx.read("tx11_font_padding", m_font_padding = 0);
    }
    bool setup_display(Display* display) {
      if (m_display == display) return false;
      m_display = display;
      m_screen = DefaultScreen(display);
      return true;
    }

  public:
    font_type create_font(font_t font, ansi::font_metric_t const& metric) const {
      using namespace contra::ansi;

      const char* fontname = m_fontnames[0].c_str();
      if (font_t const face = (font & font_face_mask) >> font_face_shft)
        if (const char* const name = m_fontnames[face].c_str())
          fontname = name;

      int slant = FC_SLANT_ROMAN;
      int weight = FC_WEIGHT_NORMAL;
      double height = metric.height();
      double width = metric.width();
      FcMatrix matrix;

      if (font & font_flag_italic)
        slant = FC_SLANT_OBLIQUE;

      switch (font & font_weight_mask) {
      case font_weight_bold: weight = FC_WEIGHT_BOLD; break;
      case font_weight_faint: weight = FC_WEIGHT_THIN; break;
      case font_weight_heavy: weight = FC_WEIGHT_HEAVY; break;
      }

      if (font & font_flag_small) {
        height = metric.small_height();
        width = metric.small_width();
      }
      if (font & font_decdhl) height *= 2;
      if (font & font_decdwl) width *= 2;

      // Note: ぴったりだと隣の文字とくっついてしまう様だ (使っているフォントの問題?)
      if (!(font & font_flag_small) && m_font_padding) {
        int const padding = std::min(m_font_padding, (int) std::ceil(width / 4));
        height -= padding;
        width -= padding;
      }
      double const xscale = std::floor(std::max(width, 1.0)) / (height * 0.5);

      if (font_t const rotation = (font & font_rotation_mask) >> font_rotation_shft) {
        double const radian = 0.25 * M_PI * rotation;
        double const c = std::cos(radian), s = std::sin(radian);
        matrix.xx = c * xscale;
        matrix.xy = -s;
        matrix.yx = s * xscale;
        matrix.yy = c;
      } else {
        matrix.xx = xscale;
        matrix.yy = 1.0;
        matrix.xy = 0.0;
        matrix.yx = 0.0;
      }

      return XftFontOpen(
        m_display, m_screen,
        XFT_FAMILY, XftTypeString, fontname,
        XFT_PIXEL_SIZE, XftTypeDouble, height,
        XFT_SLANT, XftTypeInteger, slant,
        XFT_WEIGHT, XftTypeInteger, weight,
        XFT_MATRIX, XftTypeMatrix, &matrix,
        NULL);
    }
    void delete_font(font_type font) const {
      ::XftFontClose(m_display, font);
    }
  };

  struct xft_character_buffer {
    using coord_t = ansi::coord_t;
    std::vector<byte> characters;
    std::vector<coord_t> position;
    std::vector<std::size_t> next;
  public:
    bool empty() const { return position.empty(); }
    void add_char(std::uint32_t code, coord_t x, bool is_extension) {
      contra::encoding::put_u8(code, characters);
      if (is_extension && position.size()) {
        next.back() = characters.size();
      } else {
        next.push_back(characters.size());
        position.push_back(x);
      }
    }
    void clear() {
      characters.clear();
      position.clear();
      next.clear();
    }
    void reserve(std::size_t capacity) {
      characters.reserve(capacity * 3);
      position.reserve(capacity);
    }
  };

  class xft_text_drawer_t {
    using font_t = ansi::font_t;
    using coord_t = ansi::coord_t;
    using color_t = ansi::color_t;

    Display* m_display = NULL;
    Colormap m_cmap = 0;
    Visual* m_visual = NULL;
    x11_color_manager_t* m_color_manager = NULL;

    ansi::font_manager_t<xft_font_factory> font_manager;

    Drawable m_drawable = 0;
    XftDraw* m_draw = NULL;

    Region m_clip_region = NULL;
  public:
    void clip_rectangle(coord_t x1, coord_t y1, coord_t x2, coord_t y2) {
      if (m_clip_region) XDestroyRegion(m_clip_region);
      m_clip_region = XCreateRegion();
      XRectangle rect;
      rect.x = x1;
      rect.y = y1;
      rect.width = x2 - x1;
      rect.height = y2 - y1;
      XUnionRectWithRegion(&rect, m_clip_region, m_clip_region);
      XftDrawSetClip(m_draw, m_clip_region);
    }
    void clip_clear() {
      XftDrawSetClip(m_draw, 0);
    }

  private:
    void release() {
      if (m_draw) {
        XftDrawDestroy(m_draw);
        m_draw = NULL;
      }
      if (m_clip_region) {
        XDestroyRegion(m_clip_region);
        m_clip_region = NULL;
      }
    }
  public:
    xft_text_drawer_t(contra::app::context& actx): font_manager(actx) {}
    ~xft_text_drawer_t() { release(); }
    void initialize(Display* display, x11_color_manager_t* color_manager) {
      m_display = display;
      int const screen = DefaultScreen(display);
      m_cmap = DefaultColormap(m_display, screen);
      m_visual = DefaultVisual(m_display, screen);
      m_color_manager = color_manager;
      if (font_manager.factory().setup_display(display))
        font_manager.clear();
    }
    void set_size(coord_t width, coord_t height) {
      font_manager.set_size(width, height);
    }

  private:
    void update_target(Drawable drawable) {
      if (drawable == m_drawable) return;
      release();
      this->m_drawable = drawable;
      this->m_draw = ::XftDrawCreate(m_display, drawable, m_visual, m_cmap);
    }

  private:
    void render(coord_t x, coord_t y, xft_character_buffer const& buff, XftFont* xftFont, color_t color) {
      XftColor xftcolor;
      xftcolor.pixel = m_color_manager ? m_color_manager->pixel(color) : contra::ansi::rgba2bgr(color);
      xftcolor.color.red   = 257 * contra::ansi::rgba2r(color);
      xftcolor.color.green = 257 * contra::ansi::rgba2g(color);
      xftcolor.color.blue  = 257 * contra::ansi::rgba2b(color);
      xftcolor.color.alpha = 0xFFFF;

      std::size_t index = 0;
      for (std::size_t i = 0, iN = buff.position.size(); i < iN; i++) {
        ::XftDrawStringUtf8(
          m_draw, &xftcolor, xftFont,
          x + buff.position[i], y, (FcChar8*) &buff.characters[index], buff.next[i] - index);
        index = buff.next[i];
      }
    }
  public:
    void draw_characters(Drawable drawable, coord_t x1, coord_t y1, xft_character_buffer const& buff, font_t font, color_t color) {
      this->update_target(drawable);

      coord_t const h = font_manager.get_font_height(font);
      XftFont* const xftFont = font_manager.get_font(font);
      double const xftHeight = std::max({xftFont->ascent + xftFont->descent, xftFont->height, 1});
      render(x1, y1 + h - h * xftFont->descent / xftHeight, buff, xftFont, color);
    }
    void draw_rotated_characters(
      Drawable drawable,
      coord_t x0, coord_t y0, coord_t dx, coord_t dy, coord_t w,
      xft_character_buffer const& buff, font_t font, color_t color
    ) {
      this->update_target(drawable);
      coord_t const h = font_manager.get_font_height(font);
      font_t const sco = (font & ansi::font_rotation_mask) >> ansi::font_rotation_shft;

      XftFont* const xftFont0 = font_manager.get_font(font & ~ansi::font_rotation_mask);
      double const xftHeight = std::max({xftFont0->ascent + xftFont0->descent, xftFont0->height, 1});
      double const yshift = h * xftFont0->descent / xftHeight + (sco & 1 ? 1.0 : 0.0);

      double const angle = -(M_PI / 4.0) * sco;
      double const xc1 = 0.5 * w - dx;
      double const yc1 = -0.5 * h + yshift;
      double const xc2 = xc1 * std::cos(angle) - yc1 * std::sin(angle);
      double const yc2 = xc1 * std::sin(angle) + yc1 * std::cos(angle);
      int const rot_dx = (int) std::round(xc2 - xc1);
      int const rot_dy = (int) std::round(yc2 - yc1 + yshift);
      x0 += -rot_dx;
      y0 += -rot_dy;

      render(x0 + dx, y0 + dy + h, buff, font_manager.get_font(font), color);
    }
  };


  class tx11_graphics_t;

  class tx11_graphics_buffer {
    using coord_t = ansi::coord_t;

  private:
    Display* m_display = NULL;
    int m_screen = 0;
    std::unique_ptr<x11_color_manager_t> m_color_manager;
    xft_text_drawer_t m_text_drawer;
  public:
    Display* display() const { return m_display; }
    x11_color_manager_t* color_manager() { return m_color_manager.get(); }
    xft_text_drawer_t& text_drawer() { return m_text_drawer; }

    void initialize(Display* display) {
      this->m_display = display;
      this->m_screen = DefaultScreen(display);

      Visual* const visual = DefaultVisual(m_display, m_screen);
      if (visual->c_class != TrueColor)
        m_color_manager = std::make_unique<x11_color_manager_t>(m_display);

      this->m_text_drawer.initialize(display, color_manager());
    }


  private:
    coord_t m_width = -1, m_height = -1;
  public:
    coord_t width() { return m_width; }
    coord_t height() { return m_height; }
    void update_window_size(coord_t width, coord_t height, bool* out_resized = NULL) {
      bool const is_resized = width != m_width || height != m_height;
      if (is_resized) {
        release();
        m_width = width;
        m_height = height;
      }
      if (out_resized) *out_resized = is_resized;
    }

  private:
    Window m_window = 0;
    GC m_gc = NULL;
  public:
    void setup(Window hWnd, GC hdc) {
      this->m_window = hWnd;
      this->m_gc = hdc;
    }

  private:
    struct layer_t {
      GC gc = NULL;
      Pixmap pixmap = 0;
    };
    std::vector<layer_t> m_layers;
    void release() {
      for (layer_t& layer : m_layers) release_layer(layer);
      m_layers.clear();
    }
    void release_layer(layer_t& layer) {
      if (layer.gc) {
        XFreeGC(m_display, layer.gc);
        layer.gc = NULL;
      }
      if (layer.pixmap) {
        XFreePixmap(m_display, layer.pixmap);
        layer.pixmap = 0;
      }
    }
    void initialize_layer(layer_t& layer) {
      release_layer(layer);
      layer.pixmap = XCreatePixmap(m_display, m_window, m_width, m_height, DefaultDepth(m_display, m_screen));

      // create GC
      XGCValues gcparams;
      gcparams.graphics_exposures = false;
      layer.gc = XCreateGC(m_display, layer.pixmap, GCGraphicsExposures, &gcparams);
    }
  public:
    typedef layer_t context_t;
    context_t layer(std::size_t index) {
      if (index < m_layers.size() && m_layers[index].gc)
        return m_layers[index];

      if (index >= m_layers.size()) m_layers.resize(index + 1);
      layer_t& layer = m_layers[index];
      initialize_layer(layer);
      return layer;
    }

  public:
    tx11_graphics_buffer(contra::app::context& actx, ansi::window_state_t& wstat): m_text_drawer(actx) {
      m_text_drawer.set_size(wstat.m_xunit, wstat.m_yunit);
    }
    ~tx11_graphics_buffer() {
      release();
    }

    void reset_size(ansi::window_state_t const& wstat) {
      m_text_drawer.set_size(wstat.m_xunit, wstat.m_yunit);
    }

  public:
    void bitblt(
      context_t ctx1, coord_t x1, coord_t y1, coord_t w, coord_t h,
      context_t ctx2, coord_t x2, coord_t y2
    ) {
      ::XCopyArea(m_display, ctx2.pixmap, ctx1.pixmap, ctx1.gc, x2, y2, w, h, x1, y1);
    }
    void render(coord_t x1, coord_t y1, coord_t w, coord_t h, context_t ctx2, coord_t x2, coord_t y2) {
      ::XCopyArea(m_display, ctx2.pixmap, m_window, m_gc, x2, y2, w, h, x1, y1);
    }

    typedef tx11_graphics_t graphics_t;

  };

  class tx11_graphics_t {
    using coord_t = ansi::coord_t;
    using color_t = ansi::color_t;
    using font_t = ansi::font_t;

    Display* m_display = NULL;
    x11_color_manager_t* color_manager = nullptr;
    xft_text_drawer_t& text_drawer;

    Drawable m_drawable = 0;
    GC m_gc = NULL;
    GC m_gc_delete = NULL;

    void release() {
      if (m_stipple) {
        XFreePixmap(m_display, m_stipple);
        m_stipple = 0;
      }
      if (m_gc_delete) {
        XFreeGC(m_display, m_gc_delete);
        m_gc_delete = NULL;
      }
    }

  public:
    typedef xft_character_buffer character_buffer;

  public:
    tx11_graphics_t(tx11_graphics_buffer& gbuffer, Drawable drawable):
      m_display(gbuffer.display()),
      color_manager(gbuffer.color_manager()),
      text_drawer(gbuffer.text_drawer())
    {
      XGCValues gcparams;
      gcparams.graphics_exposures = false;
      this->m_drawable = drawable;
      this->m_gc_delete = XCreateGC(m_display, drawable, GCGraphicsExposures, &gcparams);
      this->m_gc = m_gc_delete;
    }
    tx11_graphics_t(tx11_graphics_buffer& gbuffer, tx11_graphics_buffer::context_t const& context):
      m_display(gbuffer.display()),
      color_manager(gbuffer.color_manager()),
      text_drawer(gbuffer.text_drawer())
    {
      this->m_drawable = context.pixmap;
      this->m_gc = context.gc;
    }

    ~tx11_graphics_t() {
      this->release();
    }

    GC gc() const { return m_gc; }

  private:
    unsigned long color2pixel(color_t color) const {
      return color_manager ? color_manager->pixel(color) : contra::ansi::rgba2bgr(color);
    }
    void set_foreground(color_t color) {
      XSetForeground(m_display, m_gc, color2pixel(color));
    }

  public:
    void clip_rectangle(coord_t x1, coord_t y1, coord_t x2, coord_t y2) {
      XRectangle rect;
      rect.x = 0;
      rect.y = 0;
      rect.width = x2 - x1;
      rect.height = y2 - y1;
      XSetClipRectangles(m_display, m_gc, x1, y1, &rect, 1, YXBanded);
      text_drawer.clip_rectangle(x1, y1, x2, y2);
    }
    void clip_clear() {
      XSetClipMask(m_display, m_gc, None);
      text_drawer.clip_clear();
    }

  private:
    std::vector<XPoint> xpoints;
    Pixmap m_stipple = 0;
    Pixmap get_stipple() {
      if (!m_stipple) {
        m_stipple = XCreatePixmap(m_display, m_drawable, 2, 2, 1);
        GC gc2 = XCreateGC(m_display, m_stipple, 0, 0);
        XSetForeground(m_display, gc2, 0);
        XFillRectangle(m_display, m_stipple, gc2, 0, 0, 2, 2);
        XSetForeground(m_display, gc2, 1);
        XDrawPoint(m_display, m_stipple, gc2, 0, 0);
        XDrawPoint(m_display, m_stipple, gc2, 1, 1);
        XFreeGC(m_display, gc2);
      }
      return m_stipple;
    }
  public:
    void fill_rectangle(coord_t x1, coord_t y1, coord_t x2, coord_t y2, color_t color) {
      set_foreground(color);
      ::XFillRectangle(m_display, m_drawable, m_gc, x1, y1, x2 - x1, y2 - y1);
    }
    void invert_rectangle(coord_t x1, coord_t y1, coord_t x2, coord_t y2) {
      XGCValues params;
      params.function = GXcopyInverted;
      XChangeGC(m_display, m_gc, GCFunction, &params);
      XCopyArea(m_display, m_drawable, m_drawable, m_gc, x1, y1, x2 - x1, y2 - y1, x1, y1);
      params.function = GXcopy; // default
      XChangeGC(m_display, m_gc, GCFunction, &params);
    }
    void checked_rectangle(coord_t x1, coord_t y1, coord_t x2, coord_t y2, color_t color) {
      XGCValues params;
      params.foreground = color2pixel(color);
      params.fill_style = FillStippled;
      params.stipple = get_stipple();
      XChangeGC(m_display, m_gc, GCForeground | GCStipple | GCFillStyle, &params);
      XFillRectangle(m_display, m_drawable, m_gc, x1, y1, x2 - x1, y2 - y1);
      params.fill_style = FillSolid; // default
      XChangeGC(m_display, m_gc, GCFillStyle, &params);
    }
    void draw_ellipse(coord_t x1, coord_t y1, coord_t x2, coord_t y2, color_t color, int line_width) {
      XGCValues params;
      params.foreground = color2pixel(color);
      params.line_width = line_width;
      XChangeGC(m_display, m_gc, GCForeground | GCLineWidth, &params);
      XDrawArc(m_display, m_drawable, m_gc, x1, y1, x2 - x1, y2 - y1, 0 * 64, 360 * 64);
    }
    void fill_ellipse(coord_t x1, coord_t y1, coord_t x2, coord_t y2, color_t color) {
      set_foreground(color);
      XFillArc(m_display, m_drawable, m_gc, x1, y1, x2 - x1, y2 - y1, 0 * 64, 360 * 64);
    }
    void fill_polygon(coord_t const (*points)[2], std::size_t count, color_t color) {
      set_foreground(color);
      xpoints.resize(count);
      for (std::size_t i = 0; i < count; i++) {
        xpoints[i].x = points[i][0];
        xpoints[i].y = points[i][1];
      }
      XFillPolygon(m_display, m_drawable, m_gc, &xpoints[0], count, Nonconvex, CoordModeOrigin);
    }
    void draw_line(coord_t x1, coord_t y1, coord_t x2, coord_t y2, color_t color, int line_width) {
      XGCValues params;
      params.foreground = color2pixel(color);
      params.line_width = line_width;
      params.cap_style = CapRound;
      XChangeGC(m_display, m_gc, GCForeground | GCLineWidth | GCCapStyle, &params);
      XDrawLine(m_display, m_drawable, m_gc, x1, y1, x2, y2);
    }

  public:
    void draw_characters(coord_t x1, coord_t y1, character_buffer const& buff, font_t font, color_t color) {
      // set_foreground(color);
      // contra_unused(font);
      // coord_t x = x1, y = y1 + wstat.m_yunit - 1;
      // for (std::size_t i = 0, iN = buff.characters.size(); i < iN; i++) {
      //   ::XDrawString(m_display, m_drawable, m_gc, x, y, &buff.characters[i], 1);
      //   x += buff.progress[i];
      // }

      text_drawer.draw_characters(m_drawable, x1, y1, buff, font, color);
    }
    void draw_rotated_characters(
      coord_t x0, coord_t y0, coord_t dx, coord_t dy, coord_t width,
      character_buffer const& buff, font_t font, color_t color
    ) {
      text_drawer.draw_rotated_characters(m_drawable, x0, y0, dx, dy, width, buff, font, color);
    }
    void draw_text(coord_t x1, coord_t y1, character_buffer const& buff, font_t font, color_t color) {
      draw_characters(x1, y1, buff, font, color);
    }
  };

  struct tx11_settings: public ansi::window_settings_base {
    const char* m_env_term = "xterm-256color";
    const char* m_env_shell = "/bin/bash";
  };

  class tx11_window_t: term::terminal_events {
    contra::app::context& actx;

    ansi::window_state_t wstat;
    ansi::status_tracer_t m_tracer;
    term::terminal_manager manager;
    ansi::window_renderer_t<tx11_graphics_t> renderer { wstat };

    ::Display* display = NULL;
    ::Window main = 0;
    ::Atom WM_DELETE_WINDOW;
    tx11_settings settings;
    tx11_graphics_buffer gbuffer { actx, wstat };

  public:
    tx11_window_t(contra::app::context& actx): actx(actx) {
      // size and dimension
      wstat.configure_metric(actx);
      manager.reset_size(wstat.m_col, wstat.m_row, wstat.m_xunit, wstat.m_yunit);

      // other settings
      settings.configure(actx);

      std::fill(std::begin(m_kbflags), std::end(m_kbflags), 0);
    }
    ~tx11_window_t() {}

  private:
    bool is_session_ready() { return display && manager.is_active(); }

#ifdef CONTRA_TX11_SUPPORT_HIGHDPI
    double get_dpi(Display* display) {
      const char* dpi_resource = ::XGetDefault(display, "Xft", "dpi");
      if (!dpi_resource) return 0.0;
      char* endptr;
      double const result = std::strtod(dpi_resource, &endptr);
      if (endptr && *endptr) return 0.0;
      return result;
    }
#endif

  public:
    Display* display_handle() { return this->display; }
    Window window_handle() const { return this->main; }
    bool create_window() {
      // Setup X11
      this->display = ::XOpenDisplay(NULL);
      if (this->display == NULL) {
        std::fprintf(stderr, "contra: Failed to open DISPLAY=%s\n", std::getenv("DISPLAY"));
        return false;
      }
#ifdef CONTRA_TX11_SUPPORT_HIGHDPI
      if (double const dpi = get_dpi(display)) {
        double const dpiscale = std::max(1.0, dpi / 96.0);
        wstat.configure_metric(actx, dpiscale);
        gbuffer.reset_size(wstat);
        manager.initialize_zoom(wstat.m_xunit, wstat.m_yunit);
        manager.reset_size(wstat.m_col, wstat.m_row, wstat.m_xunit, wstat.m_yunit);
      }
#endif

      int const screen = DefaultScreen(display);
      Window const root = RootWindow(display, screen);

      unsigned long const black = BlackPixel(display, screen);
      unsigned long const white = WhitePixel(display, screen);

      // Main Window
      this->m_window_width = wstat.calculate_client_width();
      this->m_window_height = wstat.calculate_client_height();
      this->main = XCreateSimpleWindow(display, root, 100, 100, m_window_width, m_window_height, 1, black, white);
      {
        // Properties
        XTextProperty win_name;
        win_name.value = (unsigned char*) "Contra/X11";
        win_name.encoding = XA_STRING;
        win_name.format = 8;
        win_name.nitems = std::strlen((char const*) win_name.value);
        ::XSetWMName(display, this->main, &win_name);

        // Events
        XSelectInput(display, this->main, ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask);
      }

      // #D0162 何だかよく分からないが終了する時はこうするらしい。
      this->WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", False);
      XSetWMProtocols(display, this->main, &this->WM_DELETE_WINDOW, 1);

      this->gbuffer.initialize(this->display);

      XAutoRepeatOn(display);
      XkbSetDetectableAutoRepeat(display, True, NULL); // #D0238
      this->manager.set_events(static_cast<term::terminal_events&>(*this));
      return true;
    }

  private:
    ansi::coord_t m_window_width = 100;
    ansi::coord_t m_window_height = 100;
  public:
    void process_window_resize() {
      if (!is_session_ready()) return;

      XWindowAttributes attrs;
      XGetWindowAttributes(this->display, this->main, &attrs);
      this->m_window_width = attrs.width;
      this->m_window_height = attrs.height;

      ansi::curpos_t const new_col = std::max(1, (attrs.width - 2 * wstat.m_xframe) / wstat.m_xunit);
      ansi::curpos_t const new_row = std::max(1, (attrs.height - 2 * wstat.m_yframe) / wstat.m_yunit);
      if (new_col != wstat.m_col || new_row != wstat.m_row) {
        wstat.m_col = new_col;
        wstat.m_row = new_row;
        manager.reset_size(new_col, new_row);
      }
    }

  private:
    friend class ansi::window_renderer_t<tx11_graphics_t>;
    void unset_cursor_timer() {}
    void reset_cursor_timer() {}

  private:
    void render_window() {
      tx11_graphics_t g(gbuffer, this->main);
      if (!is_session_ready()) {
        const char* message = "Hello, world!";
        XDrawString(display, this->main, g.gc(), 3, 15, message, std::strlen(message));
      } else {
        bool resized = true;
        gbuffer.setup(this->main, g.gc());
        gbuffer.update_window_size(this->m_window_width, this->m_window_height, &resized);
        renderer.render_view(*this, gbuffer, manager.app().view(), resized);
      }
      XFlush(display);
    }

  private:
    void process_input(std::uint32_t key) {
      //contra::print_key(key, stderr);
      manager.input_key(key);
    }

  private:
    enum extended_key_flags {
      toggle_capslock   = 0x100,
      toggle_numlock    = 0x200,
      toggle_scrolllock = 0x400,
    };
    key_t get_modifiers() {
      char table[32];
      ::XQueryKeymap(display, table);

      auto const key_state = [&] (KeySym sym) -> bool {
        KeyCode const keycode = XKeysymToKeycode(display, sym);
        int mask = 1 << (keycode & 7);
        return table[keycode >> 3] & mask;
      };

      key_t ret = 0;
      if (key_state(XK_Shift_L)) ret |= settings.term_mod_lshift;
      if (key_state(XK_Shift_R)) ret |= settings.term_mod_rshift;
      if (key_state(XK_Control_L)) ret |= settings.term_mod_lcontrol;
      if (key_state(XK_Control_R)) ret |= settings.term_mod_rcontrol;
      if (key_state(XK_Alt_L)) ret |= settings.term_mod_lalter;
      if (key_state(XK_Alt_R)) ret |= settings.term_mod_ralter;
      if (key_state(XK_Meta_L)) ret |= settings.term_mod_lmeta;
      if (key_state(XK_Meta_R)) ret |= settings.term_mod_rmeta;
      if (key_state(XK_Super_L)) ret |= settings.term_mod_lsuper;
      if (key_state(XK_Super_R)) ret |= settings.term_mod_rsuper;
      if (key_state(XK_Hyper_L)) ret |= settings.term_mod_lhyper;
      if (key_state(XK_Hyper_R)) ret |= settings.term_mod_rhyper;
      if (key_state(XK_Menu)) ret |= settings.term_mod_menu;

      unsigned indicators = 0;
      if (XkbGetIndicatorState(display, XkbUseCoreKbd, &indicators) == Success) {
        if (indicators & 1) ret |= toggle_capslock;
        if (indicators & 2) ret |= toggle_numlock;
        if (indicators & 4) ret |= toggle_scrolllock; // 効かない様だ @ Cygwin
      }

      return ret;
    }


    byte m_kbflags[256];
    enum kbflag_flags {
      kbflag_pressed = 1,
    };
    void process_keyup(KeyCode keycode) {
      m_kbflags[(byte) keycode] &= ~kbflag_pressed;
    }
    bool process_key(KeyCode keycode, key_t modifiers) {
      // Note #D0238: autorepeat 検出
      if (m_kbflags[(byte) keycode] & kbflag_pressed)
        modifiers |= modifier_autorepeat;
      else
        m_kbflags[(byte) keycode] |= kbflag_pressed;

      key_t code = XkbKeycodeToKeysym(display, keycode, 0, 0);
      if (ascii_sp <= code && code <= ascii_tilde) {
        // 前提: G1 の文字は XK は ASCII/Unicode に一致している。
        mwg_assert(XK_space == ascii_sp && XK_asciitilde == ascii_tilde);

        // 観察した感じ XkbdKeycodeToKeysym に 0 を渡していれば此処には来ない?
        if (ascii_A <= code && code <= ascii_Z) code += ascii_a - ascii_A;

        if (ascii_a <= code && code <= ascii_z) {
          bool const modified = modifiers & ~modifier_shift & _modifier_mask;
          if (!modified) {
            bool const capslock = modifiers & toggle_capslock;
            bool const shifted = modifiers & modifier_shift;
            if (capslock != shifted) code += ascii_A - ascii_a;
            modifiers &= ~modifier_shift;
          }
        } else {
          if (modifiers & modifier_shift) {
            key_t code2 = XkbKeycodeToKeysym(display, keycode, 0, 1) & _character_mask;
            if (code2 != code) {
              modifiers &= ~modifier_shift;
              code = code2;
            }
          }
        }
        process_input(code | (modifiers & _modifier_mask));
        return true;
      } else if (XK_F1 <= code && code < XK_F24) { // X11 では実は35まで実はあったりする。
        process_input((code - XK_F1 + key_f1) | (modifiers & _modifier_mask));
        return true;
      } else {
        key_t numlock = 0;
        switch (code) {
        case XK_BackSpace: code = ascii_bs;   goto function_key;
        case XK_Tab      : code = ascii_ht;   goto function_key;
        case XK_Return   : code = ascii_cr;   goto function_key;
        case XK_Escape   : code = ascii_esc;  goto function_key;
        case XK_Delete   : code = key_delete; goto function_key;
        case XK_Insert   : code = key_insert; goto function_key;
        case XK_Prior    : code = key_prior;  goto function_key;
        case XK_Next     : code = key_next;   goto function_key;
        case XK_End      : code = key_end;    goto function_key;
        case XK_Home     : code = key_home;   goto function_key;
        case XK_Left     : code = key_left;   goto function_key;
        case XK_Up       : code = key_up;     goto function_key;
        case XK_Right    : code = key_right;  goto function_key;
        case XK_Down     : code = key_down;   goto function_key;
        function_key:
          process_input(code | (modifiers & _modifier_mask));
          return true;

        case XK_KP_Insert  : code = key_insert; numlock = ascii_0; goto keypad;
        case XK_KP_End     : code = key_end;    numlock = ascii_1; goto keypad;
        case XK_KP_Down    : code = key_down;   numlock = ascii_2; goto keypad;
        case XK_KP_Next    : code = key_next;   numlock = ascii_3; goto keypad;
        case XK_KP_Left    : code = key_left;   numlock = ascii_4; goto keypad;
        case XK_KP_Begin   : code = key_begin;  numlock = ascii_5; goto keypad;
        case XK_KP_Right   : code = key_right;  numlock = ascii_6; goto keypad;
        case XK_KP_Home    : code = key_home;   numlock = ascii_7; goto keypad;
        case XK_KP_Up      : code = key_up;     numlock = ascii_8; goto keypad;
        case XK_KP_Prior   : code = key_prior;  numlock = ascii_9; goto keypad;
        case XK_KP_Delete  : code = key_delete; numlock = key_kpdec; goto keypad;
        keypad:
          if ((modifiers & toggle_numlock) != (modifiers & modifier_shift)) code = numlock;
          modifiers &= ~modifier_shift;
          goto function_key;

        case XK_KP_Divide  : code = key_kpdiv; goto function_key;
        case XK_KP_Multiply: code = key_kpmul; goto function_key;
        case XK_KP_Subtract: code = key_kpsub; goto function_key;
        case XK_KP_Add     : code = key_kpadd; goto function_key;
        case XK_KP_Enter   : code = key_kpent; goto function_key;
        default:
          // std::printf("ksym=%04x:XK_%s mods=%08x\n", code, XKeysymToString((KeySym) code), modifiers);
          // std::fflush(stdout);
          break;
        }
      }
      return false;
    }
    void process_event(XEvent const& event) {
      switch (event.type) {
      case Expose:
        if (event.xexpose.count == 0)
          render_window();
        break;
      case KeyPress:
        process_key(event.xkey.keycode, get_modifiers());
        break;
      case KeyRelease:
        process_keyup(event.xkey.keycode);
        break;

      case ClientMessage: // #D0162
        if ((Atom) event.xclient.data.l[0] == WM_DELETE_WINDOW)
          XDestroyWindow(display, this->main);
        break;
      case DestroyNotify: // #D0162
        ::XSetCloseDownMode(display, DestroyAll);
        ::XCloseDisplay(display);
        this->display = NULL;
        break;

      case ConfigureNotify:
        this->process_window_resize();
        break;

      case MapNotify:
      case ReparentNotify:
        // 無視
        break;

      default:
        std::fprintf(stderr, "Event %s (%d)\n", get_x11_event_name(event), event.type);
        break;
      }
    }

  private:
    // この部分は twin.cpp からのコピー。後で整理する必要がある。
    static void exec_error_handler(int errno1, std::uintptr_t) {
      char const* msg = std::strerror(errno1);
      std::ostringstream buff;
      buff << "exec: " << msg << " (errno=" << errno1 << ")";
      //::MessageBoxA(NULL, buff.str().c_str(), "Contra/X11 - exec failed", MB_OK);
    }
    bool add_terminal_session() {
      term::terminal_session_parameters params;
      params.col = wstat.m_col;
      params.row = wstat.m_row;
      params.xunit = wstat.m_xunit;
      params.yunit = wstat.m_yunit;
      params.exec_error_handler = &exec_error_handler;
      actx.read("session_term", params.env["TERM"] = "xterm-256color");
      actx.read("session_shell", params.shell = "/bin/bash");
      std::unique_ptr<term::terminal_application> sess = contra::term::create_terminal_session(params);
      if (!sess) return false;

      contra::ansi::tstate_t& s = sess->state();
      s.m_default_fg_space = contra::ansi::attribute_t::color_space_rgb;
      s.m_default_bg_space = contra::ansi::attribute_t::color_space_rgb;
      s.m_default_fg_color = contra::ansi::rgb(0x00, 0x00, 0x00);
      s.m_default_bg_color = contra::ansi::rgb(0xFF, 0xFF, 0xFF);
      // s.m_default_fg_color = contra::ansi::rgb(0xD0, 0xD0, 0xD0);//@color
      // s.m_default_bg_color = contra::ansi::rgb(0x00, 0x00, 0x00);
      manager.add_app(std::move(sess));
      return true;
    }
    virtual bool create_new_session() override {
      return add_terminal_session();
    }

  public:
    bool do_loop() {
      // CheckIfEvent 用のダミーフィルター
      Bool (*event_filter_proc)(Display*, XEvent*, XPointer) = [] (auto...) -> Bool { return True; };

      if (!this->add_terminal_session()) return false;

      XEvent event;
      while (this->display) {
        bool processed = manager.do_events();
        if (manager.m_dirty) {
          render_window();
          manager.m_dirty = false;
        }

        while (::XCheckIfEvent(display, &event, event_filter_proc, NULL)) {
          processed = true;
          process_event(event);
          if (!display) goto exit;
        }

        if (!manager.is_alive()) break;
        if (!processed)
          contra::term::msleep(10);
      }
    exit:
      manager.terminate();
      return true;
    }
  };
}

  bool run(contra::app::context& actx) {
    contra::tx11::tx11_window_t win(actx);
    if (!win.create_window()) return 1;
    ::XMapWindow(win.display_handle(), win.window_handle());
    ::XFlush(win.display_handle());
    return win.do_loop();
  }
}
