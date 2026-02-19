'use strict'

// Point helpers
function cmp (a, b) {
  if (a.row !== b.row) return a.row < b.row ? -1 : 1
  if (a.column !== b.column) return a.column < b.column ? -1 : 1
  return 0
}

function traverse (a, b) {
  if (b.row === 0) return {row: a.row, column: a.column + b.column}
  return {row: a.row + b.row, column: b.column}
}

function traversal (a, b) {
  if (a.row === b.row) return {row: 0, column: a.column - b.column}
  return {row: a.row - b.row, column: a.column}
}

const ZERO = {row: 0, column: 0}

function isZero (p) {
  return p.row === 0 && p.column === 0
}

function pointMin (a, b) {
  return cmp(a, b) <= 0 ? a : b
}

// Text helpers – texts are plain JS strings
function textSize (text) {
  return text == null ? 0 : text.length
}

function textExtent (text) {
  if (!text) return ZERO
  let row = 0
  let column = 0
  for (let i = 0; i < text.length; i++) {
    if (text.charCodeAt(i) === 10) { row++; column = 0 } else { column++ }
  }
  return {row, column}
}

function textOffsetForPoint (text, point) {
  if (point.row === 0 && point.column === 0) return 0
  let row = 0
  let column = 0
  for (let i = 0; i < text.length; i++) {
    if (row === point.row && column === point.column) return i
    if (text.charCodeAt(i) === 10) { row++; column = 0 } else { column++ }
  }
  return text.length
}

function textPositionForOffset (text, offset) {
  let row = 0
  let column = 0
  for (let i = 0; i < offset && i < text.length; i++) {
    if (text.charCodeAt(i) === 10) { row++; column = 0 } else { column++ }
  }
  return {row, column}
}

// Get prefix of text up to (not including) the given extent.
// Returns null if the text cannot reach the requested extent exactly.
function textPrefix (text, extent) {
  if (!text) return isZero(extent) ? '' : null
  if (isZero(extent)) return ''
  const offset = textOffsetForPoint(text, extent)
  const prefix = text.slice(0, offset)
  // The prefix must reach exactly the requested extent.
  // If the actual extent differs (text too short or jumped to wrong row), return null.
  if (cmp(textExtent(prefix), extent) !== 0) return null
  return prefix
}

// Get suffix of text starting at the given extent from the start.
// Returns null if the position cannot be reached exactly (e.g. extent crosses a newline boundary).
function textSuffix (text, fromExtent) {
  if (!text) return isZero(fromExtent) ? '' : null
  if (isZero(fromExtent)) return text
  const offset = textOffsetForPoint(text, fromExtent)
  // Verify the position was actually reached: the text up to offset must have exactly fromExtent.
  const consumed = text.slice(0, offset)
  if (cmp(textExtent(consumed), fromExtent) !== 0) return null
  return text.slice(offset)
}

// Coordinate spaces (like C++ template specialisations)
const OldCoordinates = {
  distanceFromLeftAncestor: (node) => node.oldDistanceFromLeftAncestor,
  extent: (node) => node.oldExtent,
  start: (change) => change.oldStart,
  end: (change) => change.oldEnd
}

const NewCoordinates = {
  distanceFromLeftAncestor: (node) => node.newDistanceFromLeftAncestor,
  extent: (node) => node.newExtent,
  start: (change) => change.newStart,
  end: (change) => change.newEnd
}

function buildNode (left, right, oldDistanceFromLeftAncestor, newDistanceFromLeftAncestor,
  oldExtent, newExtent, oldText, newText, oldTextSize) {
  const node = {
    left,
    right,
    oldExtent,
    newExtent,
    oldDistanceFromLeftAncestor,
    newDistanceFromLeftAncestor,
    oldText,  // string or null
    newText,  // string or null
    oldTextSize_: (oldText != null) ? 0 : (oldTextSize || 0),
    oldSubtreeTextSize: 0,
    newSubtreeTextSize: 0
  }
  computeSubtreeTextSizes(node)
  return node
}

function nodeOldTextSize (node) {
  return node.oldText != null ? node.oldText.length : node.oldTextSize_
}

function nodeNewTextSize (node) {
  return node.newText != null ? node.newText.length : 0
}

function leftSubtreeOldTextSize (node) {
  return node.left ? node.left.oldSubtreeTextSize : 0
}

function rightSubtreeOldTextSize (node) {
  return node.right ? node.right.oldSubtreeTextSize : 0
}

function leftSubtreeNewTextSize (node) {
  return node.left ? node.left.newSubtreeTextSize : 0
}

function rightSubtreeNewTextSize (node) {
  return node.right ? node.right.newSubtreeTextSize : 0
}

function computeSubtreeTextSizes (node) {
  node.oldSubtreeTextSize = nodeOldTextSize(node) + leftSubtreeOldTextSize(node) + rightSubtreeOldTextSize(node)
  node.newSubtreeTextSize = nodeNewTextSize(node) + leftSubtreeNewTextSize(node) + rightSubtreeNewTextSize(node)
}

// Get the rightmost old/new end in a subtree
function getSubtreeEnd (node) {
  let oldEnd = ZERO
  let newEnd = ZERO
  let cur = node
  while (cur) {
    oldEnd = traverse(oldEnd, traverse(cur.oldDistanceFromLeftAncestor, cur.oldExtent))
    newEnd = traverse(newEnd, traverse(cur.newDistanceFromLeftAncestor, cur.newExtent))
    cur = cur.right
  }
  return {oldEnd, newEnd}
}

function copyNode (node) {
  if (!node) return null
  const result = Object.assign({}, node)
  result.oldText = node.oldText
  result.newText = node.newText
  return result
}

function invertNode (node) {
  if (!node) return null
  return {
    left: node.left,
    right: node.right,
    oldExtent: node.newExtent,
    newExtent: node.oldExtent,
    oldDistanceFromLeftAncestor: node.newDistanceFromLeftAncestor,
    newDistanceFromLeftAncestor: node.oldDistanceFromLeftAncestor,
    oldText: node.newText,
    newText: node.oldText,
    oldTextSize_: node.newText != null ? 0 : nodeNewTextSize(node),
    oldSubtreeTextSize: node.newSubtreeTextSize,
    newSubtreeTextSize: node.oldSubtreeTextSize
  }
}

// Serialization helpers – we use a simple flat uint32 array
const SERIALIZATION_VERSION = 1
const LEFT = 1
const RIGHT = 2
const UP = 3

function serializePoint (out, p) {
  out.push(p.row)
  out.push(p.column)
}

function deserializePoint (arr, idx) {
  return {row: arr[idx], column: arr[idx + 1]}
}

function serializeNode (out, node) {
  serializePoint(out, node.oldExtent)
  serializePoint(out, node.newExtent)
  serializePoint(out, node.oldDistanceFromLeftAncestor)
  serializePoint(out, node.newDistanceFromLeftAncestor)
  if (node.oldText != null) {
    out.push(1)
    serializeText(out, node.oldText)
    out.push(0) // placeholder for old_text_size_ (not used when text present)
  } else {
    out.push(0)
    out.push(node.oldTextSize_)
  }
  if (node.newText != null) {
    out.push(1)
    serializeText(out, node.newText)
  } else {
    out.push(0)
  }
}

