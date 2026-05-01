'use strict'

const {Patch, textExtent, textPositionForOffset, textOffsetForPoint, traverse, traversal, cmp} = require('./patch')

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
function compilePattern (pattern, extraFlags) {
  if (typeof pattern === 'string') {
    try {
      return new RegExp(pattern, 'gm' + (extraFlags || ''))
    } catch (e) {
      throw new Error(e.message)
    }
  }
  // It's already a RegExp – rebuild with global+multiline forced, preserving flags
  const flags = new Set([...pattern.flags, 'g', 'm'])
  flags.delete('y') // sticky is incompatible with global for our use
  try {
    return new RegExp(pattern.source, [...flags].join(''))
  } catch (e) {
    throw new Error(e.message)
  }
}

// ---------------------------------------------------------------------------
// Subsequence scoring
// ---------------------------------------------------------------------------
// Approximate port of the C++ scoring logic used for find_words_with_subsequence.
// This doesn't have to be bit-for-bit identical; it just needs to produce
// reasonable ordering.

const SCORE_CONSECUTIVE = 5
const SCORE_WORD_BOUNDARY = 4
const SCORE_CAMEL_CASE = 3
const SCORE_BASE = 1
const SCORE_SKIP_PENALTY = 1
const MAX_WORD_LENGTH = 80

function isWordBoundary (text, index) {
  if (index === 0) return true
  const prev = text[index - 1]
  const cur = text[index]
  if (prev === '_' || prev === ' ' || prev === '-') return true
  if (cur >= 'A' && cur <= 'Z' && (prev >= 'a' && prev <= 'z')) return true // camelCase
  return false
}

