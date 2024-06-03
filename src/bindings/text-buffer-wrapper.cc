#include <sstream>
#include <iomanip>
#include <stdio.h>
#include <sys/stat.h>

#include "v8.h"
#include "node.h"
#include "text-buffer-wrapper.h"
#include "number-conversion.h"
#include "point-wrapper.h"
#include "range-wrapper.h"
#include "marker-index-wrapper.h"
#include "string-conversion.h"
#include "patch-wrapper.h"
#include "text-buffer-snapshot-wrapper.h"
#include "text-writer.h"
#include "text-slice.h"
#include "text-diff.h"
#include "util.h"

using namespace Napi;
using std::move;
using std::pair;
using std::string;
using std::u16string;
using std::vector;
using std::wstring;

using SubsequenceMatch = TextBuffer::SubsequenceMatch;

#define REGEX_CACHE_KEY "__textBufferRegex"

#ifdef WIN32

#include <windows.h>
#include <io.h>

static wstring ToUTF16(string input) {
  wstring result;
  int length = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.length(), NULL, 0);
  if (length > 0) {
    result.resize(length);
    MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.length(), &result[0], length);
  }
  return result;
}

static size_t get_file_size(FILE *file) {
  LARGE_INTEGER result;
  if (!GetFileSizeEx((HANDLE)_get_osfhandle(fileno(file)), &result)) {
    errno = GetLastError();
    return -1;
  }
  return static_cast<size_t>(result.QuadPart);
}

static FILE *open_file(const string &name, const char *flags) {
  wchar_t wide_flags[6] = {0, 0, 0, 0, 0, 0};
  size_t flag_count = strlen(flags);
  MultiByteToWideChar(CP_UTF8, 0, flags, flag_count, wide_flags, flag_count);
  return _wfopen(ToUTF16(name).c_str(), wide_flags);
}

#else

static size_t get_file_size(FILE *file) {
  struct stat file_stats;
  if (fstat(fileno(file), &file_stats) != 0) return -1;
  return file_stats.st_size;
}

static FILE *open_file(const std::string &name, const char *flags) {
  return fopen(name.c_str(), flags);
}

#endif

static size_t CHUNK_SIZE = 10 * 1024;

class RegexWrapper : public ObjectWrap<RegexWrapper> {
public:
  RegexWrapper(const CallbackInfo &info): ObjectWrap<RegexWrapper>(info) {
    if (info[0].IsExternal()) {
      auto wrapper = info[0].As<External<Regex>>();
      regex.reset(wrapper.Data());
    }
  }

  static const Regex *regex_from_js(const Napi::Value &value) {
    auto env = value.Env();

    String js_pattern;
    bool ignore_case = false;
    bool unicode = false;
    Object js_regex;

    if (value.IsString()) {
      js_pattern = value.As<String>();
    } else {
      v8::Local<v8::Value> js_regex_value = V8LocalValueFromJsValue(value);
      if (!value.IsObject() || !js_regex_value->IsRegExp()) {
          Napi::Error::New(env, "Argument must be a RegExp").ThrowAsJavaScriptException();
          return nullptr;
      }

      // Check if there is any cached regex inside the js object.
      js_regex = value.As<Object>();
      if (js_regex.Has(REGEX_CACHE_KEY)) {
        Napi::Value js_regex_wrapper = js_regex.Get(REGEX_CACHE_KEY);
        if (js_regex_wrapper.IsObject()) {
          return Unwrap(js_regex_wrapper.As<Object>())->regex.get();
        }
      }

      // Extract necessary parameters from RegExp
      v8::Local<v8::RegExp> v8_regex = js_regex_value.As<v8::RegExp>();
      js_pattern = Napi::Value(env, JsValueFromV8LocalValue(v8_regex->GetSource())).As<String>();
      if (v8_regex->GetFlags() & v8::RegExp::kIgnoreCase) ignore_case = true;
      if (v8_regex->GetFlags() & v8::RegExp::kUnicode) unicode = true;
    }

    // initialize Regex
    u16string error_message;
    optional<u16string> pattern = string_conversion::string_from_js(js_pattern);
    Regex regex = Regex(*pattern, &error_message, ignore_case, unicode);
    if (!error_message.empty()) {
      Napi::Error::New(env, string_conversion::string_to_js(env, error_message)).ThrowAsJavaScriptException();
      return nullptr;
    }

    // initialize RegexWrapper
    auto wrapper = External<Regex>::New(env, new Regex(move(regex)));
    auto js_regex_wrapper = constructor.New({wrapper});

    // cache Regex
    if (!js_regex.IsEmpty()) {
      js_regex.Set(REGEX_CACHE_KEY, js_regex_wrapper);
    }

    return Unwrap(js_regex_wrapper)->regex.get();
  }

  static void init(Napi::Env env) {
    Function func = DefineClass(env, "RegexWrapper", {});
    constructor.Reset(func, 1);
  }

private:
  static FunctionReference constructor;
  std::unique_ptr<Regex> regex;
};

FunctionReference RegexWrapper::constructor;

