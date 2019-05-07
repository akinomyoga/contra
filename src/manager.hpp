// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_manager_hpp
#define contra_manager_hpp
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <iterator>
#include <memory>
#include <chrono>
#include "ansi/line.hpp"
#include "ansi/term.hpp"
#include "enc.utf8.hpp"

namespace contra {
namespace term {

  using curpos_t = contra::ansi::curpos_t;
  using coord_t = contra::ansi::coord_t;
  using board_t = contra::ansi::board_t;
  using tstate_t = contra::ansi::tstate_t;
  using term_t = contra::ansi::term_t;

  class terminal_application {
    curpos_t m_width = 80, m_height = 24;
    coord_t m_xpixel = 13, m_ypixel = 7;
  public:
    curpos_t width() const { return m_width; }
    curpos_t height() const { return m_height; }
    curpos_t xpixel() const { return m_xpixel; }
    curpos_t ypixel() const { return m_ypixel; }

    virtual void reset_size(curpos_t width, curpos_t height) {
      m_width = std::clamp(width, limit::minimal_terminal_col, limit::maximal_terminal_col);
      m_height = std::clamp(height, limit::minimal_terminal_row, limit::maximal_terminal_row);
      if (m_term) m_term->board().reset_size(m_width, m_height);
    }
    virtual void reset_size(curpos_t width, curpos_t height, coord_t xpixel, coord_t ypixel) {
      this->reset_size(width, height);
      m_xpixel = std::clamp(xpixel, limit::minimal_terminal_xpixel, limit::maximal_terminal_xpixel);
      m_ypixel = std::clamp(ypixel, limit::minimal_terminal_ypixel, limit::maximal_terminal_ypixel);
    }

  private:
    std::unique_ptr<contra::ansi::term_t> m_term;
  public:
    term_t& term() { return *m_term; }
    term_t const& term() const { return *m_term; }
    board_t& board() { return m_term->board(); }
    board_t const& board() const { return m_term->board(); }
    tstate_t& state() { return m_term->state(); }
    tstate_t const& state() const { return m_term->state(); }

  public:
    terminal_application() {
      reset_size(80, 24, 13, 7);
    }
    virtual ~terminal_application() {}

    bool initialize() {
      if (m_term) return true;
      m_term = std::make_unique<contra::ansi::term_t>(m_width, m_height);
      return true;
    }

  public:
    virtual bool process() { return false; }
    virtual bool is_active() const { return true; }
    virtual bool is_alive() { return true; }
    virtual void terminate() {}

  public:
    virtual bool input_key(key_t key) {
      return m_term->input_key(key);
    }
    virtual bool input_mouse(key_t key, coord_t px, coord_t py, curpos_t x, curpos_t y) {
      return m_term->input_mouse(key, px, py, x, y);
    }
  };

  class terminal_manager {
    std::vector<std::shared_ptr<terminal_application> > m_apps;

  public:
    terminal_manager() {}

  public:
    terminal_application& app() const {
      mwg_check(m_apps.size());
      return *m_apps[0].get();
    }
    template<typename T>
    void add_app(T&& app) { m_apps.emplace_back(std::forward<T>(app)); }

  private:
    // ToDo: foreground/background で優先順位をつけたい。
    // 後、現在 process1 を呼び出す度に全ての app に対して処理を行っているが、
    // 一度 processed = false になった物は何度も試さない様にする。
    bool process1() {
      bool processed = false;
      for (auto const& app : m_apps)
        if (app->process())
          processed = true;
      return processed;
    }
  public:
    bool do_events() {
      bool processed = false;
      auto const time0 = std::chrono::high_resolution_clock::now();
      while (this->process1()) {
        processed = true;
        auto const time1 = std::chrono::high_resolution_clock::now();
        auto const msec = std::chrono::duration_cast<std::chrono::milliseconds>(time1 - time0);
        if (msec.count() > 20) break;
      }
      if (processed) m_dirty = true;
      return processed;
    }

    bool is_active() {
      for (auto const& app : m_apps)
        if (app->is_active()) return true;
      return false;
    }
    bool is_alive() {
      // ToDo: 死んだ物に関しては閉じる?
      for (auto const& app : m_apps)
        if (app->is_alive()) return true;
      return false;
    }
    void terminate() {
      for (auto const& app : m_apps)
        app->terminate();
    }

  public:
    bool input_key(key_t key) {
      return app().input_key(key);
    }

  private:
    curpos_t      m_sel_beg_x = 0;
    curpos_t      m_sel_beg_y = 0;
    bool          m_sel_beg_online = false;
    std::uint32_t m_sel_beg_lineid = 0;

    int      m_sel_type = 0;
    curpos_t m_sel_end_x = 0;
    curpos_t m_sel_end_y = 0;

  private:
    void selection_clear() {
      m_sel_type = 0;
      for (auto& line : app().board().m_lines)
        m_dirty |= line.clear_selection();
    }

