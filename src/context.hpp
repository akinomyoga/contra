// -*- mode: c++; indent-tabs-mode: nil -*-
#ifndef contra_context_hpp
#define contra_context_hpp
#include <cctype>
#include <cstring>
#include <string>
#include <iostream>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <type_traits>
#include <limits>
namespace contra::app {

  template<typename T, typename X = void>
  struct conv_impl {
    bool operator()(T& value, const char* text) const {
      std::istringstream istr(text);
      istr >> value;
      return true;
    }
  };

  template<>
  struct conv_impl<const char*, void> {
    bool operator()(const char*& value, const char* text) const {
      value = text;
      return true;
    }
  };

  template<>
  struct conv_impl<std::string, void> {
    bool operator()(std::string& value, const char* text) const {
      value = text;
      return true;
    }
  };

  template<>
  struct conv_impl<bool, void> {
    bool operator()(bool& value, const char* text) const {
      if (std::strcmp(text, "true") == 0)
        value = true;
      else if (std::strcmp(text, "false") == 0)
        value = false;
      else
        return false;
      return true;
    }
  };

  template<typename T>
  struct conv_impl<T, std::enable_if_t<std::is_floating_point_v<T>, void>> {
    bool operator()(T& value, const char* text) const {
      char* endp;
      double const v = std::strtod(text, &endp);
      if (endp && endp[0] == '\0') {
        value = v;
        return true;
      }
      return false;
    }
  };

  template<typename T>
  struct conv_impl<T, std::enable_if_t<std::is_integral_v<T>, void>> {
    static int xdigit2int(char c) {
      if ('0' <= c && c <= '9') return c - '0';
      switch (c) {
      case 'a': case 'A': return 10;
      case 'b': case 'B': return 11;
      case 'c': case 'C': return 12;
      case 'd': case 'D': return 13;
      case 'e': case 'E': return 14;
      case 'f': case 'F': return 15;
      default: return -1;
      }
    }

    static bool mul_checked(T& value, unsigned rhs) {
      if (std::numeric_limits<T>::is_signed && value < 0) {
        if (value < std::numeric_limits<T>::min() / (T) rhs) return false;
      } else {
        if (value > std::numeric_limits<T>::max() / (T) rhs) return false;
      }
      value *= rhs;
      return true;
    }
    static bool add_checked(T& value, int delta) {
      if (delta < 0) {
        if (value < std::numeric_limits<T>::min() + -delta) return false;
        value -= -delta;
      } else {
        if (value > std::numeric_limits<T>::max() - delta) return false;
        value += delta;
      }
      return true;
    }

  public:
    bool operator()(T& value, const char* text) const {
      std::size_t index = 0;

      int sign = 1;
      if (text[index] == '-') {
        if (!std::numeric_limits<T>::is_signed) return false;
        sign = -1;
        index++;
      } else if (text[index] == '+') {
        index++;
      }

      T ret = 0;
      char c = text[index++];
      if (!std::isdigit(c)) {
        return false;
      } else if (c == '0' && (text[index] == 'x' || text[index] == 'X')) {
        index++;
        while ((c = text[index++]) && std::isxdigit(c)) {
          if (!mul_checked(ret, 16)) return false;
          if (!add_checked(ret, xdigit2int(c - '0') * sign)) return false;
        }
        if (c) return false;
      } else {
        ret = (c - '0') * sign;
        while ((c = text[index++]) && std::isdigit(c)) {
          if (!mul_checked(ret, 10)) return false;
          if (!add_checked(ret, (c - '0') * sign)) return false;
        }
        if (c) return false;
      }
      value = ret;
      return true;
    }
  };


  class context {
    std::unordered_map<std::string, std::string> data;

  public:
    template<typename T, typename Converter>
    bool read(const char* name, T& value, Converter&& conv) {
      auto it = data.find(name);
      if (it == data.end()) return false;
      if (!conv(value, it->second.c_str())) {
        std::cerr << "$" << name << ": invalid value" << std::endl;
        return false;
      }
      return true;
    }

    template<typename T>
    bool read(const char* name, T& value) {
      return read(name, value, conv_impl<T>());
    }

    template<typename T>
    T get(const char* name, T const& defaultValue) {
      T ret;
      if (!read(name, ret)) ret = defaultValue;
      return ret;
    }

  private:
    struct line_reader {
      std::string line;
      std::size_t index;
      char c;

    public:
      bool next() {
        if (index >= line.size()) {
          c = '\0';
          return false;
        } else {
          c = line[index++];
          return true;
        }
      }

      static bool iscsymf(char c) {
        return std::isalpha(c) || c == '_';
      }
      static bool iscsym(char c) {
        return std::isalnum(c) || c == '_';
      }

      void initialize() {
        this->index = 0;
        this->c = line[index++];
      }
      void skip_space() {
        while (std::isspace(c) && next());
      }
      std::string get_identifier() {
        std::string data;
        data += c;
        while (next() && iscsym(c)) data += c;
        return data;
      }


      bool read_single_quote(std::string& data) {
        if (!next()) return false;
        for (;;) {
          switch (c) {
          case '\'':
            next();
            return true;
          case '\\':
            if (!next()) return false;
            data += c;
            break;
          default:
            data += c;
            break;
          }
          if (!next()) return false;
        }
      }

      bool read_value(std::string& data) {
        for (;;) {
          if (isspace(c)) {
            return true;
          } else if (c == '\'') {
            if (!read_single_quote(data)) return false;
          } else {
            data += c;
            if (!next()) return true;
          }
        }
      }
    };

  public:
    void load(const char* filename) {
      std::ifstream ifs(filename);
      if (!ifs) {
        std::cerr << filename << ": failed to open for read." << std::endl;
        return;
      }

      std::size_t lineno = 0;
      line_reader reader;
      while (std::getline(ifs, reader.line)) {
        lineno++;
        reader.initialize();

        while (reader.c) {
          reader.skip_space();
          if (reader.iscsymf(reader.c)) {
            std::string const identifier = reader.get_identifier();
            if (reader.c != '=') {
              std::cerr << filename << ":" << lineno << ": unrecognized line" << std::endl;
              break;
            }

            reader.next();
            std::string value;
            if (!reader.read_value(value)) {
              std::cerr << filename << ":" << lineno << ": unterminated rhs" << std::endl;
              break;
            }

            data[identifier] = value;
          } else if (reader.c == '#') {
            break;
          }
        }
      }

    }
  };

}
#endif
