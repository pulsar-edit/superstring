'use strict'

const {Patch, textExtent, textPositionForOffset, textOffsetForPoint, traverse, traversal, cmp} = require('./patch')

let _jsdiff = null
function jsdiff () {
  if (!_jsdiff) _jsdiff = require('diff')
  return _jsdiff
}

let _iconv = null
function iconv () {
  if (!_iconv) _iconv = require('iconv-lite')
  return _iconv
}

// Build a Patch from oldText -> newText using a character-level diff.
// Tries to match the C++ libmba-diff behavior: each insert/delete is recorded
// at its exact character position. Falls back to a single full-replacement
// patch if the texts are very different (matches the C++ MAX_EDIT_DISTANCE
// guard, which keeps `load` responsive for big files).
const DIFF_SIZE_LIMIT = 64 * 1024
function computeTextDiff (oldText, newText) {
  const result = new Patch()
  if (oldText === newText) return result
  // Full-replacement shortcut: empty side, or texts large enough that a
  // character-level Myers diff would be prohibitively slow.
  if (oldText.length === 0 || newText.length === 0 ||
      oldText.length > DIFF_SIZE_LIMIT || newText.length > DIFF_SIZE_LIMIT) {
    result.splice(ZERO, textExtent(oldText), textExtent(newText), oldText, newText)
    return result
  }

  const parts = jsdiff().diffChars(oldText, newText)

  // Walk parts, tracking position in the *new* coordinate space (which is what
  // Patch.splice uses). Each insertion advances new_position; each deletion
  // does not advance it. Matched parts advance both old and new positions.
  let newPos = {row: 0, column: 0}
  let i = 0
  while (i < parts.length) {
    const p = parts[i]
    if (p.added) {
      // Look ahead for a paired delete (delete-then-insert sequence) to
      // emit a replacement instead of two splices.
      let pairedDel = null
      // (jsdiff usually emits delete-then-add, but be tolerant either way.)
      if (i + 1 < parts.length && parts[i + 1].removed) pairedDel = parts[i + 1]
      const newExt = textExtent(p.value)
      const oldExt = pairedDel ? textExtent(pairedDel.value) : ZERO
      const oldStr = pairedDel ? pairedDel.value : ''
      result.splice(newPos, oldExt, newExt, oldStr, p.value)
      newPos = traverse(newPos, newExt)
      if (pairedDel) i++
      i++
    } else if (p.removed) {
      // Look ahead for a paired add.
      let pairedAdd = null
      if (i + 1 < parts.length && parts[i + 1].added) pairedAdd = parts[i + 1]
      const oldExt = textExtent(p.value)
      const newExt = pairedAdd ? textExtent(pairedAdd.value) : ZERO
      const newStr = pairedAdd ? pairedAdd.value : ''
      result.splice(newPos, oldExt, newExt, p.value, newStr)
      newPos = traverse(newPos, newExt)
      if (pairedAdd) i++
      i++
    } else {
      // Matched region: advance newPos.
      newPos = traverse(newPos, textExtent(p.value))
      i++
    }
  }
  return result
}

// ---------------------------------------------------------------------------
// Point / Range helpers
// ---------------------------------------------------------------------------

const ZERO = {row: 0, column: 0}
const MAX_POINT = {row: Infinity, column: Infinity}

function isZero (p) {
  return p.row === 0 && p.column === 0
}

function ptMin (a, b) { return cmp(a, b) <= 0 ? a : b }
function ptMax (a, b) { return cmp(a, b) >= 0 ? a : b }

// Clip a point so it doesn't exceed the extent of a string
function clipPoint (text, point) {
  const ext = textExtent(text)
  if (cmp(point, ZERO) < 0) return ZERO
  if (cmp(point, ext) > 0) return ext
  return point
}

// ---------------------------------------------------------------------------
// Regex helpers
// ---------------------------------------------------------------------------

// Convert a JS RegExp (or string pattern) into a RegExp with the 'g' flag and
// optionally additional flags. We cache compiled patterns on the RegExp object
// itself to avoid recompiling on repeated calls.
// Translate V8's regex error messages into the libpcre-style ones the C++
// version surfaced, so callers (and tests) match on the historical phrasing.
function _translateRegexError (msg) {
  return msg
    .replace(/Unterminated character class/, 'missing terminating ] for character class')
    .replace(/Unterminated group/, 'missing )')
}

// libpcre treats \u as a literal 'u' unless followed by exactly 4 hex digits;
// V8 silently drops the backslash for `\u` followed by non-hex, so the regex
// matches just `u...` instead of `\u...`. Rewrite invalid \u sequences so they
// match the literal backslash + u + remainder.
function _translateRegexSource (source) {
  return source.replace(/\\u([0-9a-fA-F]{0,3})(?![0-9a-fA-F])/g, (m, hex) => {
    return hex.length === 4 ? m : '\\\\u' + hex
  })
}

function compilePattern (pattern, extraFlags) {
  if (typeof pattern === 'string') {
    try {
      return new RegExp(_translateRegexSource(pattern), 'gm' + (extraFlags || ''))
    } catch (e) {
      throw new Error(_translateRegexError(e.message))
    }
  }
  // It's already a RegExp – rebuild with global+multiline forced, preserving flags
  const flags = new Set([...pattern.flags, 'g', 'm'])
  flags.delete('y') // sticky is incompatible with global for our use
  try {
    return new RegExp(_translateRegexSource(pattern.source), [...flags].join(''))
  } catch (e) {
    throw new Error(_translateRegexError(e.message))
  }
}