class SubsequenceMatchWrapper : public ObjectWrap<SubsequenceMatchWrapper> {
public:
  static void init(Napi::Env env) {
    Function func = DefineClass(env, "SubsequenceMatch", {
      InstanceAccessor<&SubsequenceMatchWrapper::get_word>("word", static_cast<napi_property_attributes>(napi_enumerable | napi_configurable)),
      InstanceAccessor<&SubsequenceMatchWrapper::get_match_indices>("matchIndices", static_cast<napi_property_attributes>(napi_enumerable | napi_configurable)),
      InstanceAccessor<&SubsequenceMatchWrapper::get_score, &SubsequenceMatchWrapper::set_score>(
        "score", static_cast<napi_property_attributes>(napi_enumerable | napi_configurable)
      ),
    });

    constructor.Reset(func, 1);
  }

  static Napi::Value from_subsequence_match(Napi::Env env, SubsequenceMatch match) {
    auto wrapper = External<TextBuffer::SubsequenceMatch>::New(env, &match);
    return constructor.New({wrapper});
  }

  SubsequenceMatchWrapper(const CallbackInfo &info): ObjectWrap<SubsequenceMatchWrapper>(info) {
    if (info[0].IsExternal()) {
      auto wrapper = info[0].As<External<TextBuffer::SubsequenceMatch>>();
      match = std::move(*wrapper.Data());
    }
  }

 private:
  Napi::Value get_word(const CallbackInfo &info) {
    return string_conversion::string_to_js(info.Env(), this->match.word);
  }

  Napi::Value get_match_indices(const CallbackInfo &info) {
    auto env = info.Env();
    SubsequenceMatch &match = this->match;
    Array js_result = Array::New(env);
    for (size_t i = 0; i < match.match_indices.size(); i++) {
      js_result[i] = Number::New(env, match.match_indices[i]);
    }
    return js_result;
  }

  Napi::Value get_score(const CallbackInfo &info) {
    return Number::New(info.Env(), this->match.score);
  }

  void set_score(const CallbackInfo &info, const Napi::Value &value) {
    if (value.IsNumber()) {
      this->match.score = value.As<Number>().DoubleValue();
    } else {
      auto env = info.Env();
      Error::New(env, "Expected a number.").ThrowAsJavaScriptException();
    }
  }

  TextBuffer::SubsequenceMatch match;
  static FunctionReference constructor;
};

void TextBufferWrapper::init(Object exports) {
  auto env = exports.Env();

  RegexWrapper::init(env);
  SubsequenceMatchWrapper::init(env);


  Napi::Function func = DefineClass(env, "TextBuffer", {
    InstanceMethod<&TextBufferWrapper::get_length>("getLength", napi_default_method),
    InstanceMethod<&TextBufferWrapper::get_extent>("getExtent", napi_default_method),
    InstanceMethod<&TextBufferWrapper::get_line_count>("getLineCount", napi_default_method),
    InstanceMethod<&TextBufferWrapper::has_astral>("hasAstral", napi_default_method),
    InstanceMethod<&TextBufferWrapper::get_character_at_position>("getCharacterAtPosition", napi_default_method),
    InstanceMethod<&TextBufferWrapper::get_text_in_range>("getTextInRange", napi_default_method),
    InstanceMethod<&TextBufferWrapper::set_text_in_range>("setTextInRange", napi_default_method),
    InstanceMethod<&TextBufferWrapper::get_text>("getText", napi_default_method),
    InstanceMethod<&TextBufferWrapper::set_text>("setText", napi_default_method),
    InstanceMethod<&TextBufferWrapper::line_for_row>("lineForRow", napi_default_method),
    InstanceMethod<&TextBufferWrapper::line_length_for_row>("lineLengthForRow", napi_default_method),
    InstanceMethod<&TextBufferWrapper::line_ending_for_row>("lineEndingForRow", napi_default_method),
    InstanceMethod<&TextBufferWrapper::get_lines>("getLines", napi_default_method),
    InstanceMethod<&TextBufferWrapper::character_index_for_position>("characterIndexForPosition", napi_default_method),
    InstanceMethod<&TextBufferWrapper::position_for_character_index>("positionForCharacterIndex", napi_default_method),
    InstanceMethod<&TextBufferWrapper::is_modified>("isModified", napi_default_method),
    InstanceMethod<&TextBufferWrapper::load>("load", napi_default_method),
    InstanceMethod<&TextBufferWrapper::base_text_matches_file>("baseTextMatchesFile", napi_default_method),
    InstanceMethod<&TextBufferWrapper::save>("save", napi_default_method),
    InstanceMethod<&TextBufferWrapper::load_sync>("loadSync", napi_default_method),
    InstanceMethod<&TextBufferWrapper::serialize_changes>("serializeChanges", napi_default_method),
    InstanceMethod<&TextBufferWrapper::deserialize_changes>("deserializeChanges", napi_default_method),
    InstanceMethod<&TextBufferWrapper::reset>("reset", napi_default_method),
    InstanceMethod<&TextBufferWrapper::base_text_digest>("baseTextDigest", napi_default_method),
    InstanceMethod<&TextBufferWrapper::find>("find", napi_default_method),
    InstanceMethod<&TextBufferWrapper::find_sync>("findSync", napi_default_method),
    InstanceMethod<&TextBufferWrapper::find_all>("findAll", napi_default_method),
    InstanceMethod<&TextBufferWrapper::find_all_sync>("findAllSync", napi_default_method),
    InstanceMethod<&TextBufferWrapper::find_and_mark_all_sync>("findAndMarkAllSync", napi_default_method),
    InstanceMethod<&TextBufferWrapper::find_words_with_subsequence_in_range>("findWordsWithSubsequenceInRange", napi_default_method),
    InstanceMethod<&TextBufferWrapper::dot_graph>("getDotGraph", napi_default_method),
    InstanceMethod<&TextBufferWrapper::get_snapshot>("getSnapshot", napi_default_method),
  });

  constructor.Reset(func, 1);
  exports.Set("TextBuffer", func);
}
FunctionReference SubsequenceMatchWrapper::constructor;
FunctionReference TextBufferWrapper::constructor;

