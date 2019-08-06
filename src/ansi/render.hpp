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
    coord_t m_xpixel = 7;
    coord_t m_ypixel = 13;
    coord_t m_xframe = 1;
    coord_t m_yframe = 1;
  public:
    void configure_metric(contra::app::context& actx) {
      actx.read("term_col", this->m_col = 80);
      actx.read("term_row", this->m_row = 30);
      actx.read("term_xpixel", this->m_xpixel = 7);
      actx.read("term_ypixel", this->m_ypixel = 13);
      actx.read("term_xframe", this->m_xframe = 1);
      actx.read("term_yframe", this->m_yframe = 1);
      this->m_col = limit::term_col.clamp(this->m_col);
      this->m_row = limit::term_row.clamp(this->m_row);
      this->m_xpixel = limit::term_xpixel.clamp(this->m_xpixel);
      this->m_ypixel = limit::term_ypixel.clamp(this->m_ypixel);
    }

  public:
    coord_t calculate_client_width() const {
      return m_xpixel * m_col + 2 * m_xframe;
    }
    coord_t calculate_client_height() const {
      return m_ypixel * m_row + 2 * m_yframe;
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
      store_metric(wstat);
      store_color(view);
      store_content(view);
      store_cursor(wstat, view);
      store_blinking_state(wstat);
    }

  private:
    term_view_t const* m_view = nullptr;
  public:
    bool is_view_changed(term_view_t const& view) const {
      return m_view != &view;
    }
  private:
    void store_view(term_view_t const& view) {
      this->m_view = &view;
    }

  private:
    coord_t m_xframe = 0;
    coord_t m_yframe = 0;
    curpos_t m_col = 0;
    curpos_t m_row = 0;
    coord_t m_xpixel = 0;
    coord_t m_ypixel = 0;
  public:
    bool is_metric_changed(window_state_t const& wstat) const {
      if (m_xframe != wstat.m_xframe) return true;
      if (m_yframe != wstat.m_yframe) return true;
      if (m_col != wstat.m_col) return true;
      if (m_row != wstat.m_row) return true;
      if (m_xpixel != wstat.m_xpixel) return true;
      if (m_ypixel != wstat.m_ypixel) return true;
      return false;
    }
  private:
    void store_metric(window_state_t const& wstat) {
      m_xframe = wstat.m_xframe;
      m_yframe = wstat.m_yframe;
      m_col = wstat.m_col;
      m_row = wstat.m_row;
      m_xpixel = wstat.m_xpixel;
      m_ypixel = wstat.m_ypixel;
    }

  private:
    byte m_fg_space, m_bg_space;
    color_t m_fg_color, m_bg_color;
  public:
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
  public:
    curpos_t cur_x() const { return m_cur_x; }
    curpos_t cur_y() const { return m_cur_y; }
    bool is_cursor_changed(window_state_t const& wstat, term_view_t const& view) const {
      if (m_cur_visible != wstat.is_cursor_visible(view)) return true;
      if (m_cur_x != view.x() || m_cur_y != view.y() || m_cur_xenl != view.xenl()) return true;
      if (m_cur_shape != view.cursor_shape()) return true;
      if (m_cur_blinking != view.is_cursor_blinking()) return true;
      return false;
    }
    void store_cursor(window_state_t const& wstat, term_view_t const& view) {
      m_cur_visible = wstat.is_cursor_visible(view);
      m_cur_x = view.x();
      m_cur_y = view.y();
      m_cur_xenl = view.xenl();
      m_cur_shape = view.cursor_shape();
    }
  };

  class color_resolver_t {
    tstate_t const* s;
    byte m_space = (byte) attribute_t::color_space_default;
    color_t m_color = color_t(-1);
    color_t m_rgba = 0;

  public:
    color_resolver_t(tstate_t const& s): s(&s) {}

  public:
    color_t resolve(byte space, color_t color) {
      color_t rgba = 0;
      switch (space) {
      case attribute_t::color_space_default:
      case attribute_t::color_space_transparent:
      default:
        // transparent なので色はない。
        rgba = 0x00000000; // black
        break;
      case attribute_t::color_space_indexed:
        rgba = s->m_rgba256[color & 0xFF];
        break;
      case attribute_t::color_space_rgb:
        rgba = contra::dict::rgb2rgba(color);
        break;
      case attribute_t::color_space_cmy:
        rgba = contra::dict::cmy2rgba(color);
        break;
      case attribute_t::color_space_cmyk:
        rgba = contra::dict::cmyk2rgba(color);
        break;
      }

      m_space = space;
      m_color = color;
      m_rgba = rgba;
      return rgba;
    }

  private:
    std::pair<byte, color_t> get_fg(attribute_t const& attr) {
      byte space = attr.fg_space();
      color_t color = attr.fg_color();
      if (space == 0) {
        space = s->m_default_fg_space;
        color = s->m_default_fg_color;
      }
      return {space, color};
    }
    std::pair<byte, color_t> get_bg(attribute_t const& attr) {
      byte space = attr.bg_space();
      color_t color = attr.bg_color();
      if (space == 0) {
        space = s->m_default_bg_space;
        color = s->m_default_bg_color;
      }
      return {space, color};
    }
  public:
    color_t resolve_fg(attribute_t const& attr) {
      bool const inverse = attr.aflags & attribute_t::is_inverse_set;
      bool const selected = attr.xflags & attribute_t::ssa_selected;
      auto [space, color] = inverse != selected ? get_bg(attr) : get_fg(attr);
      if (space == m_space && color == m_color) return m_rgba;
      return resolve(space, color);
    }

    color_t resolve_bg(attribute_t const& attr) {
      bool const inverse = attr.aflags & attribute_t::is_inverse_set;
      bool const selected = attr.xflags & attribute_t::ssa_selected;
      auto [space, color] = inverse != selected ? get_fg(attr) : get_bg(attr);
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
    coord_t m_width, m_height;

  public:
    font_metric_t(coord_t xpixel, coord_t ypixel) {
      m_width = xpixel;
      m_height = ypixel;
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
     * @fn std::tuple<coord_t, coord_t, double> get_displacement(font_t font);
     * @return dx は描画の際の横のずれ量(文字幅1の文字に対して)
     * @return dy は描画の際の縦のずれ量
     * @return dxW は (文字の幅-1) に比例して増える各文字の横のずれ量
     */
    std::tuple<coord_t, coord_t, double> get_displacement(font_t font) const {
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
        dx = std::round(dxI + dxW);
        dy = std::round(dy);
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
    using attribute_t = contra::dict::attribute_t;
    using xflags_t = contra::dict::xflags_t;
    using aflags_t = contra::dict::aflags_t;

  private:
    aflags_t m_aflags = 0;
    xflags_t m_xflags = 0;
    font_t m_font = 0;
  public:
    font_t resolve_font(attribute_t const& attr) {
      if (attr.aflags == m_aflags && attr.xflags == m_xflags) return m_font;

      font_t ret = 0;
      if (xflags_t const face = (attr.xflags & attribute_t::ansi_font_mask) >> attribute_t::ansi_font_shift)
        ret |= font_face_mask & face << font_face_shft;

      switch (attr.aflags & attribute_t::is_heavy_set) {
      case attribute_t::is_bold_set: ret |= font_weight_bold; break;
      case attribute_t::is_faint_set: ret |= font_weight_faint; break;
      case attribute_t::is_heavy_set: ret |= font_weight_heavy; break;
      }

      if (attr.aflags & attribute_t::is_italic_set)
        ret |= font_flag_italic;
      else if (attr.aflags & attribute_t::is_fraktur_set)
        ret = (ret & ~font_face_mask) | font_face_fraktur;

      if (attr.xflags & attribute_t::is_sup_set)
        ret |= font_flag_small | font_layout_sup;
      else if (attr.xflags & attribute_t::is_sub_set)
        ret |= font_flag_small | font_layout_sub;

      if (attr.xflags & (attribute_t::is_frame_set | attribute_t::is_circle_set))
        ret |= font_flag_small | font_layout_framed;

      if (attr.xflags & attribute_t::is_proportional_set)
        ret |= font_layout_proportional;

      if (xflags_t const sco = (attr.xflags & attribute_t::sco_mask) >> attribute_t::sco_shift)
        ret |= font_rotation_mask & sco << font_rotation_shft;

      switch (attr.xflags & attribute_t::decdhl_mask) {
      case attribute_t::decdhl_double_width:
        ret |= font_decdwl;
        break;
      case attribute_t::decdhl_upper_half:
        ret |= font_decdwl | font_decdhl | font_layout_upper_half;
        break;
      case attribute_t::decdhl_lower_half:
        ret |= font_decdwl | font_decdhl | font_layout_lower_half;
        break;
      }

      m_aflags = attr.aflags;
      m_xflags = attr.xflags;
      m_font = ret;
      return ret;
    }
  };

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

      update.is_full_update = requests_full_update;
      if (m_tracer.is_view_changed(view))
        update.is_full_update = true;
      else if (m_tracer.is_metric_changed(wstat))
        update.is_full_update = true;
      else if (m_tracer.is_color_changed(view))
        update.is_full_update = true;

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
          view.get_cells_in_presentation(line.cells, view.line(i));
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
            view.get_cells_in_presentation(line.cells, view.line(i));
        }
      }
      return true;
    }

  public:
    template<typename Graphics>
    void draw_cursor(Graphics& graphics, term_view_t const& view) {
      int const cursor_shape = view.cursor_shape();

      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const ypixel = wstat.m_ypixel;
      coord_t const xpixel = wstat.m_xpixel;

      coord_t x0 = xorigin + xpixel * view.x();
      coord_t const y0 = yorigin + ypixel * view.y();

      coord_t size;
      bool underline = false;
      if (view.xenl()) {
        // 行末にいる時は設定に関係なく縦棒にする。
        x0 -= 2;
        size = 2;
      } else if (cursor_shape == 0) {
        underline = true;
        size = 3;
      } else if (cursor_shape < 0) {
        size = (xpixel * std::min(100, -cursor_shape) + 99) / 100;
      } else if (cursor_shape) {
        underline = true;
        size = (ypixel * std::min(100, cursor_shape) + 99) / 100;
      }

      if (underline) {
        coord_t const height = std::min(ypixel, std::max(size, wstat.m_caret_underline_min_height));
        coord_t const x1 = x0, y1 = y0 + ypixel - height;
        graphics.invert_rectangle(x1, y1, x1 + xpixel, y1 + height);
      } else {
        coord_t const width = std::min(xpixel, std::max(size, wstat.m_caret_vertical_min_width));
        graphics.invert_rectangle(x0, y0, x0 + width, y0 + ypixel);
      }
    }

  public:
    template<typename Graphics>
    void draw_background(Graphics& graphics, term_view_t const& view, content_update& update) {
      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const ypixel = wstat.m_ypixel;
      coord_t const xpixel = wstat.m_xpixel;
      curpos_t const height = view.height();
      tstate_t const& s = view.state();
      color_resolver_t _color(s);

      color_t const bg = _color.resolve(s.m_default_bg_space, s.m_default_bg_color);
      if (update.is_full_update) {
        graphics.fill_rectangle(0, 0, wstat.m_canvas_width, wstat.m_canvas_height, bg);
      } else {
        coord_t y1 = update.is_tmargin_invalidated ? 0 : yorigin;
        coord_t y2 = yorigin;
        for (curpos_t iline = 0; iline < height; iline++) {
          if (update.lines[iline].is_invalidated) {
            y2 += ypixel;
          } else {
            if (y1 < y2)
              graphics.fill_rectangle(0, y1, wstat.m_canvas_width, y2, bg);
            y1 = y2 += ypixel;
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
        graphics.fill_rectangle(x0, y, x, y + ypixel, bg0);
      };

      for (curpos_t iline = 0; iline < height; iline++, y += ypixel) {
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
          x += cell.width * xpixel;
        }
        _fill();
      }
    }

  private:
    template<typename Graphics>
    void clip_decdhl_upper(Graphics& g, coord_t y) {
      g.clip_rectangle(0, 0, wstat.m_canvas_width, y + wstat.m_ypixel);
    }
    template<typename Graphics>
    void clip_decdhl_lower(Graphics& g, coord_t y) {
      g.clip_rectangle(0, y, wstat.m_canvas_width, wstat.m_canvas_height);
    }

  private:
    template<typename Graphics>
    void acs_draw_invariant(Graphics& g, coord_t x, coord_t y, std::uint32_t code, curpos_t width, color_t color, font_t font) {
      coord_t const x1 = x, x2 = x + wstat.m_xpixel * width;
      coord_t y1 = y;
      coord_t y2 = y + wstat.m_ypixel;
      bool const bold = font & font_weight_bold; // bold or heavy

      // DECDWL, DECDHL
      bool const dwl = font & font_decdwl;
      bool const dhl = font & font_decdhl;
      if (dhl) {
        if (font & font_layout_lower_half) {
          y1 -= wstat.m_ypixel;
          this->clip_decdhl_lower(g, y);
        } else {
          y2 += wstat.m_ypixel;
          this->clip_decdhl_upper(g, y);
        }
      }

      auto const _center = [&] (coord_t x1, coord_t x2, bool decdwl) -> std::tuple<coord_t, coord_t> {
        coord_t xc1 = (x1 + x2) / 2;
        coord_t xc2 = xc1 + 1;
        if (decdwl || bold){
          xc1--;
          if (decdwl && bold) {
            xc1--;
            xc2++;
          }
        }
        return { xc1, xc2 };
      };

      int vparam = 0, hparam = 0;
      switch (code) {
      case ascii_0: // ACS_BLOCK
        {
          coord_t const w = (bold ? 2 : 1) * (dwl ? 2 : 1);
          g.fill_rectangle(x1, y1, x2, y1 + w, color);
          g.fill_rectangle(x1, y2 - w, x2, y2, color);
          g.fill_rectangle(x1, y1, x1 + w, y2, color);
          g.fill_rectangle(x2 - w, y1, x2, y2, color);
        }
        break;
      case ascii_h: // ACS_BOARD
        g.fill_rectangle(x1, y1, x2, y2, color);
        break;
      case ascii_a: // ACS_CKBOARD
        g.checked_rectangle(x1, y1, x2, y2, color);
        break;

      case ascii_x: // ACS_VLINE
        {
          auto [xc1, xc2] = _center(x1, x2, dwl);
          g.fill_rectangle(xc1, y1, xc2, y2, color);
        }
        break;
      case ascii_o: // ACS_S1
        g.fill_rectangle(x1, y1, x2, y1 + (bold ? 2 : 1) * (dhl ? 2 : 1), color);
        break;
      case ascii_p: hparam = -(y2 - y1) / 4; goto hline; // ACS_S3
      case ascii_q: hparam = 0; goto hline; // ACS_HLINE
      case ascii_r: hparam = +(y2 - y1) / 4; goto hline; // ACS_S7
      case ascii_s: // ACS_S9
        g.fill_rectangle(x1, y2 - (bold ? 2 : 1) * (dhl ? 2 : 1), x2, y2, color);
        break;
      hline:
        {
          auto [yc1, yc2] = _center(y1, y2, dhl);
          g.fill_rectangle(x1, yc1 + hparam, x2, yc2 + hparam, color);
        }
        break;
      case ascii_j: hparam = 2; vparam = 2; goto hvlines; // ACS_LRCORNER
      case ascii_k: hparam = 2; vparam = 3; goto hvlines; // ACS_URCORNER
      case ascii_l: hparam = 3; vparam = 3; goto hvlines; // ACS_ULCORNER
      case ascii_m: hparam = 3; vparam = 2; goto hvlines; // ACS_LLCORNER
      case ascii_n: hparam = 1; vparam = 1; goto hvlines; // ACS_PLUS
      case ascii_t: hparam = 3; vparam = 1; goto hvlines; // ACS_LTEE
      case ascii_u: hparam = 2; vparam = 1; goto hvlines; // ACS_RTEE
      case ascii_v: hparam = 1; vparam = 2; goto hvlines; // ACS_BTEE
      case ascii_w: hparam = 1; vparam = 3; goto hvlines; // ACS_TTEE
      hvlines:
        {
          auto [xc1, xc2] = _center(x1, x2, dwl);
          auto [yc1, yc2] = _center(y1, y2, dhl);
          switch (hparam) {
          case 1: g.fill_rectangle(x1, yc1, x2, yc2, color); break;
          case 2: g.fill_rectangle(x1, yc1, xc2, yc2, color); break;
          case 3: g.fill_rectangle(xc1, yc1, x2, yc2, color); break;
          }
          switch (vparam) {
          case 1: g.fill_rectangle(xc1, y1, xc2, y2, color); break;
          case 2: g.fill_rectangle(xc1, y1, xc2, yc2, color); break;
          case 3: g.fill_rectangle(xc1, yc1, xc2, y2, color); break;
          }
        }
        break;
      }
      if (dhl) g.clip_clear();
    }
    template<typename Graphics>
    void acs_draw_glyph(Graphics& g, coord_t x, coord_t y, std::uint32_t code, curpos_t width, color_t color, font_t font) {
      font_metric_t fmetric(wstat.m_xpixel, wstat.m_ypixel);
      auto [dx, dy, dxW] = fmetric.get_displacement(font);
      x += dx + dxW * (width - 1);
      y += dy;

      auto [w, h] = fmetric.get_font_size(font);

      if (font & font_layout_lower_half)
        this->clip_decdhl_lower(g, y);
      else if (font & font_layout_upper_half)
        this->clip_decdhl_upper(g, y);

      bool const bold = font & font_weight_bold; // bold or heavy

      auto const _polygon = [&] (coord_t (*points)[2], std::size_t count) {
        if (font & font_flag_italic) {
          for (std::size_t i = 0; i < count; i++)
            points[i][0] -= (points[i][1] - (y + h / 2)) / 3;
        }
        g.fill_polygon(points, count, color);
      };
      auto const _line = [&] (coord_t x1, coord_t y1, coord_t x2, coord_t y2, coord_t w) {
        if (font & font_flag_italic) {
          x1 -= (y1 - (y + h / 2)) / 3;
          x2 -= (y2 - (y + h / 2)) / 3;
        }
        g.draw_line(x1, y1, x2, y2, color, w);
      };

      switch (code) {
      case ascii_back_quote: // ACS_DIAMOND
        {
          coord_t const w1 = bold ? w * 9 / 10 : std::min(w - 2, w * 4 / 5);
          coord_t const h1 = bold ? h * 9 / 10 : std::min(h - 2, h * 4 / 5);
          coord_t const x1 = x + (w - w1) / 2;
          coord_t const y1 = y + (h - h1) / 2;
          coord_t const x2 = x1 + w1, xM = x + w / 2;
          coord_t const y2 = y1 + h1, yM = y + h / 2;
          coord_t points[4][2] = { {xM, y1}, {x1, yM}, {xM, y2}, {x2, yM} };
          _polygon(points, 4);
        }
        break;
      case ascii_minus: // ACS_UARROW
      case ascii_dot: // ACS_DARROW
        {
          coord_t const xM = x + w / 2;
          coord_t const vlw = std::ceil(wstat.m_ypixel / 20.0) * (1 + bold + !!(font & font_decdwl));
          coord_t const h1 = std::max<coord_t>(5, (bold ? h * 9 / 10 : std::min(h - 2, std::max(5, h * 4 / 5))) - (vlw - 1));
          coord_t const xwing = std::max<coord_t>(3, (w - vlw - 1) / 2);
          coord_t const ywing = !(font & font_decdwl) || (font & font_decdhl) ? xwing : (xwing + 1) / 2;
          if (code == ascii_minus) {
            coord_t const y1 = y + h  - (h - h1) / 2;
            coord_t const y2 = y1 - h1;
            _line(xM, y1, xM, y2, vlw);
            _line(xM, y2, xM - xwing, y2 + ywing, vlw);
            _line(xM, y2, xM + xwing, y2 + ywing, vlw);
          } else {
            coord_t const y1 = y + (h - h1) / 2;
            coord_t const y2 = y1 + h1;
            _line(xM, y1, xM, y2, vlw);
            _line(xM, y2, xM - xwing, y2 - ywing, vlw);
            _line(xM, y2, xM + xwing, y2 - ywing, vlw);
          }
        }
        break;
      case ascii_plus: // ACS_RARROW
      case ascii_comma: // ACS_LARROW
        {
          coord_t const yM = y + h / 2;
          coord_t const hlw = std::ceil(wstat.m_ypixel / 20.0) * (1 + bold + !!(font & font_decdhl));
          coord_t const w1 = std::max<coord_t>(4, (bold ? w * 9 / 10 : std::min(w - 2, std::max(5, w * 4 / 5))) - (hlw - 1));
          coord_t const ywing = std::max<coord_t>(3, (h - hlw - 1) / 4);
          coord_t const xwing = !(font & font_decdwl) || (font & font_decdhl) ? ywing : ywing * 2;
          if (code == ascii_comma) {
            coord_t const x1 = x + w  - (w - w1) / 2;
            coord_t const x2 = x1 - w1;
            _line(x1, yM, x2, yM, hlw);
            _line(x2, yM, x2 + xwing, yM - ywing, hlw);
            _line(x2, yM, x2 + xwing, yM + ywing, hlw);
          } else {
            coord_t const x1 = x + (w - w1) / 2;
            coord_t const x2 = x1 + w1;
            _line(x1, yM, x2, yM, hlw);
            _line(x2, yM, x2 - xwing, yM - ywing, hlw);
            _line(x2, yM, x2 - xwing, yM + ywing, hlw);
          }
        }
        break;
      case ascii_g: // ACS_PLMINUS
      case ascii_y: // ACS_LEQUAL
      case ascii_z: // ACS_GEQUAL
        {
          coord_t const hlw = std::ceil(wstat.m_ypixel / 20.0) * (1 + bold + !!(font & font_decdhl));
          coord_t const w1 = std::max<coord_t>(4, (bold ? w * 9 / 10 : std::min(w - 1, std::max(5, w * 7 / 8))) - hlw);

          coord_t const x2 = x + w / 2;
          coord_t const x1 = x2 - w1 / 2;
          coord_t const x3 = x1 + w1;
          coord_t const y2 = y + h * 9 / 20 - hlw;
          coord_t const y1 = y2 - h / 4;
          coord_t const y3 = y2 + h / 4;
          coord_t const y4 = y3 + hlw * 2;

          _line(x1, y4, x3, y4, hlw);
          switch (code) {
          case ascii_g:
            _line(x1, y2, x3, y2, hlw);
            _line(x2, y1, x2, y3, hlw);
            break;
          case ascii_y:
            _line(x3, y1, x1, y2, hlw);
            _line(x1, y2, x3, y3, hlw);
            break;
          case ascii_z:
            _line(x1, y1, x3, y2, hlw);
            _line(x3, y2, x1, y3, hlw);
            break;
          }
        }
        break;
      case ascii_vertical_bar: // ACS_NEQUAL
        {
          coord_t const hlw = std::ceil(wstat.m_ypixel / 20.0) * (1 + bold + !!(font & font_decdhl));
          coord_t const w1 = std::max<coord_t>(4, (bold ? w * 9 / 10 : std::min(w - 1, std::max(5, w * 7 / 8))) - hlw);
          coord_t const x2 = x + w / 2;
          coord_t const x1 = x2 - w1 / 2;
          coord_t const x3 = x1 + w1;
          coord_t const y2 = y + h * 9 / 20;
          coord_t const y1 = y2 - h / 10;
          coord_t const y3 = y1 + h / 5;
          coord_t const y0 = y2 - h / 4;
          coord_t const y4 = y2 + h / 4;
          _line(x1, y1, x3, y1, hlw);
          _line(x1, y3, x3, y3, hlw);
          _line(x2 + w / 4, y0, x2 -  w / 4, y4, hlw);
        }
        break;
      case ascii_left_brace: // ACS_PI (A)
        {
          coord_t const hlw = std::ceil(wstat.m_ypixel / 20.0) * (1 + bold + !!(font & font_decdhl));
          coord_t const w1 = std::max<coord_t>(4, (bold ? w * 9 / 10 : std::min(w - 1, std::max(5, w * 7 / 8))) - hlw);
          coord_t const x2 = x + w / 2;
          coord_t const x1 = x2 - w1 / 2;
          coord_t const x3 = x1 + w1;

          coord_t const y1 = y + h * 4 / 10;
          coord_t const y2 = y + h * 7 / 10;
          coord_t const tip = (w1 + 5) / 6;
          _line(x1, y1, x3, y1, hlw);
          _line(x1 + tip, y1, x1 + tip, y2, hlw);
          _line(x3 - tip, y1, x3 - tip, y2, hlw);
        }
        break;
      case ascii_tilde: // ACS_BULLET
        {
          coord_t const xo = x + w / 2, yo = y + h * 2 / 5;
          coord_t const hlw = std::ceil(wstat.m_ypixel / 20.0) * (1 + bold + !!(font & font_decdhl));
          g.fill_ellipse(xo - hlw, yo - hlw, xo + hlw + 1, yo + hlw + 1, color);
        }
        break;
      case ascii_f: // ACS_DEGREE
        {
          coord_t const hlw = std::ceil(wstat.m_ypixel / 20.0) * (1 + bold + !!(font & font_decdhl));
          coord_t const xo = x + w / 2 - hlw * 2, yo = y + hlw / 2;
          g.draw_ellipse(xo, yo, xo + 3 * hlw + 1, yo + 3 * hlw + 1, color, hlw);
        }
        break;
      case ascii_i: // ACS_LANTERN
        {
          coord_t const hlw = std::ceil(wstat.m_ypixel / 20.0) * (1 + bold + !!(font & font_decdhl));
          coord_t const xc3 = x + w - hlw / 2;
          coord_t const xc2 = xc3 - std::max(hlw * 2, w / 3);
          coord_t const xc1 = xc2 - std::max(hlw * 2, w / 3);
          coord_t const yc3 = y + h * 5 / 6- hlw / 2;
          coord_t const yc2 = y + h * 2 / 6;
          _line(xc1, yc2, xc3, yc2, hlw);
          _line(xc1, yc3, xc3, yc3, hlw);
          _line(xc1, yc2, xc1, yc3, hlw);
          _line(xc3, yc2, xc3, yc3, hlw);
          _line(xc2, (yc2 + yc3) / 2, xc2, yc3, hlw);
          _line(xc2, yc2 - hlw * 2, xc2, yc2, hlw);
          _line(x, yc2 - hlw * 2, xc2, yc2 - hlw * 4, hlw);
        }
      }

      if (font & (font_layout_upper_half | font_layout_lower_half))
        g.clip_clear();
    }

    template<typename Graphics>
    std::uint32_t draw_acs(Graphics& g, coord_t x, coord_t y, std::uint32_t code, curpos_t width, color_t color, font_t font) {
      // ACS の対象でない文字は普通に文字として描画
      bool const is_acs = (ascii_plus <= code && code <= ascii_dot) ||
        (ascii_back_quote <= code && code < ascii_b) ||
        (ascii_f <= code && code <= ascii_tilde) || code == ascii_0;
      if (!is_acs) return code;

      // ACS で別の Unicode 文字として描画する場合
      switch (code) {
      case ascii_plus: // ACS_RARROW (A)
      case ascii_comma: // ACS_LARROW (A)
      case ascii_minus: // ACS_UARROW (A)
      case ascii_dot: // ACS_DARROW (A)
      case ascii_back_quote: // ACS_DIAMOND (A)
      case ascii_f: // ACS_DEGREE (A)
      case ascii_g: // ACS_PLMINUS (A)
      case ascii_i: // ACS_LANTERN
      case ascii_y: // ACS_LEQUAL (A)
      case ascii_z: // ACS_GEQUAL (A)
      case ascii_left_brace: // ACS_PI (A)
      case ascii_vertical_bar: // ACS_NEQUAL (A)
      case ascii_tilde: // ACS_BULLET (A)
        acs_draw_glyph(g, x, y, code, width, color, font);
        return 0;
      // case ascii_back_quote:   return 0x2666; // ACS_DIAMOND ***
      // case ascii_a:            return 0x2592; // ACS_CKBOARD (A)
      // case ascii_f:            return 0x00B0; // ACS_DEGREE (A)
      // case ascii_g:            return 0x00B1; // ACS_PLMINUS (A)
      // case ascii_i:            return 0x240B; // ACS_LANTERN
      // case ascii_left_brace:   return 0x03C0; // ACS_PI (A)
      // case ascii_vertical_bar: return 0x2260; // ACS_NEQUAL (A)***
      case ascii_right_brace:  return 0x00A3; // ACS_STERLING
      // case ascii_tilde:        return 0x2022; // ACS_BULLET (A)
      // case ascii_plus:         return 0x2192; // ACS_RARROW (A)
      // case ascii_comma:        return 0x2190; // ACS_LARROW (A)
      // case ascii_minus:        return 0x2191; // ACS_UARROW (A)
      // case ascii_dot:          return 0x2193; // ACS_DARROW (A)
      // case ascii_y:            return 0x2264; // ACS_LEQUAL (A)
      // case ascii_z:            return 0x2265; // ACS_GEQUAL (A)
      }

      acs_draw_invariant(g, x, y, code, width, color, font);
      return 0;
    }
  public:
    template<typename Graphics>
    void draw_characters(Graphics& g, term_view_t const& view, std::vector<line_content>& content) {
      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const ypixel = wstat.m_ypixel;
      coord_t const xpixel = wstat.m_xpixel;
      curpos_t const height = view.height();
      tstate_t const& s = view.state();

      constexpr std::uint32_t flag_processed = charflag_private1;

      aflags_t invisible_flags = attribute_t::is_invisible_set;
      if (wstat.m_blinking_count & 1) invisible_flags |= attribute_t::is_rapid_blink_set;
      if (wstat.m_blinking_count & 2) invisible_flags |= attribute_t::is_blink_set;
      auto _visible = [invisible_flags] (std::uint32_t code, aflags_t aflags, bool sp_visible = false) {
        return code != ascii_nul && (sp_visible || code != ascii_sp)
          && !(code & flag_processed)
          && !(aflags & invisible_flags);
      };

      color_resolver_t _color(s);
      font_resolver_t _font;
      font_metric_t _fmetric(wstat.m_xpixel, wstat.m_ypixel);

      coord_t x = xorigin, y = yorigin, xL = xorigin, xR = xorigin;
      coord_t dx = 0, dy = 0;
      double dxW = 0.0;

      typename Graphics::character_buffer charbuff;
      auto _push_char = [&charbuff, &xR, &dx, &dxW] (std::uint32_t code, coord_t prog, curpos_t width) {
        xR += prog;

        // 文字位置の補正
        if (dxW && width > 1) {
          int const shift = dxW * (width - 1);
          if (charbuff.empty())
            dx += shift;
          else
            charbuff.shift(shift);
          prog -= shift;
        }

        charbuff.add_char(code, prog);
      };

      for (curpos_t iline = 0; iline < height; iline++, y += ypixel) {
        if (!content[iline].is_redraw_requested) continue;

        std::vector<cell_t>& cells = content[iline].cells;
        x = xorigin;
        charbuff.reserve(cells.size());

        for (std::size_t i = 0; i < cells.size(); ) {
          auto const& cell = cells[i];
          auto const& attr = cell.attribute;
          xL = xR = x;
          coord_t const cell_progress = cell.width * xpixel;
          i++;
          x += cell_progress;
          std::uint32_t code = cell.character.value;
          code &= ~charflag_cluster_extension;
          if (!_visible(code, cell.attribute.aflags)) continue;

          // 色の決定
          color_t const fg = _color.resolve_fg(attr);
          font_t const font = _font.resolve_font(attr);

          if (!character_t::is_char(code)) {
            if (code & charflag_iso2022) {
              std::uint32_t const charset = (code & ~charflag_iso2022) >> 7;
              switch (charset) {
              case iso2022_94_vt100_acs:
                code = draw_acs(g, xL, y, code & 0x7F, cell.width, fg, font);
                if (!code) continue;
              }
            } else
              continue;
          }

          std::tie(dx, dy, dxW) = _fmetric.get_displacement(font);
          charbuff.clear();
          _push_char(code, cell_progress, cell.width);

          // 同じ色を持つ文字は同時に描画してしまう。
          for (std::size_t j = i; j < cells.size(); j++) {
            auto const& cell2 = cells[j];
            std::uint32_t code2 = cell2.character.value;
            coord_t const cell2_progress = cell2.width * xpixel;

            // 回転文字の場合は一つずつ書かなければならない。
            // 零幅の cluster などだけ一緒に描画する。
            if ((font & font_rotation_mask) && cell2_progress) break;

            bool const is_cluster = code2 & charflag_cluster_extension;
            color_t const fg2 = _color.resolve_fg(cell2.attribute);
            font_t const font2 = _font.resolve_font(cell2.attribute);
            code2 &= ~charflag_cluster_extension;
            if (!_visible(code2, cell2.attribute.aflags, font & font_layout_proportional)) {
              if (font & font_layout_proportional) break;
              charbuff.shift(cell2_progress);
            } else if (!character_t::is_char(code2)) {
              charbuff.shift(cell2_progress);
              continue;
            } else if (is_cluster || (fg == fg2 && font == font2)) {
              _push_char(code2, cell2_progress, cell2.width);
            } else {
              if (font & font_layout_proportional) break;
              charbuff.shift(cell2_progress);
              continue;
            }

            if (i == j) {
              i++;
              x += cell2_progress;
            } else
              cells[j].character.value |= flag_processed;
          }

          // DECDHL用の制限
          if (font & font_layout_upper_half)
            this->clip_decdhl_upper(g, y);
          else if (font & font_layout_lower_half)
            this->clip_decdhl_lower(g, y);

          if (font & font_rotation_mask) {
            g.draw_rotated_characters(xL, y, dx, dy, xR - xL, charbuff, font, fg);
          } else if (font & font_layout_proportional) {
            // 配置を GDI に任せる
            g.draw_text(xL + dx, y + dy, charbuff, font, fg);
          } else {
            g.draw_characters(xL + dx, y + dy, charbuff, font, fg);
          }

          // DECDHL用の制限の解除
          if (font & (font_layout_upper_half | font_layout_lower_half))
            g.clip_clear();
        }

        // clear private flags
        for (cell_t& cell : cells) cell.character.value &= ~flag_processed;
      }
    }
    template<typename Graphics>
    void draw_characters_mono(Graphics& g, term_view_t const& view, std::vector<line_content> const& content) {
      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const ypixel = wstat.m_ypixel;
      coord_t const xpixel = wstat.m_xpixel;
      curpos_t const height = view.height();

      std::vector<cell_t> cells;
      typename Graphics::character_buffer charbuff;
      for (curpos_t iline = 0; iline < height; iline++) {
        if (!content[iline].is_redraw_requested) continue;
        std::vector<cell_t> const& cells = content[iline].cells;

        coord_t xoffset = xorigin;
        coord_t yoffset = yorigin + iline * ypixel;
        charbuff.clear();
        charbuff.reserve(cells.size());
        for (auto const& cell : cells) {
          std::uint32_t code = cell.character.value;
          if (code & charflag_cluster_extension)
            code &= ~charflag_cluster_extension;
          if (code == ascii_nul || code == ascii_sp) {
            if (charbuff.empty())
              xoffset += cell.width * xpixel;
            else
              charbuff.shift(cell.width * xpixel);
          } else if (code == (code & unicode_mask)) {
            charbuff.add_char(code, cell.width * xpixel);
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
          switch (xflags & attribute_t::decdhl_mask) {
          case attribute_t::decdhl_upper_half:
            new_style |= attribute_t::decdhl_upper_half;
            break;
          case attribute_t::decdhl_lower_half:
            new_style |= attribute_t::decdhl_lower_half;
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
    template<typename Graphics>
    void draw_decoration(Graphics& g, term_view_t const& view, std::vector<line_content>& content) {
      coord_t const xorigin = wstat.m_xframe;
      coord_t const yorigin = wstat.m_yframe;
      coord_t const ypixel = wstat.m_ypixel;
      coord_t const xpixel = wstat.m_xpixel;
      curpos_t const height = view.height();
      tstate_t const& s = view.state();
      color_resolver_t _color(s);

      aflags_t invisible_flags = attribute_t::is_invisible_set;
      if (wstat.m_blinking_count & 1) invisible_flags |= attribute_t::is_rapid_blink_set;
      if (wstat.m_blinking_count & 2) invisible_flags |= attribute_t::is_blink_set;

      coord_t x = xorigin, y = yorigin;

      color_t color0 = 0;

      decoration_horizontal_t dec_ul([&] (coord_t x1, coord_t x2, std::uint32_t style) {
        coord_t h = (coord_t) std::ceil(ypixel * 0.05);
        switch (style & attribute_t::decdhl_mask) {
        case attribute_t::decdhl_upper_half: return;
        case attribute_t::decdhl_lower_half:
          h *= 2;
          break;
        }

        g.fill_rectangle(x1, y + ypixel - h, x2, y + ypixel, color0);
        if ((style & ~attribute_t::decdhl_mask) == 2)
          g.fill_rectangle(x1, y + ypixel - 3 * h, x2, y + ypixel - 2 * h, color0);
      });

      decoration_horizontal_t dec_sl([&] (coord_t x1, coord_t x2, std::uint32_t style) {
        coord_t h = (coord_t) std::ceil(ypixel * 0.05), y2 = y + ypixel / 2;
        if ((style & ~attribute_t::decdhl_mask) != 2) {
          // 一重打ち消し線
          switch (style & attribute_t::decdhl_mask) {
          case attribute_t::decdhl_lower_half: return;
          case attribute_t::decdhl_upper_half:
            h *= 2; y2 = y + ypixel;
            break;
          }
        } else {
          // 二重打ち消し線
          switch (style & attribute_t::decdhl_mask) {
          case attribute_t::decdhl_lower_half:
            h *= 2; y2 = y + h; break;
          case attribute_t::decdhl_upper_half:
            h *= 2; y2 = y + ypixel - h; break;
          default:
            g.fill_rectangle(x1, y2 - 2 * h, x2, y2 - h, color0);
            y2 += h;
            break;
          }
        }
        g.fill_rectangle(x1, y2 - h, x2, y2, color0);
      });

      decoration_horizontal_t dec_ol([&] (coord_t x1, coord_t x2, std::uint32_t style) {
        coord_t h = (coord_t) std::ceil(ypixel * 0.05);
        switch (style & attribute_t::decdhl_mask) {
        case attribute_t::decdhl_lower_half: return;
        case attribute_t::decdhl_upper_half:
          h *= 2;
          return;
        }
        g.fill_rectangle(x1, y, x2, y + h, color0);
        if ((style & ~attribute_t::decdhl_mask) == 2)
          g.fill_rectangle(x1, y + 2 * h, x2, y + 3 * h, color0);
      });

      auto _draw_frame = [&] (coord_t x1, coord_t x2, xflags_t xflags, int lline, int rline) {
        // 上の線と下の線は dec_ul, dec_ol に任せる。
        coord_t w = (coord_t) std::ceil(ypixel * 0.05);
        if (xflags & attribute_t::decdhl_mask) w *= 2;
        if (lline) {
          g.fill_rectangle(x1, y, x1 + w, y + ypixel, color0);
          if (lline > 1)
            g.fill_rectangle(x1 + 2 * w, y, x1 + 3 * w, y + ypixel, color0);
        }
        if (rline) {
          g.fill_rectangle(x2 - w, y, x2, y + ypixel, color0);
          if (rline > 1)
            g.fill_rectangle(x2 - 3 * w, y, x2 - 2 * w, y + ypixel, color0);
        }
      };

      auto _draw_circle = [&] (coord_t x1, coord_t x2, xflags_t xflags) {
        coord_t w = (coord_t) std::ceil(ypixel * 0.04);
        coord_t y1 = y, y2 = y + ypixel;
        bool is_clipped = false;
        switch (xflags & attribute_t::decdhl_mask) {
        case attribute_t::decdhl_upper_half:
          is_clipped = true;
          this->clip_decdhl_upper(g, y);
          w *= 2;
          y2 = y + 2 * ypixel;
          break;
        case attribute_t::decdhl_lower_half:
          is_clipped = true;
          this->clip_decdhl_lower(g, y);
          w *= 2;
          y1 = y - ypixel;
          y2 = y + ypixel;
          break;
        }
        g.draw_ellipse(x1, y1, x2, y2, color0, w);
        if (is_clipped)
          g.clip_clear();
      };

      auto _draw_stress = [&] (coord_t x1, coord_t x2, xflags_t xflags) {
        double w1 = ypixel * 0.15, h1 = ypixel * 0.15;
        switch (xflags & attribute_t::decdhl_mask) {
        case attribute_t::decdhl_lower_half: return;
        case attribute_t::decdhl_upper_half:
          w1 *= 2.0;
          h1 *= 2.0;
          break;
        case attribute_t::decdhl_double_width:
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

      for (curpos_t iline = 0; iline < height; iline++, y += ypixel) {
        if (!content[iline].is_redraw_requested) continue;

        std::vector<cell_t>& cells = content[iline].cells;
        x = xorigin;
        color0 = 0;

        for (std::size_t i = 0; i < cells.size(); ) {
          auto const& cell = cells[i++];
          auto const& code = cell.character.value;
          auto const& aflags = cell.attribute.aflags;
          auto const& xflags = cell.attribute.xflags;
          if (cell.width == 0) continue;
          coord_t const cell_width = cell.width * xpixel;
          color_t color = 0;
          if (code != ascii_nul && !(aflags & invisible_flags))
            color = _color.resolve_fg(cell.attribute);
          if (color != color0) {
            dec_ul.update(x, 0, (xflags_t) 0);
            dec_sl.update(x, 0, (xflags_t) 0);
            dec_ol.update(x, 0, (xflags_t) 0);
          }

          if (color) {
            xflags_t const ideo = xflags & attribute_t::is_ideogram_mask & ~attribute_t::is_ideogram_line_left;

            // 下線
            bool const ul2 = aflags & attribute_t::is_double_underline_set ||
              ideo == attribute_t::is_ideogram_line_double_rb;
            bool const ul1 = aflags & attribute_t::is_underline_set ||
              xflags & attribute_t::is_frame_set ||
              ideo == attribute_t::is_ideogram_line_single_rb;
            dec_ul.update(x, ul2 ? 2 : ul1 ? 1 : 0, xflags);

            // 上線
            bool const ol2 = ideo == attribute_t::is_ideogram_line_double_rt;
            bool const ol1 = xflags & (attribute_t::is_overline_set | attribute_t::is_frame_set) ||
              ideo == attribute_t::is_ideogram_line_single_rt;
            dec_ol.update(x, ol2 ? 2 : ol1 ? 1 : 0, xflags);

            // 打ち消し線
            bool const sl2 = xflags & attribute_t::rlogin_double_strike;
            bool const sl1 = aflags & attribute_t::is_strike_set;
            dec_sl.update(x, sl2 ? 2 : sl1 ? 1 : 0, xflags);

            // 左線・右線
            bool const ll2 = xflags & attribute_t::rlogin_double_lline;
            bool const ll1 = xflags & (attribute_t::is_frame_set | attribute_t::rlogin_single_lline);
            int const ll = ll2 ? 2 : ll1 ? 1 : 0;
            bool const rl2 = xflags & attribute_t::rlogin_double_rline;
            bool const rl1 = xflags & (attribute_t::is_frame_set | attribute_t::rlogin_single_rline);
            int const rl = rl2 ? 2 : rl1 ? 1 : 0;
            _draw_frame(x, x + cell_width, xflags, ll, rl);

            // 丸
            if (xflags & attribute_t::is_circle_set)
              _draw_circle(x, x + cell_width, xflags);

            // 圏点
            if (ideo == attribute_t::is_ideogram_stress)
              _draw_stress(x, x + cell_width, xflags);
          }

          color0 = color;
          x += cell_width;
        }
        dec_ul.update(x, 0, (xflags_t) 0);
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
      coord_t const ypixel = wstat.m_ypixel;

      curpos_t y1dst = -1, y1src = -1;
      curpos_t y2dst = 0;
      auto const _transfer = [&, yorigin, ypixel, width = wstat.m_canvas_width] () {
        if (y1dst >= 0) {
          coord_t const y1dst_pixel = yorigin + y1dst * ypixel;
          coord_t const y1src_pixel = yorigin + y1src * ypixel;
          coord_t const height = (y2dst - y1dst) * ypixel;
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
        coord_t const old_x1 = wstat.m_xframe + wstat.m_xpixel * m_tracer.cur_x();
        coord_t const old_y1 = wstat.m_yframe + wstat.m_ypixel * m_tracer.cur_y();
        gbuffer.bitblt(ctx0, old_x1, old_y1, wstat.m_xpixel, wstat.m_ypixel, ctx1, old_x1, old_y1);
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