function serializeText (out, text) {
  out.push(text.length)
  for (let i = 0; i < text.length; i++) {
    out.push(text.charCodeAt(i))
  }
}

function deserializeText (arr, idx) {
  const len = arr[idx++]
  let text = ''
  for (let i = 0; i < len; i++) {
    text += String.fromCharCode(arr[idx + i])
  }
  return {text, nextIdx: idx + len}
}

// Patch class
class Patch {
  constructor (options) {
    this._root = null
    this._changeCount = 0
    this._mergesAdjacentChanges = (options && options.mergeAdjacentChanges === false) ? false : true
    this._nodeStack = []
    this._leftAncestorStack = []
  }

  // --- Splay tree rotations ---

  _rotateLeft (pivot, root, rootParent) {
    if (rootParent) {
      if (rootParent.left === root) rootParent.left = pivot
      else rootParent.right = pivot
    } else {
      this._root = pivot
    }

    root.right = pivot.left
    pivot.left = root

    pivot.oldDistanceFromLeftAncestor = traverse(
      traverse(root.oldDistanceFromLeftAncestor, root.oldExtent),
      pivot.oldDistanceFromLeftAncestor
    )
    pivot.newDistanceFromLeftAncestor = traverse(
      traverse(root.newDistanceFromLeftAncestor, root.newExtent),
      pivot.newDistanceFromLeftAncestor
    )

    computeSubtreeTextSizes(root)
    computeSubtreeTextSizes(pivot)
  }

  _rotateRight (pivot, root, rootParent) {
    if (rootParent) {
      if (rootParent.left === root) rootParent.left = pivot
      else rootParent.right = pivot
    } else {
      this._root = pivot
    }

    root.left = pivot.right
    pivot.right = root

    root.oldDistanceFromLeftAncestor = traversal(
      root.oldDistanceFromLeftAncestor,
      traverse(pivot.oldDistanceFromLeftAncestor, pivot.oldExtent)
    )
    root.newDistanceFromLeftAncestor = traversal(
      root.newDistanceFromLeftAncestor,
      traverse(pivot.newDistanceFromLeftAncestor, pivot.newExtent)
    )

    computeSubtreeTextSizes(root)
    computeSubtreeTextSizes(pivot)
  }

  _splayNode (node) {
    while (this._nodeStack.length > 0) {
      const parent = this._nodeStack.pop()
      const grandparent = this._nodeStack.length > 0 ? this._nodeStack[this._nodeStack.length - 1] : null

      if (grandparent) {
        const greatGrandparent = this._nodeStack.length > 1 ? this._nodeStack[this._nodeStack.length - 2] : null
        if (grandparent.left === parent && parent.right === node) {
          this._rotateLeft(node, parent, grandparent)
          this._rotateRight(node, grandparent, greatGrandparent)
        } else if (grandparent.right === parent && parent.left === node) {
          this._rotateRight(node, parent, grandparent)
          this._rotateLeft(node, grandparent, greatGrandparent)
        } else if (grandparent.left === parent && parent.left === node) {
          this._rotateRight(parent, grandparent, greatGrandparent)
          this._rotateRight(node, parent, greatGrandparent)
        } else if (grandparent.right === parent && parent.right === node) {
          this._rotateLeft(parent, grandparent, greatGrandparent)
          this._rotateLeft(node, parent, greatGrandparent)
        }
        this._nodeStack.pop() // grandparent was consumed
      } else {
        if (parent.left === node) {
          this._rotateRight(node, parent, null)
        } else if (parent.right === node) {
          this._rotateLeft(node, parent, null)
        }
      }
    }
  }

  _splayNodeEndingBefore (space, target) {
    let splayed = null
    let leftAncestorEnd = ZERO
    let node = this._root
    this._nodeStack.length = 0
    let splayedAncestorCount = 0

    while (node) {
      const nodeStart = traverse(leftAncestorEnd, space.distanceFromLeftAncestor(node))
      const nodeEnd = traverse(nodeStart, space.extent(node))
      if (cmp(nodeEnd, target) <= 0) {
        splayed = node
        splayedAncestorCount = this._nodeStack.length
        if (node.right) {
          leftAncestorEnd = traverse(nodeStart, space.extent(node))
          this._nodeStack.push(node)
          node = node.right
        } else {
          break
        }
      } else {
        if (node.left) {
          this._nodeStack.push(node)
          node = node.left
        } else {
          break
        }
      }
    }

    if (splayed) {
      this._nodeStack.length = splayedAncestorCount
      this._splayNode(splayed)
    }

    return splayed
  }

  _splayNodeStartingBefore (space, target) {
    let splayed = null
    let leftAncestorEnd = ZERO
    let node = this._root
    this._nodeStack.length = 0
    let splayedAncestorCount = 0

    while (node) {
      const nodeStart = traverse(leftAncestorEnd, space.distanceFromLeftAncestor(node))
      const nodeEnd = traverse(nodeStart, space.extent(node))
      if (cmp(nodeStart, target) <= 0) {
        splayed = node
        splayedAncestorCount = this._nodeStack.length
        if (node.right) {
          leftAncestorEnd = nodeEnd
          this._nodeStack.push(node)
          node = node.right
        } else {
          break
        }
      } else {
        if (node.left) {
          this._nodeStack.push(node)
          node = node.left
        } else {
          break
        }
      }
    }

    if (splayed) {
      this._nodeStack.length = splayedAncestorCount
      this._splayNode(splayed)
    }

    return splayed
  }

  _splayNodeEndingAfter (space, target, exclusiveLowerBound) {
    let splayed = null
    let leftAncestorEnd = ZERO
    let node = this._root
    this._nodeStack.length = 0
    let splayedAncestorCount = 0

    while (node) {
      const nodeStart = traverse(leftAncestorEnd, space.distanceFromLeftAncestor(node))
      const nodeEnd = traverse(nodeStart, space.extent(node))
      const qualifies = cmp(nodeEnd, target) >= 0 &&
        (!exclusiveLowerBound || cmp(nodeEnd, exclusiveLowerBound) > 0)
      if (qualifies) {
        splayed = node
        splayedAncestorCount = this._nodeStack.length
        if (node.left) {
          this._nodeStack.push(node)
          node = node.left
        } else {
          break
        }
      } else {
        if (node.right) {
          leftAncestorEnd = nodeEnd
          this._nodeStack.push(node)
          node = node.right
        } else {
          break
        }
      }
    }

    if (splayed) {
      this._nodeStack.length = splayedAncestorCount
      this._splayNode(splayed)
    }

    return splayed
  }

