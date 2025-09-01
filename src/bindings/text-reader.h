#ifndef SUPERSTRING_TEXT_READER_H
#define SUPERSTRING_TEXT_READER_H

#include "napi.h"
#include "text.h"
#include "text-buffer.h"
#include "encoding-conversion.h"

class TextReader : public Napi::ObjectWrap<TextReader> {
public:
  static void init(Napi::Env env, Napi::Object exports);
  explicit TextReader(const Napi::CallbackInfo &info);
  ~TextReader();

private:
  Napi::Value read(const Napi::CallbackInfo &info);
  void end(const Napi::CallbackInfo &info);
  void destroy(const Napi::CallbackInfo &info);

  Napi::ObjectReference js_text_buffer;
  TextBuffer::Snapshot *snapshot;
  std::vector<TextSlice> slices;
  size_t slice_index;
  size_t text_offset;
  std::unique_ptr<EncodingConversion> conversion;
};

#endif // SUPERSTRING_TEXT_READER_H
