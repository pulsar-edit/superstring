#ifndef SUPERSTRING_TEXT_WRITER_H
#define SUPERSTRING_TEXT_WRITER_H

#include "napi.h"
#include "text.h"
#include "encoding-conversion.h"

class TextWriter : public Napi::ObjectWrap<TextWriter> {
public:
  static void init(Napi::Env env, Napi::Object exports);
  explicit TextWriter(const Napi::CallbackInfo &info);
  std::u16string get_text();

private:
  void write(const Napi::CallbackInfo &info);
  void end(const Napi::CallbackInfo &info);

  std::unique_ptr<EncodingConversion> conversion;
  std::vector<char> leftover_bytes;
  std::u16string content;
};

#endif // SUPERSTRING_TEXT_WRITER_H
