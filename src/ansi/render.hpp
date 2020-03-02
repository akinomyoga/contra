// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_ansi_render_hpp
#define contra_ansi_render_hpp
#include <mwg/except.h>
#include <cstdint>
#include <cstdlib>
#include <cmath>
#include <utility>
#include <algorithm>
#include <tuple>
#include <vector>
#include "../contradef.hpp"
#include "../context.hpp"
#include "line.hpp"
#include "term.hpp"

namespace contra {
namespace ansi {

  struct window_state_t {
    curpos_t m_col = 80;
    curpos_t m_row = 30;
    coord_t m_xunit = 7;
    coord_t m_yunit = 13;
    coord_t m_xframe = 1;
    coord_t m_yframe = 1;
  public:
    void configure_metric(contra::app::context& actx, double scale = 1.0) {
      actx.read("term_col", this->m_col = 80);
      actx.read("term_row", this->m_row = 30);
      this->m_col = limit::term_col.clamp(this->m_col);
      this->m_row = limit::term_row.clamp(this->m_row);

      double xunit, yunit, xframe, yframe;
      actx.read("term_xunit", xunit = 7);
      actx.read("term_yunit", yunit = 13);
      actx.read("term_xframe", xframe = 1);
      actx.read("term_yframe", yframe = 1);
      this->m_xunit = limit::term_xunit.clamp(std::round(xunit * scale));
      this->m_yunit = limit::term_yunit.clamp(std::round(yunit * scale));
      this->m_xframe = std::round(xframe * scale);
      this->m_yframe = std::round(yframe * scale);
    }

  public:
    coord_t calculate_client_width() const {
      return m_xunit * m_col + 2 * m_xframe;
    }
    coord_t calculate_client_height() const {
      return m_yunit * m_row + 2 * m_yframe;
    }

    // 実際の幅 (要求した幅と実際の幅が異なるかもしれないので)
    coord_t m_canvas_width;
    coord_t m_canvas_height;

  public:
    window_state_t() {
      m_canvas_width = calculate_client_width();
      m_canvas_height = calculate_client_height();
    }

  public:
    std::uint32_t m_blinking_count = 0;
    std::uint32_t m_cursor_timer_count = 0;
    bool m_hide_cursor = false;
    coord_t m_caret_underline_min_height = 2;
    coord_t m_caret_vertical_min_width = 2;

    // カーソルが表示されるかどうかを取得します。
    // 点滅中で一時的に消えている場合は true です。
    bool is_cursor_visible(term_view_t const& view) const {
      return !m_hide_cursor && view.is_cursor_visible();
    }
    // カーソルが今の瞬間表示されているかどうかを取得します。
    // 点滅中で一時的に消えている場合は false です。
    bool is_cursor_appearing(term_view_t const& view) const {
      return is_cursor_visible(view) && (!view.is_cursor_blinking() || !(m_cursor_timer_count & 1));
    }
  };

  struct window_settings_base {
    key_t term_mod_lshift = modifier_shift;
    key_t term_mod_rshift = modifier_shift;
    key_t term_mod_lcontrol = modifier_control;
    key_t term_mod_rcontrol = modifier_control;
    key_t term_mod_lalter = modifier_meta;
    key_t term_mod_ralter = modifier_meta;
    key_t term_mod_lmeta = modifier_meta;
    key_t term_mod_rmeta = modifier_meta;
    key_t term_mod_lsuper = modifier_super;
    key_t term_mod_rsuper = modifier_super;
    key_t term_mod_lhyper = modifier_hyper;
    key_t term_mod_rhyper = modifier_hyper;
    key_t term_mod_menu = modifier_application;

    void configure(contra::app::context& actx) {
      // modifiers
      actx.read("term_mod_lshift", term_mod_lshift, &parse_modifier);
      actx.read("term_mod_rshift", term_mod_rshift, &parse_modifier);
      actx.read("term_mod_lcontrol", term_mod_lcontrol, &parse_modifier);
      actx.read("term_mod_rcontrol", term_mod_rcontrol, &parse_modifier);
      actx.read("term_mod_lalter", term_mod_lalter, &parse_modifier);
      actx.read("term_mod_ralter", term_mod_ralter, &parse_modifier);
      actx.read("term_mod_lmeta", term_mod_lmeta, &parse_modifier);
      actx.read("term_mod_rmeta", term_mod_rmeta, &parse_modifier);
      actx.read("term_mod_lsuper", term_mod_lsuper, &parse_modifier);
      actx.read("term_mod_rsuper", term_mod_rsuper, &parse_modifier);
      actx.read("term_mod_lhyper", term_mod_lhyper, &parse_modifier);
      actx.read("term_mod_rhyper", term_mod_rhyper, &parse_modifier);
      actx.read("term_mod_menu", term_mod_menu, &parse_modifier);
    }
  };

  class status_tracer_t {
  public:
    status_tracer_t() {}

    void store(window_state_t const& wstat, term_view_t const& view) {
      store_view(view);
      store_metric(wstat, view);
      store_color(view);
      store_content(view);
      store_cursor(wstat, view);
      store_blinking_state(wstat);
    }

  private:
    term_view_t const* m_view = nullptr;
    bool is_view_changed(term_view_t const& view) const {
      return m_view != &view;
    }
    void store_view(term_view_t const& view) {
      this->m_view = &view;
    }

  private:
    coord_t m_xframe = 0;
    coord_t m_yframe = 0;
    curpos_t m_col = 0;
    curpos_t m_row = 0;
    coord_t m_xunit = 0;
    coord_t m_yunit = 0;
    presentation_direction_t m_presentation_direction = presentation_direction_default;
    bool is_metric_changed(window_state_t const& wstat, term_view_t const& view) const {
      if (m_xframe != wstat.m_xframe) return true;
      if (m_yframe != wstat.m_yframe) return true;
      if (m_col != wstat.m_col) return true;
      if (m_row != wstat.m_row) return true;
      if (m_xunit != wstat.m_xunit) return true;
      if (m_yunit != wstat.m_yunit) return true;
      if (view.presentation_direction() != m_presentation_direction) return true;
      return false;
    }
    void store_metric(window_state_t const& wstat, term_view_t const& view) {
      m_xframe = wstat.m_xframe;
      m_yframe = wstat.m_yframe;
      m_col = wstat.m_col;
      m_row = wstat.m_row;
      m_xunit = wstat.m_xunit;
      m_yunit = wstat.m_yunit;
      m_presentation_direction = view.presentation_direction();
    }

  private:
    byte m_fg_space, m_bg_space;
    color_t m_fg_color, m_bg_color;
    bool is_color_changed(term_view_t const& view) const {
      if (m_fg_space != view.fg_space()) return true;
      if (m_fg_color != view.fg_color()) return true;
      if (m_bg_space != view.bg_space()) return true;
      if (m_bg_color != view.bg_color()) return true;
      return false;
    }
    void store_color(term_view_t const& view) {
      m_fg_space = view.fg_space();
      m_fg_color = view.fg_color();
      m_bg_space = view.bg_space();
      m_bg_color = view.bg_color();
    }

  public:
    bool requests_full_update(window_state_t const& wstat, term_view_t const& view) const {
      return this->is_view_changed(view) ||
        this->is_metric_changed(wstat, view) ||
        this->is_color_changed(view);
    }

  public:
    struct line_trace_t {
      std::uint32_t id = (std::uint32_t) -1;
      std::uint32_t version = 0;
      bool has_blinking = false;
    };
  private:
    std::vector<line_trace_t> m_lines;
  public:
    bool is_content_changed(term_view_t const& view) const {
      std::size_t const height = view.height();
      if (height != m_lines.size()) return true;
      for (std::size_t iline = 0; iline < height; iline++) {
        line_t const& line = view.line(iline);
        if (line.id() != m_lines[iline].id) return true;
        if (line.version() != m_lines[iline].version) return true;
      }
      return false;
    }
    void store_content(term_view_t const& view) {
      curpos_t const height = view.height();
      m_lines.resize(height);
      for (curpos_t i = 0; i < height; i++) {
        line_t const& original_line = view.line(i);
        m_lines[i].id = original_line.id();
        m_lines[i].version = original_line.version();
        m_lines[i].has_blinking = original_line.has_blinking_cells();
      }
    }
    std::vector<line_trace_t> const& lines() const { return m_lines; }

  private:
    std::uint32_t m_blinking_count = 0;
  public:
    bool has_blinking_cells() const {
      for (line_trace_t const& line : m_lines)
        if (line.has_blinking) return true;
      return false;
    }
    bool is_blinking_changed(window_state_t const& wstat) const {
      if (!this->has_blinking_cells()) return false;
      return this->m_blinking_count != wstat.m_blinking_count;
    }
    void store_blinking_state(window_state_t const& wstat) {
      this->m_blinking_count = wstat.m_blinking_count;
    }

  private:
    // todo: カーソル状態の記録
    //   実はカーソルは毎回描画でも良いのかもしれない
    bool m_cur_visible = false;
    bool m_cur_blinking = true;
    curpos_t m_cur_x = 0;
    curpos_t m_cur_y = 0;
    bool m_cur_xenl = false;
    int m_cur_shape = 0;
    bool m_cur_r2l = false;
  public:
    curpos_t cur_x() const { return m_cur_x; }
    curpos_t cur_y() const { return m_cur_y; }
    bool is_cursor_changed(window_state_t const& wstat, term_view_t const& view) const {
      if (m_cur_visible != wstat.is_cursor_visible(view)) return true;
      if (m_cur_x != view.client_x() || m_cur_y != view.client_y() || m_cur_xenl != view.xenl()) return true;
      if (m_cur_r2l != view.cur_r2l()) return true;
      if (m_cur_shape != view.cursor_shape()) return true;
      if (m_cur_blinking != view.is_cursor_blinking()) return true;
      return false;
    }
    void store_cursor(window_state_t const& wstat, term_view_t const& view) {
      m_cur_visible = wstat.is_cursor_visible(view);
      m_cur_x = view.client_x();
      m_cur_y = view.client_y();
      m_cur_xenl = view.xenl();
      m_cur_r2l = view.cur_r2l();
      m_cur_shape = view.cursor_shape();
    }
  };

  class color_resolver_t {
    tstate_t const* s = nullptr;
    attribute_table const* atable = nullptr;
    byte m_space = (byte) color_space_default;
    color_t m_color = color_t(-1);
    color_t m_rgba = 0;

