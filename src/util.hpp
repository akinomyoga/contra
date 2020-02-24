// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_util_hpp
#define contra_util_hpp
#include <iterator>
#include <vector>
#include <cstddef>
#include <utility>
#include <algorithm>

namespace contra {
namespace util {

  template<typename Pointer, typename Deleter>
  struct raii {
    Pointer pointer;
    Deleter deleter;
  public:
    raii(Pointer pointer, Deleter deleter): pointer(pointer), deleter(deleter) {}
    ~raii() { if (pointer) deleter(pointer); }
    raii& operator=(raii const&) = delete;
    raii& operator=(raii&&) = delete;
    Pointer operator->() const { return pointer; }
    Pointer get() const { return pointer; }
    operator Pointer() const { return pointer; }
  };

  template<typename T, typename Container, typename Index>
  class indexer_iterator: std::iterator<std::random_access_iterator_tag, T, Index> {
    typedef indexer_iterator self;
    typedef std::iterator<std::random_access_iterator_tag, T> base;

  private:
    Container* container;
    Index index;
  public:
    indexer_iterator(Container* container, Index index): container(container), index(index) {}

  public:
    // LegacyIterator requirements
    typedef typename base::iterator_category iterator_category;
    typedef typename base::value_type value_type;
    typedef typename base::difference_type difference_type;
    typedef typename base::reference reference;
    typedef typename base::pointer pointer;
    reference operator*() const { return (*container)[index]; }
    self& operator++() { this->index++; return *this; }

  public:
    // LegacyInputIterator requirements
    bool operator==(self const& rhs) const {
      return this->container == rhs.container && this->index == rhs.index;
    }
    bool operator!=(self const& rhs) const { return !(*this == rhs); }
    pointer operator->() const { return &**this; }
    self operator++(int) const { return {this->container, this->index + 1}; }

    // LegacyOutputIterator requirements: *r = o, *r++ = o, ++r
    // LegacyForwardIterator requirements: multipass ok

  public:
    // LegacyBidirectionalIterator requirements
    self& operator--() { this->index--; return *this; }
    self operator--(int) const { return {this->container, this->index - 1}; }

  public:
    // LegacyRandomAccessIterator requirements
    self& operator+=(difference_type delta) { this->index += delta; return *this; }
    self& operator-=(difference_type delta) { this->index -= delta; return *this; }
    self operator+(difference_type delta) const { return {this->container, this->index + delta}; }
    self operator-(difference_type delta) const { return {this->container, this->index - delta}; }
    difference_type operator-(self const& rhs) const { return this->index - rhs.index; }
    reference operator[](difference_type delta) const { return (*this->container)[this->index + delta]; }
    bool operator<(self const& rhs) const { return this->index < rhs.index; }
    bool operator>(self const& rhs) const { return this->index > rhs.index; }
    bool operator<=(self const& rhs) const { return this->index <= rhs.index; }
    bool operator>=(self const& rhs) const { return this->index >= rhs.index; }
  };
  template<typename T, typename Container, typename Index>
  indexer_iterator<T, Container, Index> operator+(Index index, indexer_iterator<T, Container, Index> const& iter) { return iter + index; }


  template<typename T>
  struct ring_buffer {
    std::vector<T> data;
    std::size_t m_rotate;

    template<typename... Args>
    ring_buffer(Args&&... args): data(std::forward<Args>(args)...), m_rotate(0) {}

    T& operator[](std::size_t index) {
      return data[(m_rotate + index) % data.size()];
    }
    T const& operator[](std::size_t index) const {
      return data[(m_rotate + index) % data.size()];
    }

    void rotate(std::size_t delta) {
      m_rotate = (m_rotate + delta) % data.size();
    }
    std::size_t size() const { return data.size(); }
    void resize(std::size_t new_size) {
      std::rotate(data.begin(), data.begin() + m_rotate, data.end());
      this->m_rotate = 0;
      data.resize(new_size);
    }

    typedef indexer_iterator<T, ring_buffer, std::size_t> iterator;
    typedef indexer_iterator<const T, const ring_buffer, std::size_t> const_iterator;
    iterator begin() { return {this, (std::size_t) 0}; }
    iterator end() { return {this, data.size()}; }
    const_iterator begin() const { return {this, (std::size_t) 0}; }
    const_iterator end() const { return {this, data.size()}; }
  };

  // std::rotate に似るが結果の最初の count 個の要素だけ正しければ良い場合に使うアルゴリズム
  template<typename ForwardIterator>
  void partial_rotate(ForwardIterator first, ForwardIterator mid, ForwardIterator last, std::size_t count) {
    if (mid == first || mid == last) return;
    ForwardIterator dst = first;
    ForwardIterator src = mid;
    while (count--) {
      std::iter_swap(dst++, src++);
      if (src == last) {
        if (dst == mid) break;
        src = mid;
      } else if (dst == mid)
        mid = src;
    }
  }
}
}