  _splayNodeStartingAfter (space, target, exclusiveLowerBound) {
    let splayed = null
    let leftAncestorEnd = ZERO
    let node = this._root
    this._nodeStack.length = 0
    let splayedAncestorCount = 0

    while (node) {
      const nodeStart = traverse(leftAncestorEnd, space.distanceFromLeftAncestor(node))
      const nodeEnd = traverse(nodeStart, space.extent(node))
      const qualifies = cmp(nodeStart, target) >= 0 &&
        (!exclusiveLowerBound || cmp(nodeStart, exclusiveLowerBound) > 0)
      if (qualifies) {
        splayed = node
        splayedAncestorCount = this._nodeStack.length
        if (node.left) {
          this._nodeStack.push(node)
          node = node.left
        } else {
          break
        }
      } else {
        if (node.right) {
          leftAncestorEnd = nodeEnd
          this._nodeStack.push(node)
          node = node.right
        } else {
          break
        }
      }
    }

    if (splayed) {
      this._nodeStack.length = splayedAncestorCount
      this._splayNode(splayed)
    }

    return splayed
  }

  _deleteRoot () {
    let node = this._root
    let parent = null
    while (true) {
      if (node.left) {
        const left = node.left
        this._rotateRight(node.left, node, parent)
        parent = left
      } else if (node.right) {
        const right = node.right
        this._rotateLeft(node.right, node, parent)
        parent = right
      } else if (parent) {
        if (parent.left === node) {
          parent.left = null
        } else {
          parent.right = null
        }
        this._changeCount--
        break
      } else {
        this._root = null
        this._changeCount--
        break
      }

      if (parent) {
        parent.oldSubtreeTextSize -= nodeOldTextSize(node)
        parent.newSubtreeTextSize -= nodeNewTextSize(node)
      }
    }
  }

  _deleteNode (node) {
    if (!node) return
    // Iterative subtree deletion
    const stack = [node]
    while (stack.length > 0) {
      const n = stack.pop()
      if (n.left) stack.push(n.left)
      if (n.right) stack.push(n.right)
      this._changeCount--
    }
  }

  _buildNode (left, right, oldDist, newDist, oldExtent, newExtent, oldText, newText, oldTextSize) {
    this._changeCount++
    return buildNode(left, right, oldDist, newDist, oldExtent, newExtent, oldText, newText, oldTextSize)
  }

  _changeForRoot () {
    const root = this._root
    const oldStart = root.oldDistanceFromLeftAncestor
    const newStart = root.newDistanceFromLeftAncestor
    return {
      oldStart,
      oldEnd: traverse(oldStart, root.oldExtent),
      newStart,
      newEnd: traverse(newStart, root.newExtent),
      oldText: root.oldText,
      newText: root.newText,
      precedingOldTextSize: leftSubtreeOldTextSize(root),
      precedingNewTextSize: leftSubtreeNewTextSize(root),
      oldTextSize: nodeOldTextSize(root)
    }
  }

  _removeNoopChange () {
    const root = this._root
    if (root && root.oldText != null && root.newText != null && root.oldText === root.newText) {
      this.spliceOld(root.oldDistanceFromLeftAncestor, ZERO, ZERO)
    }
  }

  // --- Compute old text from overlapping changes ---

  _computeOldText (deletedText, newSpliceStart, newDeletionEnd) {
    if (deletedText == null) return {text: null, ok: true}

    const overlapping = this._grabChangesInRange(NewCoordinates, newSpliceStart, newDeletionEnd, this._mergesAdjacentChanges)

    let result = ''
    let deletedTextSliceStart = newSpliceStart
    let deletedText_ = deletedText

    for (const change of overlapping) {
      if (change.oldText == null) return {text: null, ok: true}

      if (cmp(change.newStart, deletedTextSliceStart) > 0) {
        const prefixExtent = traversal(change.newStart, deletedTextSliceStart)
        const prefix = textPrefix(deletedText_, prefixExtent)
        if (prefix === null) return {text: null, ok: false}
        result += prefix
        deletedText_ = textSuffix(deletedText_, prefixExtent)
        deletedTextSliceStart = change.newStart
      }

      result += change.oldText

      // advance past change.newEnd: consume the part of deletedText_ that corresponds to change.newText
      const advanceExtent = traversal(change.newEnd, deletedTextSliceStart)
      const useExtent = pointMin(textExtent(deletedText_), advanceExtent)
      const newDeletedText_ = textSuffix(deletedText_, useExtent)
      if (newDeletedText_ === null) return {text: null, ok: false}
      deletedText_ = newDeletedText_
      deletedTextSliceStart = change.newEnd
    }

    result += deletedText_
    return {text: result, ok: true}
  }

  _computeOldTextSize (deletedTextSize, newSpliceStart, newDeletionEnd) {
    let size = deletedTextSize
    const overlapping = this._grabChangesInRange(NewCoordinates, newSpliceStart, newDeletionEnd, this._mergesAdjacentChanges)
    for (const change of overlapping) {
      if (change.newText == null) return 0
      let overlappingNewText = change.newText
      if (cmp(newDeletionEnd, change.newEnd) < 0) {
        const p = textPrefix(overlappingNewText, traversal(newDeletionEnd, change.newStart))
        overlappingNewText = p != null ? p : overlappingNewText
      }
      if (cmp(newSpliceStart, change.newStart) > 0) {
        const s = textSuffix(overlappingNewText, traversal(newSpliceStart, change.newStart))
        overlappingNewText = s != null ? s : overlappingNewText
      }
      size -= overlappingNewText.length
      size += change.oldTextSize != null ? change.oldTextSize : nodeOldTextSize(change._node || {oldText: change.oldText, oldTextSize_: 0})
    }
    return size
  }

  // --- Non-splaying reads ---