  public:
    color_resolver_t() {}
    color_resolver_t(tstate_t const& s, attribute_table const& atable): s(&s), atable(&atable) {}

  public:
    color_t resolve(byte space, color_t color) {
      color_t rgba = 0;
      switch (space) {
      case color_space_default:
      case color_space_transparent:
      default:
        // transparent なので色はない。
        rgba = 0x00000000; // black
        break;
      case color_space_indexed:
        rgba = s->m_rgba256[color & 0xFF];
        break;
      case color_space_rgb:
        rgba = contra::ansi::rgb2rgba(color);
        break;
      case color_space_cmy:
        rgba = contra::ansi::cmy2rgba(color);
        break;
      case color_space_cmyk:
        rgba = contra::ansi::cmyk2rgba(color);
        break;
      }

      m_space = space;
      m_color = color;
      m_rgba = rgba;
      return rgba;
    }

  private:
    std::pair<byte, color_t> get_fg(cattr_t const& attr) {
      byte space = atable->fg_space(attr);
      color_t color = atable->fg_color(attr);
      if (space == 0) {
        space = s->m_default_fg_space;
        color = s->m_default_fg_color;
      }
      return {space, color};
    }
    std::pair<byte, color_t> get_bg(cattr_t const& attr) {
      byte space = atable->bg_space(attr);
      color_t color = atable->bg_color(attr);
      if (space == 0) {
        space = s->m_default_bg_space;
        color = s->m_default_bg_color;
      }
      return {space, color};
    }
    std::pair<byte, color_t> get_dc(cattr_t const& attr) {
      byte const space = atable->dc_space(attr);
      color_t const color = atable->dc_color(attr);
      if (space == 0)
        return get_fg(attr);
      else
        return {space, color};
    }
  public:
    color_t resolve_fg(cattr_t const& attr) {
      bool const inverse = atable->is_inverse(attr);
      bool const selected = atable->is_selected(attr);
      auto [space, color] = inverse != selected ? get_bg(attr) : get_fg(attr);
      if (space == m_space && color == m_color) return m_rgba;
      return resolve(space, color);
    }

    color_t resolve_bg(cattr_t const& attr) {
      bool const inverse = atable->is_inverse(attr);
      bool const selected = atable->is_selected(attr);
      auto [space, color] = inverse != selected ? get_fg(attr) : get_bg(attr);
      if (space == m_space && color == m_color) return m_rgba;
      return resolve(space, color);
    }

    color_t resolve_dc(cattr_t const& attr) {
      byte const space = atable->dc_space(attr);
      if (space == 0) return resolve_fg(attr);
      color_t const color = atable->dc_color(attr);
      if (space == m_space && color == m_color) return m_rgba;
      return resolve(space, color);
    }
  };

  typedef std::uint32_t font_t;

  enum font_flags {
    // bit 0-3: fontface
    font_face_mask     = 0x000F,
    font_face_shft     = 0,
    font_face_fraktur  = 0x000A,

    // bit 4: italic
    font_flag_italic   = 0x0010,

    // bit 5-6: weight
    font_weight_mask   = 0x0060,
    font_weight_shft   = 4,
    font_weight_bold   = 0x0020,
    font_weight_faint  = 0x0040,
    font_weight_heavy  = 0x0060,

    // bit 7: small
    font_flag_small    = 0x0080,

    // bit 8-10: rotation
    font_rotation_mask = 0x0700,
    font_rotation_shft = 8,

    // bit 12-13: DECDWL, DECDHL
    font_decdhl_mask   = 0x3000,
    font_decdhl_shft   = 12,
    font_decdhl        = 0x1000,
    font_decdwl        = 0x2000,

    // 11x2x2 種類のフォントは記録する (下位6bit)。
    font_basic_mask = font_face_mask | font_weight_bold | font_flag_italic,
    font_basic_bits = 6,

    font_cache_count = 32,

    font_layout_mask         = 0xFF0000,
    font_layout_shft         = 16,
    font_layout_sup          = 0x010000,
    font_layout_sub          = 0x020000,
    font_layout_framed       = 0x040000,
    font_layout_upper_half   = 0x080000,
    font_layout_lower_half   = 0x100000,
    font_layout_proportional = 0x200000,
  };

  class font_metric_t {
  private:
    coord_t m_width = 0, m_height = 0;

  public:
    font_metric_t() = default;
    font_metric_t(coord_t xunit, coord_t yunit) {
      this->initialize(xunit, yunit);
    }
    void initialize(coord_t xunit, coord_t yunit) {
      m_width = xunit;
      m_height = yunit;
    }

    coord_t width() const { return m_width; }
    coord_t height() const { return m_height; }
    void set_size(coord_t width, coord_t height) {
      this->m_width = width;
      this->m_height = height;
    }

  public:
    coord_t small_height() const {
      return std::max<coord_t>(8u, std::min(m_height - 4, (m_height * 7 + 9) / 10));
    }
    coord_t small_width() const {
      return std::max<coord_t>(4u, std::min(m_width - 4, (m_width * 8 + 9) / 10));
    }

    coord_t get_font_height(font_t font) const {
      coord_t height = font & font_flag_small ? small_height() : m_height;
      if (font & font_decdhl) height *= 2;
      return height;
    }

    std::pair<coord_t, coord_t> get_font_size(font_t font) const {
      coord_t width = m_width, height = m_height;
      if (font & font_flag_small) {
        height = small_height();
        width = small_width();
      }
      if (font & font_decdhl) height *= 2;
      if (font & font_decdwl) width *= 2;
      return {width, height};
    }

  public:
    /*?lwiki
     * @fn std::tuple<double, double, double> get_displacement(font_t font);
     * @return dx は描画の際の横の絶対ずれ量
     * @return dy は描画の際の縦のずれ量
     * @return dxW は (文字の幅) に比例して増える各文字の横のずれ量
     */
    std::tuple<double, double, double> get_displacement(font_t font) const {
      double dx = 0, dy = 0, dxW = 0;
      if (font & (font_layout_mask | font_decdwl | font_flag_italic)) {
        // Note: 横のずれ量は dxI + dxW * cell.width である。
        // これを実際には幅1の時の値 dx = dxI + dxW を分離して、
        // ずれ量 = dx + dxW * (cell.width - 1) と計算する。

        double dxI = 0;
        if (font & font_layout_sup)
          dy -= 0.2 * this->m_height;
        else if (font & font_layout_sub) {
          // Note: 下端は baseline より下なので下端に合わせれば十分。
          dy += this->m_height - this->small_height();
        }
        if (font & (font_layout_framed | font_layout_sup | font_layout_sub)) {
          dxW = 0.5 * (this->m_width - this->small_width());
          dy += 0.5 * (this->m_height - this->small_height());
        }
        if (font & font_flag_italic) {
          coord_t width = this->m_width;
          if (font & (font_layout_framed | font_layout_sup | font_layout_sub))
            width = this->small_width();
          dx -= 0.4 * width;
        }
        if (font & (font_layout_upper_half | font_layout_lower_half)) dy *= 2;
        if (font & font_decdwl) dxI *= 2;
        if (font & font_layout_lower_half) dy -= this->m_height;
        dx = dxI;
      }
      return {dx, dy, dxW};
    }
  };

  template<typename FontFactory>
  class font_manager_t: public font_metric_t {
    typedef font_metric_t base;

  public:
    using font_type = typename FontFactory::font_type;
  private:
    FontFactory m_factory;
  public:
    FontFactory& factory() { return m_factory; }
    FontFactory const& factory() const { return m_factory; }

  private:
    font_type m_fonts[1u << font_basic_bits];
    std::pair<font_t, font_type> m_cache[font_cache_count];

  public:
    font_manager_t(contra::app::context& actx): base(7, 14), m_factory(actx) {
      std::fill(std::begin(m_fonts), std::end(m_fonts), (font_type) NULL);
      std::fill(std::begin(m_cache), std::end(m_cache), std::pair<font_t, font_type>(0u, NULL));
    }

    void set_size(coord_t width, coord_t height) {
      if (base::width() == width && base::height() == height) return;
      release();
      base::set_size(width, height);
    }

  private:
    void release() {
      for (auto& hfont : m_fonts) {
        if (hfont) m_factory.delete_font(hfont);
        hfont = NULL;
      }
      auto const init = std::pair<font_t, font_type>(0u, NULL);
      for (auto& pair : m_cache) {
        if (pair.second) m_factory.delete_font(pair.second);
        pair = init;
      }
    }
  private:
    font_type create_font(font_t font) {
      return m_factory.create_font(font, static_cast<font_metric_t const&>(*this));
    }
  public:
    font_type get_font(font_t font) {
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
        if (font_type last = std::end(m_cache)[-1].second) m_factory.delete_font(last);
        std::move_backward(std::begin(m_cache), std::end(m_cache) - 1, std::end(m_cache));
        font_type const ret = create_font(font);
        m_cache[0] = std::make_pair(font, ret);
        return ret;
      }
    }

    void clear() {
      release();
    }

    ~font_manager_t() {
      release();
    }
  };