function scoreSubsequence (word, query) {
  if (word.length > MAX_WORD_LENGTH) return -1

  const wl = word.toLowerCase()
  const ql = query.toLowerCase()
  let wi = 0
  let qi = 0
  let score = 0
  let lastMatch = -1
  const matchIndices = []

  while (qi < ql.length && wi < wl.length) {
    if (ql[qi] === wl[wi]) {
      let s = SCORE_BASE
      if (wi === lastMatch + 1) s += SCORE_CONSECUTIVE
      if (isWordBoundary(word, wi)) s += SCORE_WORD_BOUNDARY
      if (wi > 0 && word[wi] >= 'A' && word[wi] <= 'Z') s += SCORE_CAMEL_CASE
      score += s
      score -= (wi - (lastMatch + 1)) * SCORE_SKIP_PENALTY
      matchIndices.push(wi)
      lastMatch = wi
      qi++
    }
    wi++
  }

  if (qi < ql.length) return -1 // didn't match all query chars
  return {score, matchIndices}
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
    return this._offsetForPosition(clipped)
  }

  positionForCharacterIndex (offset) {
    if (offset < 0) offset = 0
    if (offset > this._text.length) offset = this._text.length
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

  // Internal: find first match in text (with optional range restriction)
  _findInText (text, pattern, range) {
    const re = compilePattern(pattern)

    let searchText = text
    let rowOffset = 0
    let columnOffset = 0
    let charOffset = 0

    if (range) {
      const start = this._clipPoint(range.start)
      const end = this._clipPoint(range.end)
      charOffset = textOffsetForPoint(text, start)
      const endOffset = textOffsetForPoint(text, end)
      searchText = text.slice(charOffset, endOffset)
      rowOffset = start.row
      columnOffset = start.column
    }

    re.lastIndex = 0
    const match = re.exec(searchText)
    if (!match) return null

    // Convert match offsets to row/column positions
    const startPos = textPositionForOffset(searchText, match.index)
    const endPos = textPositionForOffset(searchText, match.index + match[0].length)

    return {
      start: _addOffset(startPos, rowOffset, columnOffset),
      end: _addOffset(endPos, rowOffset, columnOffset)
    }
  }

  // Internal: find all matches in text (with optional range restriction)
  _findAllInText (text, pattern, range) {
    const re = compilePattern(pattern)
    const results = []

    let searchText = text
    let rowOffset = 0
    let columnOffset = 0
    let charOffset = 0
    let endOffset = text.length

    if (range) {
      const start = this._clipPoint(range.start)
      const end = this._clipPoint(range.end)
      charOffset = textOffsetForPoint(text, start)
      endOffset = textOffsetForPoint(text, end)
      searchText = text.slice(charOffset, endOffset)
      rowOffset = start.row
      columnOffset = start.column
    }

    re.lastIndex = 0
    let match
    let lastIndex = 0

    while ((match = re.exec(searchText)) !== null) {
      const matchStart = match.index
      const matchEnd = match.index + match[0].length

      const startPos = textPositionForOffset(searchText, matchStart)
      const endPos = textPositionForOffset(searchText, matchEnd)

      results.push({
        start: _addOffset(startPos, rowOffset, columnOffset),
        end: _addOffset(endPos, rowOffset, columnOffset)
      })

      // Avoid infinite loop on zero-length matches
      if (matchEnd === lastIndex) {
        re.lastIndex++
      }
      lastIndex = matchEnd
    }

    return results
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
      if (result === -1 || result.score <= 0) continue
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
  // Accepts a file path string or a readable stream, plus options:
  //   encoding    – ignored in pure-JS (always read as UTF-8)
  //   force       – if false (default) and buffer is modified, resolve null
  //   patch       – if true (default), compute and return a Patch from the
  //                 current buffer text to the loaded file text
  //
  // Returns a Promise that resolves to a Patch or null.

  load (source, options) {
    const fs = require('fs')
    const computePatch = !options || options.patch !== false
    const force = options && options.force === true

    if (!force && this.isModified()) {
      return Promise.resolve(null)
    }

    const readAll = (source) => new Promise((resolve, reject) => {
      if (typeof source === 'string') {
        fs.readFile(source, 'utf8', (err, data) => {
          if (err) reject(err)
          else resolve(data)
        })
      } else {
        const chunks = []
        source.on('data', (chunk) => chunks.push(Buffer.isBuffer(chunk) ? chunk.toString('utf8') : chunk))
        source.on('error', reject)
        source.on('end', () => resolve(chunks.join('')))
      }
    })

    return readAll(source).then((newText) => {
      const oldText = this._text
      let resultPatch = null

      if (computePatch) {
        // Build patch from current buffer text → file text.
        // If the buffer has no pending changes, this is just base→file.
        // If it does, we need: invert(current→base) composed with base→file,
        // which equals current→file.
        const baseToFile = new Patch()
        if (this._baseText !== newText) {
          baseToFile.splice(
            ZERO,
            textExtent(this._baseText),
            textExtent(newText),
            this._baseText,
            newText
          )
        }

        if (this._text !== this._baseText) {
          // inverted current patch: current→base
          const currentToBase = new Patch()
          currentToBase.splice(
            ZERO,
            textExtent(this._text),
            textExtent(this._baseText),
            this._text,
            this._baseText
          )
          resultPatch = Patch.compose([currentToBase, baseToFile])
        } else {
          resultPatch = baseToFile
        }
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

  save (destination, _encoding) {
    const fs = require('fs')
    const snapshot = this._text
    const generation = ++this._saveGeneration

    return new Promise((resolve, reject) => {
      const onDone = (err) => {
        if (err) return reject(err)
        // Advance the base to this snapshot if this save is newer than the
        // last one that updated the base. This correctly handles concurrent
        // saves: the most-recent save (highest generation) wins, ensuring
        // isModified() returns false once all saves settle on the same text.
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
        fs.writeFile(destination, snapshot, 'utf8', onDone)
      } else {
        // Writable stream
        const stream = destination
        stream.on('error', reject)
        stream.write(snapshot, 'utf8', (err) => {
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