TextBufferWrapper::TextBufferWrapper(const CallbackInfo &info): ObjectWrap<TextBufferWrapper>(info) {
  if (info.Length() > 0 && info[0].IsString()) {
    auto text = string_conversion::string_from_js(info[0]);
    if (text) {
      this->text_buffer.reset(move(*text));
    }
  }
}

Napi::Value TextBufferWrapper::get_length(const CallbackInfo &info) {
  auto &text_buffer = this->text_buffer;
  return Number::New(info.Env(), text_buffer.size());
}

Napi::Value TextBufferWrapper::get_extent(const CallbackInfo &info) {
  auto &text_buffer = this->text_buffer;
  return PointWrapper::from_point(info.Env(), text_buffer.extent());
}

Napi::Value TextBufferWrapper::get_line_count(const CallbackInfo &info) {
  auto &text_buffer = this->text_buffer;
  return Number::New(info.Env(), text_buffer.extent().row + 1);
}

Napi::Value TextBufferWrapper::has_astral(const CallbackInfo &info) {
  auto &text_buffer = this->text_buffer;
  return Boolean::New(info.Env(), text_buffer.has_astral());
}

Napi::Value TextBufferWrapper::get_character_at_position(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;
  auto point = PointWrapper::point_from_js(info[0]);
  if (point) {
    return string_conversion::char_to_js(env, text_buffer.character_at(*point));
  }

  return env.Undefined();
}

Napi::Value TextBufferWrapper::get_text_in_range(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;
  auto range = RangeWrapper::range_from_js(info[0]);
  if (range) {
    return string_conversion::string_to_js(env, text_buffer.text_in_range(*range));
  }

  return env.Undefined();
}

Napi::Value TextBufferWrapper::get_text(const CallbackInfo &info) {
  auto &text_buffer = this->text_buffer;
  return string_conversion::string_to_js(
    info.Env(),
    text_buffer.text(),
    "This buffer's content is too large to fit into a string.\n"
    "\n"
    "Consider using APIs like `getTextInRange` to access the data you need."
  );
}

void TextBufferWrapper::set_text_in_range(const CallbackInfo &info) {
  this->cancel_queued_workers();
  auto &text_buffer = this->text_buffer;
  auto range = RangeWrapper::range_from_js(info[0]);
  auto text = string_conversion::string_from_js(info[1]);
  if (range && text) {
    text_buffer.set_text_in_range(*range, move(*text));
  }
}

void TextBufferWrapper::set_text(const CallbackInfo &info) {
  this->cancel_queued_workers();
  auto &text_buffer = this->text_buffer;
  auto text = string_conversion::string_from_js(info[0]);
  if (text) {
    text_buffer.set_text(move(*text));
  }
}

Napi::Value TextBufferWrapper::line_for_row(const CallbackInfo &info) {
  auto &text_buffer = this->text_buffer;
  Napi::Value result;
  if (info.Length() > 0 && info[0].IsNumber()) {
    uint32_t row = info[0].As<Number>().Uint32Value();
    if (row <= text_buffer.extent().row) {
      text_buffer.with_line_for_row(row, [&info, &result](const char16_t *data, uint32_t size) {
        auto env = info.Env();
        result = String::New(env, data, size);
      });
    }
  }
  return result;
}

Napi::Value TextBufferWrapper::line_length_for_row(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;
  if (info.Length() > 0 && info[0].IsNumber()) {
    uint32_t row = info[0].As<Number>().Uint32Value();
    auto result = text_buffer.line_length_for_row(row);
    if (result) {
      return Number::New(env, *result);
    }
  }
  return env.Undefined();
}

Napi::Value TextBufferWrapper::line_ending_for_row(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;
  if (info.Length() > 0 && info[0].IsNumber()) {
    uint32_t row = info[0].As<Number>().Uint32Value();
    auto result = text_buffer.line_ending_for_row(row);
    if (result) {
      return String::New(env, reinterpret_cast<const char16_t*>(result));
    }
  }
  return env.Undefined();
}