  class font_resolver_t {
  private:
    aflags_t m_aflags = 0;
    xflags_t m_xflags = 0;
    font_t m_font = 0;
    attribute_table const* atable = nullptr;
  public:
    font_resolver_t() {}
    font_resolver_t(attribute_table const& atable): atable(&atable) {}
  public:
    font_t resolve_font(cattr_t const& attr) {
      constexpr aflags_t aflags_mask = attr_weight_mask | attr_shape_mask | aflags_font_mask;
      constexpr xflags_t xflags_mask = xflags_subsup_mask | xflags_mintty_subsup_mask
        | xflags_frame_mask | xflags_proportional_set | xflags_sco_shift | xflags_decdhl_mask;
      aflags_t const aflags = atable->aflags(attr) & aflags_mask;
      xflags_t const xflags = atable->xflags(attr) & xflags_mask;
      if (aflags == m_aflags && xflags == m_xflags) return m_font;

      font_t ret = 0;
      if (std::uint32_t const face = std::uint32_t(aflags & aflags_font_mask) >> aflags_font_shift)
        ret |= font_face_mask & face << font_face_shft;

      switch ((aflags & attr_weight_mask).value()) {
      case attr_bold_set.value():  ret |= font_weight_bold; break;
      case attr_faint_set.value(): ret |= font_weight_faint; break;
      case attr_heavy_set.value(): ret |= font_weight_heavy; break;
      }

      if (aflags & attr_italic_set)
        ret |= font_flag_italic;
      else if (aflags & attr_fraktur_set)
        ret = (ret & ~font_face_mask) | font_face_fraktur;

      if (xflags & xflags_sup_set)
        ret |= font_flag_small | font_layout_sup;
      else if (xflags & xflags_sub_set)
        ret |= font_flag_small | font_layout_sub;
      else if (xflags & xflags_mintty_sup)
        ret |= font_flag_small | font_layout_sup;
      else if (xflags & xflags_mintty_sub)
        ret |= font_flag_small | font_layout_sub;

      if (xflags & xflags_frame_mask)
        ret |= font_flag_small | font_layout_framed;

      if (xflags & xflags_proportional_set)
        ret |= font_layout_proportional;

      if (std::uint32_t const sco = std::uint32_t(xflags & xflags_sco_mask) >> xflags_sco_shift)
        ret |= font_rotation_mask & sco << font_rotation_shft;

      switch ((xflags & xflags_decdhl_mask).value()) {
      case xflags_decdhl_double_width.value():
        ret |= font_decdwl;
        break;
      case xflags_decdhl_upper_half.value():
        ret |= font_decdwl | font_decdhl | font_layout_upper_half;
        break;
      case xflags_decdhl_lower_half.value():
        ret |= font_decdwl | font_decdhl | font_layout_lower_half;
        break;
      }

      m_aflags = aflags;
      m_xflags = xflags;
      m_font = ret;
      return ret;
    }
  };

  template<typename Graphics>
  class batch_string_drawer {
    typename Graphics::character_buffer charbuff;
    font_metric_t fmetric;
    coord_t u;
    double dx, dy, dxW;

  public:
    void initialize(coord_t xunit, coord_t yunit) {
      this->fmetric.initialize(xunit, yunit);
    }

    void reserve(std::size_t size) {
      charbuff.reserve(size);
    }

    void start(font_t font) {
      std::tie(dx, dy, dxW) = fmetric.get_displacement(font);
      charbuff.clear();
      u = 0;
    }

    void push(std::uint32_t code, curpos_t w) {
      coord_t const char_dx = std::round(dx + dxW * w);
      charbuff.add_char(code, u + char_dx, w == 0);
      u += w * fmetric.width();
    }

    void skip(curpos_t w) {
      u += w * fmetric.width();
    }

    coord_t x() const { return u; }

    void render(Graphics* g, coord_t x1, coord_t y1, font_t font, color_t fg) {
      dy = std::round(dy);
      if (font & font_rotation_mask) {
        g->draw_rotated_characters(x1, y1, 0, dy, u, charbuff, font, fg);
      } else if (font & font_layout_proportional) {
        g->draw_text(x1, y1 + dy, charbuff, font, fg);
      } else {
        g->draw_characters(x1, y1 + dy, charbuff, font, fg);
      }
    }
  };

  template<typename Graphics>
  class graphics_drawer {
    coord_t xunit, yunit;
    Graphics* g;
    coord_t x, y, w, h;
    color_t color;
    font_t font;
  public:
    void initialize(coord_t xunit, coord_t yunit) {
      this->xunit = xunit;
      this->yunit = yunit;
    }
    void set_parameters(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      this->g = g;
      this->x = x;
      this->y = y;
      this->w = w;
      this->h = h;
      this->color = color;
      this->font = font;
    }

  private:
    // bold or heavy
    bool bold() const { return font & font_weight_bold; }

    coord_t hline_width() const {
      return std::ceil(yunit / 20.0) * (1 + bold() + !!(font & font_decdhl));
    }

    coord_t vline_width() const {
      return std::ceil(xunit / 10.0) * (1 + bold() + !!(font & font_decdwl));
    }

  private:
    void glyph_line(coord_t x1, coord_t y1, coord_t x2, coord_t y2, coord_t w) const {
      if (font & font_flag_italic) {
        x1 -= (y1 - (y + h / 2)) / 3;
        x2 -= (y2 - (y + h / 2)) / 3;
      }
      g->draw_line(x1, y1, x2, y2, color, w);
    }
    void glyph_polygon(coord_t (*points)[2], std::size_t count) const {
      if (font & font_flag_italic) {
        for (std::size_t i = 0; i < count; i++)
          points[i][0] -= (points[i][1] - (y + h / 2)) / 3;
      }
      g->fill_polygon(points, count, color);
    }

  private:
    void impl_varrow(bool is_minus) {
      coord_t const xM = x + w / 2;
      coord_t const vlw = std::ceil(yunit / 20.0) * (1 + bold() + !!(font & font_decdwl));
      coord_t const h1 = std::max<coord_t>(5, (bold() ? h * 9 / 10 : std::min(h - 2, std::max(5, h * 4 / 5))) - (vlw - 1));
      coord_t const xwing = std::max<coord_t>(3, (w - vlw - 1) / 2);
      coord_t const ywing = !(font & font_decdwl) || (font & font_decdhl) ? xwing : (xwing + 1) / 2;
      if (is_minus) {
        coord_t const y1 = y + h  - (h - h1) / 2;
        coord_t const y2 = y1 - h1;
        glyph_line(xM, y1, xM, y2, vlw);
        glyph_line(xM, y2, xM - xwing, y2 + ywing, vlw);
        glyph_line(xM, y2, xM + xwing, y2 + ywing, vlw);
      } else {
        coord_t const y1 = y + (h - h1) / 2;
        coord_t const y2 = y1 + h1;
        glyph_line(xM, y1, xM, y2, vlw);
        glyph_line(xM, y2, xM - xwing, y2 - ywing, vlw);
        glyph_line(xM, y2, xM + xwing, y2 - ywing, vlw);
      }
    }
  public:
    void uarrow(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      impl_varrow(true);
    }
    void darrow(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      impl_varrow(false);
    }

  private:
    void impl_harrow(bool is_left) {
      coord_t const yM = y + h / 2;
      coord_t const hlw = hline_width();
      coord_t const w1 = std::max<coord_t>(4, (bold() ? w * 9 / 10 : std::min(w - 2, std::max(5, w * 4 / 5))) - (hlw - 1));
      coord_t const ywing = std::max<coord_t>(3, (h - hlw - 1) / 4);
      coord_t const xwing = !(font & font_decdwl) || (font & font_decdhl) ? ywing : ywing * 2;
      if (is_left) {
        coord_t const x1 = x + w  - (w - w1) / 2;
        coord_t const x2 = x1 - w1;
        glyph_line(x1, yM, x2, yM, hlw);
        glyph_line(x2, yM, x2 + xwing, yM - ywing, hlw);
        glyph_line(x2, yM, x2 + xwing, yM + ywing, hlw);
      } else {
        coord_t const x1 = x + (w - w1) / 2;
        coord_t const x2 = x1 + w1;
        glyph_line(x1, yM, x2, yM, hlw);
        glyph_line(x2, yM, x2 - xwing, yM - ywing, hlw);
        glyph_line(x2, yM, x2 - xwing, yM + ywing, hlw);
      }
    }
  public:
    void rarrow(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      impl_harrow(false);
    }
    void larrow(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      impl_harrow(true);
    }

  private:
    void impl_pm(int type) {
      coord_t const hlw = hline_width();
      coord_t const w1 = std::max<coord_t>(4, (bold() ? w * 9 / 10 : std::min(w - 1, std::max(5, w * 7 / 8))) - hlw);

      coord_t const x2 = x + w / 2;
      coord_t const x1 = x2 - w1 / 2;
      coord_t const x3 = x1 + w1;
      coord_t const y2 = y + h * 9 / 20 - hlw;
      coord_t const y1 = y2 - h / 4;
      coord_t const y3 = y2 + h / 4;
      coord_t const y4 = y3 + hlw * 2;

      glyph_line(x1, y4, x3, y4, hlw);
      if (type == 0) {
        // +-
        glyph_line(x1, y2, x3, y2, hlw);
        glyph_line(x2, y1, x2, y3, hlw);
      } else if (type < 0) {
        // <=
        glyph_line(x3, y1, x1, y2, hlw);
        glyph_line(x1, y2, x3, y3, hlw);
      } else {
        // >=
        glyph_line(x1, y1, x3, y2, hlw);
        glyph_line(x3, y2, x1, y3, hlw);
      }
    }
  public:
    void pm(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      impl_pm(0);
    }
    void le(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      impl_pm(-1);
    }
    void ge(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      impl_pm(1);
    }

  public:
    void ne(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      coord_t const hlw = hline_width();
      coord_t const w1 = std::max<coord_t>(4, (bold() ? w * 9 / 10 : std::min(w - 1, std::max(5, w * 7 / 8))) - hlw);
      coord_t const x2 = x + w / 2;
      coord_t const x1 = x2 - w1 / 2;
      coord_t const x3 = x1 + w1;
      coord_t const y2 = y + h * 9 / 20;
      coord_t const y1 = y2 - h / 10;
      coord_t const y3 = y1 + h / 5;
      coord_t const y0 = y2 - h / 4;
      coord_t const y4 = y2 + h / 4;
      glyph_line(x1, y1, x3, y1, hlw);
      glyph_line(x1, y3, x3, y3, hlw);
      glyph_line(x2 + w / 4, y0, x2 -  w / 4, y4, hlw);
    }