  _getChangesInRange (space, start, end, inclusive) {
    const result = []
    let node = this._root
    const nodeStack = []
    const leftAncestorStack = [{oldEnd: ZERO, newEnd: ZERO, totalOldTextSize: 0, totalNewTextSize: 0}]

    let foundNode = null
    let foundAncestorCount = 0
    let foundLeftAncestorCount = 0

    // Find the leftmost node whose end is > start (or >= start if inclusive)
    while (node) {
      const lai = leftAncestorStack[leftAncestorStack.length - 1]
      const nodeOldStart = traverse(lai.oldEnd, node.oldDistanceFromLeftAncestor)
      const nodeNewStart = traverse(lai.newEnd, node.newDistanceFromLeftAncestor)
      const nodeOldEnd = traverse(nodeOldStart, node.oldExtent)
      const nodeNewEnd = traverse(nodeNewStart, node.newExtent)

      const nodeEnd = space === OldCoordinates ? nodeOldEnd : nodeNewEnd
      const inRange = inclusive ? cmp(nodeEnd, start) >= 0 : cmp(nodeEnd, start) > 0

      if (inRange) {
        foundAncestorCount = nodeStack.length
        foundLeftAncestorCount = leftAncestorStack.length
        foundNode = node
        if (node.left) {
          nodeStack.push(node)
          node = node.left
        } else {
          break
        }
      } else {
        if (node.right) {
          leftAncestorStack.push({
            oldEnd: nodeOldEnd,
            newEnd: nodeNewEnd,
            totalOldTextSize: lai.totalOldTextSize + leftSubtreeOldTextSize(node) + nodeOldTextSize(node),
            totalNewTextSize: lai.totalNewTextSize + leftSubtreeNewTextSize(node) + nodeNewTextSize(node)
          })
          nodeStack.push(node)
          node = node.right
        } else {
          break
        }
      }
    }

    node = foundNode
    nodeStack.length = foundAncestorCount
    leftAncestorStack.length = foundLeftAncestorCount

    while (node) {
      const lai = leftAncestorStack[leftAncestorStack.length - 1]
      const oldStart = traverse(lai.oldEnd, node.oldDistanceFromLeftAncestor)
      const newStart = traverse(lai.newEnd, node.newDistanceFromLeftAncestor)
      const nodeStart = space === OldCoordinates ? oldStart : newStart
      const atEnd = inclusive ? cmp(nodeStart, end) > 0 : cmp(nodeStart, end) >= 0
      if (atEnd) break

      const oldEnd = traverse(oldStart, node.oldExtent)
      const newEnd = traverse(newStart, node.newExtent)
      const precedingOldTextSize = lai.totalOldTextSize + leftSubtreeOldTextSize(node)
      const precedingNewTextSize = lai.totalNewTextSize + leftSubtreeNewTextSize(node)

      result.push({
        oldStart,
        oldEnd,
        newStart,
        newEnd,
        oldText: node.oldText,
        newText: node.newText,
        precedingOldTextSize,
        precedingNewTextSize,
        oldTextSize: nodeOldTextSize(node)
      })

      if (node.right) {
        leftAncestorStack.push({
          oldEnd,
          newEnd,
          totalOldTextSize: precedingOldTextSize + nodeOldTextSize(node),
          totalNewTextSize: precedingNewTextSize + nodeNewTextSize(node)
        })
        nodeStack.push(node)
        node = node.right
        while (node.left) {
          nodeStack.push(node)
          node = node.left
        }
      } else {
        while (nodeStack.length > 0 && nodeStack[nodeStack.length - 1].right === node) {
          node = nodeStack.pop()
          leftAncestorStack.pop()
        }
        node = nodeStack.length > 0 ? nodeStack.pop() : null
      }
    }

    return result
  }

  _grabChangesInRange (space, start, end, inclusive) {
    const result = []
    if (!this._root) return result

    const lower = this._splayNodeStartingBefore(space, start)

    this._nodeStack.length = 0
    this._leftAncestorStack = [{oldEnd: ZERO, newEnd: ZERO, totalOldTextSize: 0, totalNewTextSize: 0}]

    let node = this._root
    if (!lower) {
      while (node.left) {
        this._nodeStack.push(node)
        node = node.left
      }
    }

    while (node) {
      const lai = this._leftAncestorStack[this._leftAncestorStack.length - 1]
      const oldStart = traverse(lai.oldEnd, node.oldDistanceFromLeftAncestor)
      const newStart = traverse(lai.newEnd, node.newDistanceFromLeftAncestor)
      const oldEnd = traverse(oldStart, node.oldExtent)
      const newEnd = traverse(newStart, node.newExtent)
      const precedingOldTextSize = lai.totalOldTextSize + leftSubtreeOldTextSize(node)
      const precedingNewTextSize = lai.totalNewTextSize + leftSubtreeNewTextSize(node)

      const change = {
        oldStart, oldEnd, newStart, newEnd,
        oldText: node.oldText,
        newText: node.newText,
        precedingOldTextSize,
        precedingNewTextSize,
        oldTextSize: nodeOldTextSize(node)
      }

      const changeStart = space === OldCoordinates ? change.oldStart : change.newStart
      const changeEnd = space === OldCoordinates ? change.oldEnd : change.newEnd

      if (inclusive) {
        if (cmp(changeStart, end) > 0) break
        if (cmp(changeEnd, start) >= 0) result.push(change)
      } else {
        if (cmp(changeStart, end) >= 0) break
        if (cmp(changeEnd, start) > 0) result.push(change)
      }

      if (node.right) {
        this._leftAncestorStack.push({
          oldEnd,
          newEnd,
          totalOldTextSize: precedingOldTextSize + nodeOldTextSize(node),
          totalNewTextSize: precedingNewTextSize + nodeNewTextSize(node)
        })
        this._nodeStack.push(node)
        node = node.right
        while (node.left) {
          this._nodeStack.push(node)
          node = node.left
        }
      } else {
        while (this._nodeStack.length > 0 && this._nodeStack[this._nodeStack.length - 1].right === node) {
          node = this._nodeStack.pop()
          this._leftAncestorStack.pop()
        }
        node = this._nodeStack.length > 0 ? this._nodeStack.pop() : null
      }
    }

    return result
  }

  _getChangeStartingBeforePosition (space, target) {
    let found = null
    let node = this._root
    let lai = {oldEnd: ZERO, newEnd: ZERO, totalOldTextSize: 0, totalNewTextSize: 0}
    let foundLai = null

    while (node) {
      const nodeOldStart = traverse(lai.oldEnd, node.oldDistanceFromLeftAncestor)
      const nodeNewStart = traverse(lai.newEnd, node.newDistanceFromLeftAncestor)
      const nodeStart = space === OldCoordinates ? nodeOldStart : nodeNewStart
      if (cmp(nodeStart, target) <= 0) {
        found = node
        foundLai = lai
        if (node.right) {
          lai = {
            oldEnd: traverse(nodeOldStart, node.oldExtent),
            newEnd: traverse(nodeNewStart, node.newExtent),
            totalOldTextSize: lai.totalOldTextSize + leftSubtreeOldTextSize(node) + nodeOldTextSize(node),
            totalNewTextSize: lai.totalNewTextSize + leftSubtreeNewTextSize(node) + nodeNewTextSize(node)
          }
          node = node.right
        } else {
          break
        }
      } else {
        if (node.left) {
          node = node.left
        } else {
          break
        }
      }
    }

    if (!found) return null
    const oldStart = traverse(foundLai.oldEnd, found.oldDistanceFromLeftAncestor)
    const newStart = traverse(foundLai.newEnd, found.newDistanceFromLeftAncestor)
    return {
      oldStart,
      oldEnd: traverse(oldStart, found.oldExtent),
      newStart,
      newEnd: traverse(newStart, found.newExtent),
      oldText: found.oldText,
      newText: found.newText,
      precedingOldTextSize: foundLai.totalOldTextSize + leftSubtreeOldTextSize(found),
      precedingNewTextSize: foundLai.totalNewTextSize + leftSubtreeNewTextSize(found),
      oldTextSize: nodeOldTextSize(found)
    }
  }