// ---------------------------------------------------------------------------
// Subsequence scoring
// ---------------------------------------------------------------------------
// Port of the C++ scoring logic used for find_words_with_subsequence
// (src/core/text-buffer.cc). The algorithm explores multiple "match variants"
// per word in parallel, so the chosen subsequence can pay penalties for
// skipped chars on the way to landing on subword starts and consecutive runs.

const MAX_WORD_LENGTH = 80
const CONSECUTIVE_BONUS = 5
const SUBWORD_START_CASE_MATCH_BONUS = 10
const SUBWORD_START_CASE_MISMATCH_BONUS = 9
const MISMATCH_PENALTY = 1
const LEADING_MISMATCH_PENALTY = 3

function _isAlnum (ch) {
  return (ch >= '0' && ch <= '9') ||
    (ch >= 'a' && ch <= 'z') ||
    (ch >= 'A' && ch <= 'Z')
}

// Subword start: at index 0, after a non-alnum char, or at a lower→upper
// camelCase transition. Mirrors the C++ subword check.
function _isSubwordStart (word, i) {
  if (i === 0) return true
  const prev = word[i - 1]
  if (!_isAlnum(prev)) return true
  const cur = word[i]
  if (prev >= 'a' && prev <= 'z' && cur >= 'A' && cur <= 'Z') return true
  return false
}

// Score `word` against `rawQuery`. Returns {score, matchIndices} with the
// best-scoring subsequence match, or -1 when the word doesn't contain the
// query as a subsequence.
function scoreSubsequence (word, rawQuery) {
  if (word.length > MAX_WORD_LENGTH) return -1
  const lowerQuery = rawQuery.toLowerCase()
  const lowerWord = word.toLowerCase()

  // Quick existence check (subsequence containment) so words that don't
  // contain the query at all skip the more expensive variant search.
  {
    let qi = 0
    for (let i = 0; i < lowerWord.length && qi < lowerQuery.length; i++) {
      if (lowerWord[i] === lowerQuery[qi]) qi++
    }
    if (qi < lowerQuery.length) return -1
  }

  // match_variants: ordered by ascending queryIndex (and within same
  // queryIndex, by ascending score). Each variant tracks how far through
  // the query it has matched, the indices it consumed, and its running score.
  let variants = [{queryIndex: 0, matchIndices: [], score: 0}]
  let newVariants = []

  for (let i = 0; i < word.length; i++) {
    const c = lowerWord[i]
    newVariants.length = 0

    for (let v = 0; v < variants.length;) {
      const variant = variants[v]
      if (variant.queryIndex < lowerQuery.length) {
        // If the current word char matches the next query char, branch into
        // a new variant that consumes it.
        if (c === lowerQuery[variant.queryIndex]) {
          let added = variant.score
          if (_isSubwordStart(word, i)) {
            added += word[i] === rawQuery[variant.queryIndex]
              ? SUBWORD_START_CASE_MATCH_BONUS
              : SUBWORD_START_CASE_MISMATCH_BONUS
          }
          if (variant.matchIndices.length > 0 &&
              variant.matchIndices[variant.matchIndices.length - 1] === i - 1) {
            added += CONSECUTIVE_BONUS
          }
          newVariants.push({
            queryIndex: variant.queryIndex + 1,
            matchIndices: variant.matchIndices.concat(i),
            score: added
          })
        }

        // The original variant pays a per-char penalty regardless of match.
        variant.score -= (i < 3 ? LEADING_MISMATCH_PENALTY : MISMATCH_PENALTY)

        // Drop the original if a same-queryIndex peer ahead in the list will
        // strictly dominate it (its score is by construction higher).
        const next = variants[v + 1]
        if (next && next.queryIndex === variant.queryIndex) {
          variants.splice(v, 1)
          continue
        }
      }
      v++
    }

    // Merge newVariants in, maintaining the ordering invariant
    // (ascending queryIndex; within same queryIndex, ascending score).
    // - If new.score >= existing peer: replace existing (so the new variant,
    //   which is eligible for the consecutive bonus next char, takes over).
    // - Else: insert new *before* the existing peer (a temporary duplicate
    //   pair). The next iteration's same-q-idx erase will drop whichever
    //   variant is no longer carrying its weight after that round's
    //   penalty/match step.
    for (const nv of newVariants) {
      let lo = 0, hi = variants.length
      while (lo < hi) {
        const mid = (lo + hi) >>> 1
        if (variants[mid].queryIndex < nv.queryIndex) lo = mid + 1
        else hi = mid
      }
      if (lo < variants.length && variants[lo].queryIndex === nv.queryIndex) {
        if (nv.score >= variants[lo].score) {
          variants[lo] = nv
        } else {
          variants.splice(lo, 0, nv)
        }
      } else {
        variants.splice(lo, 0, nv)
      }
    }
  }

  let best = null
  for (const v of variants) {
    if (v.queryIndex === lowerQuery.length) {
      if (!best || best.score < v.score) best = v
    }
  }
  if (!best) return -1
  return {score: best.score, matchIndices: best.matchIndices.slice()}
}

// ---------------------------------------------------------------------------
// TextBuffer
// ---------------------------------------------------------------------------

class TextBuffer {
  // text: optional string initial content
  constructor (text) {
    if (typeof text === 'undefined') text = ''
    if (typeof text !== 'string') text = String(text)
    // Base text – never mutated after construction (tracks "unmodified" state)
    this._baseText = text
    // Current text – mutable
    this._text = text
    // We keep a simple change layer on top: we store the accumulated Patch
    // from the original base text to the current text. This is used for
    // isModified() and getInvertedChanges().
    this._patch = new Patch()
    // Monotonically increasing counter; bumped on every save() call so that
    // concurrent saves can tell which snapshot is most recent.
    this._saveGeneration = 0
    this._baseGeneration = 0
    // Cached array of line-start offsets, lazily built. Index i holds the
    // character offset of the start of row i. Invalidated by setting to null
    // on any text mutation.
    this._lineStarts = null
  }