    void selection_start(curpos_t x, curpos_t y) {
      selection_clear();
      m_sel_type = 0;
      m_sel_beg_x = x;
      m_sel_beg_y = y;
      m_sel_beg_online = y < (curpos_t) app().board().m_lines.size();
      if (m_sel_beg_online)
        m_sel_beg_lineid = app().board().m_lines[y].id();
    }

    bool selection_find_start_line() {
      using namespace contra::ansi;

      auto const& lines = app().board().m_lines;
      if (!m_sel_beg_online)
        return m_sel_beg_y >= (curpos_t) lines.size();

      if (m_sel_beg_y < (curpos_t) lines.size() &&
        lines[m_sel_beg_y].id() == m_sel_beg_lineid)
        return true;

      for (std::size_t i = 0; i < lines.size(); i++) {
        if (lines[i].id() == m_sel_beg_lineid) {
          m_sel_beg_y = i;
          return true;
        }
      }

      return false;
    }

    bool selection_update(key_t key, curpos_t x, curpos_t y) {
      using namespace contra::ansi;

      m_sel_type = 1 | (key & _modifier_mask);
      m_sel_end_x = x;
      m_sel_end_y = y;

      board_t& b = app().board();
      tstate_t& s = app().state();

      bool const gatm =  s.get_mode(mode_gatm);
      bool const truncate = !(m_sel_type & modifier_shift);
      if (!selection_find_start_line()) {
        selection_clear();
        return false;
      }

      // 選択範囲 (データ部での範囲に変換)
      curpos_t x1 = m_sel_beg_x;
      curpos_t y1 = m_sel_beg_y;
      curpos_t x2 = m_sel_end_x;
      curpos_t y2 = m_sel_end_y;
      curpos_t const iN = b.m_lines.size();
      if (y1 < iN) x1 = b.to_data_position(y1, x1);
      if (y2 < iN) x2 = b.to_data_position(y2, x2);

      if (m_sel_type & modifier_meta) {
        // 矩形選択
        if (y1 > y2) std::swap(y1, y2);
        if (x1 > x2) std::swap(x1, x2);
        if (y1 >= iN) {
          y1 = 0; y2 = -1;
        } else if (y2 >= iN) {
          y2 = iN - 1;
        }

        curpos_t i = 0;
        while (i < y1)
          m_dirty |= b.m_lines[i++].clear_selection();
        while (i <= y2)
          m_dirty |= b.m_lines[i++].set_selection(x1, x2 + 1, truncate, gatm, true);
        while (i < iN)
          m_dirty |= b.m_lines[i++].clear_selection();
      } else {
        if (y1 > y2) {
          std::swap(y1, y2);
          std::swap(x1, x2);
        } else if (y1 == y2 && x1 > x2) {
          std::swap(x1, x2);
        }
        if (y1 >= iN) {
          y1 = 0; y2 = -1;
        } else if (y2 >= iN) {
          y2 = iN - 1;
          x2 = b.m_width + 1;
        }

        // 選択状態の更新 (前回と同じ場合は skip できたりしないか?)
        curpos_t i = 0;
        while (i < y1)
          m_dirty |= b.m_lines[i++].clear_selection();
        if (y1 == y2) {
          m_dirty |= b.m_lines[i++].set_selection(x1, x2 + 1, truncate, gatm, true);
        } else if (y1 < y2) {
          m_dirty |= b.m_lines[i++].set_selection(x1, b.m_width, truncate, gatm, true);
          while (i < y2)
            m_dirty |= b.m_lines[i++].set_selection(0, b.m_width + 1, truncate, gatm, true);
          m_dirty |= b.m_lines[i++].set_selection(0, x2 + 1, truncate, gatm, true);
        }
        while (i < iN)
          m_dirty |= b.m_lines[i++].clear_selection();
      }

      return true;
    }

  public:
    bool m_dirty = false;
  private:
    int m_drag_state = 0;
  public:
    bool input_mouse(key_t key, [[maybe_unused]] coord_t px, [[maybe_unused]] coord_t py, curpos_t x, curpos_t y) {
      if (app().input_mouse(key, px, py, x, y)) return true;

      using namespace contra::ansi;
      switch (key & _character_mask) {
      case key_mouse1_down:
        m_drag_state = 1;
        selection_start(x, y);
        return true;
      case key_mouse1_drag:
        if (m_drag_state) {
          m_drag_state = 2;
          if (!selection_update(key, x, y))
            m_drag_state = 0;
        }
        return true;
      case key_mouse1_up:
        if (m_drag_state == 2) {
          m_drag_state = 3;
          if (!selection_update(key, x, y))
            m_drag_state = 0;
        }
        return true;
      }

      return false;
    }
    void reset_size(std::size_t width, std::size_t height) {
      app().reset_size(width, height);
    }

  };
}
}
#endif