namespace contra {
namespace util {

  namespace flags_detail {
    template<typename Flags, typename Extends>
    struct import_from: std::false_type {};

    struct flags_base {};
    template<typename Flags>
    constexpr bool is_flags = std::is_base_of<flags_base, Flags>::value;
    template<typename Flags, typename T = void>
    using enable_flags = typename std::enable_if<is_flags<Flags>, T>::type;

    template<typename Flags, typename Other>
    constexpr bool is_rhs = is_flags<Flags> && (
      std::is_same<Other, Flags>::value ||
      std::is_integral<Other>::value && (sizeof(Other) <= sizeof(Flags)) ||
      import_from<Flags, Other>::value);

    template<typename Flags, typename Other>
    using enable_lhs = typename std::enable_if<is_rhs<Flags, Other>, Flags>::type;

    struct auto_tag {};
    template<typename Flags1, typename Flags2, typename R = auto_tag>
    using result_t = typename std::enable_if<
      is_rhs<Flags1, Flags2> || is_rhs<Flags2, Flags1>,
      std::conditional_t<!std::is_same_v<R, auto_tag>, R,
        typename std::conditional<is_rhs<Flags1, Flags2>, Flags1, Flags2>::type>
        >::type;

    template<typename Flags, int = is_flags<Flags> ? 1 : std::is_signed_v<Flags> ? 2 : 0>
    struct underlying { typedef Flags type; };
    template<typename Flags>
    struct underlying<Flags, 1> { typedef typename Flags::underlying_type type; };
    template<typename Flags>
    struct underlying<Flags, 2> { typedef std::make_unsigned_t<Flags> type; };

    template<typename Flags, bool = is_flags<Flags> >
    constexpr auto peel(Flags const& value) { return (typename underlying<Flags>::type) value; }

    template<typename Flags1, typename Flags2>
    constexpr flags_detail::result_t<Flags1, Flags2>
    operator|(Flags1 const& lhs, Flags2 const& rhs) { return { peel(lhs) | peel(rhs)}; }
    template<typename Flags1, typename Flags2>
    constexpr flags_detail::result_t<Flags1, Flags2>
    operator&(Flags1 const& lhs, Flags2 const& rhs) { return { peel(lhs) & peel(rhs)}; }
    template<typename Flags1, typename Flags2>
    constexpr flags_detail::result_t<Flags1, Flags2>
    operator^(Flags1 const& lhs, Flags2 const& rhs) { return { peel(lhs) ^ peel(rhs)}; }
    template<typename Flags1, typename Flags2>
    constexpr flags_detail::result_t<Flags1, Flags2, bool>
    operator==(Flags1 const& lhs, Flags2 const& rhs) { return peel(lhs) == peel(rhs); }
    template<typename Flags1, typename Flags2>
    constexpr flags_detail::result_t<Flags1, Flags2, bool>
    operator!=(Flags1 const& lhs, Flags2 const& rhs) { return peel(lhs) != peel(rhs); }
  }

  template<typename Unsigned, typename Tag>
  class flags_t: flags_detail::flags_base {
    Unsigned m_value;
  public:
    typedef Unsigned underlying_type;

    template<typename Other, typename = flags_detail::enable_lhs<flags_t, Other> >
    constexpr flags_t(Other const& value): m_value((Unsigned) value) {}

    constexpr explicit operator Unsigned() const { return m_value; }
    constexpr explicit operator bool() const { return m_value; }
    constexpr bool operator!() const { return !m_value; }
    constexpr flags_t operator~() const { return { ~m_value }; }

    template<typename Other>
    constexpr flags_detail::enable_lhs<flags_t, Other>&
    operator&=(Other const& rhs) { m_value &= (Unsigned) rhs; return *this; }
    template<typename Other>
    constexpr flags_detail::enable_lhs<flags_t, Other>&
    operator|=(Other const& rhs) { m_value |= (Unsigned) rhs; return *this; }
    template<typename Other>
    constexpr flags_detail::enable_lhs<flags_t, Other>&
    operator^=(Other const& rhs) { m_value ^= (Unsigned) rhs; return *this; }

    flags_t operator>>(int shift) const { return { m_value >> shift }; }
    flags_t operator<<(int shift) const { return { m_value << shift }; }

    flags_t& reset(flags_t mask, flags_t value) {
      this->m_value = (this->m_value & ~(Unsigned) mask) | (Unsigned) value;
      return *this;
    }
    constexpr Unsigned value() const { return m_value; }
  };

  using flags_detail::operator|;
  using flags_detail::operator^;
  using flags_detail::operator&;
  using flags_detail::operator==;
  using flags_detail::operator!=;
}
}

#endif