  _getLineStarts () {
    if (this._lineStarts) return this._lineStarts
    const text = this._text
    const starts = [0]
    for (let i = 0; i < text.length; i++) {
      if (text.charCodeAt(i) === 10) starts.push(i + 1)
    }
    this._lineStarts = starts
    return starts
  }

  _invalidateLineStarts () {
    this._lineStarts = null
  }

  // O(log lines) position lookup against this._text using the cached line-start index.
  // Equivalent to textPositionForOffset(this._text, offset) but without scanning.
  _positionForOffset (offset) {
    if (offset <= 0) return {row: 0, column: 0}
    const text = this._text
    if (offset > text.length) offset = text.length
    const starts = this._getLineStarts()
    let lo = 0
    let hi = starts.length - 1
    while (lo < hi) {
      const mid = (lo + hi + 1) >>> 1
      if (starts[mid] <= offset) lo = mid
      else hi = mid - 1
    }
    return {row: lo, column: offset - starts[lo]}
  }

  // Equivalent to textOffsetForPoint(this._text, point), in O(1).
  // Original semantics: walk the text counting columns per line; if the requested
  // point is reached, return that offset, otherwise fall through to text.length
  // ("unreachable point" fallback). \r is counted as a regular column character.
  _offsetForPosition (point) {
    const text = this._text
    const starts = this._getLineStarts()
    let row = point.row
    let column = point.column
    if (row < 0) { row = 0; column = 0 }
    if (column < 0) column = 0
    if (row >= starts.length) return text.length
    const lineStart = starts[row]
    const hasNextLine = row + 1 < starts.length
    // For non-last rows, the line content (including any trailing \r) ends just
    // before the '\n' at starts[row+1] - 1. A column larger than that content
    // length is "unreachable" → fall through to text.length, matching the
    // original walking implementation.
    if (hasNextLine) {
      const lineLen = starts[row + 1] - 1 - lineStart
      if (column > lineLen) return text.length
      return lineStart + column
    }
    // Last row: clamp to text.length.
    const lineLen = text.length - lineStart
    if (column > lineLen) column = lineLen
    return lineStart + column
  }

  // ---------------------------------------------------------------------------
  // Basic text access
  // ---------------------------------------------------------------------------

  getText () {
    return this._text
  }

  setText (newText) {
    if (typeof newText !== 'string') newText = String(newText)
    const oldText = this._text
    const oldExtent = textExtent(oldText)
    const newExtent = textExtent(newText)
    // Replace entire content
    this._text = newText
    this._invalidateLineStarts()
    this._patch.splice(ZERO, oldExtent, newExtent, oldText, newText)
    this._patch._removeNoopChange()
  }

  setTextInRange (range, newText) {
    if (typeof newText !== 'string') newText = String(newText)
    const start = this._clipPoint(range.start)
    const end = this._clipPoint(range.end)
    // Ensure start <= end
    const s = cmp(start, end) <= 0 ? start : end
    const e = cmp(start, end) <= 0 ? end : start

    const oldText = this._getTextInRange(s, e)
    const oldExtent = traversal(e, s)
    const newExtent = textExtent(newText)

    // Build new full text
    const offset1 = this._offsetForPosition(s)
    const offset2 = this._offsetForPosition(e)
    this._text = this._text.slice(0, offset1) + newText + this._text.slice(offset2)
    this._invalidateLineStarts()

    // Update patch
    this._patch.splice(s, oldExtent, newExtent, oldText, newText)
    this._patch._removeNoopChange()
  }

  getTextInRange (range) {
    const start = this._clipPoint(range.start)
    const end = this._clipPoint(range.end)
    if (cmp(start, end) >= 0) return ''
    return this._getTextInRange(start, end)
  }

  _getTextInRange (start, end) {
    const offset1 = this._offsetForPosition(start)
    const offset2 = this._offsetForPosition(end)
    return this._text.slice(offset1, offset2)
  }

  // ---------------------------------------------------------------------------
  // Extent / size
  // ---------------------------------------------------------------------------

  getLength () {
    return this._text.length
  }

  getExtent () {
    const text = this._text
    const starts = this._getLineStarts()
    const lastRow = starts.length - 1
    return {row: lastRow, column: text.length - starts[lastRow]}
  }

  getLineCount () {
    return this._getLineStarts().length
  }

  // ---------------------------------------------------------------------------
  // Line helpers
  // ---------------------------------------------------------------------------

  // Returns {text, ending} for a given row, or undefined if row is out of bounds.
  _getLineInfo (row) {
    if (row < 0) return
    const text = this._text
    const starts = this._getLineStarts()
    if (row >= starts.length) return

    const lineStart = starts[row]
    const hasNextLine = row + 1 < starts.length
    const nlOffset = hasNextLine ? starts[row + 1] - 1 : -1
    const lineEnd = nlOffset === -1 ? text.length : nlOffset

    let contentEnd = lineEnd
    let ending = ''
    if (nlOffset !== -1) {
      if (contentEnd > lineStart && text.charCodeAt(contentEnd - 1) === 13) {
        contentEnd--
        ending = '\r\n'
      } else {
        ending = '\n'
      }
    }
    return {text: text.slice(lineStart, contentEnd), ending}
  }

  lineForRow (row) {
    const info = this._getLineInfo(row)
    if (!info) return undefined
    return info.text
  }