Napi::Value TextBufferWrapper::get_lines(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;
  auto result = Array::New(env);

  for (uint32_t row = 0, row_count = text_buffer.extent().row + 1; row < row_count; row++) {
    auto text = text_buffer.text_in_range({{row, 0}, {row, UINT32_MAX}});
    result[row] = string_conversion::string_to_js(env, text);
  }

  return result;
}

Napi::Value TextBufferWrapper::character_index_for_position(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;
  auto position = PointWrapper::point_from_js(info[0]);
  if (position) {
    return Number::New(env, text_buffer.clip_position(*position).offset);
  }

  return env.Undefined();
}

Napi::Value TextBufferWrapper::position_for_character_index(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;
  if (info.Length() > 0 && info[0].IsNumber()) {
    int64_t offset = info[0].As<Number>().Int64Value();
    return PointWrapper::from_point(env, text_buffer.position_for_offset(
        std::max<int64_t>(0, offset)
      ));
  }
  return env.Undefined();
}

static Value encode_ranges(Env env, const vector<Range> &ranges) {
  auto length = ranges.size() * 4;
  Uint32Array js_array_buffer = Uint32Array::New(env, length);
  memcpy(js_array_buffer.Data(), ranges.data(), length * sizeof(uint32_t));
  return js_array_buffer;
}

template <bool single_result>
class TextBufferSearcher : public Napi::AsyncWorker {
  const TextBuffer::Snapshot *snapshot;
  const Regex *regex;
  Range search_range;
  vector<Range> matches;

public:
  TextBufferSearcher(Function &completion_callback,
                     const TextBuffer::Snapshot *snapshot,
                     const Regex *regex,
                     const Range &search_range) :
    AsyncWorker(completion_callback, "TextBuffer.find"),
    snapshot{snapshot},
    regex{regex},
    search_range(search_range) {
  }

  void Execute() override {
    if (single_result) {
      auto find_result = snapshot->find(*regex, search_range);
      if (find_result) {
        matches.push_back(*find_result);
      }
    } else {
      matches = snapshot->find_all(*regex, search_range);
    }
  }

  void OnOK() override{
    auto env = Env();
    delete snapshot;
    snapshot = nullptr;
    Callback().Call({env.Null(), encode_ranges(env, matches)});
  }
};

Napi::Value TextBufferWrapper::find_sync(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;
  const Regex *regex = RegexWrapper::regex_from_js(info[0]);
  if (regex) {
    optional<Range> search_range;
    if (info[1].IsObject()) {
      search_range = RangeWrapper::range_from_js(info[1]);
      if (!search_range) return env.Null();
    }

    auto match = text_buffer.find(
      *regex,
      search_range ? *search_range : Range::all_inclusive()
    );
    vector<Range> matches;
    if (match) matches.push_back(*match);

    return encode_ranges(env, matches);
  }

  return env.Undefined();
}

Napi::Value TextBufferWrapper::find_all_sync(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;
  const Regex *regex = RegexWrapper::regex_from_js(info[0]);
  if (regex) {
    optional<Range> search_range;
    if (info[1].IsObject()) {
      search_range = RangeWrapper::range_from_js(info[1]);
      if (!search_range) return env.Null();
    }

    vector<Range> matches = text_buffer.find_all(
      *regex,
      search_range ? *search_range : Range::all_inclusive()
    );

    return encode_ranges(env, matches);
  }

  return env.Undefined();
}

Napi::Value TextBufferWrapper::find_and_mark_all_sync(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;
  MarkerIndex *marker_index = MarkerIndexWrapper::from_js(info[0]);
  if (!marker_index) return env.Undefined();

  if (!info[1].IsNumber()) {
    return env.Undefined();
  }
  auto next_id = info[1].As<Number>().Uint32Value();

  if (!info[2].IsBoolean()) {
    return env.Undefined();
  }
  bool exclusive = info[2].As<Boolean>().Value();

  if (info.Length() < 4) return env.Undefined();
  const Regex *regex = RegexWrapper::regex_from_js(info[3]);
  if (regex) {
    optional<Range> search_range;
    if (info.Length() > 4 && info[4].IsObject()) {
      search_range = RangeWrapper::range_from_js(info[4]);
      if (!search_range) return env.Undefined();
    }

    unsigned count = text_buffer.find_and_mark_all(
      *marker_index,
      next_id,
      exclusive,
      *regex,
      search_range ? *search_range : Range::all_inclusive()
    );

    return Number::New(env, count);
  }

  return env.Undefined();
}

void TextBufferWrapper::find(const CallbackInfo &info) {
  auto &text_buffer = this->text_buffer;
  auto callback = info[1].As<Function>();
  const Regex *regex = RegexWrapper::regex_from_js(info[0]);
  if (regex) {
    optional<Range> search_range;
    if (info[2].IsObject()) {
      search_range = RangeWrapper::range_from_js(info[2]);
      if (!search_range) return;
    }

    auto async_worker = new TextBufferSearcher<true>(
      callback,
      text_buffer.create_snapshot(),
      regex,
      search_range ? *search_range : Range::all_inclusive()
    );
    async_worker->Queue();
  }
}

