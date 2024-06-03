#ifndef SUPERSTRING_TEXT_BUFFER_WRAPPER_H
#define SUPERSTRING_TEXT_BUFFER_WRAPPER_H

#include <unordered_set>

#include "napi.h"
#include "text-buffer.h"

class CancellableWorker {
public:
  virtual void CancelIfQueued() = 0;
};

class TextBufferWrapper : public Napi::ObjectWrap<TextBufferWrapper> {
public:
  static void init(Napi::Object exports);
  TextBuffer text_buffer;
  std::unordered_set<Napi::AsyncWorker *> outstanding_workers;
  std::mutex outstanding_workers_mutex;

  TextBufferWrapper(const Napi::CallbackInfo &info);

private:
  Napi::Value get_length(const Napi::CallbackInfo &info);
  Napi::Value get_extent(const Napi::CallbackInfo &info);
  Napi::Value get_line_count(const Napi::CallbackInfo &info);
  Napi::Value has_astral(const Napi::CallbackInfo &info);
  Napi::Value get_text(const Napi::CallbackInfo &info);
  Napi::Value get_character_at_position(const Napi::CallbackInfo &info);
  Napi::Value get_text_in_range(const Napi::CallbackInfo &info);
  void set_text(const Napi::CallbackInfo &info);
  void set_text_in_range(const Napi::CallbackInfo &info);
  Napi::Value line_for_row(const Napi::CallbackInfo &info);
  Napi::Value line_length_for_row(const Napi::CallbackInfo &info);
  Napi::Value line_ending_for_row(const Napi::CallbackInfo &info);
  Napi::Value get_lines(const Napi::CallbackInfo &info);
  Napi::Value character_index_for_position(const Napi::CallbackInfo &info);
  Napi::Value position_for_character_index(const Napi::CallbackInfo &info);
  void find(const Napi::CallbackInfo &info);
  Napi::Value find_sync(const Napi::CallbackInfo &info);
  void find_all(const Napi::CallbackInfo &info);
  Napi::Value find_all_sync(const Napi::CallbackInfo &info);
  Napi::Value find_and_mark_all_sync(const Napi::CallbackInfo &info);
  void find_words_with_subsequence_in_range(const Napi::CallbackInfo &info);
  Napi::Value is_modified(const Napi::CallbackInfo &info);
  void load(const Napi::CallbackInfo &info);
  void base_text_matches_file(const Napi::CallbackInfo &info);
  void save(const Napi::CallbackInfo &info);
  Napi::Value load_sync(const Napi::CallbackInfo &info);
  Napi::Value save_sync(const Napi::CallbackInfo &info);
  Napi::Value serialize_changes(const Napi::CallbackInfo &info);
  void deserialize_changes(const Napi::CallbackInfo &info);
  void reset(const Napi::CallbackInfo &info);
  Napi::Value base_text_digest(const Napi::CallbackInfo &info);
  Napi::Value get_snapshot(const Napi::CallbackInfo &info);
  Napi::Value dot_graph(const Napi::CallbackInfo &info);

  void cancel_queued_workers();
  static Napi::FunctionReference constructor;
};

#endif // SUPERSTRING_TEXT_BUFFER_WRAPPER_H