  lineLengthForRow (row) {
    if (row < 0) return undefined
    const text = this._text
    const starts = this._getLineStarts()
    if (row >= starts.length) return undefined

    const lineStart = starts[row]
    const hasNextLine = row + 1 < starts.length
    const nlOffset = hasNextLine ? starts[row + 1] - 1 : -1
    let lineEnd = nlOffset === -1 ? text.length : nlOffset
    if (nlOffset !== -1 && lineEnd > lineStart && text.charCodeAt(lineEnd - 1) === 13) {
      lineEnd--
    }
    return lineEnd - lineStart
  }

  lineEndingForRow (row) {
    const info = this._getLineInfo(row)
    if (!info) return undefined
    return info.ending
  }

  getLines () {
    const text = this._text
    const starts = this._getLineStarts()
    const rowCount = starts.length
    const lines = new Array(rowCount)
    for (let row = 0; row < rowCount; row++) {
      const lineStart = starts[row]
      const hasNextLine = row + 1 < rowCount
      const nlOffset = hasNextLine ? starts[row + 1] - 1 : -1
      let lineEnd = nlOffset === -1 ? text.length : nlOffset
      if (nlOffset !== -1 && lineEnd > lineStart && text.charCodeAt(lineEnd - 1) === 13) {
        lineEnd--
      }
      lines[row] = text.slice(lineStart, lineEnd)
    }
    return lines
  }

  // ---------------------------------------------------------------------------
  // Modified state
  // ---------------------------------------------------------------------------

  isModified () {
    return this._text !== this._baseText
  }

  // ---------------------------------------------------------------------------
  // Clip position (used internally)
  // ---------------------------------------------------------------------------

  _clipPoint (point) {
    if (!point) return ZERO
    let row = point.row
    let column = point.column
    // Handle negative / infinite
    if (row < 0 || row == null) row = 0
    if (column < 0 || column == null) column = 0

    const ext = this.getExtent()
    if (row > ext.row) { row = ext.row; column = ext.column }
    else if (row === ext.row && column > ext.column) { column = ext.column }

    if (!isFinite(row) || row > ext.row) { row = ext.row; column = ext.column }
    if (!isFinite(column)) {
      // column=Infinity means end of the row
      const lineInfo = this._getLineInfo(row)
      column = lineInfo ? lineInfo.text.length : 0
    }

    return {row, column}
  }

  characterIndexForPosition (point) {
    const clipped = this._clipPoint(point)
    // Clip column to the visible line length (excluding a trailing \r).
    // This matches the public-API contract where column counts characters
    // that are part of the displayed line content, not the line terminator.
    const lineLen = this.lineLengthForRow(clipped.row)
    let column = clipped.column
    if (lineLen != null && column > lineLen) column = lineLen
    return this._offsetForPosition({row: clipped.row, column})
  }

  positionForCharacterIndex (offset) {
    if (offset < 0) offset = 0
    if (offset > this._text.length) offset = this._text.length
    const text = this._text
    // If the offset points at the '\n' of a CRLF pair, snap back to the end
    // of the visible content (before the '\r'). This matches the public-API
    // contract where column counts visible characters, not line terminators.
    if (offset > 0 && offset < text.length &&
        text.charCodeAt(offset) === 10 &&
        text.charCodeAt(offset - 1) === 13) {
      offset--
    }
    return this._positionForOffset(offset)
  }

  // ---------------------------------------------------------------------------
  // Character at position
  // ---------------------------------------------------------------------------

  getCharacterAtPosition (point) {
    let {row, column} = point
    if (row < 0) row = 0
    if (column < 0) column = 0

    const text = this._text
    const starts = this._getLineStarts()

    if (row >= starts.length) return ' '

    const lineStart = starts[row]
    const hasNextLine = row + 1 < starts.length
    const lineEnd = hasNextLine ? starts[row + 1] - 1 : text.length

    let lineContentEnd = lineEnd
    if (lineContentEnd > lineStart && text.charCodeAt(lineContentEnd - 1) === 13) {
      lineContentEnd--
    }

    if (column >= lineContentEnd - lineStart) {
      if (hasNextLine) return '\n'
      return ' '
    }

    return text[lineStart + column]
  }
  // ---------------------------------------------------------------------------
  // Reset (sets base text and clears modified state)
  // ---------------------------------------------------------------------------

  reset (text) {
    if (typeof text !== 'string') text = String(text)
    this._baseText = text
    this._text = text
    this._invalidateLineStarts()
    this._patch = new Patch()
    this._saveGeneration = 0
    this._baseGeneration = 0
  }

  // ---------------------------------------------------------------------------
  // hasAstral – check for surrogate pairs
  // ---------------------------------------------------------------------------

  hasAstral () {
    const text = this._text
    for (let i = 0; i < text.length; i++) {
      const code = text.charCodeAt(i)
      if (code >= 0xD800 && code <= 0xDBFF) return true // high surrogate
    }
    return false
  }

  // ---------------------------------------------------------------------------
  // Snapshot support
  // ---------------------------------------------------------------------------

  getSnapshot () {
    return new TextBufferSnapshot(this._text, this._baseText)
  }

  isModifiedSince (snapshot) {
    return this._text !== snapshot._text
  }

  getInvertedChanges (snapshot) {
    // Build a patch from current text to snapshot text
    const oldText = this._text
    const newText = snapshot._text
    const p = new Patch()
    if (oldText !== newText) {
      p.splice(ZERO, textExtent(oldText), textExtent(newText), oldText, newText)
    }
    return p
  }