void TextBufferWrapper::find_all(const CallbackInfo &info) {
  auto &text_buffer = this->text_buffer;
  auto callback = info[1].As<Function>();
  const Regex *regex = RegexWrapper::regex_from_js(info[0]);
  if (regex) {
    optional<Range> search_range;
    if (info[2].IsObject()) {
      search_range = RangeWrapper::range_from_js(info[2]);
      if (!search_range) return;
    }
    auto async_worker = new TextBufferSearcher<false>(
      callback,
      text_buffer.create_snapshot(),
      regex,
      search_range ? *search_range : Range::all_inclusive()
    );
    async_worker->Queue();
  }
}

void TextBufferWrapper::find_words_with_subsequence_in_range(const CallbackInfo &info) {
  class FindWordsWithSubsequenceInRangeWorker : public Napi::AsyncWorker {
    Napi::ObjectReference buffer;
    const TextBuffer::Snapshot *snapshot;
    const u16string query;
    const u16string extra_word_characters;
    const size_t max_count;
    const Range range;
    vector<TextBuffer::SubsequenceMatch> result;
    TextBufferWrapper *text_buffer_wrapper;

  public:
    FindWordsWithSubsequenceInRangeWorker(Object buffer,
                                   Function &completion_callback,
                                   const u16string query,
                                   const u16string extra_word_characters,
                                   const size_t max_count,
                                   const Range range) :
      AsyncWorker(completion_callback, "TextBuffer.findWordsWithSubsequence"),
      query{query},
      extra_word_characters{extra_word_characters},
      max_count{max_count},
      range(range) {
      this->buffer.Reset(buffer, 1);
      text_buffer_wrapper = TextBufferWrapper::Unwrap(buffer);
      snapshot = text_buffer_wrapper->text_buffer.create_snapshot();
    }

    ~FindWordsWithSubsequenceInRangeWorker() {
      if (snapshot) {
        delete snapshot;
        snapshot = nullptr;
      }
    }

    void OnWorkComplete(Napi::Env env, napi_status status) override {
      if (status == napi_cancelled) {
        Callback().Call({env.Null()});
      }

      AsyncWorker::OnWorkComplete(env, status);
    }

    void Execute() override {
      {
        std::lock_guard<std::mutex> guard(text_buffer_wrapper->outstanding_workers_mutex);
        text_buffer_wrapper->outstanding_workers.erase(this);
      }

      if (!snapshot) {
        return;
      }
      result = snapshot->find_words_with_subsequence_in_range(query, extra_word_characters, range);
    }

    void OnOK() override {
      auto env = Env();
      if (!snapshot) {
        Callback().Call({env.Null()});
        return;
      }

      delete snapshot;
      snapshot = nullptr;

      Array js_matches_array = Array::New(env);

      uint32_t positions_buffer_size = 0;
      for (const auto &subsequence_match : result) {
        positions_buffer_size += sizeof(uint32_t) + subsequence_match.positions.size() * sizeof(Point);
      }

      auto positions_buffer = ArrayBuffer::New(env, positions_buffer_size);
      uint32_t *positions_data = reinterpret_cast<uint32_t *>(positions_buffer.Data());

      uint32_t positions_array_index = 0;
      for (size_t i = 0; i < result.size() && i < max_count; i++) {
        const SubsequenceMatch &match = result[i];
        positions_data[positions_array_index++] = match.positions.size();
        uint32_t bytes_to_copy = match.positions.size() * sizeof(Point);
        memcpy(
          positions_data + positions_array_index,
          match.positions.data(),
          bytes_to_copy
        );
        positions_array_index += bytes_to_copy / sizeof(uint32_t);
        js_matches_array[i] = SubsequenceMatchWrapper::from_subsequence_match(env, match);
      }

      auto positions_array = Uint32Array::New(env, positions_buffer_size / sizeof(uint32_t), positions_buffer, 0);
      Callback().Call({js_matches_array, positions_array});
    }
  };

  auto query = string_conversion::string_from_js(info[0]);
  auto extra_word_characters = string_conversion::string_from_js(info[1]);
  auto max_count = number_conversion::number_from_js<uint32_t>(info[2]);
  auto range = RangeWrapper::range_from_js(info[3]);
  Function callback = info[4].As<Function>();

  if (query && extra_word_characters && max_count && range && callback) {
    Napi::Object js_buffer = info.This().As<Object>();

    auto worker = new FindWordsWithSubsequenceInRangeWorker(
      js_buffer,
      callback,
      *query,
      *extra_word_characters,
      *max_count,
      *range
    );

    {
      std::lock_guard<std::mutex> guard(outstanding_workers_mutex);
      this->outstanding_workers.insert(worker);
    }
    worker->Queue();
  } else {
    Napi::Error::New(Env(), "Invalid arguments").ThrowAsJavaScriptException();
  }
}

Napi::Value TextBufferWrapper::is_modified(const CallbackInfo &info) {
  auto &text_buffer = this->text_buffer;
  return Boolean::New(info.Env(), text_buffer.is_modified());
}

static const int INVALID_ENCODING = -1;

namespace textbuffer {
  struct Error {
    int number;
    const char *syscall;
  };

