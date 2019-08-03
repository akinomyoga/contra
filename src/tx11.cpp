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
#include <memory>

namespace contra {
namespace tx11 {

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
        color256[index] = contra::dict::rgb(
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
      byte r = contra::dict::rgba2r(color);
      byte g = contra::dict::rgba2g(color);
      byte b = contra::dict::rgba2b(color);
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

  class xft_font_manager_t: public ansi::font_metric_t {
    typedef ansi::font_metric_t base;
    using font_t = ansi::font_t;
    using coord_t = ansi::coord_t;
    typedef XftFont* font_type;

    Display* m_display = NULL;
    int m_screen = 0;

    const char* m_fontnames[16];
    font_type m_fonts[1u << ansi::font_basic_bits];
    std::pair<font_t, font_type> m_cache[ansi::font_cache_count];

  public:
    xft_font_manager_t(): base(7, 14) {
      std::fill(std::begin(m_fonts), std::end(m_fonts), (font_type) NULL);
      std::fill(std::begin(m_cache), std::end(m_cache), std::pair<font_t, font_type>(0u, NULL));
      std::fill(std::begin(m_fontnames), std::end(m_fontnames), (const char*) NULL);

      // My configuration@font
      m_fontnames[0]  = "MeiryoKe_Console";
      m_fontnames[1]  = "MS Gothic,monospace";
      m_fontnames[2]  = "MS Mincho,monospace";
      m_fontnames[3]  = "HGMaruGothicMPRO,monospace"; // HG丸ｺﾞｼｯｸM-PRO
      m_fontnames[4]  = "HGKyokashotai,monospace"; // HG教科書体
      m_fontnames[5]  = "HGGyoshotai,monospace"; // HG行書体
      m_fontnames[6]  = "HGSeikaishotaiPRO,monospace"; // HG正楷書体-PRO
      m_fontnames[7]  = "HGSoeiKakupoptai,monospace"; // HG創英角ﾎﾟｯﾌﾟ体
      m_fontnames[8]  = "HGGothicM,monospace"; // HGｺﾞｼｯｸM
      // m_fontnames[9]  = "HGMinchoB,monospace"; // HG明朝B
      m_fontnames[9]  = "Times New Roman,monospace"; // HG明朝B
      m_fontnames[10] = "aghtex_mathfrak,monospace";
    }

    void initialize(Display* display) {
      if (m_display == display) return;
      release();
      m_display = display;
      m_screen = DefaultScreen(display);
    }
    void set_size(coord_t width, coord_t height) {
      if (base::width() == width && base::height() == height) return;
      release();
      base::set_size(width, height);
    }

  private:
    font_type create_font(font_t font) const {
      using namespace contra::ansi;

      const char* fontname = m_fontnames[0];
      int slant = FC_SLANT_ROMAN;
      int weight = FC_WEIGHT_NORMAL;
      double height = base::height();
      double width = base::width();
      FcMatrix matrix;

      if (font_t const face = (font & font_face_mask) >> font_face_shft)
        if (const char* const name = m_fontnames[face])
          fontname = name;
      if (font & font_flag_italic)
        slant = FC_SLANT_OBLIQUE;

      switch (font & font_weight_mask) {
      case font_weight_bold: weight = FC_WEIGHT_BOLD; break;
      case font_weight_faint: weight = FC_WEIGHT_THIN; break;
      case font_weight_heavy: weight = FC_WEIGHT_HEAVY; break;
      }

      if (font & font_flag_small) {
        height = small_height();
        width = small_width();
      }
      if (font & font_decdhl) height *= 2;
      if (font & font_decdwl) width *= 2;

      // Note: ぴったりだと隣の文字とくっついてしまう様だ。
      height--;
      width--;
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
    void delete_font(font_type font) {
      ::XftFontClose(m_display, font);
    }

  private:
    void release() {
      for (auto& hfont : m_fonts) {
        if (hfont) delete_font(hfont);
        hfont = NULL;
      }
      auto const init = std::pair<font_t, font_type>(0u, NULL);
      for (auto& pair : m_cache) {
        if (pair.second) delete_font(pair.second);
        pair = init;
      }
    }
  public:
    font_type get_font(font_t font) {
      using namespace contra::ansi;
      font &= ~font_layout_mask;
      if (!(font & ~font_basic_mask)) {
        // basic fonts
        if (!m_fonts[font]) m_fonts[font] = create_font(font);
        return m_fonts[font];
      } else {
        // cached fonts (先頭にある物の方が新しい)
        for (std::size_t i = 0; i < std::size(m_cache); i++) {
          if (m_cache[i].first == font) {
            // 見つかった時は、見つかった物を先頭に移動して、返す。
            font_type const ret = m_cache[i].second;
            std::move_backward(&m_cache[0], &m_cache[i], &m_cache[i + 1]);
            m_cache[0] = std::make_pair(font, ret);
            return ret;
          }
        }
        // 見つからない時は末尾にあった物を削除して、新しく作る。
        if (font_type last = std::end(m_cache)[-1].second) delete_font(last);
        std::move_backward(std::begin(m_cache), std::end(m_cache) - 1, std::end(m_cache));
        font_type const ret = create_font(font);
        m_cache[0] = std::make_pair(font, ret);
        return ret;
      }
    }

    ~xft_font_manager_t() {
      release();
    }
  };

  struct xft_character_buffer {
    using coord_t = ansi::coord_t;
    std::vector<byte> characters;
    std::vector<coord_t> progress;
    std::vector<std::size_t> next;
  public:
    bool empty() const { return progress.empty(); }
    void add_char(std::uint32_t code, coord_t prog) {
      contra::encoding::put_u8(code, characters);
      if (prog == 0 && progress.size()) {
        next.back() = characters.size();
      } else {
        next.push_back(characters.size());
        progress.push_back(prog);
      }
      // if (code < 0x10000) {
      //   characters.push_back(code);
      //   progress.push_back(prog);
      // } else {
      //   // surrogate pair
      //   code -= 0x10000;
      //   characters.push_back(0xD800 | (code >> 10 & 0x3FF));
      //   progress.push_back(prog);
      //   characters.push_back(0xDC00 | (code & 0x3FF));
      //   progress.push_back(0);
      // }
    }
    void shift(coord_t shift) {
      mwg_assert(!progress.empty());
      progress.back() += shift;
    }
    void clear() {
      characters.clear();
      progress.clear();
      next.clear();
    }
    void reserve(std::size_t capacity) {
      characters.reserve(capacity * 3);
      progress.reserve(capacity);
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

    xft_font_manager_t font_manager;

    Drawable m_drawable = 0;
    XftDraw* m_draw = NULL;

  public:
    xft_text_drawer_t() {}

    void initialize(Display* display, x11_color_manager_t* color_manager) {
      m_display = display;
      int const screen = DefaultScreen(display);
      m_cmap = DefaultColormap(m_display, screen);
      m_visual = DefaultVisual(m_display, screen);
      m_color_manager = color_manager;
      font_manager.initialize(display);
    }
    void set_size(coord_t width, coord_t height) {
      font_manager.set_size(width, height);
    }

  private:
    void update_target(Drawable drawable) {
      if (drawable == m_drawable) return;
      this->m_drawable = drawable;
      this->m_draw = ::XftDrawCreate(m_display, drawable, m_visual, m_cmap);
    }

  private:
    void render(coord_t x, coord_t y, xft_character_buffer const& buff, XftFont* xftFont, color_t color) {
      XftColor xftcolor;
      xftcolor.pixel = m_color_manager ? m_color_manager->pixel(color) : contra::dict::rgba2bgr(color);
      xftcolor.color.red   = 257 * contra::dict::rgba2r(color);
      xftcolor.color.green = 257 * contra::dict::rgba2g(color);
      xftcolor.color.blue  = 257 * contra::dict::rgba2b(color);
      xftcolor.color.alpha = 0xFFFF;

      std::size_t index = 0;
      for (std::size_t i = 0, iN = buff.progress.size(); i < iN; i++) {
        ::XftDrawStringUtf8(
          m_draw, &xftcolor, xftFont,
          x, y, (FcChar8*) &buff.characters[index], buff.next[i] - index);
        x += buff.progress[i];
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


  class tx11_graphics_t {
    using coord_t = ansi::coord_t;
    using color_t = ansi::color_t;
    using font_t = ansi::font_t;

    Display* m_display = NULL;
    Drawable m_drawable = 0;
    GC m_gc = NULL;

    ansi::window_state_t& wstat;
    std::unique_ptr<x11_color_manager_t> color_manager;

    xft_text_drawer_t text_drawer;
  public:
    typedef xft_character_buffer character_buffer;

  public:
    tx11_graphics_t(ansi::window_state_t& wstat): wstat(wstat) {
      text_drawer.set_size(wstat.m_xpixel, wstat.m_ypixel);
    }
    ~tx11_graphics_t() {
      this->finalize();
    }

    void initialize(Display* display, Drawable drawable) {
      int const screen = DefaultScreen(display);
      this->m_display = display;
      this->m_drawable = drawable;
      this->m_gc = XCreateGC(display, drawable, 0, 0);

      Visual* const visual = DefaultVisual(display, screen);
      if (visual->c_class != TrueColor)
        color_manager = std::make_unique<x11_color_manager_t>(display);

      this->text_drawer.initialize(display, color_manager.get());
    }
    void finalize() {
      // todo: m_draw, m_gc
    }

    GC gc() const { return m_gc; }

  private:
    void _set_foreground(color_t color) {
      if (!color_manager) {
        XSetForeground(m_display, m_gc, contra::dict::rgba2bgr(color));
      } else {
        XSetForeground(m_display, m_gc, color_manager->pixel(color));
      }
    }

  public:
    // ToDo:
    void clip_rectangle(coord_t x1, coord_t y1, coord_t x2, coord_t y2) {
      contra_unused(x1);
      contra_unused(y1);
      contra_unused(x2);
      contra_unused(y2);
    }
    void clip_clear() {}

  public:
    void fill_rectangle(coord_t x1, coord_t y1, coord_t x2, coord_t y2, color_t color) {
      _set_foreground(color);
      ::XFillRectangle(m_display, m_drawable, m_gc, x1, y1, x2 - x1, y2 - y1);
    }
    void invert_rectangle(coord_t x1, coord_t y1, coord_t x2, coord_t y2) {
      contra_unused(x1);
      contra_unused(y1);
      contra_unused(x2);
      contra_unused(y2);
      fill_rectangle(x1, y1, x2, y2, 0);
      //::PatBlt(hdc, x1, y1, x2 - x1, y2 - y1, DSTINVERT);
    }
    void draw_ellipse(coord_t x1, coord_t y1, coord_t x2, coord_t y2, color_t color, int line_width) {
      contra_unused(x1);
      contra_unused(y1);
      contra_unused(x2);
      contra_unused(y2);
      contra_unused(color);
      contra_unused(line_width);
      // ::SelectObject(hdc, bstore.get_pen(color0, line_width));
      // ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
      // ::Ellipse(hdc, x1, y1, x2, y2);
      // ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
    }
    void fill_ellipse(coord_t x1, coord_t y1, coord_t x2, coord_t y2, color_t color) {
      contra_unused(x1);
      contra_unused(y1);
      contra_unused(x2);
      contra_unused(y2);
      contra_unused(color);
      // ::SelectObject(hdc, ::GetStockObject(NULL_PEN));
      // ::SelectObject(hdc, bstore.get_brush(color0));
      // ::Ellipse(hdc, x1, y1, x2, y2);
      // ::SelectObject(hdc, ::GetStockObject(NULL_BRUSH));
    }

  public:
    void draw_characters(coord_t x1, coord_t y1, character_buffer const& buff, font_t font, color_t color) {
      // _set_foreground(color);
      // contra_unused(font);
      // coord_t x = x1, y = y1 + wstat.m_ypixel - 1;
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

  struct tx11_setting_t {
    const char* m_env_term = "xterm-256color";
    const char* m_env_shell = "/bin/bash";
  };

  class tx11_window_t {
    ansi::window_state_t wstat;
    ansi::window_renderer_t renderer { wstat };
    ansi::status_tracer_t m_tracer;
    term::terminal_manager manager;

    ::Display* display = NULL;
    ::Window main = 0;
    ::Atom WM_DELETE_WINDOW;
    tx11_setting_t settings;
    tx11_graphics_t g { wstat };

  public:
    ~tx11_window_t() {}

  private:
    bool is_session_ready() { return display && manager.is_active(); }
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
      int const screen = DefaultScreen(display);
      Window const root = RootWindow(display, screen);

      unsigned long const black = BlackPixel(display, screen);
      unsigned long const white = WhitePixel(display, screen);

      // Main Window
      ansi::coord_t const width = wstat.calculate_client_width();
      ansi::coord_t const height = wstat.calculate_client_height();
      this->main = XCreateSimpleWindow(display, root, 100, 100, width, height, 1, black, white);
      {
        // Properties
        XTextProperty win_name;
        win_name.value = (unsigned char*) "Contra/X11";
        win_name.encoding = XA_STRING;
        win_name.format = 8;
        win_name.nitems = std::strlen((char const*) win_name.value);
        ::XSetWMName(display, this->main, &win_name);

        // Events
        XSelectInput(display, this->main, ExposureMask | KeyPressMask | StructureNotifyMask );
      }

      // #D0162 何だかよく分からないが終了する時はこうするらしい。
      this->WM_DELETE_WINDOW = XInternAtom(display, "WM_DELETE_WINDOW", False);
      XSetWMProtocols(display, this->main, &this->WM_DELETE_WINDOW, 1);

      this->g.initialize(this->display, this->main);

      XAutoRepeatOn(display);
      return true;
    }

  private:
    void unset_cursor_timer() {}
    void reset_cursor_timer() {}

  private:
    template<typename Graphics>
    void paint_terminal_content(Graphics& g, term::terminal_application& app, bool full_update) {
      // update status
      ansi::term_view_t& view = app.view();
      view.update();

      bool const content_changed = m_tracer.is_content_changed(view);
      bool const cursor_changed = m_tracer.is_cursor_changed(wstat, view);
      {
        // update cursor state
        bool const cursor_blinking = view.state().is_cursor_blinking();
        bool const cursor_visible = wstat.is_cursor_visible(view);
        if (!cursor_visible || !cursor_blinking)
          this->unset_cursor_timer();
        else if (content_changed || cursor_changed)
          this->reset_cursor_timer();
      }

      if (m_tracer.is_metric_changed(wstat)) full_update = true;

      // 暫定:
      // HDC const hdc1 = m_background.hdc(hWnd, hdc0, 1);
      {
        Window root;
        int x, y;
        unsigned w, h, border, depth;
        XGetGeometry(display, main, &root, &x, &y, &w, &h, &border, &depth);
        wstat.m_window_width = w;
        wstat.m_window_height = h;
      }

      bool content_redraw = full_update || content_changed;
      if (m_tracer.is_blinking_changed(wstat)) content_redraw = true;
      if (content_redraw) {
        std::vector<std::vector<contra::ansi::cell_t>> content;
        ansi::curpos_t const height = view.height();
        content.resize(height);

        for (contra::ansi::curpos_t y = 0; y < height; y++) {
          ansi::line_t const& line = view.line(y);
          view.get_cells_in_presentation(content[y], line);
        }

        renderer.draw_background(g, view, content);
        renderer.draw_characters(g, view, content);
        renderer.draw_decoration(g, view, content);

        //renderer.draw_characters_mono(g, view, content);
        //::BitBlt(hdc0, 0, 0, m_background.width(), m_background.height(), hdc1, 0, 0, SRCCOPY);
      }

      // if (!full_update) {
      //   // 前回のカーソル位置のセルをカーソルなしに戻す。
      //   coord_t const old_x1 = wstat.m_xframe + wstat.m_xpixel * m_tracer.cur_x();
      //   coord_t const old_y1 = wstat.m_yframe + wstat.m_ypixel * m_tracer.cur_y();
      //   ::BitBlt(hdc0, old_x1, old_y1, wstat.m_xpixel, wstat.m_ypixel, hdc1, old_x1, old_y1, SRCCOPY);
      // }
      if (wstat.is_cursor_appearing(view)) {
        renderer.draw_cursor(g, view);
      }

      m_tracer.store(wstat, view);
    }
    void render_window() {
      if (!is_session_ready()) {
        const char* message = "Hello, world!";
        XDrawString(display, this->main, g.gc(), 3, 15, message, std::strlen(message));
      } else {
        // resized?
        paint_terminal_content(g, manager.app(), false);
      }
      XFlush(display);
    }

  private:
    void process_input(std::uint32_t key) {
      contra::print_key(key, stderr);
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
      if (key_state(XK_Shift_L)) ret |= modifier_shift;
      if (key_state(XK_Control_L)) ret |= modifier_control;
      if (key_state(XK_Alt_L)) ret |= modifier_meta;
      if (key_state(XK_Alt_R)) ret |= modifier_alter;
      if (key_state(XK_Control_R)) ret |= modifier_super;
      if (key_state(XK_Shift_R)) ret |= modifier_hyper;
      if (key_state(XK_Menu)) ret |= modifier_application;

      // 本来は以下の様にしたい:
      // if (key_state(XK_Shift_L) || key_state(XK_Shift_R)) ret |= modifier_shift;
      // if (key_state(XK_Control_L) || key_state(XK_Control_R)) ret |= modifier_control;
      // if (key_state(XK_Meta_L) || key_state(XK_Meta_R)) ret |= modifier_meta;
      // if (key_state(XK_Alt_L) || key_state(XK_Alt_R)) ret |= modifier_alter;
      // if (key_state(XK_Super_L) || key_state(XK_Super_R)) ret |= modifier_super;
      // if (key_state(XK_Hyper_L) || key_state(XK_Hyper_R)) ret |= modifier_hyper;

      unsigned indicators = 0;
      if (XkbGetIndicatorState(display, XkbUseCoreKbd, &indicators) == Success) {
        if (indicators & 1) ret |= toggle_capslock;
        if (indicators & 2) ret |= toggle_numlock;
        if (indicators & 4) ret |= toggle_scrolllock; // 効かない様だ @ Cygwin
      }

      return ret;
    }

    bool process_key(KeyCode keycode, key_t modifiers) {
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

      case ClientMessage: // #D0162
        if ((Atom) event.xclient.data.l[0] == WM_DELETE_WINDOW)
          XDestroyWindow(display, this->main);
        break;
      case DestroyNotify: // #D0162
        ::XSetCloseDownMode(display, DestroyAll);
        ::XCloseDisplay(display);
        this->display = NULL;
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
      //::MessageBoxA(NULL, buff.str().c_str(), "Contra/Cygwin - exec failed", MB_OK);
    }
    bool setup_session() {
      term::terminal_session_parameters params;
      params.col = wstat.m_col;
      params.row = wstat.m_row;
      params.xpixel = wstat.m_xpixel;
      params.ypixel = wstat.m_ypixel;
      params.exec_error_handler = &exec_error_handler;
      params.env["TERM"] = settings.m_env_term;
      params.shell = settings.m_env_shell;
      std::unique_ptr<term::terminal_application> sess = contra::term::create_terminal_session(params);
      if (!sess) return false;

      contra::ansi::tstate_t& s = sess->state();
      s.m_default_fg_space = contra::dict::attribute_t::color_space_rgb;
      s.m_default_bg_space = contra::dict::attribute_t::color_space_rgb;
      s.m_default_fg_color = contra::dict::rgb(0x00, 0x00, 0x00);
      s.m_default_bg_color = contra::dict::rgb(0xFF, 0xFF, 0xFF);
      // s.m_default_fg_color = contra::dict::rgb(0xD0, 0xD0, 0xD0);//@color
      // s.m_default_bg_color = contra::dict::rgb(0x00, 0x00, 0x00);
      manager.add_app(std::move(sess));
      return true;
    }
  public:
    int do_loop() {
      // CheckIfEvent 用のダミーフィルター
      Bool (*event_filter_proc)(Display*, XEvent*, XPointer) = [] (auto...) -> Bool { return True; };

      if (!this->setup_session()) return 2;

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
      return 0;
    }
  };

}
}

int main() {
  contra::tx11::tx11_window_t win;
  if (!win.create_window()) return 1;
  ::XMapWindow(win.display_handle(), win.window_handle());
  ::XFlush(win.display_handle());
  return win.do_loop();
}