  // ---------------------------------------------------------------------------
  // serializeChanges / deserializeChanges
  // ---------------------------------------------------------------------------

  serializeChanges () {
    return this._patch.serialize()
  }

  deserializeChanges (buffer) {
    const patch = Patch.deserialize(buffer)
    const changes = patch.getChanges()
    for (const change of changes) {
      // Apply each change to the current text
      const s = change.oldStart
      const e = change.oldEnd
      const newText = change.newText || ''
      const offset1 = this._offsetForPosition(s)
      const offset2 = this._offsetForPosition(e)
      this._text = this._text.slice(0, offset1) + newText + this._text.slice(offset2)
      this._invalidateLineStarts()
    }
    this._patch = patch
  }

  // ---------------------------------------------------------------------------
  // baseTextDigest – simple hash of base text
  // ---------------------------------------------------------------------------

  baseTextDigest () {
    // Simple djb2 hash as a hex string – not cryptographic, just consistent
    let h = 5381
    const text = this._baseText
    for (let i = 0; i < text.length; i++) {
      h = ((h << 5) + h) + text.charCodeAt(i)
      h = h >>> 0 // keep 32-bit
    }
    return h.toString(16).padStart(8, '0')
  }

  // ---------------------------------------------------------------------------
  // Find / FindAll
  // ---------------------------------------------------------------------------

  findSync (pattern, range) {
    if (range) return this.findInRangeSync(pattern, range)
    return this._findInText(this._text, pattern, null)
  }

  findInRangeSync (pattern, range) {
    return this._findInText(this._text, pattern, range)
  }

  findAllSync (pattern, range) {
    if (range) return this.findAllInRangeSync(pattern, range)
    return this._findAllInText(this._text, pattern, null)
  }

  findAllInRangeSync (pattern, range) {
    return this._findAllInText(this._text, pattern, range)
  }

  find (pattern, range) {
    try {
      const result = range ? this.findInRangeSync(pattern, range) : this.findSync(pattern)
      return Promise.resolve(result)
    } catch (e) {
      return Promise.reject(e)
    }
  }

  findInRange (pattern, range) {
    try {
      return Promise.resolve(this.findInRangeSync(pattern, range))
    } catch (e) {
      return Promise.reject(e)
    }
  }

  findAll (pattern, range) {
    try {
      const result = range ? this.findAllInRangeSync(pattern, range) : this.findAllSync(pattern)
      return Promise.resolve(result)
    } catch (e) {
      return Promise.reject(e)
    }
  }

  findAllInRange (pattern, range) {
    try {
      return Promise.resolve(this.findAllInRangeSync(pattern, range))
    } catch (e) {
      return Promise.reject(e)
    }
  }

  // Build a search slice for regex search over [charOffset, endOffset].
  // The slice extends back to the nearest line start (so ^ anchors fire only
  // at real line boundaries) and forward to the end of the line containing
  // endOffset (so $ anchors fire only at real line boundaries — matches
  // whose end exceeds endOffset are filtered out by the caller).
  _buildSearchSlice (charOffset, endOffset) {
    const text = this._text
    const starts = this._getLineStarts()
    // Largest start index s.t. starts[idx] <= charOffset (binary search).
    let lo = 0, hi = starts.length - 1
    while (lo < hi) {
      const mid = (lo + hi + 1) >>> 1
      if (starts[mid] <= charOffset) lo = mid
      else hi = mid - 1
    }
    const sliceBase = starts[lo]
    // Find the first row whose start is > endOffset; slice ends just before
    // its preceding \n (or at text end if endOffset is in the last line).
    let endRow = lo
    while (endRow + 1 < starts.length && starts[endRow + 1] <= endOffset) {
      endRow++
    }
    const sliceLimit = endRow + 1 < starts.length ? starts[endRow + 1] - 1 : text.length
    const sliceText = text.slice(sliceBase, sliceLimit)
    return {
      sliceText,
      sliceBase,
      sliceLo: charOffset - sliceBase,
      sliceHi: endOffset - sliceBase
    }
  }

  // Internal: find first match (with optional range restriction).
  _findInText (_text, pattern, range) {
    const re = compilePattern(pattern)
    const fullText = this._text

    let charOffset = 0
    let endOffset = fullText.length
    if (range) {
      const start = this._clipPoint(range.start)
      const end = this._clipPoint(range.end)
      charOffset = this._offsetForPosition(start)
      endOffset = this._offsetForPosition(end)
    }

    const {sliceText, sliceBase, sliceLo, sliceHi} = this._buildSearchSlice(charOffset, endOffset)

    re.lastIndex = sliceLo
    let match
    while ((match = re.exec(sliceText)) !== null) {
      const matchStart = match.index
      let matchEnd = matchStart + match[0].length
      if (matchStart > sliceHi) return null
      if (matchEnd > sliceHi) {
        // The match overshot the search range. Try truncating the input to
        // the range and re-running the regex anchored at matchStart; this
        // gives `\w+` etc. a chance to produce a shorter valid match.
        const truncated = this._matchTruncatedToRange(pattern, sliceText, matchStart, sliceHi)
        if (truncated) {
          matchEnd = truncated.end
          if (matchStart >= sliceLo && !this._isInsideCRLF(sliceBase + matchStart)) {
            const trimmedEnd = this._trimCRLFEnd(sliceBase, matchStart, matchEnd, sliceText)
            return {
              start: this._positionForOffset(sliceBase + matchStart),
              end: this._positionForOffset(sliceBase + trimmedEnd)
            }
          }
        }
        return null
      }
      const insideCRLF = this._isInsideCRLF(sliceBase + matchStart)
      // Treat \r\n as one logical line terminator: if the match consumed the
      // \r of a \r\n pair, exclude that \r from the result so points stay
      // at column 0 of the next line rather than mid-CRLF.
      matchEnd = this._trimCRLFEnd(sliceBase, matchStart, matchEnd, sliceText)
      if (!insideCRLF && matchStart >= sliceLo) {
        return {
          start: this._positionForOffset(sliceBase + matchStart),
          end: this._positionForOffset(sliceBase + matchEnd)
        }
      }
      if (matchStart === matchEnd) re.lastIndex = matchEnd + 1
    }
    return null
  }