  _getChangeEndingAfterPosition (space, target) {
    let found = null
    let node = this._root
    let lai = {oldEnd: ZERO, newEnd: ZERO, totalOldTextSize: 0, totalNewTextSize: 0}
    let foundLai = null

    while (node) {
      const nodeOldStart = traverse(lai.oldEnd, node.oldDistanceFromLeftAncestor)
      const nodeNewStart = traverse(lai.newEnd, node.newDistanceFromLeftAncestor)
      const nodeOldEnd = traverse(nodeOldStart, node.oldExtent)
      const nodeNewEnd = traverse(nodeNewStart, node.newExtent)
      const nodeEnd = space === OldCoordinates ? nodeOldEnd : nodeNewEnd
      if (cmp(nodeEnd, target) > 0) {
        found = node
        foundLai = lai
        if (node.left) {
          node = node.left
        } else {
          break
        }
      } else {
        if (node.right) {
          lai = {
            oldEnd: nodeOldEnd,
            newEnd: nodeNewEnd,
            totalOldTextSize: lai.totalOldTextSize + leftSubtreeOldTextSize(node) + nodeOldTextSize(node),
            totalNewTextSize: lai.totalNewTextSize + leftSubtreeNewTextSize(node) + nodeNewTextSize(node)
          }
          node = node.right
        } else {
          break
        }
      }
    }

    if (!found) return null
    const oldStart = traverse(foundLai.oldEnd, found.oldDistanceFromLeftAncestor)
    const newStart = traverse(foundLai.newEnd, found.newDistanceFromLeftAncestor)
    return {
      oldStart,
      oldEnd: traverse(oldStart, found.oldExtent),
      newStart,
      newEnd: traverse(newStart, found.newExtent),
      oldText: found.oldText,
      newText: found.newText,
      precedingOldTextSize: foundLai.totalOldTextSize + leftSubtreeOldTextSize(found),
      precedingNewTextSize: foundLai.totalNewTextSize + leftSubtreeNewTextSize(found),
      oldTextSize: nodeOldTextSize(found)
    }
  }

  // --- Public API ---