  static Napi::Value error_to_js(Env env, Error error, string encoding_name, string file_name) {
    if (error.number == INVALID_ENCODING) {
      return Napi::Error::New(env, ("Invalid encoding name: " + encoding_name).c_str()).Value();
    } else {
      return Napi::Value(env, JsValueFromV8LocalValue(node::ErrnoException(
        v8::Isolate::GetCurrent(), error.number, error.syscall, error.syscall, file_name.c_str()
      )));
    }
  }
}

template <typename Callback>
static u16string load_file(
  const string &file_name,
  const string &encoding_name,
  optional<textbuffer::Error> *error,
  const Callback &callback
) {
  auto conversion = transcoding_from(encoding_name.c_str());
  if (!conversion) {
    *error = textbuffer::Error{INVALID_ENCODING, nullptr};
    return u"";
  }

  FILE *file = open_file(file_name, "rb");
  if (!file) {
    *error = textbuffer::Error{errno, "open"};
    return u"";
  }

  size_t file_size = get_file_size(file);
  if (file_size == static_cast<size_t>(-1)) {
    *error = textbuffer::Error{errno, "stat"};
    return u"";
  }

  u16string loaded_string;
  vector<char> input_buffer(CHUNK_SIZE);
  loaded_string.reserve(file_size);
  if (!conversion->decode(
    loaded_string,
    file,
    input_buffer,
    [&callback, file_size](size_t bytes_read) {
      size_t percent_done = file_size > 0 ? 100 * bytes_read / file_size : 100;
      callback(percent_done);
    }
  )) {
    *error = textbuffer::Error{errno, "read"};
  }

  fclose(file);
  return loaded_string;
}

class Loader {
  FunctionReference progress_callback;
  TextBuffer *buffer;
  TextBuffer::Snapshot *snapshot;
  string file_name;
  string encoding_name;
  optional<Text> loaded_text;
  optional<textbuffer::Error> error;
  Patch patch;
  bool force;
  bool compute_patch;

 public:
  bool cancelled;

  Loader(FunctionReference progress_callback,
         TextBuffer *buffer, TextBuffer::Snapshot *snapshot, string &&file_name,
         string &&encoding_name, bool force, bool compute_patch) :
    progress_callback{move(progress_callback)},
    buffer{buffer},
    snapshot{snapshot},
    file_name{move(file_name)},
    encoding_name{move(encoding_name)},
    force{force},
    compute_patch{compute_patch},
    cancelled{false} {
    }

  Loader(FunctionReference progress_callback,
         TextBuffer *buffer, TextBuffer::Snapshot *snapshot, Text &&text,
         bool force, bool compute_patch) :
    progress_callback{move(progress_callback)},
    buffer{buffer},
    snapshot{snapshot},
    loaded_text{move(text)},
    force{force},
    compute_patch{compute_patch},
    cancelled{false} {
    }

  template <typename Function>
  void Execute(const Function &callback) {
    if (!loaded_text) loaded_text = Text{load_file(file_name, encoding_name, &error, callback)};
    if (!error && compute_patch) patch = text_diff(snapshot->base_text(), *loaded_text);
  }

  pair<Value, Value> Finish(Napi::Env env) {
    if (error) {
      delete snapshot;
      snapshot = nullptr;
      return {textbuffer::error_to_js(env, *error, encoding_name, file_name), env.Undefined()};
    }

    if (cancelled || (!force && buffer->is_modified())) {
      delete snapshot;
      snapshot = nullptr;
      return {env.Null(), env.Null()};
    }

    Patch inverted_changes = buffer->get_inverted_changes(snapshot);
    delete snapshot;
    snapshot = nullptr;

    if (compute_patch && inverted_changes.get_change_count() > 0) {
      inverted_changes.combine(patch);
      patch = move(inverted_changes);
    }

    bool has_changed;
    Value patch_wrapper;
    if (compute_patch) {
      has_changed = !compute_patch || patch.get_change_count() > 0;
      patch_wrapper = PatchWrapper::from_patch(env, move(patch));
    } else {
      has_changed = true;
      patch_wrapper = env.Null();
    }

    if (!progress_callback.IsEmpty()) {
      Napi::Value progress_result = progress_callback.Call({Number::New(env, 100), patch_wrapper});
      if (!progress_result.IsEmpty() && progress_result.IsBoolean() && !progress_result.As<Boolean>().Value()) {
        return {env.Null(), env.Null()};
      }
    }

    if (has_changed) {
      buffer->reset(move(*loaded_text));
    } else {
      buffer->flush_changes();
    }

    return {env.Null(), patch_wrapper};
  }

  void CallProgressCallback(size_t percent_done) {
    if (!cancelled && !progress_callback.IsEmpty()) {
      auto env = progress_callback.Env();
      Napi::Value progress_result = progress_callback.Call({Number::New(env, static_cast<uint32_t>(percent_done))});
      if (!progress_result.IsEmpty() && progress_result.IsBoolean() && !progress_result.As<Boolean>().Value()) cancelled = true;
    }
  }
};

class LoadWorker : public AsyncProgressWorker<uint32_t> {
  Loader loader;