  public:
    void diamond(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      coord_t const w1 = bold() ? w * 9 / 10 : std::min(w - 2, w * 4 / 5);
      coord_t const h1 = bold() ? h * 9 / 10 : std::min(h - 2, h * 4 / 5);
      coord_t const x1 = x + (w - w1) / 2;
      coord_t const y1 = y + (h - h1) / 2;
      coord_t const x2 = x1 + w1, xM = x + w / 2;
      coord_t const y2 = y1 + h1, yM = y + h / 2;
      coord_t points[4][2] = { {xM, y1}, {x1, yM}, {xM, y2}, {x2, yM} };
      glyph_polygon(points, 4);
    }
    void pi(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      coord_t const hlw = hline_width();
      coord_t const w1 = std::max<coord_t>(4, (bold() ? w * 9 / 10 : std::min(w - 1, std::max(5, w * 7 / 8))) - hlw);
      coord_t const x2 = x + w / 2;
      coord_t const x1 = x2 - w1 / 2;
      coord_t const x3 = x1 + w1;

      coord_t const y1 = y + h * 4 / 10;
      coord_t const y2 = y + h * 7 / 10;
      coord_t const tip = (w1 + 5) / 6;
      glyph_line(x1, y1, x3, y1, hlw);
      glyph_line(x1 + tip, y1, x1 + tip, y2, hlw);
      glyph_line(x3 - tip, y1, x3 - tip, y2, hlw);
    }
    void bullet(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      coord_t const xo = x + w / 2, yo = y + h * 2 / 5;
      coord_t const hlw = hline_width();
      g->fill_ellipse(xo - hlw, yo - hlw, xo + hlw + 1, yo + hlw + 1, color);
    }
    void degree(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      coord_t const hlw = hline_width();
      coord_t const xo = x + w / 2 - hlw * 2, yo = y + hlw / 2;
      g->draw_ellipse(xo, yo, xo + 2 * hlw + 2, yo + 2 * hlw + 2, color, hlw);
    }
    void lantern(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      coord_t const hlw = hline_width();
      coord_t const xL = x + (hlw - 1) / 2, xR = x + w - hlw / 2;
      coord_t const yT = y + (hlw - 1) / 2, yB = y + h - hlw / 2;
      coord_t const ww = xR - xL, hh = yB - yT;

      coord_t const lantern_half_width = std::clamp(hlw + 1, (ww + 1) / 3, w / 2);
      coord_t const xc3 = xR;
      coord_t const xc2 = xc3 - lantern_half_width;
      coord_t const xc1 = xc2 - lantern_half_width;

      coord_t const yc3 = yB - hlw;
      coord_t const yc2 = std::max(y + hlw * 2, yc3 - hh / 2);
      coord_t const yb1 = std::max(yT, yc2 - hlw * 4);
      coord_t const yc1 = (yb1 + yc2) / 2;

      glyph_line(xc1, yc2, xc3, yc2, hlw);
      glyph_line(xc1, yc3, xc3, yc3, hlw);
      glyph_line(xc1, yc2, xc1, yc3, hlw);
      glyph_line(xc3, yc2, xc3, yc3, hlw);
      glyph_line(xc2, (yc2 + yc3) / 2, xc2, yc3, hlw);
      glyph_line(xc2, yc1, xc2, yc2, hlw);
      glyph_line(x, yc1, std::min(xc2 + hlw + 1, xR), yb1, hlw);
    }

  public:
    // draw_boxline で使う flags を構築します。
    // 引数には中心から左・右・上・下に向かうそれぞれの罫線素片の線種を指定します。
    // 線の種類は次の整数で指定します。
    //   0: 線なし, 1: 実線, 2: 太線, 3: 2重線
    static constexpr std::uint32_t boxline_flags(unsigned l, unsigned r, unsigned t, unsigned b) {
      return l | r << 2 | t << 4 | b << 6;
    }

  public:
    void boxline(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font, std::uint32_t flags) {
      set_parameters(g, x, y, w, h, color, font);

      coord_t const hlw = hline_width();
      coord_t const vlw = vline_width();
      int const L = 3 & flags;
      int const R = 3 & flags >> 2;
      int const T = 3 & flags >> 4;
      int const B = 3 & flags >> 6;

      coord_t const xc = x + w / 2, yc = y + h / 2;
      coord_t const v3iw = std::ceil(vlw * 0.5);
      coord_t const h3iw = std::ceil(hlw * 0.5);

      coord_t const v1L = xc - vlw / 2, v1R = xc + (vlw + 1) / 2;
      coord_t const v2L = xc - vlw, v2R = xc + vlw;
      coord_t const v3iL = xc - v3iw / 2, v3iR = xc + (v3iw + 1) / 2;
      coord_t const v3L = v3iL - vlw, v3R = v3iR + vlw;
      coord_t const vL[5] = {0, v1L, v2L, v3L, v3iL};
      coord_t const vR[5] = {0, v1R, v2R, v3R, v3iR};

      coord_t const h1T = yc - hlw / 2, h1B = yc + (hlw + 1) / 2;
      coord_t const h2T = yc - hlw, h2B = yc + hlw;
      coord_t const h3iT = yc - h3iw / 2, h3iB = yc + (h3iw + 1) / 2;
      coord_t const h3T = h3iT - hlw, h3B = h3iB + hlw;
      coord_t const hT[5] = {0, h1T, h2T, h3T, h3iT};
      coord_t const hB[5] = {0, h1B, h2B, h3B, h3iB};

      auto const _segment =
        [] (int L, int T, int B, coord_t const* vL, coord_t const* vR, coord_t const* hT, coord_t const* hB, auto rect) {
          if (L == 0) return;
          int crossing = std::max(T, B);
          coord_t const x2 = vR[crossing];
          switch (L) {
          case 1:
            rect(crossing ? x2 : vR[1], hT[1], hB[1]);
            break;
          case 2:
            rect(crossing ? x2 : vR[2], hT[2], hB[2]);
            break;
          case 3:
            rect(T == 3 ? vL[4] : crossing ? x2 : vR[3], hT[3], hT[4]);
            rect(B == 3 ? vL[4] : crossing ? x2 : vR[3], hB[4], hB[3]);
            break;
          }
        };

      if (flags & 0x0F) {
        if ((flags & 0x0F) == boxline_flags(1, 1, 0, 0)) {
          g->fill_rectangle(x, hT[1], x + w, hB[1], color);
        } else if ((flags & 0x0F) == boxline_flags(2, 2, 0, 0)) {
          g->fill_rectangle(x, hT[2], x + w, hB[2], color);
        } else if (flags == boxline_flags(3, 3, 0, 0)) {
          g->fill_rectangle(x, hT[3], x + w, hT[4], color);
          g->fill_rectangle(x, hB[4], x + w, hB[3], color);
        } else if (flags & 0x0F) {
          _segment(
            L, T, B, vL, vR, hT, hB,
            [&] (coord_t x2, coord_t y1, coord_t y2) { g->fill_rectangle(x, y1, x2, y2, color); });
          _segment(
            R, T, B, vR, vL, hT, hB,
            [&] (coord_t x1, coord_t y1, coord_t y2) { g->fill_rectangle(x1, y1, x + w, y2, color); });
        }
      }

      if (flags & 0xF0) {
        if ((flags & 0xF0) == boxline_flags(0, 0, 1, 1)) {
          g->fill_rectangle(vL[1], y, vR[1], y + h, color);
        } else if ((flags & 0xF0) == boxline_flags(0, 0, 2, 2)) {
          g->fill_rectangle(vL[2], y, vR[2], y + h, color);
        } else if (flags == boxline_flags(0, 0, 3, 3)) {
          g->fill_rectangle(vL[3], y, vL[4], y + h, color);
          g->fill_rectangle(vR[4], y, vR[3], y + h, color);
        } else if (flags & 0xF0) {
          _segment(
            T, L, R, hT, hB, vL, vR,
            [&] (coord_t y2, coord_t x1, coord_t x2) { g->fill_rectangle(x1, y, x2, y2, color); });
          _segment(
            B, L, R, hB, hT, vL, vR,
            [&] (coord_t y1, coord_t x1, coord_t x2) { g->fill_rectangle(x1, y1, x2, y + h, color); });
        }
      }
    }

  public:
    void white_box(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      coord_t const hlw = hline_width();
      coord_t const vlw = vline_width();
      coord_t const x1 = x, y1 = y, x2 = x + w, y2 = y + h;
      g->fill_rectangle(x1, y1, x2, y1 + hlw, color);
      g->fill_rectangle(x1, y2 - hlw, x2, y2, color);
      g->fill_rectangle(x1, y1, x1 + vlw, y2, color);
      g->fill_rectangle(x2 - vlw, y1, x2, y2, color);
    }
    void black_box(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      g->fill_rectangle(x, y, x + w, y + h, color);
    }
    void check_box(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      g->checked_rectangle(x, y, x + w, y + h, color);
    }

  public:
    void overline(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      coord_t const hlw = hline_width();
      coord_t const x1 = x, x2 = x + w;
      g->fill_rectangle(x1, y, x2, y + hlw, color);
    }
    void underline(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font) {
      set_parameters(g, x, y, w, h, color, font);
      coord_t const hlw = hline_width();
      coord_t const x1 = x, x2 = x + w, yB = y + h;
      g->fill_rectangle(x1, yB - hlw, x2, yB, color);
    }
    void hline(Graphics* g, coord_t x, coord_t y, coord_t w, coord_t h, color_t color, font_t font, double position) {
      set_parameters(g, x, y, w, h, color, font);
      coord_t const hlw = hline_width();
      coord_t const x1 = x, x2 = x + w;
      coord_t const y0 = std::round(y + h * position);
      coord_t const yT = y0 - hlw / 2, yB = y0 + (hlw + 1) / 2;
      g->fill_rectangle(x1, yT, x2, yB, color);
    }
  };

  template<typename Graphics>
  class window_renderer_t {
    window_state_t& wstat;

  public:
    window_renderer_t(window_state_t& wstat): wstat(wstat) {}

  private:
    status_tracer_t m_tracer;
  public:
    bool has_blinking_cells() const {
      return m_tracer.has_blinking_cells();
    }

  private:
    //
    // line_tracer の実装
    //
    //   line_tracer は行の移動と変更を追跡し、
    //   どの行を BitBlt/XCopyArea でコピーできて、
    //   どの行を再描画する必要があるかを決定する。
    //   特に、行の描画内容が隣の行にはみ出ている行がある事を考慮に入れて、
    //   はみ出ている量に応じて invalidate を実施する。
    //
    //   変化を追跡する為に前回の内容と新しい内容を比較する。
    //   行の追跡と変更の検出には行の id/version を用いる。
    //   はみ出ている量は現在は 1 で固定している。
    //   将来的に拡張する時は traceline_trace の中で a/d を設定する様にすれば良い。
    //   はみ出ている量は同じ id の行でも前回と今回で変化している可能性がある事に注意する。
    //
    class line_tracer {
    public:
      struct line_record {
        curpos_t y0 = -1;         //!< 移動前・移動後の対応する表示位置
        curpos_t a = 1;           //!< 行の描画内容が上にa行はみ出ている
        curpos_t d = 1;           //!< 行の描画内容が下にd行はみ出ている
        bool changed = false;     //!< 自身が変更されたかどうか
        bool invalidated = false; //!< 自領域の再描画の必要性
        bool redraw = false;      //!< 再描画が要求されるかどうか
      };