  // _spliceInternal is the raw implementation returning true/false.
  // The public splice method is set up in index.js to throw on false.
  // combine() uses _spliceInternal directly to avoid the throw wrapper.
  _spliceInternal (newStart, oldExtent, newExtent, oldText, newText) {
    if (typeof oldText === 'undefined') oldText = null
    if (typeof newText === 'undefined') newText = null

    // Normalise: make sure strings are strings or null
    if (oldText != null && typeof oldText !== 'string') oldText = String(oldText)
    if (newText != null && typeof newText !== 'string') newText = String(newText)

    if (isZero(oldExtent) && isZero(newExtent)) return true

    const newDeletionEnd = traverse(newStart, oldExtent)
    const newInsertionEnd = traverse(newStart, newExtent)

    if (!this._root) {
      this._root = this._buildNode(null, null, newStart, newStart, oldExtent, newExtent, oldText, newText, 0)
      return true
    }

    const lowerBound = this._splayNodeStartingBefore(NewCoordinates, newStart)

    const oldTextResult = this._computeOldText(oldText, newStart, newDeletionEnd)
    if (!oldTextResult.ok) return false
    const computedOldText = oldTextResult.text

    let oldTextSize = 0
    if (computedOldText == null) {
      oldTextSize = this._computeOldTextSize(0, newStart, newDeletionEnd)
    }

    const upperBound = this._splayNodeEndingAfter(NewCoordinates, newDeletionEnd, newStart)

    if (upperBound && lowerBound && lowerBound !== upperBound) {
      if (lowerBound !== upperBound.left) {
        this._rotateRight(lowerBound, upperBound.left, upperBound)
      }
    }

    if (lowerBound && upperBound) {
      const lowerOldStart = lowerBound.oldDistanceFromLeftAncestor
      const lowerNewStart = lowerBound.newDistanceFromLeftAncestor
      const upperOldStart = upperBound.oldDistanceFromLeftAncestor
      const upperNewStart = upperBound.newDistanceFromLeftAncestor
      const lowerOldEnd = traverse(lowerOldStart, lowerBound.oldExtent)
      const lowerNewEnd = traverse(lowerNewStart, lowerBound.newExtent)
      const upperOldEnd = traverse(upperOldStart, upperBound.oldExtent)
      const upperNewEnd = traverse(upperNewStart, upperBound.newExtent)

      let overlapsLower, overlapsUpper
      if (this._mergesAdjacentChanges) {
        overlapsLower = cmp(newStart, lowerNewEnd) <= 0
        overlapsUpper = cmp(newDeletionEnd, upperNewStart) >= 0
      } else {
        overlapsLower = cmp(newStart, lowerNewEnd) < 0 && cmp(newDeletionEnd, lowerNewStart) > 0
        overlapsUpper = cmp(newStart, upperNewEnd) < 0 && cmp(newDeletionEnd, upperNewStart) > 0
      }

      if (overlapsLower && overlapsUpper) {
        const newExtentPrefix = traversal(newStart, lowerNewStart)
        const newExtentSuffix = traversal(upperNewEnd, newDeletionEnd)

        if (newText != null && lowerBound.newText != null && upperBound.newText != null) {
          const prefix = textPrefix(lowerBound.newText, newExtentPrefix)
          const suffix = textSuffix(upperBound.newText, traversal(newDeletionEnd, upperNewStart))
          if (suffix === null) return false
          upperBound.newText = prefix + newText + suffix
        } else {
          upperBound.newText = null
        }

        upperBound.oldText = computedOldText
        upperBound.oldTextSize_ = computedOldText != null ? 0 : oldTextSize
        upperBound.oldExtent = traversal(upperOldEnd, lowerOldStart)
        upperBound.newExtent = traverse(traverse(newExtentPrefix, newExtent), newExtentSuffix)
        upperBound.oldDistanceFromLeftAncestor = lowerOldStart
        upperBound.newDistanceFromLeftAncestor = lowerNewStart

        if (lowerBound === upperBound) {
          if (isZero(this._root.oldExtent) && isZero(this._root.newExtent)) {
            this._deleteRoot()
          }
        } else {
          upperBound.left = lowerBound.left
          lowerBound.left = null
          this._deleteNode(lowerBound)
          this._changeCount++ // deleteNode decrements, but we keep upperBound
        }
      } else if (overlapsUpper) {
        const oldSpliceStart = traverse(lowerOldEnd, traversal(newStart, lowerNewEnd))
        const newExtentSuffix = traversal(upperNewEnd, newDeletionEnd)

        if (newText != null && upperBound.newText != null) {
          const suffix = textSuffix(upperBound.newText, traversal(newDeletionEnd, upperNewStart))
          if (suffix === null) return false
          upperBound.newText = newText + suffix
        } else {
          upperBound.newText = null
        }

        upperBound.oldText = computedOldText
        upperBound.oldTextSize_ = computedOldText != null ? 0 : oldTextSize
        upperBound.oldDistanceFromLeftAncestor = oldSpliceStart
        upperBound.newDistanceFromLeftAncestor = newStart
        upperBound.oldExtent = traversal(upperOldEnd, oldSpliceStart)
        upperBound.newExtent = traverse(newExtent, newExtentSuffix)

        this._deleteNode(lowerBound.right)
        lowerBound.right = null
        if (upperBound.left !== lowerBound) {
          this._deleteNode(upperBound.left)
          upperBound.left = null
        }
      } else if (overlapsLower) {
        const {oldEnd: rightmostOldEnd, newEnd: rightmostNewEnd} = getSubtreeEnd(lowerBound)
        const oldDeletionEnd = traverse(rightmostOldEnd, traversal(newDeletionEnd, rightmostNewEnd))
        const newExtentPrefix = traversal(newStart, lowerNewStart)

        upperBound.newDistanceFromLeftAncestor = traverse(newInsertionEnd, traversal(upperNewStart, newDeletionEnd))
        lowerBound.oldExtent = traversal(oldDeletionEnd, lowerOldStart)
        lowerBound.newExtent = traverse(newExtentPrefix, newExtent)

        if (newText != null && lowerBound.newText != null) {
          const prefix = textPrefix(lowerBound.newText, newExtentPrefix)
          lowerBound.newText = prefix + newText
        } else {
          lowerBound.newText = null
        }

        lowerBound.oldText = computedOldText
        lowerBound.oldTextSize_ = computedOldText != null ? 0 : oldTextSize

        this._deleteNode(lowerBound.right)
        lowerBound.right = null
        this._rotateRight(lowerBound, upperBound, null)
      } else {
        if (lowerBound === upperBound) {
          // insertion at beginning of node, mergeAdjacentChanges = false
          const newNode = this._buildNode(
            upperBound.left, upperBound,
            upperOldStart, upperNewStart,
            ZERO, newExtent,
            computedOldText, newText, oldTextSize
          )
          this._root = newNode
          upperBound.left = null
          upperBound.oldDistanceFromLeftAncestor = ZERO
          upperBound.newDistanceFromLeftAncestor = ZERO
        } else {
          const {oldEnd: rightmostOldEnd, newEnd: rightmostNewEnd} = getSubtreeEnd(lowerBound)
          const oldSpliceStart = traverse(lowerOldEnd, traversal(newStart, lowerNewEnd))
          const oldDeletionEnd = traverse(rightmostOldEnd, traversal(newDeletionEnd, rightmostNewEnd))
          const newNode = this._buildNode(
            lowerBound, upperBound,
            oldSpliceStart, newStart,
            traversal(oldDeletionEnd, oldSpliceStart), newExtent,
            computedOldText, newText, oldTextSize
          )
          this._root = newNode
          this._deleteNode(lowerBound.right)
          lowerBound.right = null
          upperBound.left = null
          upperBound.oldDistanceFromLeftAncestor = traversal(upperOldStart, oldDeletionEnd)
          upperBound.newDistanceFromLeftAncestor = traversal(upperNewStart, newDeletionEnd)
        }
      }
    } else if (lowerBound) {
      const lowerOldStart = lowerBound.oldDistanceFromLeftAncestor
      const lowerNewStart = lowerBound.newDistanceFromLeftAncestor
      const lowerNewEnd = traverse(lowerNewStart, lowerBound.newExtent)
      const lowerOldEnd = traverse(lowerOldStart, lowerBound.oldExtent)
      const {oldEnd: rightmostOldEnd, newEnd: rightmostNewEnd} = getSubtreeEnd(lowerBound)
      const oldDeletionEnd = traverse(rightmostOldEnd, traversal(newDeletionEnd, rightmostNewEnd))
      const overlapsLower = cmp(newStart, lowerNewEnd) < 0 ||
        (this._mergesAdjacentChanges && cmp(newStart, lowerNewEnd) === 0)

      if (overlapsLower) {
        if (newText != null && lowerBound.newText != null) {
          const prefix = textPrefix(lowerBound.newText, traversal(newStart, lowerNewStart))
          if (prefix === null) return false
          lowerBound.newText = prefix + newText
        } else {
          lowerBound.newText = null
        }
        lowerBound.oldText = computedOldText
        lowerBound.oldTextSize_ = computedOldText != null ? 0 : oldTextSize
        lowerBound.oldExtent = traversal(oldDeletionEnd, lowerOldStart)
        lowerBound.newExtent = traversal(newInsertionEnd, lowerNewStart)
      } else {
        const oldSpliceStart = traverse(lowerOldEnd, traversal(newStart, lowerNewEnd))
        const newNode = this._buildNode(
          lowerBound, null,
          oldSpliceStart, newStart,
          traversal(oldDeletionEnd, oldSpliceStart), newExtent,
          computedOldText, newText, oldTextSize
        )
        this._root = newNode
      }

      this._deleteNode(lowerBound.right)
      lowerBound.right = null
    } else if (upperBound) {
      const upperNewStart = upperBound.newDistanceFromLeftAncestor
      const upperOldStart = upperBound.oldDistanceFromLeftAncestor
      const upperNewEnd = traverse(upperNewStart, upperBound.newExtent)
      const overlapsUpper = cmp(newDeletionEnd, upperNewStart) > 0 ||
        (this._mergesAdjacentChanges && cmp(newDeletionEnd, upperNewStart) === 0)

      let oldDeletionEnd
      if (upperBound.left) {
        const {oldEnd: rightmostOldEnd, newEnd: rightmostNewEnd} = getSubtreeEnd(upperBound.left)
        oldDeletionEnd = traverse(rightmostOldEnd, traversal(newDeletionEnd, rightmostNewEnd))
      } else {
        oldDeletionEnd = newDeletionEnd
      }

      if (overlapsUpper) {
        if (newText != null && upperBound.newText != null) {
          const suffix = textSuffix(upperBound.newText, traversal(newDeletionEnd, upperNewStart))
          if (suffix === null) return false
          upperBound.newText = newText + suffix
        } else {
          upperBound.newText = null
        }
        upperBound.oldText = computedOldText
        upperBound.oldTextSize_ = computedOldText != null ? 0 : oldTextSize
        upperBound.oldDistanceFromLeftAncestor = newStart
        upperBound.newDistanceFromLeftAncestor = newStart
        upperBound.oldExtent = traverse(traversal(upperOldStart, newStart), upperBound.oldExtent)
        upperBound.newExtent = traverse(newExtent, traversal(upperNewEnd, newDeletionEnd))
      } else {
        const newNode = this._buildNode(
          null, upperBound,
          newStart, newStart,
          traversal(oldDeletionEnd, newStart), newExtent,
          computedOldText, newText, oldTextSize
        )
        this._root = newNode
        const distToUpper = traversal(upperNewStart, newDeletionEnd)
        upperBound.oldDistanceFromLeftAncestor = distToUpper
        upperBound.newDistanceFromLeftAncestor = distToUpper
      }

      this._deleteNode(upperBound.left)
      upperBound.left = null
    } else {
      // Both bounds are null – the new splice entirely covers all existing changes
      const {oldEnd: rightmostOldEnd, newEnd: rightmostNewEnd} = getSubtreeEnd(this._root)
      const oldDeletionEnd = traverse(rightmostOldEnd, traversal(newDeletionEnd, rightmostNewEnd))
      this._deleteNode(this._root)
      this._root = null
      this._root = this._buildNode(
        null, null,
        newStart, newStart,
        traversal(oldDeletionEnd, newStart), newExtent,
        computedOldText, newText, oldTextSize
      )
    }

    if (lowerBound) computeSubtreeTextSizes(lowerBound)
    if (upperBound) computeSubtreeTextSizes(upperBound)

    return true
  }