 public:
  LoadWorker(Function &completion_callback, FunctionReference progress_callback,
             TextBuffer *buffer, TextBuffer::Snapshot *snapshot, string &&file_name,
             string &&encoding_name, bool force, bool compute_patch) :
    AsyncProgressWorker(completion_callback, "TextBuffer.load"),
    loader(move(progress_callback), buffer, snapshot, move(file_name), move(encoding_name), force, compute_patch) {}

  LoadWorker(Function &completion_callback, FunctionReference progress_callback,
             TextBuffer *buffer, TextBuffer::Snapshot *snapshot, Text &&text,
             bool force, bool compute_patch) :
    AsyncProgressWorker(completion_callback, "TextBuffer.load"),
    loader(move(progress_callback), buffer, snapshot, move(text), force, compute_patch) {}

  void Execute(const ExecutionProgress &progress) override {
    loader.Execute([&progress](uint32_t percent_done) {
      progress.Send(&percent_done, 1);
    });
  }

  void OnProgress(const uint32_t *percent_done, size_t count) override {
    if (percent_done) {
      loader.CallProgressCallback(*percent_done);
    }
  }

  void OnOK() override {
    auto results = loader.Finish(Env());
    Callback().Call({results.first, results.second});
  }
};

Napi::Value TextBufferWrapper::load_sync(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;

  if (text_buffer.is_modified()) {
    return env.Undefined();
  }

  if (!info[0].IsString()) {
    return env.Undefined();
  }

  String js_file_path = info[0].As<String>();
  string file_path =js_file_path.Utf8Value();

  if (!info[1].IsString()) {
    return env.Undefined();
  }

  String js_encoding_name = info[1].As<String>();
  string encoding_name = js_encoding_name.Utf8Value();

  FunctionReference progress_callback;
  if (info[2].IsFunction()) {
    progress_callback = Persistent(info[2].As<Function>());
  }

  Loader worker(
    move(progress_callback),
    &text_buffer,
    text_buffer.create_snapshot(),
    move(file_path),
    move(encoding_name),
    false,
    true
  );

  worker.Execute([&worker](size_t percent_done) {
    worker.CallProgressCallback(percent_done);
  });

  auto results = worker.Finish(env);
  if (results.first.IsNull()) {
    return results.second;
  } else {
    results.first.As<Error>().ThrowAsJavaScriptException();
  }
  return env.Undefined();
}

void TextBufferWrapper::load(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;

  bool force = false;
  if (info[2].IsBoolean() && info[2].As<Boolean>()) force = true;

  bool compute_patch = true;
  if (info[3].IsBoolean() && !info[3].As<Boolean>()) compute_patch = false;

  if (!force && text_buffer.is_modified()) {
    auto callback = info[0].As<Function>();
    callback.Call({env.Null(), env.Null()});
    return;
  }

  Function completion_callback = info[0].As<Function>();

  FunctionReference progress_callback;
  if (info[1].IsFunction()) {
    progress_callback = Persistent(info[1].As<Function>());
  }

  LoadWorker *worker;
  if (info[4].IsString()) {
    String js_file_path =info[4].As<String>();
    string file_path = js_file_path.Utf8Value();

    if (!info[5].IsString()) return;
    String js_encoding_name = info[5].As<String>();
    string encoding_name = js_encoding_name.Utf8Value();

    worker = new LoadWorker(
      completion_callback,
      move(progress_callback),
      &text_buffer,
      text_buffer.create_snapshot(),
      move(file_path),
      move(encoding_name),
      force,
      compute_patch
    );
  } else {
    auto text_writer = TextWriter::Unwrap(info[4].As<Object>());
    worker = new LoadWorker(
      completion_callback,
      move(progress_callback),
      &text_buffer,
      text_buffer.create_snapshot(),
      text_writer->get_text(),
      force,
      compute_patch
    );
  }

  worker->Queue();
}

class BaseTextComparisonWorker : public AsyncWorker {
  TextBuffer::Snapshot *snapshot;
  string file_name;
  string encoding_name;
  optional<textbuffer::Error> error;
  bool result;

 public:
  BaseTextComparisonWorker(Function &completion_callback, TextBuffer::Snapshot *snapshot,
                       string &&file_name, string &&encoding_name) :
    AsyncWorker(completion_callback, "TextBuffer.baseTextMatchesFile"),
    snapshot{snapshot},
    file_name{move(file_name)},
    encoding_name{move(encoding_name)},
    result{false} {}

  void Execute() override {
    u16string file_contents = load_file(file_name, encoding_name, &error, [](size_t progress) {});
    result = std::equal(file_contents.begin(), file_contents.end(), snapshot->base_text().begin());
  }

  void OnOK() override {
    auto env = Env();
    delete snapshot;
    snapshot = nullptr;
    if (error) {
      Callback().Call({error_to_js(env, *error, encoding_name, file_name)});
    } else {
      Callback().Call({env.Null(), Boolean::New(env, result)});
    }
  }
};

