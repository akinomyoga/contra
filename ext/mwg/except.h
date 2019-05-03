#define STANDALONE_MWG_EXCEPT_H
// -*- mode: c++; coding: utf-8 -*-
//
// ----- Usage of mwg/except.h as a standalone header file --------------------
//
// If you are using this file without entire libmwg,
// please define STANDALONE_MWG_EXCEPT_H before includes.
// For example, with this file placed into `mwg' sub directory,
// you can include it as follows:
//
// #define STANDALONE_MWG_EXCEPT_H
// #include "mwg/except.h"
//
// ---------------------------------------------------------------------------
#ifndef MWG_EXCEPT_H
#define MWG_EXCEPT_H
#include <cstdio>
#include <cerrno>
#include <string>
#include <typeinfo>
#include <exception>
#include <stdexcept>

#ifdef STANDALONE_MWG_EXCEPT_H
# ifndef MWG_ATTRIBUTE_UNUSED
#  ifdef __GNUC__
#   define MWG_ATTRIBUTE_UNUSED __attribute__((unused))
#  else
#   define MWG_ATTRIBUTE_UNUSED
#  endif
# endif
# ifndef MWG_STD_VA_ARGS
#  if defined(_MSC_VER)? (_MSC_VER >= 1400): (defined(__GNUC__)? (__GNUC__ >= 3): 1)
#   define MWG_STD_VA_ARGS
#  endif
# endif
#else
# include <mwg_config.h>
# include <mwg/defs.h>
#endif

#ifndef MWG_ATTRIBUTE_NORETURN
//?mconf S -t'[[noreturn]]' -o'MWGCONF_STD_ATTRIBUTE_NORETURN' '' '[[noreturn]] void func(){throw 1;}'
# ifdef MWGCONF_STD_ATTRIBUTE_NORETURN
#  define MWG_ATTRIBUTE_NORETURN [[noreturn]]
# elif defined(_MSC_VER)
#  define MWG_ATTRIBUTE_NORETURN __declspec(noreturn)
# elif defined(__GNUC__)
#  define MWG_ATTRIBUTE_NORETURN __attribute__((unused))
# else
#  define MWG_ATTRIBUTE_NORETURN
# endif
#endif
#ifndef mwg_noinline
# ifdef _MSC_VER
#  define mwg_noinline __declspec(noinline)
# else
#  define mwg_noinline
# endif
#endif

namespace mwg {
  // エラーコード
  typedef unsigned ecode_t;

  static const ecode_t ecode_error = 0;
  //static const ecode_t ecode_base = 0x00100000;

  struct ecode {
    static const ecode_t error = 0;

    static const ecode_t CATEGORY_MASK = 0xFFFF0000;

    //--------------------------------------------------------------------------
    //  Flags
    //--------------------------------------------------------------------------
    static const ecode_t FLAG_MASK = 0x0FF00000;
    // bit 27
    static const ecode_t EAssert  = 0x08000000; // assert 失敗。プログラム自体が誤っている

    // bit 24-26 (種別)
    static const ecode_t ESupport = 0x01000000; // サポートしていない操作・値
    static const ecode_t ETrial   = 0x02000000; // 試行失敗 (通信など潜在的に失敗の可能性を含む物)
    static const ecode_t EAccess  = 0x03000000; // アクセス権限
    static const ecode_t EAbort   = 0x04000000; // 操作中止の為の人為的例外

    // bit 20-21 (主格)
    static const ecode_t EArgument  = 0x00100000; // (明らかな)引数指定の誤りに起因
    static const ecode_t EData      = 0x00200000; // 自身・引数の内部状態に起因
    static const ecode_t EOperation = 0x00300000; // 操作の実装

    // bit 22-23 (原因)
    static const ecode_t ENull      = 0x00400000; // 値が NULL 又は未初期化である事による物
    static const ecode_t ERange     = 0x00800000; // 値の範囲がおかしい物
    static const ecode_t EClose     = 0x00C00000; // 既に始末処理が行われている物

