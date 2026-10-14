#ifndef SERIALIZER_H_
#define SERIALIZER_H_

#include <vector>
#include <cstdint>
#include <cstddef>

class Serializer {
  std::vector<uint8_t> &vector;

 public:
  inline Serializer(std::vector<uint8_t> &output) :
    vector(output) {};

  template <typename T>
  void append(T value) {
    for (auto i = 0u; i < sizeof(T); i++) {
      vector.push_back(value & 0xFF);
      value >>= 8;
    }
  }
};

class Deserializer {
  const uint8_t *read_ptr;
  const uint8_t *end_ptr;

  // Bytes still available. Clamped at zero so that a `read_ptr` at `end_ptr`
  // reports nothing left rather than a negative difference: the previous
  // bounds check computed `end_ptr - read_ptr` and cast it to `unsigned`, so
  // once `read_ptr` had advanced past `end_ptr` the difference went negative
  // and read back as an enormous count. One short read therefore disabled
  // bounds checking for every read after it.
  inline size_t remaining() const {
    return read_ptr < end_ptr ? static_cast<size_t>(end_ptr - read_ptr) : 0;
  }

 public:
  inline Deserializer(const std::vector<uint8_t> &input) :
    read_ptr(input.data()),
    end_ptr(input.data() + input.size()) {};

  template <typename T>
  T peek() const {
    T value = 0;
    if (remaining() >= sizeof(T)) {
      const uint8_t *temp_ptr = read_ptr;
      for (auto i = 0u; i < sizeof(T); i++) {
        value |= static_cast<T>(*(temp_ptr++)) << static_cast<T>(8 * i);
      }
    }
    return value;
  }

  template <typename T>
  T read() {
    T value = peek<T>();
    // A short read already yields 0; stop at the end instead of advancing past
    // it, so the bounds check above keeps working for subsequent reads.
    read_ptr += (remaining() >= sizeof(T)) ? sizeof(T) : remaining();
    return value;
  }
};

#endif // SERIALIZER_H_