void TextBufferWrapper::base_text_matches_file(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;

  if (info[1].IsString()) {
    Function completion_callback = info[0].As<Function>();
    String js_file_path = info[1].As<String>();
    string file_path = js_file_path.Utf8Value();

    if (!info[2].IsString()) return;
    String js_encoding_name = info[2].As<String>();
    string encoding_name = js_encoding_name.Utf8Value();

    (new BaseTextComparisonWorker(
      completion_callback,
      text_buffer.create_snapshot(),
      move(file_path),
      move(encoding_name)
    ))->Queue();
  } else {
    auto file_contents = TextWriter::Unwrap(info[1].As<Object>())->get_text();
    bool result = std::equal(file_contents.begin(), file_contents.end(), text_buffer.base_text().begin());
    auto callback = info[0].As<Function>();
    callback.Call({env.Null(), Boolean::New(env, result)});
  }
}

class SaveWorker : public AsyncWorker {
  TextBuffer::Snapshot *snapshot;
  string file_name;
  string encoding_name;
  optional<textbuffer::Error> error;

 public:
  SaveWorker(Function &completion_callback, TextBuffer::Snapshot *snapshot,
             string &&file_name, string &&encoding_name) :
    AsyncWorker(completion_callback, "TextBuffer.save"),
    snapshot{snapshot},
    file_name{file_name},
    encoding_name(encoding_name) {}

  void Execute() override {
    auto conversion = transcoding_to(encoding_name.c_str());
    if (!conversion) {
      error = textbuffer::Error{INVALID_ENCODING, nullptr};
      return;
    }

    FILE *file = open_file(file_name, "wb+");
    if (!file) {
      error = textbuffer::Error{errno, "open"};
      return;
    }

    vector<char> output_buffer(CHUNK_SIZE);
    for (TextSlice &chunk : snapshot->chunks()) {
      if (!conversion->encode(
        chunk.text->content,
        chunk.start_offset(),
        chunk.end_offset(),
        file,
        output_buffer
      )) {
        error = textbuffer::Error{errno, "write"};
        fclose(file);
        return;
      }
    }

    fclose(file);
  }

  Value Finish() {
    auto env = Env();
    if (error) {
      delete snapshot;
      snapshot = nullptr;
      return error_to_js(env, *error, encoding_name, file_name);
    } else {
      snapshot->flush_preceding_changes();
      delete snapshot;
      snapshot = nullptr;
      return env.Null();
    }
  }

  void OnOK() override {
    Callback().Call({Finish()});
  }
};

void TextBufferWrapper::save(const CallbackInfo &info) {
  auto &text_buffer = this->text_buffer;

  if (!info[0].IsString()) return;
  String js_file_path = info[0].As<String>();
  string file_path = js_file_path.Utf8Value();

  if (!info[1].IsString()) return;
  String js_encoding_name = info[1].As<String>();
  string encoding_name = js_encoding_name.Utf8Value();

  Function completion_callback = info[2].As<Function>();
  (new SaveWorker(
    completion_callback,
    text_buffer.create_snapshot(),
    move(file_path),
    move(encoding_name)
  ))->Queue();
}

Napi::Value TextBufferWrapper::serialize_changes(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;

  static vector<uint8_t> output;
  output.clear();
  Serializer serializer(output);
  text_buffer.serialize_changes(serializer);
  auto result = Buffer<char>::Copy(env, reinterpret_cast<char *>(output.data()), output.size());
  return result;
}

void TextBufferWrapper::deserialize_changes(const CallbackInfo &info) {
  auto &text_buffer = this->text_buffer;
  if (info[0].IsTypedArray()) {
    Uint8Array array = info[0].As<Uint8Array>();
    uint8_t *data = array.Data();
    static vector<uint8_t> input;
    input.assign(data, data + array.ByteLength());
    Deserializer deserializer(input);
    text_buffer.deserialize_changes(deserializer);
  }
}

void TextBufferWrapper::reset(const CallbackInfo &info) {
  auto &text_buffer = this->text_buffer;
  auto text = string_conversion::string_from_js(info[0]);
  if (text) {
    text_buffer.reset(move(*text));
  }
}

Napi::Value TextBufferWrapper::base_text_digest(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;
  std::stringstream stream;
  stream <<
    std::setfill('0') <<
    std::setw(2 * sizeof(size_t)) <<
    std::hex <<
    text_buffer.base_text().digest();
  String result = String::New(env, stream.str());
  return result;
}

Napi::Value TextBufferWrapper::get_snapshot(const CallbackInfo &info) {
  auto env = info.Env();
  Napi::HandleScope scope(env);
  auto &text_buffer = this->text_buffer;
  auto snapshot = text_buffer.create_snapshot();
  return TextBufferSnapshotWrapper::new_instance(env, info.This().As<Object>(), snapshot);
}

Napi::Value TextBufferWrapper::dot_graph(const CallbackInfo &info) {
  auto env = info.Env();
  auto &text_buffer = this->text_buffer;
  return String::New(env, text_buffer.get_dot_graph());
}

void TextBufferWrapper::cancel_queued_workers() {
  std::lock_guard<std::mutex> guard(outstanding_workers_mutex);
  auto env = Env();

  for (auto worker: outstanding_workers) {
    worker->Cancel();
    if (env.IsExceptionPending()) {
      auto e = env.GetAndClearPendingException();
    }
  }
}