    // 複合
    static const ecode_t EArgRange = EArgument | ERange;
    static const ecode_t EArgNull  = EArgument | ENull;
    static const ecode_t EDatRange = EData | ERange;
    static const ecode_t EDatNull  = EData | ENull;
    static const ecode_t EImpl     = EOperation | ENull;    // 未実装
    static const ecode_t EInvalid  = EOperation | ESupport; // 禁止操作・無効操作
    //--------------------------------------------------------------------------

    static const ecode_t io = 0x00020000;
  };

  //****************************************************************************
  //    class except
  //============================================================================
  class except;
  class assertion_error;
  class invalid_operation;
  // std::bad_alloc
  // std::bad_cast
  // std::bad_typeid
  // std::bad_exception
  // std::ios_base::failure
  //
  // 恐らく投げるべきはこれ。
  // std::logic_error
  //   std::out_of_range
  //   std::invalid_argument
  //   std::length_error
  //   std::domain_error
  //
  // 以下は処理系に依存して発生する例外。
  // 特に浮動小数点の取り扱い。
  // std::runtime_error
  //   std::range_error
  //   std::overflow_error
  //   std::underflow_error

  class except: public std::exception {
  protected:
    std::string msg;
    ecode_t ecode;
    mwg::except* original;
  //--------------------------------------------------------------------------
  //  初期化
  //--------------------------------------------------------------------------
  public:
    explicit except(const std::string& message, ecode_t ecode, const std::exception& orig):
      msg(message), ecode(ecode), original(CopyException(&orig)) {}
    explicit except(const std::string& message, ecode_t ecode = mwg::ecode_error):
      msg(message), ecode(ecode), original(NULL) {}
    except():
      msg("mwg::except"), ecode(0), original(NULL) {}
    virtual ~except() throw() {
      this->internal_free();
    }
  private:
    void internal_free() {
      if (this->original != NULL) {
        // virtual ~except() {} なので、全て delete で処分しておけば OK。
        delete this->original;
        this->original = NULL;
      }
    }
  public:
    except& operator=(const except& cpy) {
      if (&cpy == this) return *this;
      this->internal_free();
      this->msg = cpy.msg;
      this->ecode = cpy.ecode;
      this->original = CopyException(cpy.original);

      return *this;
    }
    except(const except& cpy):
      std::exception(cpy),
      msg(cpy.msg),
      ecode(cpy.ecode),
      original(CopyException(cpy.original))
    {}
  //--------------------------------------------------------------------------
  //  original exception の為のメモリ管理
  //--------------------------------------------------------------------------
  protected:
    virtual except* copy() const {return new except(*this);}

