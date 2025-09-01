#ifndef SUPERSTRING_TEXT_BUFFER_SNAPSHOT_WRAPPER_H
#define SUPERSTRING_TEXT_BUFFER_SNAPSHOT_WRAPPER_H

#include <string>

#include "napi.h"
#include "text-buffer.h"

// This header can be included by other native node modules, allowing them
// to access the content of a TextBuffer::Snapshot without having to call
// any superstring APIs.

class TextBufferSnapshotWrapper : public Napi::ObjectWrap<TextBufferSnapshotWrapper> {
public:
  static void init(Napi::Env env);

  static Napi::Value new_instance(Napi::Env, Napi::Object, TextBuffer::Snapshot *);

  inline const std::vector<std::pair<const char16_t *, uint32_t>> *slices() {
    return &slices_;
  }

  explicit TextBufferSnapshotWrapper(const Napi::CallbackInfo &info);
  ~TextBufferSnapshotWrapper();

private:
  void destroy(const Napi::CallbackInfo &info);

  Napi::ObjectReference js_text_buffer;
  TextBuffer::Snapshot *snapshot;
  std::vector<std::pair<const char16_t *, uint32_t>> slices_;
};

#endif // SUPERSTRING_TEXT_BUFFER_SNAPSHOT_WRAPPER_H