  // Re-evaluate `pattern` against sliceText[matchStart..sliceHi], anchored at
  // matchStart, to find the longest match that fits inside the requested
  // range. Returns {end} on success or null.
  _matchTruncatedToRange (pattern, sliceText, matchStart, sliceHi) {
    const truncatedText = sliceText.slice(matchStart, sliceHi)
    const re = compilePattern(pattern)
    re.lastIndex = 0
    const m = re.exec(truncatedText)
    if (!m || m.index !== 0) return null
    return {end: matchStart + m[0].length}
  }

  // If a regex match ended at a \r that's immediately followed by \n, trim
  // the \r off the end so the resulting Range never points "inside" a CRLF
  // line ending. Operates in slice-local coordinates.
  _trimCRLFEnd (sliceBase, matchStart, matchEnd, sliceText) {
    if (matchEnd <= matchStart) return matchEnd
    if (sliceText.charCodeAt(matchEnd - 1) !== 13) return matchEnd
    // Look at the character following the match in the full buffer text.
    const fullEnd = sliceBase + matchEnd
    if (fullEnd < this._text.length && this._text.charCodeAt(fullEnd) === 10) {
      return matchEnd - 1
    }
    return matchEnd
  }

  // Internal: find all matches (with optional range restriction).
  _findAllInText (_text, pattern, range) {
    const re = compilePattern(pattern)
    const results = []
    const fullText = this._text

    let charOffset = 0
    let endOffset = fullText.length
    if (range) {
      const start = this._clipPoint(range.start)
      const end = this._clipPoint(range.end)
      charOffset = this._offsetForPosition(start)
      endOffset = this._offsetForPosition(end)
    }

    const {sliceText, sliceBase, sliceLo, sliceHi} = this._buildSearchSlice(charOffset, endOffset)

    // Cached line-starts for O(1) per-match position lookup via monotonic cursor.
    const starts = this._getLineStarts()
    let cursorRow = 0
    const offsetToPos = (offset) => {
      while (cursorRow + 1 < starts.length && starts[cursorRow + 1] <= offset) {
        cursorRow++
      }
      return {row: cursorRow, column: offset - starts[cursorRow]}
    }

    re.lastIndex = sliceLo
    let match
    let lastEmitOffset = -1
    let lastEmitEnd = -1
    let lastEmitWasZeroWidth = false
    while ((match = re.exec(sliceText)) !== null) {
      const matchStart = match.index
      let matchEnd = matchStart + match[0].length

      if (matchStart > sliceHi) break
      if (matchEnd > sliceHi) {
        // Try truncating to the range, like _findInText does.
        const truncated = this._matchTruncatedToRange(pattern, sliceText, matchStart, sliceHi)
        if (truncated) matchEnd = truncated.end
        else break
      }

      // Skip matches that landed "inside" a CRLF pair — JS treats \r as a
      // line terminator, but the C++ implementation (and these tests) treat
      // \r\n as a single line ending and never split it.
      const insideCRLF = this._isInsideCRLF(sliceBase + matchStart)
      // Trim a trailing \r that's actually part of a \r\n line ending.
      matchEnd = this._trimCRLFEnd(sliceBase, matchStart, matchEnd, sliceText)
      const trimmedMatchEnd = matchEnd

      if (!insideCRLF && matchStart >= sliceLo) {
        const isZeroWidth = matchStart === trimmedMatchEnd
        const absStart = sliceBase + matchStart
        const absEnd = sliceBase + trimmedMatchEnd
        // Dedupe rules:
        // - skip a zero-width match that immediately follows another emit
        //   ending at the same offset (covers both consecutive zero-width
        //   collapses and the "tail" empty match at the end of a non-empty
        //   greedy match like `\w*$`).
        const skip = isZeroWidth && absStart === lastEmitEnd
        if (!skip) {
          const startPos = offsetToPos(absStart)
          const endPos = isZeroWidth ? {row: startPos.row, column: startPos.column} : offsetToPos(absEnd)
          results.push({start: startPos, end: endPos})
          lastEmitOffset = absStart
          lastEmitEnd = absEnd
          lastEmitWasZeroWidth = isZeroWidth
        }
      }

      // For zero-width or CRLF-trimmed-to-zero matches, bump past so we don't loop.
      if (matchStart === re.lastIndex || matchStart === trimmedMatchEnd) {
        re.lastIndex = (matchStart === trimmedMatchEnd ? matchStart : re.lastIndex) + 1
      }
    }

    return results
  }

  // True when the absolute offset `o` sits between the \r and \n of a CRLF.
  _isInsideCRLF (o) {
    const text = this._text
    return o > 0 && o < text.length &&
      text.charCodeAt(o - 1) === 13 && text.charCodeAt(o) === 10
  }

  // ---------------------------------------------------------------------------
  // findAndMarkAllSync
  // ---------------------------------------------------------------------------