  private:
    static except* CopyException(const except* e) {
      if (e == NULL) return NULL;
      return e->copy();
    }
    static except* CopyException(const std::exception* e) {
      if (e == NULL) return NULL;

      const except* err = dynamic_cast<const except*>(e);
      if (err) return CopyException(err);

      return new except(
        std::string("type<") + typeid(*e).name() + ">: " + e->what(),
        (ecode_t) 0);
    }
  //--------------------------------------------------------------------------
  //  機能
  //--------------------------------------------------------------------------
  public:
    virtual const char* what() const throw() {return this->msg.c_str();}
    ecode_t code() const {return this->ecode;}
  };

#define mwg_define_class_error(errorName) mwg_define_class_error_ex(errorName, mwg::except, mwg::ecode_error)
#define mwg_define_class_error_ex(errorName, BASE, ECODE)                                      \
  class errorName: public BASE {                                                               \
  public:                                                                                      \
    explicit errorName(const std::string& message, ecode_t ecode, const std::exception& orig): \
      BASE(message, ecode, orig)                                                               \
    {ecode |= (ECODE & ecode::CATEGORY_MASK);}                                                 \
    explicit errorName(const std::string& message, ecode_t ecode = ECODE):                     \
      BASE(message, ecode)                                                                     \
    {ecode |= (ECODE & ecode::CATEGORY_MASK);}                                                 \
    errorName(): BASE(#errorName, ECODE) {}                                                    \
    errorName(const errorName& err): BASE(err) {}                                              \
  public:                                                                                      \
    virtual errorName* copy() const {return new errorName(*this);}                             \
  }                                                                                         /**/

//mwg_define_class_error_ex(argument_error, except, ecode::EArgument);
//→ std::invalid_argument
//*****************************************************************************
//    ASSERTION
//=============================================================================
mwg_define_class_error_ex(assertion_error  , except, ecode::EAssert);
mwg_define_class_error_ex(invalid_operation, except, ecode::EInvalid);
}
/**----------------------------------------------------------------------------
mwg_check/mwg_assert

概要
  プログラムの実行中に特定の条件が満たされている事を確認します。

  マクロ NDEBUG 及び MWG_DEBUG の値によって、デバグ用と配布用で動作が変わります。
  マクロ NDEBUG が定義されている場合は条件式のチェックとエラーメッセージの出力が抑制されます。
  NDEBUG は、通常コンパイラに最適化オプションを指定すると自動で定義されます。
  明示的に -DNDEBUG オプションを指定して NDEBUG を定義する事も可能です。
  一方で、重い研鑽を実行しながら開発・デバグをする場合、最適化をしつつ条件式のチェックも実行したい事があるかも知れません。
  その場合にはコンパイラに -DMWB_DEBUG オプションを指定する事によって、条件式のチェックとエラーメッセージの出力を強制する事ができます。
  即ち、<code>defined(NDEBUG)&&!defined(MWG_DEBUG)</code> の条件で、チェック・メッセージ出力が抑制されます。

関数一覧
  @fn int r=mwg_check_nothrow(条件式,書式,引数...);
    条件式が偽の場合に、その事を表すエラーメッセージを出力します。
  @fn mwg_check(条件式,書式,引数...);
    条件式が偽の場合に、エラーメッセージを出力して例外を発生させます。
    <code>defined(NDEBUG)&&!defined(MWG_DEBUG)</code> の場合には、単に例外を発生させます。
  @fn int r=mwg_assert_nothrow(条件式,書式,引数...);
    条件式が偽の場合に、その事を表すエラーメッセージを出力します。
    <code>defined(NDEBUG)&&!defined(MWG_DEBUG)</code> の場合には、条件式の評価も含めて何も実行しません。
  @fn mwg_assert(条件式,書式,引数...);
    条件式が偽の場合に、エラーメッセージを出力して例外を発生させます。
    <code>defined(NDEBUG)&&!defined(MWG_DEBUG)</code> の場合には、条件式の評価も含めて何も実行しません。
  @fn int r=mwg_verify_nothrow(条件式,書式,引数...);
    条件式が偽の場合に、その事を表すエラーメッセージを出力します。
    <code>defined(NDEBUG)&&!defined(MWG_DEBUG)</code> の場合には、条件式の評価のみを実行し結果を確認しません。
  @fn mwg_verify(条件式,書式,引数...);
    条件式が偽の場合に、エラーメッセージを出力して例外を発生させます。
    <code>defined(NDEBUG)&&!defined(MWG_DEBUG)</code> の場合には、条件式の評価のみを実行し結果を確認しません。
  @def MWG_NOTREACHED;
    制御がそこに到達しない事を表します。

  マクロ              通常              NDEBUG
  ~~~~~~~~~~~~~~~~~~  ~~~~~~~~~~~~~~~~  ~~~~~~~~~~~~~~~~~
  mwg_check           評価・出力・例外  評価・例外
  mwg_check_nothrow   評価・出力        評価・出力
  mwg_verify          評価・出力・例外  評価
  mwg_verify_nothrow  評価・出力        評価
  mwg_assert          評価・出力・例外  -
  mwg_assert_nothrow  評価・出力        -

引数と戻り値
  @param 条件式 : bool
    判定対象の条件式を指定します。評価した結果として真になる事が期待される物です。
  @param 書式 : const char*
    エラーが発生した時のメッセージを決める書式指定文字列です。printf の第一引数と同様の物です。
  @param 引数...
    書式に与える引数です。
  @return : int
    今迄に失敗した条件式の回数が返ります。

-----------------------------------------------------------------------------*/
//!
//! @def mwg_assert_funcname
//!
#ifdef _MSC_VER
# define mwg_assert_funcname __FUNCSIG__
#elif defined(__INTEL_COMPILER)
// # define mwg_assert_funcname __FUNCTION__
# define mwg_assert_funcname __PRETTY_FUNCTION__
#elif defined(__GNUC__)
# define mwg_assert_funcname __PRETTY_FUNCTION__
#else
# define mwg_assert_funcname 0
#endif
//!
//! @def mwg_assert_position
//!
#ifdef MWG_STD_VA_ARGS
# define mwg_assert_stringize2(...) #__VA_ARGS__
# define mwg_assert_stringize(...) mwg_assert_stringize2(__VA_ARGS__)
#else
# define mwg_assert_stringize2(ARGS) #ARGS
# define mwg_assert_stringize(ARGS) mwg_assert_stringize2(ARGS)
#endif
# define mwg_assert_position __FILE__ mwg_assert_stringize(:__LINE__)

#include <cstdarg>
#include <string>

#if defined(__unix__)
# include <unistd.h>
#elif defined(_WIN32)
# include <cstdlib>
#endif

/* 実装メモ: GCC で unused-function の警告が出ることについて
 *
 * 1 一番初めに思いつきそうな以下の方法は使えない
 *
 *   #pragma GCC diagnostic push
 *   #pragma GCC diagnostic ignored "-Wunused"
 *   #pragma GCC diagnostic ignored "-Wunused-function"
 *   #pragma GCC diagnostic ignored "-Wunused-variable"
 *   ...
 *   #pragma GCC diagnostic pop
 *
 *   というのも、unused が判明するのはファイルを末尾まで読んだ後であり、
 *   その時に -Wunused etc が ignored になっていないと、結局警告が出るからである。
 *   (勿論 ignored のままにしておけば警告は出なくなるが、
 *   本当にミスで unused になっている関数について警告が出なくなってしまうのでしたくない)。
 *
 * 2 GCC の __attribute__((unused)) をつける方法?
 *
 *   | 「文は効果がありません」の警告が出る?
 *   | どうも __attribute__((unused)) をつけた関数の呼び出しは意味のない文として扱われるようである。
 *   | →どうやら勘違いであった
 *
 * 3 結局ダミー変数の初期化に使う方法?
 *
 *   参考: http://stackoverflow.com/questions/11124895/suppress-compiler-warning-function-declared-never-referenced
 *
 *   #if defined(__GNUC__)&&!defined(__INTEL_COMPILER)
 *   # ifndef MWG_SUPPRESS_WUNUSED
 *   #  define MWG_SUPPRESS_WUNUSED(var) static MWG_ATTRIBUTE_UNUSED int _dummy_tmp_##var=((int)(var)&0)
 *   # endif
 *     MWG_SUPPRESS_WUNUSED(mwg_printd_);
 *     MWG_SUPPRESS_WUNUSED(print_onfail);
 *     MWG_SUPPRESS_WUNUSED(throw_onfail);
 *   #endif
 *
 *   → icc が警告を出す: 関数ポインタを int に変換する時に符号ビットが消える, 云々.
 *
 * 4 そもそも static ではなく inline で宣言しておけば問題ない?
 *
 *   ■これは後で考える。(そもそもなぜ static にしたのであったか??)
 */

namespace mwg {
namespace except_detail {