    private:
      bool tmargin_invalidated;
      bool bmargin_invalidated;
      std::vector<line_record> new_trace;
      std::vector<line_record> old_trace;

      void initialize(term_view_t const& view, status_tracer_t const& status_tracer, bool blinking_changed) {
        auto const& old_lines = status_tracer.lines();
        std::size_t const old_height = old_lines.size();
        std::size_t const new_height = view.height();
        tmargin_invalidated = false;
        bmargin_invalidated = false;
        old_trace.clear();
        new_trace.clear();
        old_trace.resize(old_height);
        new_trace.resize(new_height);

        for (std::size_t i = 0, j = 0; i < new_height; i++) {
          auto const id = view.line(i).id();
          auto const version = view.line(i).version();
          for (std::size_t j1 = j; j1 < old_height; j1++) {
            if (old_lines[j1].id == id) {
              bool const changed = old_lines[j1].version != version ||
                (blinking_changed && old_lines[j1].has_blinking);
              old_trace[j1].y0 = i;
              new_trace[i].y0 = j1;
              old_trace[j1].changed = changed;
              new_trace[i].changed = changed;
              j = j1 + 1;
              break;
            }
          }
        }
      }

      void invalidate() {
        // invalidate by self change
        curpos_t const new_height = (curpos_t) new_trace.size();
        for (curpos_t i = 0; i < new_height; i++) {
          if (new_trace[i].y0 == -1 || new_trace[i].changed)
            new_trace[i].invalidated = true;
        }

        // invalidate around old lines
        curpos_t const old_height = (curpos_t) old_trace.size();
        for (curpos_t j = 0; j < old_height; j++) {
          auto const& entry = old_trace[j];
          curpos_t const j1a = std::max(j - entry.a, 0);
          curpos_t const j1d = std::min(j + entry.d, old_height - 1);
          if (entry.y0 == -1 || entry.changed) {
            for (curpos_t j1 = j1a; j1 < j; j1++) {
              curpos_t const i1 = old_trace[j1].y0;
              if (i1 >= 0) new_trace[i1].invalidated = true;
            }
            for (curpos_t j1 = j1d; j1 > j; j1--) {
              curpos_t const i1 = old_trace[j1].y0;
              if (i1 >= 0) new_trace[i1].invalidated = true;
            }
          } else {
            curpos_t const shift = entry.y0 - j;
            for (curpos_t j1 = j1a; j1 < j; j1++) {
              curpos_t const i1 = old_trace[j1].y0;
              if (i1 >= 0 && i1 - j1 != shift) new_trace[i1].invalidated = true;
            }
            for (curpos_t j1 = j1d; j1 > j; j1--) {
              curpos_t const i1 = old_trace[j1].y0;
              if (i1 >= 0 && i1 - j1 != shift) new_trace[i1].invalidated = true;
            }
          }
        }

        // invalidate around new lines
        for (curpos_t i = 0; i < new_height; i++) {
          auto const& entry = new_trace[i];
          curpos_t const i1a = std::max(i - entry.a, 0);
          curpos_t const i1d = std::min(i + entry.d, new_height - 1);
          if (entry.y0 == -1 || entry.changed) {
            for (curpos_t i1 = i1a; i1 < i; i1++)
              new_trace[i1].invalidated = true;
            for (curpos_t i1 = i1d; i1 > i; i1--)
              new_trace[i1].invalidated = true;
          } else {
            curpos_t const shift = i - entry.y0;
            for (curpos_t i1 = i1a; i1 < i; i1++) {
              curpos_t const j1 = new_trace[i1].y0;
              if (j1 >= 0 && i1 - j1 != shift)
                new_trace[i1].invalidated = true;
            }
            for (curpos_t i1 = i1d; i1 > i; i1--) {
              curpos_t const j1 = new_trace[i1].y0;
              if (j1 >= 0 && i1 - j1 != shift)
                new_trace[i1].invalidated = true;
            }
          }
        }
      }

      void check_redraw() {
        curpos_t const height = (curpos_t) new_trace.size();
        for (curpos_t i = 0; i < height; i++) {
          auto const& entry = new_trace[i];
          curpos_t const i1a = std::max(i - entry.a, 0);
          curpos_t const i1d = std::min(i + entry.d, height - 1);
          for (curpos_t i1 = i1a; i1 <= i1d; i1++) {
            if (new_trace[i1].invalidated) {
              new_trace[i].redraw = true;
              break;
            }
          }
        }
      }

      void invalidate_and_check_for_margin() {
        curpos_t const new_height = (curpos_t) new_trace.size();
        curpos_t const old_height = (curpos_t) old_trace.size();
        for (curpos_t j = 0; j < old_height; j++) {
          auto const& entry = old_trace[j];
          if (entry.y0 != j || entry.changed) {
            if (j - entry.a < 0)
              tmargin_invalidated = true;
            if (j + entry.d >= new_height)
              bmargin_invalidated = true;
          }
        }
        for (curpos_t i = 0; i < new_height; i++) {
          auto& entry = new_trace[i];
          if (entry.y0 != i || entry.changed) {
            if (i - entry.a < 0) {
              tmargin_invalidated = true;
              entry.redraw = true;
            }
            if (i + entry.d >= new_height) {
              bmargin_invalidated = true;
              entry.redraw = true;
            }
          }
        }
      }

    public:
      void trace(term_view_t const& view, status_tracer_t const& stracer, bool blinking_changed) {
        initialize(view, stracer, blinking_changed);
        invalidate();
        check_redraw();
        invalidate_and_check_for_margin();
      }
      line_record const& operator[](curpos_t i) const { return new_trace[i]; }
      bool is_tmargin_invalidated() const { return tmargin_invalidated; }
      bool is_bmargin_invalidated() const { return bmargin_invalidated; }
    };

  private:
    struct line_content {
      curpos_t previous_line_index;
      bool is_invalidated;
      bool is_redraw_requested;

      std::vector<cell_t> cells;
    };
    struct content_update {
      bool is_content_changed;
      bool is_cursor_changed;
      bool is_blinking_changed;
      bool is_full_update;
      bool is_tmargin_invalidated;
      bool is_bmargin_invalidated;
      std::vector<line_content> lines;
    };
    bool construct_update(term_view_t const& view, content_update& update, bool requests_full_update) {
      update.is_content_changed = m_tracer.is_content_changed(view);
      update.is_cursor_changed = m_tracer.is_cursor_changed(wstat, view);
      update.is_blinking_changed = m_tracer.is_blinking_changed(wstat);
      update.is_full_update = requests_full_update || m_tracer.requests_full_update(wstat, view);

      bool const redraw = update.is_full_update ||
        update.is_content_changed ||
        update.is_blinking_changed;
      if (!redraw) return false;

      curpos_t const height = view.height();
      update.lines.resize(height);
      if (update.is_full_update) {
        for (curpos_t i = 0; i < height; i++) {
          line_content& line = update.lines[i];
          line.previous_line_index = -1;
          line.is_invalidated = true;
          line.is_redraw_requested = true;
          view.order_cells_in(line.cells, position_client, view.line(i));
        }
      } else {
        line_tracer ltracer;
        ltracer.trace(view, m_tracer, update.is_blinking_changed);
        update.is_tmargin_invalidated = ltracer.is_tmargin_invalidated();
        update.is_bmargin_invalidated = ltracer.is_bmargin_invalidated();
        for (curpos_t i = 0; i < height; i++) {
          line_content& line = update.lines[i];
          line.previous_line_index = ltracer[i].y0;
          line.is_invalidated = ltracer[i].invalidated;
          line.is_redraw_requested = ltracer[i].redraw;
          if (line.is_redraw_requested)
            view.order_cells_in(line.cells, position_client, view.line(i));
        }
      }
      return true;
    }

  public:
    void draw_cursor(Graphics& graphics, term_view_t const& view) {
      int const cursor_shape = view.cursor_shape();

      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const yunit = wstat.m_yunit;
      coord_t const xunit = wstat.m_xunit;

      coord_t x0 = xorigin + xunit * view.client_x();
      coord_t const y0 = yorigin + yunit * view.client_y();
      bool const r2l = view.cur_r2l();
      coord_t size;
      bool underline = false;
      if (view.xenl()) {
        // 行末にいる時は設定に関係なく縦棒にする。
        x0 -= r2l ? -2 : 2;
        size = 2;
      } else if (cursor_shape == 0) {
        underline = true;
        size = 3;
      } else if (cursor_shape < 0) {
        size = (xunit * std::min(100, -cursor_shape) + 99) / 100;
      } else if (cursor_shape) {
        underline = true;
        size = (yunit * std::min(100, cursor_shape) + 99) / 100;
      }

      if (underline) {
        coord_t const height = std::min(yunit, std::max(size, wstat.m_caret_underline_min_height));
        coord_t const x1 = x0, y1 = y0 + yunit - height;
        graphics.invert_rectangle(x1, y1, x1 + xunit, y1 + height);
      } else {
        coord_t const width = std::min(xunit, std::max(size, wstat.m_caret_vertical_min_width));
        if (r2l) x0 += xunit - width;
        graphics.invert_rectangle(x0, y0, x0 + width, y0 + yunit);
      }
    }

  public:
    void draw_background(Graphics& graphics, term_view_t const& view, content_update& update) {
      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const yunit = wstat.m_yunit;
      coord_t const xunit = wstat.m_xunit;
      curpos_t const height = view.height();
      tstate_t const& s = view.state();
      color_resolver_t _color(s, view.atable());

      color_t const bg = _color.resolve(s.m_default_bg_space, s.m_default_bg_color);
      if (update.is_full_update) {
        graphics.fill_rectangle(0, 0, wstat.m_canvas_width, wstat.m_canvas_height, bg);
      } else {
        coord_t y1 = update.is_tmargin_invalidated ? 0 : yorigin;
        coord_t y2 = yorigin;
        for (curpos_t iline = 0; iline < height; iline++) {
          if (update.lines[iline].is_invalidated) {
            y2 += yunit;
          } else {
            if (y1 < y2)
              graphics.fill_rectangle(0, y1, wstat.m_canvas_width, y2, bg);
            y1 = y2 += yunit;
          }
        }
        if (update.is_bmargin_invalidated) y2 = wstat.m_canvas_height;
        if (y1 < y2)
          graphics.fill_rectangle(0, y1, wstat.m_canvas_width, y2, bg);
      }

      coord_t x = xorigin, y = yorigin;
      coord_t x0 = x;
      color_t bg0 = 0;
      auto _fill = [=, &graphics, &x, &y, &x0, &bg0] () {
        if (x0 >= x || !bg0 || bg0 == bg) return;
        graphics.fill_rectangle(x0, y, x, y + yunit, bg0);
      };

      for (curpos_t iline = 0; iline < height; iline++, y += yunit) {
        // Note: はみ出ない事は自明なので is_redraw_requested
        //   ではなくて is_invalidated の時だけ描画でOK
        if (!update.lines[iline].is_invalidated) continue;

        std::vector<cell_t>& cells = update.lines[iline].cells;
        x = xorigin;
        x0 = x;
        bg0 = 0;

        for (std::size_t i = 0; i < cells.size(); ) {
          auto const& cell = cells[i++];
          color_t const bg1 = _color.resolve_bg(cell.attribute);
          if (bg1 != bg0) {
            _fill();
            bg0 = bg1;
            x0 = x;
          }
          x += cell.width * xunit;
        }
        _fill();
      }
    }