  // Public splice: returns true/false. index.js may wrap this to throw.
  splice (newStart, oldExtent, newExtent, oldText, newText) {
    return this._spliceInternal(newStart, oldExtent, newExtent, oldText, newText)
  }

  spliceOld (oldStart, oldDeletionExtent, oldInsertionExtent) {
    if (!this._root) return

    const oldDeletionEnd = traverse(oldStart, oldDeletionExtent)
    const oldInsertionEnd = traverse(oldStart, oldInsertionExtent)

    const lowerBound = this._splayNodeEndingBefore(OldCoordinates, oldStart)
    const upperBound = this._splayNodeStartingAfter(OldCoordinates, oldDeletionEnd, oldStart)

    if (!lowerBound && !upperBound) {
      this._deleteNode(this._root)
      this._root = null
      return
    }

    if (upperBound === lowerBound) {
      // Both bounds are the same node – must be a zero-length node
      this._root.oldDistanceFromLeftAncestor = traverse(this._root.oldDistanceFromLeftAncestor, oldInsertionExtent)
      this._root.newDistanceFromLeftAncestor = traverse(this._root.newDistanceFromLeftAncestor, oldInsertionExtent)
      return
    }

    if (upperBound && lowerBound) {
      if (lowerBound !== upperBound.left) {
        this._rotateRight(lowerBound, upperBound.left, upperBound)
      }
    }

    let newDeletionEnd, newInsertionEnd

    if (lowerBound) {
      const lowerOldStart = lowerBound.oldDistanceFromLeftAncestor
      const lowerNewStart = lowerBound.newDistanceFromLeftAncestor
      const lowerOldEnd = traverse(lowerOldStart, lowerBound.oldExtent)
      const lowerNewEnd = traverse(lowerNewStart, lowerBound.newExtent)
      newDeletionEnd = traverse(lowerNewEnd, traversal(oldDeletionEnd, lowerOldEnd))
      newInsertionEnd = traverse(lowerNewEnd, traversal(oldInsertionEnd, lowerOldEnd))
      this._deleteNode(lowerBound.right)
      lowerBound.right = null
    } else {
      newDeletionEnd = oldDeletionEnd
      newInsertionEnd = oldInsertionEnd
    }

    if (upperBound) {
      const distToUpper = traversal(upperBound.oldDistanceFromLeftAncestor, oldDeletionEnd)
      upperBound.oldDistanceFromLeftAncestor = traverse(oldInsertionEnd, distToUpper)
      upperBound.newDistanceFromLeftAncestor = traverse(newInsertionEnd, distToUpper)

      if (lowerBound) {
        const lowerOldEnd = traverse(lowerBound.oldDistanceFromLeftAncestor, lowerBound.oldExtent)
        if (cmp(lowerOldEnd, upperBound.oldDistanceFromLeftAncestor) === 0) {
          // Merge
          upperBound.oldDistanceFromLeftAncestor = lowerBound.oldDistanceFromLeftAncestor
          upperBound.newDistanceFromLeftAncestor = lowerBound.newDistanceFromLeftAncestor
          upperBound.oldExtent = traverse(lowerBound.oldExtent, upperBound.oldExtent)

          if (lowerBound.oldText != null && upperBound.oldText != null) {
            upperBound.oldText = lowerBound.oldText + upperBound.oldText
          } else {
            upperBound.oldText = null
            upperBound.oldTextSize_ += lowerBound.oldTextSize_
          }

          upperBound.newExtent = traverse(lowerBound.newExtent, upperBound.newExtent)
          if (lowerBound.newText != null && upperBound.newText != null) {
            upperBound.newText = lowerBound.newText + upperBound.newText
          } else {
            upperBound.newText = null
          }

          upperBound.left = lowerBound.left
          lowerBound.left = null
          this._deleteNode(lowerBound)
          this._changeCount++ // compensate for deleteNode's decrement
        }
      } else {
        this._deleteNode(upperBound.left)
        upperBound.left = null
      }
    }

    if (lowerBound) computeSubtreeTextSizes(lowerBound)
    if (upperBound) computeSubtreeTextSizes(upperBound)
  }

  getChanges () {
    return this._getChangesInRange(NewCoordinates, ZERO, {row: Infinity, column: Infinity}, true).map(_cleanChange)
  }

  getChangeCount () {
    return this.getChanges().length
  }

  getChangesInOldRange (start, end) {
    return this._getChangesInRange(OldCoordinates, start, end, false).map(_cleanChange)
  }

  getChangesInNewRange (start, end) {
    return this._getChangesInRange(NewCoordinates, start, end, false).map(_cleanChange)
  }

  getBounds () {
    if (!this._root) return undefined

    let node = this._root
    while (node.left) node = node.left
    const oldStart = node.oldDistanceFromLeftAncestor
    const newStart = node.newDistanceFromLeftAncestor

    node = this._root
    let oldEnd = ZERO
    let newEnd = ZERO
    while (node) {
      oldEnd = traverse(oldEnd, traverse(node.oldDistanceFromLeftAncestor, node.oldExtent))
      newEnd = traverse(newEnd, traverse(node.newDistanceFromLeftAncestor, node.newExtent))
      node = node.right
    }

    return {oldStart, oldEnd, newStart, newEnd}
  }

  changeForOldPosition (position) {
    const c = this._getChangeStartingBeforePosition(OldCoordinates, position)
    return c ? _cleanChange(c) : undefined
  }

  changeForNewPosition (position) {
    const c = this._getChangeStartingBeforePosition(NewCoordinates, position)
    return c ? _cleanChange(c) : undefined
  }

  invert () {
    const newPatch = new Patch({mergeAdjacentChanges: this._mergesAdjacentChanges})
    newPatch._changeCount = this._changeCount

    if (this._root) {
      // Deep copy tree with inverted nodes
      const stack = [{srcNode: this._root, parentDst: null, isLeft: false}]
      let newRoot = null
      while (stack.length > 0) {
        const {srcNode, parentDst, isLeft} = stack.pop()
        const inverted = invertNode(srcNode)
        inverted.left = null
        inverted.right = null
        if (parentDst) {
          if (isLeft) parentDst.left = inverted
          else parentDst.right = inverted
        } else {
          newRoot = inverted
        }
        if (srcNode.left) stack.push({srcNode: srcNode.left, parentDst: inverted, isLeft: true})
        if (srcNode.right) stack.push({srcNode: srcNode.right, parentDst: inverted, isLeft: false})
      }
      // Recompute sizes
      recomputeSubtreeSizes(newRoot)
      newPatch._root = newRoot
    }

    return newPatch
  }

  copy () {
    const newPatch = new Patch({mergeAdjacentChanges: this._mergesAdjacentChanges})
    newPatch._changeCount = this._changeCount

    if (this._root) {
      const stack = [{srcNode: this._root, parentDst: null, isLeft: false}]
      let newRoot = null
      while (stack.length > 0) {
        const {srcNode, parentDst, isLeft} = stack.pop()
        const copied = copyNode(srcNode)
        copied.left = null
        copied.right = null
        if (parentDst) {
          if (isLeft) parentDst.left = copied
          else parentDst.right = copied
        } else {
          newRoot = copied
        }
        if (srcNode.left) stack.push({srcNode: srcNode.left, parentDst: copied, isLeft: true})
        if (srcNode.right) stack.push({srcNode: srcNode.right, parentDst: copied, isLeft: false})
      }
      recomputeSubtreeSizes(newRoot)
      newPatch._root = newRoot
    }

    return newPatch
  }