  struct sgr {
    int value;
    sgr(int value): value(value) {}
  };

  class dbgput {
    FILE* file;
    bool m_isatty;
  public:
#if defined(__unix__)
    dbgput(FILE* file): file(file), m_isatty(isatty(fileno(file))) {}
#elif defined(_WIN32)
    dbgput(FILE* file): file(file), m_isatty(false) {
      const char* envterm = std::getenv("TERM");
      if (envterm && *envterm)
        this->m_isatty = true;
    }
#else
    dbgput(FILE* file): file(file), m_isatty(false) {}
#endif
    dbgput& operator<<(const char* str) {
      while (*str) std::putc(*str++, file);
      return *this;
    }
    dbgput& operator<<(char c) {
      std::putc(c, file);
      return *this;
    }
  private:
    void putsgr(int num) {
      if (!m_isatty) return;

      std::putc('\x1b', file);
      std::putc('[', file);
      if (num > 0) {
        int x = 1;
        while (x * 10 <= num) x *= 10;
        for (; x >= 1; x /= 10) std::putc('0' + num / x % 10, file);
      }
      std::putc('m', file);
    }
  public:
    dbgput& operator<<(sgr const& sgrnum) {
      putsgr(sgrnum.value);
      return *this;
    }
  };

  MWG_ATTRIBUTE_UNUSED
  static void mwg_printl_(const char* fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    std::vfprintf(stderr, fmt, arg);
    va_end(arg);
    std::putc('\n', stderr);
    std::fflush(stderr);
  }