  private:
    void clip_decdhl_upper(Graphics& g, coord_t y) {
      g.clip_rectangle(0, 0, wstat.m_canvas_width, y + wstat.m_yunit);
    }
    void clip_decdhl_lower(Graphics& g, coord_t y) {
      g.clip_rectangle(0, y, wstat.m_canvas_width, wstat.m_canvas_height);
    }

  private:
    struct character_drawer {
      static constexpr std::uint32_t flag_processed = charflag_private1;

      window_renderer_t* renderer;
      Graphics* g;

      coord_t xorigin;
      coord_t yorigin;
      coord_t xunit;
      coord_t yunit;
      curpos_t height;

      attribute_table const* _atable = nullptr;
      color_resolver_t _color;
      font_resolver_t _font;

      batch_string_drawer<Graphics> m_str;
      graphics_drawer<Graphics> m_graph;

    public:
      character_drawer() {}

    private:
      aflags_t invisible_flags = 0;
      bool _visible(std::uint32_t code, cattr_t const& attr, bool sp_visible = false) const {
        return code != ascii_nul && (sp_visible || code != ascii_sp)
          && !(code & flag_processed)
          && !_atable->is_invisible(attr);
      }

      // DECDHL用の制限
      void clip(font_t font, coord_t y) const {
        if (font & font_layout_upper_half)
          renderer->clip_decdhl_upper(*g, y);
        else if (font & font_layout_lower_half)
          renderer->clip_decdhl_lower(*g, y);
      }
      void unclip(font_t font) const {
        // DECDHL用の制限の解除
        if (font & (font_layout_upper_half | font_layout_lower_half))
          g->clip_clear();
      }

    private:
      void draw_iso2022_graphics(coord_t x1, coord_t y1, char32_t code, cell_t const& cell) {
        color_t const fg = _color.resolve_fg(cell.attribute);
        font_t const font = _font.resolve_font(cell.attribute);

        this->clip(font, y1);

        code -= charflag_iso2022_mosaic_beg;

        if (code < 0x1000) {
          // 文字的な図形 (フォントと同様の変形を受ける)
          font_metric_t fmetric(xunit, yunit);
          auto [dx, dy, dxW] = fmetric.get_displacement(font);
          coord_t const x = x1 + std::round(dx + dxW * cell.width);
          coord_t const y = y1 + std::round(dy);
          auto [w, h] = fmetric.get_font_size(font);

          switch (code) {
          case mosaic_rarrow : m_graph.rarrow(g, x, y, w, h, fg, font); break;
          case mosaic_larrow : m_graph.larrow(g, x, y, w, h, fg, font); break;
          case mosaic_uarrow : m_graph.uarrow(g, x, y, w, h, fg, font); break;
          case mosaic_darrow : m_graph.darrow(g, x, y, w, h, fg, font); break;
          case mosaic_diamond: m_graph.diamond(g, x, y, w, h, fg, font); break;
          case mosaic_degree : m_graph.degree(g, x, y, w, h, fg, font); break;
          case mosaic_pm     : m_graph.pm(g, x, y, w, h, fg, font); break;
          case mosaic_lantern: m_graph.lantern(g, x, y, w, h, fg, font); break;
          case mosaic_le     : m_graph.le(g, x, y, w, h, fg, font); break;
          case mosaic_ge     : m_graph.ge(g, x, y, w, h, fg, font); break;
          case mosaic_pi     : m_graph.pi(g, x, y, w, h, fg, font); break;
          case mosaic_ne     : m_graph.ne(g, x, y, w, h, fg, font); break;
          default:
          case mosaic_bullet : m_graph.bullet(g, x, y, w, h, fg, font); break;
          }
        } else {
          // 背景的な図形
          coord_t w = xunit, h = yunit;
          if (font & font_decdwl) w *= 2;
          if (font & font_decdhl) h *= 2;
          if (font & font_layout_lower_half) y1 -= yunit;
          if (0x1001 <= code && code < 0x1100) {
            m_graph.boxline(g, x1, y1, w, h, fg, font, code & 0xFF);
          } else {
            switch (code) {
            case mosaic_white : m_graph.white_box(g, x1, y1, w, h, fg, font); break;
            case mosaic_black : m_graph.black_box(g, x1, y1, w, h, fg, font); break;
            case mosaic_check : m_graph.check_box(g, x1, y1, w, h, fg, font); break;
            case mosaic_hline1: m_graph.overline(g, x1, y1, w, h, fg, font); break;
            case mosaic_hline3: m_graph.hline(g, x1, y1, w, h, fg, font, 0.25); break;
            case mosaic_hline7: m_graph.hline(g, x1, y1, w, h, fg, font, 0.75); break;
            case mosaic_hline9: m_graph.underline(g, x1, y1, w, h, fg, font); break;
            }
          }
        }

        this->unclip(font);
      }

    private:
      void draw_unicode(coord_t x1, coord_t y1, char32_t code, cell_t const& cell) {
        auto const& attr = cell.attribute;
        color_t const fg = _color.resolve_fg(attr);
        font_t const font = _font.resolve_font(attr);
        m_str.start(font);
        m_str.push(code, cell.width);
        this->clip(font, y1);
        m_str.render(g, x1, y1, font, fg);
        this->unclip(font);
      }

      void draw_unicode_extended(
        coord_t x1, coord_t y1, cell_t const& cell,
        std::vector<cell_t>& cells, std::size_t& i, coord_t& x
      ) {
        std::uint32_t code = cell.character.value;
        code &= ~charflag_cluster_extension;
        auto const& attr = cell.attribute;
        color_t const fg = _color.resolve_fg(attr);
        font_t const font = _font.resolve_font(attr);
        m_str.start(font);
        m_str.push(code, cell.width);

        // 同じ色・フォントのセルは同時に描画してしまう。
        for (std::size_t j = i; j < cells.size(); j++) {
          auto const& cell2 = cells[j];
          std::uint32_t code2 = cell2.character.value;

          // 回転文字の場合は一つずつ書かなければならない。
          // 零幅の cluster などだけ一緒に描画する。
          if ((font & font_rotation_mask) && cell2.width) break;

          bool const is_cluster = code2 & charflag_cluster_extension;
          color_t const fg2 = _color.resolve_fg(cell2.attribute);
          font_t const font2 = _font.resolve_font(cell2.attribute);
          code2 &= ~charflag_cluster_extension;
          if (!_visible(code2, cell2.attribute, font & font_layout_proportional)) {
            if (font & font_layout_proportional) break;
            m_str.skip(cell2.width);
          } else if (!character_t::is_char(code2)) {
            m_str.skip(cell2.width);
            continue;
          } else if (is_cluster || (fg == fg2 && font == font2)) {
            m_str.push(code2, cell2.width);
          } else {
            if (font & font_layout_proportional) break;
            m_str.skip(cell2.width);
            continue;
          }

          if (i == j) {
            i++;
            x += cell2.width * xunit;
          } else
            cells[j].character.value |= flag_processed;
        }

        this->clip(font, y1);
        m_str.render(g, x1, y1, font, fg);
        this->unclip(font);
      }

    private:
      std::vector<char32_t> vec;

      void draw_iso2022_extended(
        coord_t x1, coord_t y1, cell_t const& cell,
        std::vector<cell_t>& cells, std::size_t& i, coord_t& x
      ) {
        std::uint32_t const code = cell.character.value;
        std::uint32_t const cssize = code2cssize(code);
        std::uint32_t const cs = code2charset(code, cssize);

        auto const& iso2022 = iso2022_charset_registry::instance();
        iso2022_charset const* charset = iso2022.charset(cs);
        if (!(charset && charset->get_chars(vec, (code & charflag_iso2022_mask_code) % cssize))) {
          // 文字集合で定義されていない区点は REPLACEMENT CHARACTER として表示する
          draw_unicode(x1, y1, 0xFFFD, cell);
          return;
        } else if (vec.size() == 1 && is_iso2022_mosaic(vec[0])) {
          draw_iso2022_graphics(x1, y1, vec[0], cell);
          return;
        } else if (vec.empty()) {
          return;
        }

        auto const& attr = cell.attribute;
        color_t const fg = _color.resolve_fg(attr);
        font_t const font = _font.resolve_font(attr);
        m_str.start(font);
        m_str.push(vec[0], cell.width);
        for (std::size_t k = 1; k < vec.size(); k++)
          m_str.push(vec[k], 0);

        // 同じ文字集合・同じ色・フォントのセルは同時に描画する。
        for (std::size_t j = i; j < cells.size(); j++) {
          auto const& cell2 = cells[j];
          std::uint32_t code2 = cell2.character.value;

          // 回転文字の場合は一つずつ書かなければならない。
          // 零幅の cluster などだけ一緒に描画する。
          if ((font & font_rotation_mask) && cell2.width) break;

          bool const is_cluster = code2 & charflag_cluster_extension;
          bool const cs2 = code2charset(code2);
          bool const is_same_charset = (code2 & charflag_iso2022) && cs2 == cs;
          color_t const fg2 = _color.resolve_fg(cell2.attribute);
          font_t const font2 = _font.resolve_font(cell2.attribute);

          code2 &= ~charflag_cluster_extension;
          if (!_visible(code2, cell2.attribute, font & font_layout_proportional)) {
            goto discard;
          } else if (is_cluster) {
            m_str.push(code2, cell2.width);
            goto processed;
          } else if (is_same_charset && fg == fg2 && font == font2) {
            iso2022_charset const* charset2 = iso2022.charset(cs2);
            if (!(charset2 && charset2->get_chars(vec, (code2 & charflag_iso2022_mask_code) % cssize))) {
              goto process_later;
            } else if (vec.size() == 1 && is_iso2022_mosaic(vec[0])) {
              draw_iso2022_graphics(x1 + m_str.x(), y1, vec[0], cell2);
              m_str.skip(cell2.width);
              goto processed;
            } else if (vec.empty()) {
              goto discard;
            }

            m_str.push(vec[0], cell.width);
            for (std::size_t k = 1; k < vec.size(); k++)
              m_str.push(vec[k], 0);
            goto processed;
          } else {
            goto process_later;
          }
          continue;

        discard:
          if (font & font_layout_proportional) break;
          m_str.skip(cell2.width);
          goto processed;

        process_later:
          if (font & font_layout_proportional) break;
          m_str.skip(cell2.width);
          continue;

        processed:
          if (i == j) {
            i++;
            x += cell2.width * xunit;
          } else
            cells[j].character.value |= flag_processed;
        }

        this->clip(font, y1);
        m_str.render(g, x1, y1, font, fg);
        this->unclip(font);
      }

