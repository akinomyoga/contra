// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef CONTRA_ANSI_UTIL_HPP
#define CONTRA_ANSI_UTIL_HPP
#include <iterator>
#include <vector>
#include <cstddef>
#include <utility>

namespace contra {
namespace util {

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

    typedef indexer_iterator<T, ring_buffer, std::size_t> iterator;
    typedef indexer_iterator<const T, const ring_buffer, std::size_t> const_iterator;
    iterator begin() { return {this, (std::size_t) 0}; }
    iterator end() { return {this, data.size()}; }
    const_iterator begin() const { return {this, (std::size_t) 0}; }
    const_iterator end() const { return {this, data.size()}; }
  };

  template<typename Value, typename CRTP>
  struct flags_def {
    Value value;

    constexpr flags_def() {}
    constexpr flags_def(Value value): value(value) {}

    // bitwise operators
    constexpr flags_def operator~() const { return {~this->value}; }
    constexpr flags_def operator|(flags_def const& rhs) const { return {this->value | rhs.value}; }
    constexpr flags_def operator&(flags_def const& rhs) const { return {this->value & rhs.value}; }
    constexpr flags_def operator^(flags_def const& rhs) const { return {this->value ^ rhs.value}; }
    constexpr flags_def& operator|=(flags_def const& rhs) { this->value |= rhs.value; return *this; }
    constexpr flags_def& operator&=(flags_def const& rhs) { this->value &= rhs.value; return *this; }
    constexpr flags_def& operator^=(flags_def const& rhs) { this->value ^= rhs.value; return *this; }

    // boolean operators
    constexpr operator bool() const { return value != 0; }
    constexpr bool operator!() const { return value == 0; }
  };

  template<typename Value, typename CRTP>
  struct flags {
    Value value;

    typedef flags_def<Value, CRTP> def;

    constexpr flags() {}
    constexpr flags(def flags): value(flags.value) {}
    constexpr explicit flags(Value value): value(value) {}

    // bitwise operators
    constexpr flags operator~() const { return flags {~this->value}; }
    constexpr flags operator|(flags const& rhs) const { return flags {this->value | rhs.value}; }
    constexpr flags operator&(flags const& rhs) const { return flags {this->value & rhs.value}; }
    constexpr flags operator^(flags const& rhs) const { return flags {this->value ^ rhs.value}; }
    constexpr flags& operator|=(flags const& rhs) { this->value |= rhs.value; return *this; }
    constexpr flags& operator&=(flags const& rhs) { this->value &= rhs.value; return *this; }
    constexpr flags& operator^=(flags const& rhs) { this->value ^= rhs.value; return *this; }

    // boolean operators
    constexpr operator bool() const { return value != 0; }
    constexpr bool operator!() const { return value == 0; }
  };
}
}

#endif