  static void mwg_vprintd_(const char* pos, const char* func, const char* fmt, va_list arg) {
    using namespace ::mwg::except_detail;
    dbgput d(stderr);

    std::fprintf(stderr, "%s:", pos);
    if (fmt && *fmt) {
      d << ' ' << sgr(94);//sgr(35);
      std::vfprintf(stderr, fmt, arg);
      d << sgr(0);
      if (func)
        d << " @ " << sgr(32) << '"' << func << '"' << sgr(0);
      d << '\n';
    } else {
      if (func) {
        d << " mwg_printd @ " << sgr(32) << '"' << func << '"' << sgr(0) << '\n';
      } else
        d << " mwg_printd\n";
    }
    std::fflush(stderr);
  }

  MWG_ATTRIBUTE_UNUSED
  static void mwg_printd_(const char* pos, const char* func, const char* fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    mwg_vprintd_(pos, func, fmt, arg);
    va_end(arg);
  }

  static mwg_noinline int increment_fail_count(int delta = 1) {
    static int fail_nothrow_count = 0;
    return fail_nothrow_count+=delta; // dummy operation to set break point
  }
  static bool vprint_onfail_impl(const char* expr, const char* pos, const char* func, const char* fmt, va_list arg) {
    using namespace ::mwg::except_detail;
    dbgput d(stderr);
    // first line
    d << sgr(1) << "mwg_assertion_failure!" << sgr(0) << ' ' << sgr(91) << expr << sgr(0);
    if (fmt && *fmt) {
      d << ", \"" << sgr(31);
      std::vfprintf(stderr, fmt, arg);
      d << "\"" << sgr(0);
    }
    d << '\n';

    // second line
    d << "  @ " << sgr(4) << pos << sgr(0);
    if (func)
      d << ':' << func << '\n';

    std::fflush(stderr);

    return false;
  }

  MWG_ATTRIBUTE_NORETURN
  static bool vthrow_onfail_impl(const char* expr, const char* pos, const char* /* func */, const char* fmt, va_list arg) {
    std::string buff("assertion failed! ");
    if (fmt && *fmt) {
      char message[1024];
#ifdef MWGCONF_HAS_VSNPRINTF
      // C99 vsnprintf
      /*?mconf
       * # X snprintf    cstdio           'char b[9];::snprintf(b, 9, "");'
       * X vsnprintf -h'cstdio' -h'cstdarg' 'char b[9];va_list a;::vsnprintf(b, 9, "", a);'
       */
      ::vsnprintf(message, sizeof message, fmt, arg);
#else
      std::vsprintf(message, fmt, arg);
#endif
      buff += message;
      buff += " ";
    }
    buff += "[expr = ";
    buff += expr;
    buff += " @ ";
    buff += pos;
    buff += " ]";
    throw mwg::assertion_error(buff);
  }