    public:
      void draw(window_renderer_t* renderer, Graphics& g, term_view_t const& view, std::vector<line_content>& content) {
        this->renderer = renderer;
        this->g = &g;

        auto const& wstat = renderer->wstat;
        this->xorigin = wstat.m_xframe;
        this->yorigin = wstat.m_yframe;
        this->yunit = wstat.m_yunit;
        this->xunit = wstat.m_xunit;
        this->height = view.height();
        this->_atable = &view.atable();
        this->_color = color_resolver_t(view.state(), view.atable());
        this->_font = font_resolver_t(view.atable());
        this->m_str.initialize(wstat.m_xunit, wstat.m_yunit);
        this->m_graph.initialize(wstat.m_xunit, wstat.m_yunit);

        this->invisible_flags = attr_invisible_set;
        if (wstat.m_blinking_count & 1) invisible_flags |= attr_rapid_blink_set;
        if (wstat.m_blinking_count & 2) invisible_flags |= attr_blink_set;

        for (curpos_t iline = 0; iline < height; iline++) {
          if (!content[iline].is_redraw_requested) continue;
          auto& cells = content[iline].cells;

          coord_t x = xorigin;
          coord_t const y = yorigin + iline * yunit;
          m_str.reserve(cells.size());

          for (std::size_t i = 0; i < cells.size(); ) {
            auto const& cell = cells[i];
            coord_t const x1 = x;
            coord_t const& y1 = y;
            i++;
            x += cell.width * xunit;

            std::uint32_t code = cell.character.value;
            if (!_visible(code, cell.attribute)) continue;

            if (character_t::is_char(code)) {
              draw_unicode_extended(x1, y1, cell, cells, i, x);
            } else if (code & charflag_iso2022) {
              draw_iso2022_extended(x1, y1, cell, cells, i, x);
            }
          }

          // clear private flags
          for (cell_t& cell : cells) cell.character.value &= ~flag_processed;
        }
      }
    };

  public:
    void draw_characters(Graphics& g, term_view_t const& view, std::vector<line_content>& content) {
      character_drawer drawer;
      drawer.draw(this, g, view, content);
    }

    void draw_characters_mono(Graphics& g, term_view_t const& view, std::vector<line_content> const& content) {
      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const yunit = wstat.m_yunit;
      coord_t const xunit = wstat.m_xunit;
      curpos_t const height = view.height();

      std::vector<cell_t> cells;
      typename Graphics::character_buffer charbuff;
      for (curpos_t iline = 0; iline < height; iline++) {
        if (!content[iline].is_redraw_requested) continue;
        std::vector<cell_t> const& cells = content[iline].cells;

        coord_t xoffset = xorigin;
        coord_t yoffset = yorigin + iline * yunit;
        charbuff.clear();
        charbuff.reserve(cells.size());
        for (auto const& cell : cells) {
          std::uint32_t code = cell.character.value;
          if (code & charflag_cluster_extension)
            code &= ~charflag_cluster_extension;
          if (code == ascii_nul || code == ascii_sp) {
            if (charbuff.empty())
              xoffset += cell.width * xunit;
            else
              charbuff.shift(cell.width * xunit);
          } else if (code == (code & unicode_mask)) {
            charbuff.add_char(code, cell.width * xunit);
          }
          // ToDo: その他の文字・マーカーに応じた処理。
        }
        if (!charbuff.empty())
          g.draw_characters(xoffset, yoffset, charbuff, 0, 0);
      }
    }

  private:
    template<typename F>
    struct decoration_horizontal_t {
      coord_t x0 = 0;
      std::uint32_t style = 0;
      F _draw;

    public:
      decoration_horizontal_t(F draw): _draw(draw) {}