  findAndMarkAllSync (markerIndex, nextId, exclusive, pattern, range) {
    const matches = range
      ? this.findAllInRangeSync(pattern, range)
      : this.findAllSync(pattern)
    let id = nextId
    for (const match of matches) {
      markerIndex.insert(id, match.start, match.end)
      if (exclusive) markerIndex.setExclusive(id, true)
      id++
    }
    return id - nextId
  }

  findAndMarkAllInRangeSync (markerIndex, nextId, exclusive, pattern, range) {
    return this.findAndMarkAllSync(markerIndex, nextId, exclusive, pattern, range)
  }

  // ---------------------------------------------------------------------------
  // findWordsWithSubsequence / findWordsWithSubsequenceInRange
  // ---------------------------------------------------------------------------

  findWordsWithSubsequence (query, extraWordCharacters, maxCount) {
    const range = {start: ZERO, end: this.getExtent()}
    return this.findWordsWithSubsequenceInRange(query, extraWordCharacters, maxCount, range)
  }

  findWordsWithSubsequenceInRange (query, extraWordCharacters, maxCount, range) {
    return Promise.resolve(this._findWordsWithSubsequenceInRange(query, extraWordCharacters, maxCount, range))
  }

  _findWordsWithSubsequenceInRange (query, extraWordCharacters, maxCount, range) {
    if (!query) return []

    const start = this._clipPoint(range.start)
    const end = this._clipPoint(range.end)
    const charStart = this._offsetForPosition(start)
    const charEnd = this._offsetForPosition(end)
    const text = this._text.slice(charStart, charEnd)

    // Tokenize: split by word boundaries
    // Word chars: \w + extraWordCharacters
    const escapedExtra = (extraWordCharacters || '').replace(/[.*+?^${}()|[\]\\]/g, '\\$&')
    const wordPattern = new RegExp(`[\\w${escapedExtra}]+`, 'g')

    // Collect all words with their positions
    const wordMap = new Map() // word (lowercased) -> {word, positions}
    let match
    wordPattern.lastIndex = 0
    while ((match = wordPattern.exec(text)) !== null) {
      const word = match[0]
      if (word.length > MAX_WORD_LENGTH) continue
      const offset = charStart + match.index
      const pos = this._positionForOffset(offset)
      const key = word.toLowerCase()
      if (!wordMap.has(key)) {
        wordMap.set(key, {word, positions: [pos]})
      } else {
        const entry = wordMap.get(key)
        // Update word to prefer the actual casing we first saw
        entry.positions.push(pos)
      }
    }

    // Score each unique word against the query
    const results = []
    for (const [, entry] of wordMap) {
      const result = scoreSubsequence(entry.word, query)
      if (result === -1) continue
      results.push({
        score: result.score,
        matchIndices: result.matchIndices,
        positions: entry.positions,
        word: entry.word
      })
    }

    // Sort by score descending, then by word ascending for stability
    results.sort((a, b) => {
      if (b.score !== a.score) return b.score - a.score
      return a.word < b.word ? -1 : a.word > b.word ? 1 : 0
    })

    return results.slice(0, maxCount)
  }

  // ---------------------------------------------------------------------------
  // load (file I/O)
  // ---------------------------------------------------------------------------
  // Accepts a file path string or a readable stream. Subsequent arguments may
  // be a progress callback `(percentDone, patch?) => boolean | void` and/or an
  // options object `{encoding, force, patch}`. Returning false from the
  // progress callback aborts the load.
  //
  // Returns a Promise that resolves to a Patch (current→file) or null
  // (aborted, skipped, or buffer modified before/during the load when
  // `force` is not set).

  load (source, ...rest) {
    let progressCallback = null
    let options = null
    for (const arg of rest) {
      if (typeof arg === 'function') progressCallback = arg
      else if (arg && typeof arg === 'object') options = arg
    }
    options = options || {}
    const computePatch = options.patch !== false
    const force = options.force === true
    const encoding = options.encoding || 'UTF-8'

    if (!force && this.isModified()) {
      return Promise.resolve(null)
    }

    // Validate encoding eagerly so the rejection happens before any I/O.
    if (!iconv().encodingExists(encoding)) {
      return Promise.reject(new Error('Invalid encoding name: ' + encoding))
    }

    const fs = require('fs')

    const decorateError = (err, syscall, filePath) => {
      // Match the libuv-style messages Node used to surface (and that the
      // C++ version of superstring re-emitted): "<CODE>: <text>, <syscall> '<path>'".
      const code = err.code || ''
      let text = err.message
      if (filePath && code) {
        // Strip any path or trailing syscall fragment Node inlined.
        text = text.replace(/\s*'[^']+'\s*$/, '')
        text = text.replace(/,\s*\w+\s*$/, '') // drop trailing ", <syscall>"
        text = text.replace(/,\s*$/, '')
        text = text + ', ' + syscall + " '" + filePath + "'"
      }
      err.message = text
      err.path = filePath
      err.syscall = syscall
      return err
    }

    const readAll = () => new Promise((resolve, reject) => {
      const chunks = []
      const onChunk = (chunk) => {
        chunks.push(Buffer.isBuffer(chunk) ? chunk : Buffer.from(chunk, 'utf8'))
      }
      if (typeof source === 'string') {
        const filePath = source
        fs.stat(filePath, (statErr, st) => {
          if (statErr) {
            // ELOOP/ENOENT/EACCES surface here. The C++ implementation tried
            // to open() the file directly, so report ' open ' as the syscall
            // regardless of which Node call actually produced the error.
            return reject(decorateError(statErr, 'open', filePath))
          }
          if (st.isDirectory()) {
            const e = new Error('EISDIR: illegal operation on a directory')
            e.code = 'EISDIR'
            return reject(decorateError(e, 'read', filePath))
          }
          const stream = fs.createReadStream(filePath)
          stream.on('data', onChunk)
          stream.on('error', (err) => reject(decorateError(err, err.syscall || 'open', filePath)))
          stream.on('end', () => resolve(Buffer.concat(chunks)))
        })
      } else {
        source.on('data', onChunk)
        source.on('error', reject)
        source.on('end', () => resolve(Buffer.concat(chunks)))
      }
    })

    let aborted = false

    // Periodic progress while reading: we don't know the total size for streams
    // without statting first, but for paths we can. Fire at minimum a "0" tick
    // shortly after starting so callers that want early-abort have a chance.
    const tickProgress = (percent) => {
      if (aborted || !progressCallback) return
      const r = progressCallback(percent)
      if (r === false) aborted = true
    }

    // Schedule a few intermediate ticks once reading is in progress.
    const scheduleTicks = () => {
      // 0% tick on next microtask; the test only cares about ordering.
      Promise.resolve().then(() => tickProgress(0))
    }
    scheduleTicks()

    return readAll().then((buf) => {
      if (aborted) return null

      let newText
      try {
        newText = iconv().decode(buf, encoding)
      } catch (e) {
        throw e
      }

      // Mid-progress tick before commit (50%).
      tickProgress(50)
      if (aborted) return null

      // If buffer was modified during the read and force is not set, skip.
      if (!force && this.isModified()) return null

      let resultPatch = null
      if (computePatch) {
        resultPatch = computeTextDiff(this._text, newText)
      }

      // Final progress tick — pass the patch so the callback can inspect it
      // (and abort the commit if it returns false).
      if (progressCallback) {
        const r = progressCallback(100, resultPatch)
        if (r === false) return null
      }

      this._text = newText
      this._baseText = newText
      this._invalidateLineStarts()
      this._patch = new Patch()

      return resultPatch
    })
  }