  rebalance () {
    if (!this._root) return

    // Transform tree to vine (right-spine)
    let pseudo = this._root
    let pseudoParent = null
    while (pseudo) {
      const left = pseudo.left
      if (left) {
        this._rotateRight(left, pseudo, pseudoParent)
        pseudo = left
      } else {
        pseudoParent = pseudo
        pseudo = pseudo.right
      }
    }

    // Transform vine to balanced tree (Day-Stout-Warren)
    let n = this._changeCount
    let m = Math.pow(2, Math.floor(Math.log2(n + 1))) - 1
    this._performRebalancingRotations(n - m)
    while (m > 1) {
      m = Math.floor(m / 2)
      this._performRebalancingRotations(m)
    }
  }

  _performRebalancingRotations (count) {
    let pseudo = this._root
    let pseudoParent = null
    for (let i = 0; i < count; i++) {
      if (!pseudo) return
      const rightChild = pseudo.right
      if (!rightChild) return
      this._rotateLeft(rightChild, pseudo, pseudoParent)
      pseudo = rightChild.right
      pseudoParent = rightChild
    }
  }

  serialize () {
    const out = [SERIALIZATION_VERSION, this.getChangeCount()]

    if (!this._root) {
      return Buffer.from(new Uint32Array(out).buffer)
    }

    serializeNode(out, this._root)

    let node = this._root
    const stack = []
    let previousChildIndex = -1

    while (node) {
      if (node.left && previousChildIndex < 0) {
        out.push(LEFT)
        serializeNode(out, node.left)
        stack.push(node)
        node = node.left
        previousChildIndex = -1
      } else if (node.right && previousChildIndex < 1) {
        out.push(RIGHT)
        serializeNode(out, node.right)
        stack.push(node)
        node = node.right
        previousChildIndex = -1
      } else if (stack.length > 0) {
        out.push(UP)
        const parent = stack.pop()
        previousChildIndex = (node === parent.left) ? 0 : 1
        node = parent
      } else {
        break
      }
    }

    return Buffer.from(new Uint32Array(out).buffer)
  }

  static deserialize (buffer) {
    const arr = new Uint32Array(buffer.buffer || buffer)
    let idx = 0

    const version = arr[idx++]
    if (version !== SERIALIZATION_VERSION) return new Patch()

    const changeCount = arr[idx++]
    const patch = new Patch()

    if (changeCount === 0) return patch

    function readNode () {
      const oldExtent = deserializePoint(arr, idx); idx += 2
      const newExtent = deserializePoint(arr, idx); idx += 2
      const oldDist = deserializePoint(arr, idx); idx += 2
      const newDist = deserializePoint(arr, idx); idx += 2

      let oldText = null
      let oldTextSize = 0
      const hasOldText = arr[idx++]
      if (hasOldText) {
        const {text, nextIdx} = deserializeText(arr, idx)
        oldText = text
        idx = nextIdx
        idx++ // skip placeholder
      } else {
        oldTextSize = arr[idx++]
      }

      let newText = null
      const hasNewText = arr[idx++]
      if (hasNewText) {
        const {text, nextIdx} = deserializeText(arr, idx)
        newText = text
        idx = nextIdx
      }

      return buildNode(null, null, oldDist, newDist, oldExtent, newExtent, oldText, newText, oldTextSize)
    }

    patch._root = readNode()
    patch._changeCount = 1
    let node = patch._root
    const stack = []

    for (let i = 1; i < changeCount;) {
      const transition = arr[idx++]
      if (transition === LEFT) {
        const child = readNode()
        node.left = child
        stack.push(node)
        node = child
        patch._changeCount++
        i++
      } else if (transition === RIGHT) {
        const child = readNode()
        node.right = child
        stack.push(node)
        node = child
        patch._changeCount++
        i++
      } else if (transition === UP) {
        computeSubtreeTextSizes(node)
        node = stack.pop()
      }
    }

    computeSubtreeTextSizes(node)
    for (let i = stack.length - 1; i >= 0; i--) {
      computeSubtreeTextSizes(stack[i])
    }

    return patch
  }

  static compose (patches) {
    if (!Array.isArray(patches) || patches.some(p => !(p instanceof Patch))) {
      throw new Error('Patch does not apply')
    }

    const result = new Patch()
    let leftToRight = true
    for (const p of patches) {
      if (!result.combine(p, leftToRight)) {
        throw new Error('Patch does not apply')
      }
      leftToRight = !leftToRight
    }
    return result
  }

  combine (other, leftToRight = true) {
    const changes = other.getChanges()
    if (leftToRight) {
      for (const change of changes) {
        if (!this._spliceInternal(
          change.newStart,
          traversal(change.oldEnd, change.oldStart),
          traversal(change.newEnd, change.newStart),
          change.oldText,
          change.newText
        )) return false
        this._removeNoopChange()
      }
    } else {
      for (let i = changes.length - 1; i >= 0; i--) {
        const change = changes[i]
        if (!this._spliceInternal(
          change.oldStart,
          traversal(change.oldEnd, change.oldStart),
          traversal(change.newEnd, change.newStart),
          change.oldText,
          change.newText
        )) return false
        this._removeNoopChange()
      }
    }
    return true
  }

  getDotGraph () {
    return 'digraph patch {}\n'
  }

  getJSON () {
    return this._root ? nodeToJSON(this._root, ZERO, ZERO) : 'null'
  }
}

function recomputeSubtreeSizes (node) {
  if (!node) return
  recomputeSubtreeSizes(node.left)
  recomputeSubtreeSizes(node.right)
  computeSubtreeTextSizes(node)
}

function nodeToJSON (node, leftAncestorOldEnd, leftAncestorNewEnd) {
  if (!node) return 'null'
  const oldStart = traverse(leftAncestorOldEnd, node.oldDistanceFromLeftAncestor)
  const newStart = traverse(leftAncestorNewEnd, node.newDistanceFromLeftAncestor)
  const oldEnd = traverse(oldStart, node.oldExtent)
  const newEnd = traverse(newStart, node.newExtent)
  return JSON.stringify({
    oldStart, oldEnd, newStart, newEnd,
    left: JSON.parse(nodeToJSON(node.left, leftAncestorOldEnd, leftAncestorNewEnd) || 'null'),
    right: JSON.parse(nodeToJSON(node.right, oldEnd, newEnd) || 'null')
  })
}

function _cleanChange (c) {
  const obj = {oldStart: c.oldStart, oldEnd: c.oldEnd, newStart: c.newStart, newEnd: c.newEnd}
  if (c.oldText != null) obj.oldText = c.oldText
  if (c.newText != null) obj.newText = c.newText
  return obj
}

module.exports = {Patch, textExtent, textPositionForOffset, textOffsetForPoint, traverse, traversal, cmp}
