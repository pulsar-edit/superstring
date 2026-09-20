#ifndef SUPERSTRING_OPTIONAL_H
#define SUPERSTRING_OPTIONAL_H

#include <utility>

template <typename T> class optional {
  T value;
  bool is_some;

public:
  optional(T &&value) : value(std::move(value)), is_some(true) {}
  optional(const T &value) : value(value), is_some(true) {}
  optional() : value(T()), is_some(false) {}

  T &operator*() { return value; }
  const T &operator*() const { return value; }
  const T *operator->() const { return &value; }
  T *operator->() { return &value; }
  // Deliberately `explicit`: an implicit conversion to bool makes
  // `optional<T> == optional<T>` ambiguous, because the built-in `bool ==
  // bool` becomes as good a candidate as the member below. Clang and GCC pick
  // the member anyway; MSVC rejects the comparison outright (C2666). Every
  // contextual use — `if (x)`, `!x`, `x && y`, `x ? a : b` — still works.
  explicit operator bool() const { return is_some; }
  bool operator==(const optional<T> &other) const {
    if (is_some) {
      return other.is_some && value == other.value;
    } else {
      return !other.is_some;
    }
  }
};

#endif // SUPERSTRING_OPTIONAL_H