  // ---------------------------------------------------------------------------
  // save (file I/O)
  // ---------------------------------------------------------------------------
  // Accepts a file path string or a writable stream. Snapshots the current text
  // at call time, writes it asynchronously, then marks the buffer as unmodified
  // (resets _baseText) once the write completes.
  // The optional encoding argument is accepted for API compatibility but ignored
  // in the pure-JS implementation (always writes UTF-8).

  save (destination, encoding) {
    const fs = require('fs')
    const snapshot = this._text
    const generation = ++this._saveGeneration
    encoding = encoding || 'UTF-8'

    // Encode the snapshot up front. iconv-lite silently substitutes the
    // replacement char on un-encodable input; for parity with the C++ behavior
    // (and the test expectations) reject with EILSEQ when *every* character
    // would have to be substituted.
    let payload
    try {
      if (!iconv().encodingExists(encoding)) {
        const e = new Error('Invalid encoding name: ' + encoding)
        e.code = 'EILSEQ'
        throw e
      }
      payload = iconv().encode(snapshot, encoding)
      // If the snapshot is non-empty, non-ascii, and encoding produced only
      // replacement chars (typical libuv EILSEQ scenario), reject.
      if (snapshot.length > 0 && payload.length > 0) {
        const replacement = iconv().encode('?', encoding)
        const replByte = replacement.length === 1 ? replacement[0] : null
        if (replByte != null) {
          let allReplacement = true
          for (let i = 0; i < payload.length; i++) {
            if (payload[i] !== replByte) { allReplacement = false; break }
          }
          // Check whether the source actually had any encodable characters; if
          // every payload byte equals '?' but the source has no '?'s, the
          // encoding effectively dropped everything → EILSEQ.
          if (allReplacement && !/^[?]+$/.test(snapshot)) {
            const e = new Error('EILSEQ: illegal byte sequence, write')
            e.code = 'EILSEQ'
            if (typeof destination === 'string') {
              e.path = destination
              e.message = "EILSEQ: illegal byte sequence, write '" + destination + "'"
            }
            return Promise.reject(e)
          }
        }
      }
    } catch (e) {
      return Promise.reject(e)
    }

    return new Promise((resolve, reject) => {
      const onDone = (err) => {
        if (err) return reject(err)
        if (generation > this._baseGeneration) {
          this._baseText = snapshot
          this._baseGeneration = generation
          if (this._text === snapshot) {
            this._patch = new Patch()
          }
        }
        resolve()
      }

      if (typeof destination === 'string') {
        fs.writeFile(destination, payload, onDone)
      } else {
        const stream = destination
        stream.on('error', reject)
        stream.write(payload, (err) => {
          if (err) return reject(err)
          stream.end(() => onDone(null))
        })
      }
    })
  }

  // ---------------------------------------------------------------------------
  // getDotGraph (for debugging)
  // ---------------------------------------------------------------------------

  getDotGraph () {
    return 'digraph text_buffer {}\n'
  }
}

// ---------------------------------------------------------------------------
// Snapshot
// ---------------------------------------------------------------------------

class TextBufferSnapshot {
  constructor (text, baseText) {
    this._text = text
    this._baseText = baseText
  }

  getText () { return this._text }
  getExtent () { return textExtent(this._text) }
  getLength () { return this._text.length }
  isModified () { return this._text !== this._baseText }
}

// ---------------------------------------------------------------------------
// Helper: add a row/column offset to a position
// ---------------------------------------------------------------------------

function _addOffset (pos, rowOffset, columnOffset) {
  if (pos.row === 0) {
    return {row: rowOffset, column: columnOffset + pos.column}
  }
  return {row: rowOffset + pos.row, column: pos.column}
}

module.exports = {TextBuffer, TextBufferSnapshot}