  MWG_ATTRIBUTE_UNUSED
  static bool mwg_noinline vprint_onfail(const char* expr, const char* pos, const char* func, const char* fmt, va_list arg) {
    increment_fail_count();
    vprint_onfail_impl(expr, pos, func, fmt, arg);
    return false;
  }
  MWG_ATTRIBUTE_UNUSED
  static bool mwg_noinline print_onfail(const char* expr, const char* pos, const char* func, const char* fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    mwg::except_detail::vprint_onfail(expr, pos, func, fmt, arg);
    va_end(arg);
    return false;
  }

  // [[noreturn]] は __attribute__((unused)) より前になければならない
  MWG_ATTRIBUTE_NORETURN
  MWG_ATTRIBUTE_UNUSED
  static bool mwg_noinline vthrow_onfail(const char* expr, const char* pos, const char* func, const char* fmt, va_list arg1, va_list arg2) {
    increment_fail_count();
#if defined(MWG_DEBUG) || !defined(NDEBUG)
    vprint_onfail_impl(expr, pos, func, fmt, arg1);
#endif
    vthrow_onfail_impl(expr, pos, func, fmt, arg2);

    /*NOTREACHED*/
    throw;
  }
  MWG_ATTRIBUTE_NORETURN
  MWG_ATTRIBUTE_UNUSED
  static bool mwg_noinline throw_onfail(const char* expr, const char* pos, const char* func, const char* fmt, ...) {
    va_list arg1;
    va_list arg2;
    va_start(arg1, fmt);
    va_start(arg2, fmt);
    mwg::except_detail::vthrow_onfail(expr, pos, func, fmt, arg1, arg2);
    va_end(arg1);
    va_end(arg2);
    /*NOTREACHED*/
    throw;
  }

  inline bool nop_succuss() {return true;}
}
}


/* 実装メモ: mwg_assert の展開式をどの様にするか?
 * 1 (condition||print) にする理由
 *
 *   #define assert(condition, message) check_condition_to_throw(condition, message);
 *
 *   としていると message 部分に含まれる (possibly) 複雑な式が遅延評価にならない。従って、
 *
 *   #define assert(condition, message) ((condition)||print(message));
 *
 *   の様な感じにする。
 *
 * 2 しかし上記の様にしていると condition が静的に true と評価される場合に GCC が
 *
 *   warning: statement has no effect [-Wunused-value]
 *
 *   という警告を出してきてうるさい。特に mwg_assert を使った回数だけ表示される。
 *
 *   a #define mwg_check(condition, message) do {if (!condition) print(message);} while(0)
 *     などとすれば警告は出ないがこれだと式の中に組み込めない。
 *
 *   b #define mwg_check(condition, message) ((condition)? true: (print(message)))
 *     → 同じ警告が出る。
 *
 *   c いきなり true ではなくて dummy の関数呼び出しをしたらどうだろう。
 *     #define mwg_check(condition, message) ((condition)? nop(): (throw1(message), false));
 *     → warning: right operand of comma operator has no effect
 *
 *   d #define mwg_check(condition, message) ((condition)? nop(): throw1(message));
 *     → 警告なし!
 *
 */
#ifdef MWG_STD_VA_ARGS
# define mwg_printl(...)                    mwg::except_detail::mwg_printl_("" __VA_ARGS__)
# define mwg_printd(...)                    mwg::except_detail::mwg_printd_(mwg_assert_position, mwg_assert_funcname, "" __VA_ARGS__)
# ifdef _MSC_VER
#  define mwg_check_call(condition, onfail, ...) ((condition) || (onfail(#condition, mwg_assert_position, mwg_assert_funcname, "" __VA_ARGS__), false))
# else
#  define mwg_check_call(condition, onfail, ...) ((condition)? mwg::except_detail::nop_succuss(): onfail(#condition, mwg_assert_position, mwg_assert_funcname, "" __VA_ARGS__))
# endif
# define mwg_check_nothrow(condition, ...) mwg_check_call(condition, mwg::except_detail::print_onfail, __VA_ARGS__)
# define mwg_check(condition, ...)         mwg_check_call(condition, mwg::except_detail::throw_onfail, __VA_ARGS__)
#else
// __VA_ARGS__ が使えない環境ではその場で関手を生成する。

namespace mwg {
namespace except_detail {
  struct mwg_printd_proxy {
    const char* position;
    const char* funcname;
    mwg_printd_proxy(const char* position, const char* funcname):
      position(position), funcname(funcname) {}