    public:
      void update(curpos_t x, std::uint32_t new_style, xflags_t xflags) {
        if (new_style) {
          switch ((xflags & xflags_decdhl_mask).value()) {
          case xflags_decdhl_upper_half.value():
            new_style |= (std::uint32_t) xflags_decdhl_upper_half;
            break;
          case xflags_decdhl_lower_half.value():
            new_style |= (std::uint32_t) xflags_decdhl_lower_half;
            break;
          }
        }
        if (style != new_style) {
          if (style) _draw(x0, x, style);
          x0 = x;
          style = new_style;
        }
      }
    };
  public:
    void draw_decoration(Graphics& g, term_view_t const& view, std::vector<line_content>& content) {
      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const yunit = wstat.m_yunit;
      coord_t const xunit = wstat.m_xunit;
      curpos_t const height = view.height();
      tstate_t const& s = view.state();
      color_resolver_t _color(s, view.atable());

      aflags_t invisible_flags = attr_invisible_set;
      if (wstat.m_blinking_count & 1) invisible_flags |= attr_rapid_blink_set;
      if (wstat.m_blinking_count & 2) invisible_flags |= attr_blink_set;

      coord_t x = xorigin, y = yorigin;

      color_t color0 = 0;

      decoration_horizontal_t dec_ul([&] (coord_t x1, coord_t x2, std::uint32_t style) {
        coord_t const u = (coord_t) std::ceil(yunit * 0.05);
        coord_t h = u;
        switch ((style & xflags_decdhl_mask).value()) {
        case xflags_decdhl_upper_half.value(): return;
        case xflags_decdhl_lower_half.value():
          h *= 2;
          break;
        }

        coord_t pitch1, pitch2;
        switch ((style & ~xflags_decdhl_mask).value()) {
        case 4: // dotted
          pitch1 = pitch2 = u;
          goto dashed;
        case 5: // dashed
          pitch1 = xunit * 3 / 4;
          pitch2 = xunit - pitch1;
          goto dashed;
        dashed:
          if (style & xflags_decdhl_mask) {
            pitch1 *= 2;
            pitch2 *= 2;
          }
          for (coord_t x = x1; x < x2; x += pitch1 + pitch2)
            g.fill_rectangle(x, y + yunit - h, std::min(x2, x + pitch1), y + yunit, color0);
          break;
        case 2: // double
          g.fill_rectangle(x1, y + yunit - 3 * h, x2, y + yunit - 2 * h, color0);
          [[fallthrough]];
        default: // single
          g.fill_rectangle(x1, y + yunit - h, x2, y + yunit, color0);
          break;
        }
      });

      decoration_horizontal_t dec_cl([&] (coord_t x1, coord_t x2, std::uint32_t style) {
        coord_t const u = (coord_t) std::ceil(yunit * 0.05);
        coord_t h = u;
        switch ((style & xflags_decdhl_mask).value()) {
        case xflags_decdhl_upper_half.value(): return;
        case xflags_decdhl_lower_half.value():
          h *= 2;
          break;
        }

        // 線には太さがあるので調整
        x1 += h / 2;
        x2 -= h / 2;
        coord_t y1 = y + yunit - h / 2;

        coord_t p = h * 3;
        if (style & xflags_decdhl_mask) p *= 2;
        int const count = (x2 - x1) / p;
        double const pitch = double(x2 - x1) / count;

        coord_t fx1, fx2 = x1;
        coord_t fy1 = y1 - h * 3 / 2, fy2 = y1;
        for (int i = 0; i < count; i++) {
          fx1 = fx2;
          fx2 = std::round(x1 + (i + 1) * pitch);
          g.draw_line(fx1, fy1, fx2, fy2, color0, h);
          std::swap(fy1, fy2);
        }
      });

      decoration_horizontal_t dec_sl([&] (coord_t x1, coord_t x2, std::uint32_t style) {
        coord_t h = (coord_t) std::ceil(yunit * 0.05), y2 = y + yunit / 2;
        if ((style & ~xflags_decdhl_mask) != 2) {
          // 一重打ち消し線
          switch ((style & xflags_decdhl_mask).value()) {
          case xflags_decdhl_lower_half.value(): return;
          case xflags_decdhl_upper_half.value():
            h *= 2; y2 = y + yunit;
            break;
          }
        } else {
          // 二重打ち消し線
          switch ((style & xflags_decdhl_mask).value()) {
          case xflags_decdhl_lower_half.value():
            h *= 2; y2 = y + h; break;
          case xflags_decdhl_upper_half.value():
            h *= 2; y2 = y + yunit - h; break;
          default:
            g.fill_rectangle(x1, y2 - 2 * h, x2, y2 - h, color0);
            y2 += h;
            break;
          }
        }
        g.fill_rectangle(x1, y2 - h, x2, y2, color0);
      });

      decoration_horizontal_t dec_ol([&] (coord_t x1, coord_t x2, std::uint32_t style) {
        coord_t h = (coord_t) std::ceil(yunit * 0.05);
        switch ((style & xflags_decdhl_mask).value()) {
        case xflags_decdhl_lower_half.value(): return;
        case xflags_decdhl_upper_half.value():
          h *= 2;
          return;
        }
        g.fill_rectangle(x1, y, x2, y + h, color0);
        if ((style & ~xflags_decdhl_mask) == 2)
          g.fill_rectangle(x1, y + 2 * h, x2, y + 3 * h, color0);
      });

      auto _draw_frame = [&] (coord_t x1, coord_t x2, xflags_t xflags, int lline, int rline) {
        // 上の線と下の線は dec_ul, dec_ol に任せる。
        coord_t w = (coord_t) std::ceil(yunit * 0.05);
        if (xflags & xflags_decdhl_mask) w *= 2;
        if (lline) {
          g.fill_rectangle(x1, y, x1 + w, y + yunit, color0);
          if (lline > 1)
            g.fill_rectangle(x1 + 2 * w, y, x1 + 3 * w, y + yunit, color0);
        }
        if (rline) {
          g.fill_rectangle(x2 - w, y, x2, y + yunit, color0);
          if (rline > 1)
            g.fill_rectangle(x2 - 3 * w, y, x2 - 2 * w, y + yunit, color0);
        }
      };

      auto _draw_circle = [&] (coord_t x1, coord_t x2, xflags_t xflags) {
        coord_t w = (coord_t) std::ceil(yunit * 0.04);
        coord_t y1 = y, y2 = y + yunit;
        bool is_clipped = false;
        switch ((xflags & xflags_decdhl_mask).value()) {
        case xflags_decdhl_upper_half.value():
          is_clipped = true;
          this->clip_decdhl_upper(g, y);
          w *= 2;
          y2 = y + 2 * yunit;
          break;
        case xflags_decdhl_lower_half.value():
          is_clipped = true;
          this->clip_decdhl_lower(g, y);
          w *= 2;
          y1 = y - yunit;
          y2 = y + yunit;
          break;
        }
        g.draw_ellipse(x1, y1, x2, y2, color0, w);
        if (is_clipped)
          g.clip_clear();
      };

      auto _draw_stress = [&] (coord_t x1, coord_t x2, xflags_t xflags) {
        double w1 = yunit * 0.15, h1 = yunit * 0.15;
        switch ((xflags & xflags_decdhl_mask).value()) {
        case xflags_decdhl_lower_half.value(): return;
        case xflags_decdhl_upper_half.value():
          w1 *= 2.0;
          h1 *= 2.0;
          break;
        case xflags_decdhl_double_width.value():
          w1 *= 2.0;
          break;
        }
        coord_t const w = std::max<coord_t>(4, std::ceil(w1));
        coord_t const h = std::max<coord_t>(4, std::ceil(h1));
        coord_t const xL = (x1 + x2 - w - 1) / 2;
        coord_t const xR = xL + w;
        coord_t const yT = y - h;
        coord_t const yB = yT + h;
        g.fill_ellipse(xL, yT, xR + 1, yB + 1, color0);
      };

      for (curpos_t iline = 0; iline < height; iline++, y += yunit) {
        if (!content[iline].is_redraw_requested) continue;

        std::vector<cell_t>& cells = content[iline].cells;
        x = xorigin;
        color0 = 0;

        for (std::size_t i = 0; i < cells.size(); ) {
          auto const& cell = cells[i++];
          auto const& code = cell.character.value;
          aflags_t const aflags = view.atable().aflags(cell.attribute);
          xflags_t const xflags = view.atable().xflags(cell.attribute);
          if (cell.width == 0) continue;
          coord_t const cell_width = cell.width * xunit;
          color_t color = 0;
          if (code != ascii_nul && !(aflags & invisible_flags))
            color = _color.resolve_dc(cell.attribute);
          if (color != color0) {
            dec_ul.update(x, 0, (xflags_t) 0);
            dec_cl.update(x, 0, (xflags_t) 0);
            dec_sl.update(x, 0, (xflags_t) 0);
            dec_ol.update(x, 0, (xflags_t) 0);
          }

          if (color) {
            xflags_t const ideo = xflags & (xflags_ideogram_mask & ~xflags_ideogram_line_left);

            // 下線
            bool const ul2 = (aflags & attr_underline_mask) == attr_underline_double ||
              ideo == xflags_ideogram_line_double_rb;
            bool const ul1 = (aflags & attr_underline_mask) == attr_underline_single ||
              xflags & xflags_frame_set ||
              ideo == xflags_ideogram_line_single_rb;
            bool const ul3 = (aflags & attr_underline_mask) == attr_underline_curly;
            bool const ul4 = (aflags & attr_underline_mask) == attr_underline_dotted;
            bool const ul5 = (aflags & attr_underline_mask) == attr_underline_dashed;
            dec_ul.update(x, ul2 ? 2 : ul1 ? 1 : ul4 ? 4: ul5 ? 5 : 0, xflags);
            dec_cl.update(x, ul3 ? 3 : 0, xflags);

            // 上線
            bool const ol2 = ideo == xflags_ideogram_line_double_rt;
            bool const ol1 = xflags & (xflags_overline_set | xflags_frame_set) ||
              ideo == xflags_ideogram_line_single_rt;
            dec_ol.update(x, ol2 ? 2 : ol1 ? 1 : 0, xflags);

            // 打ち消し線
            bool const sl2(xflags & xflags_rlogin_double_strike);
            bool const sl1(aflags & attr_strike_set);
            dec_sl.update(x, sl2 ? 2 : sl1 ? 1 : 0, xflags);

            // 左線・右線
            bool const ll2(xflags & xflags_rlogin_double_lline);
            bool const ll1(xflags & (xflags_frame_set | xflags_rlogin_single_lline));
            int const ll = ll2 ? 2 : ll1 ? 1 : 0;
            bool const rl2(xflags & xflags_rlogin_double_rline);
            bool const rl1(xflags & (xflags_frame_set | xflags_rlogin_single_rline));
            int const rl = rl2 ? 2 : rl1 ? 1 : 0;
            _draw_frame(x, x + cell_width, xflags, ll, rl);

            // 丸
            if (xflags & xflags_circle_set)
              _draw_circle(x, x + cell_width, xflags);

            // 圏点
            if (ideo == xflags_ideogram_stress)
              _draw_stress(x, x + cell_width, xflags);
          }

          color0 = color;
          x += cell_width;
        }
        dec_ul.update(x, 0, (xflags_t) 0);
        dec_cl.update(x, 0, (xflags_t) 0);
        dec_sl.update(x, 0, (xflags_t) 0);
        dec_ol.update(x, 0, (xflags_t) 0);
      }
    }

  private:
    template<typename GraphicsBuffer>
    void transfer_unchanged_content(
      GraphicsBuffer& gbuffer,
      typename GraphicsBuffer::context_t const& ctx0,
      typename GraphicsBuffer::context_t const& ctx1,
      std::vector<line_content> const& content
    ) {
      coord_t const yorigin = wstat.m_yframe;
      coord_t const yunit = wstat.m_yunit;

      curpos_t y1dst = -1, y1src = -1;
      curpos_t y2dst = 0;
      auto const _transfer = [&, yorigin, yunit, width = wstat.m_canvas_width] () {
        if (y1dst >= 0) {
          coord_t const y1dst_pixel = yorigin + y1dst * yunit;
          coord_t const y1src_pixel = yorigin + y1src * yunit;
          coord_t const height = (y2dst - y1dst) * yunit;
          gbuffer.bitblt(ctx0, 0, y1dst_pixel, width, height, ctx1, 0, y1src_pixel);
          y1dst = -1;
        }
      };

      for (; y2dst < (curpos_t) content.size(); y2dst++) {
        if (!content[y2dst].is_invalidated) {
          curpos_t const y2src = content[y2dst].previous_line_index;
          if (y1dst >= 0 && y2dst - y2src == y1dst - y1src) continue;
          _transfer();
          y1dst = y2dst;
          y1src = y2src;
        } else {
          _transfer();
        }
      }
      _transfer();
    }

  public:
    // Requirements
    //   win.unset_cursor_timer()
    //   win.reset_cursor_timer()
    //   gbuffer.width()
    //   gbuffer.height()
    //   gbuffer.layer(index)
    //   gbuffer.bitblt(ctx1, x1, y1, w, h, ctx2, x2, y2)
    //   gbuffer.render(x1, y1, w, h, ctx2, x2, y2)
    template<typename Window, typename GraphicsBuffer>
    void render_view(Window& win, GraphicsBuffer& gbuffer, term_view_t& view, bool requests_full_update) {
      using graphics_t = typename GraphicsBuffer::graphics_t;
      using context_t = typename GraphicsBuffer::context_t;

      // update status
      view.update();

      content_update update;
      bool const content_redraw = this->construct_update(view, update, requests_full_update);
      {
        // update cursor state
        bool const cursor_blinking = view.state().is_cursor_blinking();
        bool const cursor_visible = wstat.is_cursor_visible(view);
        if (!cursor_visible || !cursor_blinking)
          win.unset_cursor_timer();
        else if (update.is_content_changed || update.is_cursor_changed)
          win.reset_cursor_timer();
      }

      context_t const ctx0 = gbuffer.layer(0);
      context_t const ctx1 = gbuffer.layer(1);
      graphics_t g0(gbuffer, ctx0);
      if (content_redraw) {
        // 変更のあった領域の描画
        wstat.m_canvas_width = gbuffer.width();
        wstat.m_canvas_height = gbuffer.height();
        //this->draw_characters_mono(g1, view, update.lines);
        this->draw_background(g0, view, update);
        this->draw_characters(g0, view, update.lines);
        this->draw_decoration(g0, view, update.lines);

        // 変化のなかった領域の転写
        this->transfer_unchanged_content(gbuffer, ctx0, ctx1, update.lines);

        // ToDo: 更新のあった部分だけ転送する?
        // 更新のあった部分 = 内容の変更・点滅の変更・選択範囲の変更など
        gbuffer.bitblt(ctx1, 0,0, gbuffer.width(), gbuffer.height(), ctx0, 0, 0);
      } else {
        // 前回のカーソル位置のセルをカーソルなしに戻す。
        coord_t const old_x1 = wstat.m_xframe + wstat.m_xunit * m_tracer.cur_x();
        coord_t const old_y1 = wstat.m_yframe + wstat.m_yunit * m_tracer.cur_y();
        gbuffer.bitblt(ctx0, old_x1, old_y1, wstat.m_xunit, wstat.m_yunit, ctx1, old_x1, old_y1);
      }

      if (wstat.is_cursor_appearing(view)) {
        this->draw_cursor(g0, view);
      }

      gbuffer.render(0, 0, gbuffer.width(), gbuffer.height(), ctx0, 0, 0);

      // ToDo: 二重チェックになっている気がする。もっと効率的な実装?
      m_tracer.store(wstat, view);
    }

  };

}
}

#endif