    void operator()() const {
      mwg_printd_(position, funcname, "");
    }
    void operator()(const char* fmt, ...) const {
      va_list arg;
      va_start(arg, fmt);
      mwg_vprintd_(position, funcname, fmt, arg);
      va_end(arg);
    }
  };

  template<bool Throw>
  struct mwg_check_proxy {
    const char* expression;
    const char* position;
    const char* funcname;
    mwg_check_proxy(const char* expression, const char* position, const char* funcname):
      expression(expression), position(position), funcname(funcname) {}

    bool operator()(bool condition) const {
      if (!condition) {
        if (Throw) {
          throw_onfail(expression, position, funcname, "");
        } else {
          print_onfail(expression, position, funcname, "");
        }
      }
      return condition;
    }

    bool operator()(bool condition, const char* fmt, ...) const {
      if (!condition) {
        if (Throw) {
          va_list arg1;
          va_list arg2;
          va_start(arg1, fmt);
          va_start(arg2, fmt);
          vthrow_onfail(expression, position, funcname, fmt, arg1, arg2);
          va_end(arg1);
          va_end(arg2);
        } else {
          va_list arg;
          va_start(arg, fmt);
          vprint_onfail(expression, position, funcname, fmt, arg);
          va_end(arg);
        }
      }
      return condition;
    }
  };

}
}

# define mwg_printd        (::mwg::except_detail::mwg_printd_proxy(mwg_assert_position, mwg_assert_funcname))
# define mwg_check_nothrow (::mwg::except_detail::mwg_check_proxy<false>("N/A", mwg_assert_position, mwg_assert_funcname))
# define mwg_check         (::mwg::except_detail::mwg_check_proxy<true> ("N/A", mwg_assert_position, mwg_assert_funcname))
#endif

#if defined(MWG_DEBUG) || !defined(NDEBUG)
# define mwg_verify_nothrow mwg_check_nothrow
# define mwg_verify         mwg_check
# define mwg_assert_nothrow mwg_check_nothrow
# define mwg_assert         mwg_check
#else
  static inline int mwg_assert_empty() {return 1;}
  static inline int mwg_assert_empty(bool condition, ...) {return condition;}
# ifdef MWG_STD_VA_ARGS
#  define mwg_verify(condition, ...)         mwg_assert_empty(condition)
#  define mwg_verify_nothrow(condition, ...) mwg_assert_empty(condition)
#  ifdef _MSC_VER
#    define mwg_assert(condition, ...)         __assume(condition)
#    define mwg_assert_nothrow(condition, ...) __assume(condition)
#  else
#    define mwg_assert(...)                   mwg_assert_empty()
#    define mwg_assert_nothrow(...)           mwg_assert_empty()
#  endif
# else
#  define mwg_verify           mwg_assert_empty
#  define mwg_verify_nothrow   mwg_assert_empty
#  define mwg_assert           mwg_assert_empty
#  define mwg_assert_nothrow   mwg_assert_empty
# endif
#endif
#define MWG_NOTREACHED /* NOTREACHED */ mwg_assert(0, "NOTREACHED")

#endif
