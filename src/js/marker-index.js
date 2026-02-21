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
const MAX_POINT = {row: Infinity, column: Infinity}

function isZero (p) {
  return p.row === 0 && p.column === 0
}

// Simple seeded PRNG (xorshift32-based, stays in 1..INT_MAX-1)
function makeRng (seed) {
  let state = (seed >>> 0) || 1637043935
  return function () {
    state ^= state << 13
    state ^= state >>> 17
    state ^= state << 5
    state >>>= 0
    if (state === 0) state = 1637043935
    return (state % 2147483646) + 1
  }
}

// MarkerIndex node
function makeNode (parent, leftExtent) {
  return {
    parent,
    left: null,
    right: null,
    leftExtent,
    leftMarkerIds: new Set(),
    rightMarkerIds: new Set(),
    startMarkerIds: new Set(),
    endMarkerIds: new Set(),
    priority: 0
  }
}

function isMarkerEndpoint (node) {
  return node.startMarkerIds.size > 0 || node.endMarkerIds.size > 0
}

class Iterator {
  constructor (markerIndex) {
    this.markerIndex = markerIndex
    this.node = null
    this.nodePosition = ZERO
    this.leftAncestorPosition = ZERO
    this.rightAncestorPosition = MAX_POINT
    this.leftAncestorPositionStack = []
    this.rightAncestorPositionStack = []
  }

  reset () {
    this.node = this.markerIndex.root
    if (this.node) {
      this.nodePosition = this.node.leftExtent
    } else {
      this.nodePosition = ZERO
    }
    this.leftAncestorPosition = ZERO
    this.rightAncestorPosition = MAX_POINT
    this.leftAncestorPositionStack = []
    this.rightAncestorPositionStack = []
  }

  cacheNodePosition () {
    this.markerIndex._nodePositionCache.set(this.node, this.nodePosition)
  }

  descendLeft () {
    this.leftAncestorPositionStack.push(this.leftAncestorPosition)
    this.rightAncestorPositionStack.push(this.rightAncestorPosition)
    this.rightAncestorPosition = this.nodePosition
    this.node = this.node.left
    this.nodePosition = traverse(this.leftAncestorPosition, this.node.leftExtent)
  }

  descendRight () {
    this.leftAncestorPositionStack.push(this.leftAncestorPosition)
    this.rightAncestorPositionStack.push(this.rightAncestorPosition)
    this.leftAncestorPosition = this.nodePosition
    this.node = this.node.right
    this.nodePosition = traverse(this.leftAncestorPosition, this.node.leftExtent)
  }

  ascend () {
    if (this.node.parent) {
      if (this.node.parent.left === this.node) {
        this.nodePosition = this.rightAncestorPosition
      } else {
        this.nodePosition = this.leftAncestorPosition
      }
      this.leftAncestorPosition = this.leftAncestorPositionStack.pop()
      this.rightAncestorPosition = this.rightAncestorPositionStack.pop()
      this.node = this.node.parent
    } else {
      this.node = null
      this.nodePosition = ZERO
      this.leftAncestorPosition = ZERO
      this.rightAncestorPosition = MAX_POINT
    }
  }

  moveToSuccessor () {
    if (!this.node) return
    if (this.node.right) {
      this.descendRight()
      while (this.node.left) this.descendLeft()
    } else {
      while (this.node.parent && this.node.parent.right === this.node) this.ascend()
      this.ascend()
    }
  }

  seekToFirstNodeGte (position) {
    while (true) {
      this.cacheNodePosition()
      const c = cmp(position, this.nodePosition)
      if (c === 0) {
        break
      } else if (c < 0) {
        if (this.node.left) {
          this.descendLeft()
        } else {
          break
        }
      } else {
        if (this.node.right) {
          this.descendRight()
        } else {
          break
        }
      }
    }
    if (cmp(this.nodePosition, position) < 0) this.moveToSuccessor()
  }

  markRight (id, startPosition, endPosition) {
    if (cmp(this.leftAncestorPosition, startPosition) < 0 &&
        cmp(startPosition, this.nodePosition) <= 0 &&
        cmp(this.rightAncestorPosition, endPosition) <= 0) {
      this.node.rightMarkerIds.add(id)
    }
  }

  markLeft (id, startPosition, endPosition) {
    if (!isZero(this.nodePosition) &&
        cmp(startPosition, this.leftAncestorPosition) <= 0 &&
        cmp(this.nodePosition, endPosition) <= 0) {
      this.node.leftMarkerIds.add(id)
    }
  }

  insertLeftChild (position) {
    const newNode = makeNode(this.node, traversal(position, this.leftAncestorPosition))
    this.node.left = newNode
    return newNode
  }

  insertRightChild (position) {
    const newNode = makeNode(this.node, traversal(position, this.nodePosition))
    this.node.right = newNode
    return newNode
  }

  insertMarkerStart (id, startPosition, endPosition) {
    this.reset()
    if (!this.node) {
      return (this.markerIndex.root = makeNode(null, startPosition))
    }

    while (true) {
      const c = cmp(startPosition, this.nodePosition)
      if (c === 0) {
        this.markRight(id, startPosition, endPosition)
        return this.node
      } else if (c < 0) {
        this.markRight(id, startPosition, endPosition)
        if (this.node.left) {
          this.descendLeft()
        } else {
          this.insertLeftChild(startPosition)
          this.descendLeft()
          this.markRight(id, startPosition, endPosition)
          return this.node
        }
      } else {
        if (this.node.right) {
          this.descendRight()
        } else {
          this.insertRightChild(startPosition)
          this.descendRight()
          this.markRight(id, startPosition, endPosition)
          return this.node
        }
      }
    }
  }

  insertMarkerEnd (id, startPosition, endPosition) {
    this.reset()
    if (!this.node) {
      return (this.markerIndex.root = makeNode(null, endPosition))
    }

    while (true) {
      const c = cmp(endPosition, this.nodePosition)
      if (c === 0) {
        this.markLeft(id, startPosition, endPosition)
        return this.node
      } else if (c < 0) {
        if (this.node.left) {
          this.descendLeft()
        } else {
          this.insertLeftChild(endPosition)
          this.descendLeft()
          this.markLeft(id, startPosition, endPosition)
          return this.node
        }
      } else {
        this.markLeft(id, startPosition, endPosition)
        if (this.node.right) {
          this.descendRight()
        } else {
          this.insertRightChild(endPosition)
          this.descendRight()
          this.markLeft(id, startPosition, endPosition)
          return this.node
        }
      }
    }
  }

  insertSpliceBoundary (position, isInsertionEnd) {
    this.reset()

    while (true) {
      const c = cmp(position, this.nodePosition)
      if (c === 0 && !isInsertionEnd) {
        return this.node
      } else if (c < 0) {
        if (this.node.left) {
          this.descendLeft()
        } else {
          this.insertLeftChild(position)
          return this.node.left
        }
      } else {
        if (this.node.right) {
          this.descendRight()
        } else {
          this.insertRightChild(position)
          return this.node.right
        }
      }
    }
  }

  checkIntersection (start, end, result) {
    if (cmp(this.leftAncestorPosition, end) <= 0 && cmp(start, this.nodePosition) <= 0) {
      for (const id of this.node.leftMarkerIds) result.add(id)
    }
    if (cmp(start, this.nodePosition) <= 0 && cmp(this.nodePosition, end) <= 0) {
      for (const id of this.node.startMarkerIds) result.add(id)
      for (const id of this.node.endMarkerIds) result.add(id)
    }
    if (cmp(this.nodePosition, end) <= 0 && cmp(start, this.rightAncestorPosition) <= 0) {
      for (const id of this.node.rightMarkerIds) result.add(id)
    }
  }

  findIntersecting (start, end, result) {
    this.reset()
    if (!this.node) return

    while (true) {
      this.cacheNodePosition()
      if (cmp(start, this.nodePosition) < 0) {
        if (this.node.left) {
          this.checkIntersection(start, end, result)
          this.descendLeft()
        } else {
          break
        }
      } else {
        if (this.node.right) {
          this.checkIntersection(start, end, result)
          this.descendRight()
        } else {
          break
        }
      }
    }

    do {
      this.checkIntersection(start, end, result)
      this.moveToSuccessor()
      if (this.node) this.cacheNodePosition()
    } while (this.node && cmp(this.nodePosition, end) <= 0)
  }

  findContainedIn (start, end, result) {
    this.reset()
    if (!this.node) return

    this.seekToFirstNodeGte(start)

    const started = new Set()
    while (this.node && cmp(this.nodePosition, end) <= 0) {
      for (const id of this.node.startMarkerIds) started.add(id)
      for (const id of this.node.endMarkerIds) {
        if (started.has(id)) result.add(id)
      }
      this.cacheNodePosition()
      this.moveToSuccessor()
    }
  }

  findStartingIn (start, end, result) {
    this.reset()
    if (!this.node) return

    this.seekToFirstNodeGte(start)

    while (this.node && cmp(this.nodePosition, end) <= 0) {
      for (const id of this.node.startMarkerIds) result.add(id)
      this.cacheNodePosition()
      this.moveToSuccessor()
    }
  }

  findEndingIn (start, end, result) {
    this.reset()
    if (!this.node) return

    this.seekToFirstNodeGte(start)

    while (this.node && cmp(this.nodePosition, end) <= 0) {
      for (const id of this.node.endMarkerIds) result.add(id)
      this.cacheNodePosition()
      this.moveToSuccessor()
    }
  }

  findBoundariesAfter (start, maxCount, result) {
    this.reset()
    if (!this.node) return

    // Navigate to find markers containing start
    while (true) {
      this.cacheNodePosition()

      if (cmp(start, this.nodePosition) <= 0) {
        if (cmp(this.leftAncestorPosition, start) < 0) {
          for (const id of this.node.leftMarkerIds) {
            result.containingStart.push(id)
          }
        }
        if (this.node.left) {
          this.descendLeft()
        } else {
          break
        }
      } else {
        if (cmp(this.rightAncestorPosition, start) >= 0) {
          for (const id of this.node.rightMarkerIds) {
            result.containingStart.push(id)
          }
        }
        if (this.node.right) {
          this.descendRight()
        } else {
          break
        }
      }
    }

    // Sort containingStart by marker comparison
    result.containingStart.sort((a, b) => {
      const c = this.markerIndex.compare(a, b)
      return c === 0 ? a - b : c
    })

    if (cmp(this.nodePosition, start) < 0) this.moveToSuccessor()

    while (this.node && maxCount > 0) {
      this.cacheNodePosition()
      result.boundaries.push({
        position: this.nodePosition,
        starting: new Set(this.node.startMarkerIds),
        ending: new Set(this.node.endMarkerIds)
      })
      this.moveToSuccessor()
      maxCount--
    }
  }

  dump () {
    this.reset()
    const snapshot = {}

    if (!this.node) return snapshot

    while (this.node && this.node.left) {
      this.cacheNodePosition()
      this.descendLeft()
    }

    while (this.node) {
      for (const id of this.node.startMarkerIds) {
        snapshot[id] = {start: this.nodePosition, end: null}
      }
      for (const id of this.node.endMarkerIds) {
        if (snapshot[id]) snapshot[id].end = this.nodePosition
        else snapshot[id] = {start: null, end: this.nodePosition}
      }
      this.cacheNodePosition()
      this.moveToSuccessor()
    }

    return snapshot
  }
}

class MarkerIndex {
  constructor (seed) {
    this._rng = makeRng(seed || 0)
    this.root = null
    this._startNodesById = new Map()
    this._endNodesById = new Map()
    this.iterator = new Iterator(this)
    this._exclusiveMarkerIds = new Set()
    this._nodePositionCache = new Map()
  }

  _generateRandomNumber () {
    return this._rng()
  }

  _getNodePosition (node) {
    if (this._nodePositionCache.has(node)) {
      return this._nodePositionCache.get(node)
    }
    let position = node.leftExtent
    let current = node
    while (current.parent) {
      if (current.parent.right === current) {
        position = traverse(current.parent.leftExtent, position)
      }
      current = current.parent
    }
    this._nodePositionCache.set(node, position)
    return position
  }

  _bubbleNodeUp (node) {
    while (node.parent && node.priority < node.parent.priority) {
      if (node === node.parent.left) {
        this._rotateRight(node)
      } else {
        this._rotateLeft(node)
      }
    }
  }

  _bubbleNodeDown (node) {
    while (true) {
      const leftPriority = node.left ? node.left.priority : Infinity
      const rightPriority = node.right ? node.right.priority : Infinity

      if (leftPriority < rightPriority && leftPriority < node.priority) {
        this._rotateRight(node.left)
      } else if (rightPriority < node.priority) {
        this._rotateLeft(node.right)
      } else {
        break
      }
    }
  }

  // pivot is root's right child; pivot goes up, root goes left
  _rotateLeft (pivot) {
    const root = pivot.parent
    if (root.parent) {
      if (root.parent.left === root) root.parent.left = pivot
      else root.parent.right = pivot
    } else {
      this.root = pivot
    }
    pivot.parent = root.parent

    root.right = pivot.left
    if (root.right) root.right.parent = root

    pivot.left = root
    root.parent = pivot

    // Update leftExtent: pivot.leftExtent = root.leftExtent + pivot.leftExtent
    pivot.leftExtent = traverse(root.leftExtent, pivot.leftExtent)
    // root.leftExtent stays unchanged

    // Update marker IDs (matches C++ rotate_node_left exactly):
    // Add all root.rightMarkerIds to pivot.rightMarkerIds (root.rightMarkerIds NOT cleared)
    for (const id of root.rightMarkerIds) {
      pivot.rightMarkerIds.add(id)
    }

    // pivot's leftMarkerIds: those also in root.leftMarkerIds → remove from root (stay in pivot);
    // those NOT in root.leftMarkerIds → move to root.rightMarkerIds and remove from pivot
    for (const id of [...pivot.leftMarkerIds]) {
      if (root.leftMarkerIds.has(id)) {
        root.leftMarkerIds.delete(id)
        // keep in pivot.leftMarkerIds
      } else {
        pivot.leftMarkerIds.delete(id)
        root.rightMarkerIds.add(id)
      }
    }
  }

  // pivot is root's left child; pivot goes up, root goes right
  _rotateRight (pivot) {
    const root = pivot.parent
    if (root.parent) {
      if (root.parent.left === root) root.parent.left = pivot
      else root.parent.right = pivot
    } else {
      this.root = pivot
    }
    pivot.parent = root.parent

    root.left = pivot.right
    if (root.left) root.left.parent = root

    pivot.right = root
    root.parent = pivot

    // Update leftExtent: root.leftExtent = root.leftExtent - pivot.leftExtent
    root.leftExtent = traversal(root.leftExtent, pivot.leftExtent)
    // pivot.leftExtent stays unchanged

    // Update marker IDs (matches C++ rotate_node_right exactly):
    // For each id in root.leftMarkerIds: if NOT in pivot.startMarkerIds → add to pivot.leftMarkerIds
    // root.leftMarkerIds is NOT cleared; the ones added to pivot.leftMarkerIds stay in root too
    // (C++ does not erase from root.left_marker_ids here)
    for (const id of root.leftMarkerIds) {
      if (!pivot.startMarkerIds.has(id)) {
        pivot.leftMarkerIds.add(id)
      }
    }

    // pivot's rightMarkerIds: those also in root.rightMarkerIds → remove from root (stay in pivot);
    // those NOT in root.rightMarkerIds → move to root.leftMarkerIds and remove from pivot
    for (const id of [...pivot.rightMarkerIds]) {
      if (root.rightMarkerIds.has(id)) {
        root.rightMarkerIds.delete(id)
        // keep in pivot.rightMarkerIds
      } else {
        pivot.rightMarkerIds.delete(id)
        root.leftMarkerIds.add(id)
      }
    }
  }

  _deleteNode (node) {
    this._nodePositionCache.delete(node)
    node.priority = Infinity

    this._bubbleNodeDown(node)

    if (node.parent) {
      if (node.parent.left === node) node.parent.left = null
      else node.parent.right = null
    } else {
      this.root = null
    }
  }

  _deleteSubtree (node) {
    if (!node) return
    const stack = [node]
    while (stack.length > 0) {
      const n = stack.pop()
      this._nodePositionCache.delete(n)
      if (n.left) stack.push(n.left)
      if (n.right) stack.push(n.right)
    }
  }

  _getStartingAndEndingMarkersInSubtree (node, starting, ending) {
    if (!node) return
    const stack = [node]
    while (stack.length > 0) {
      const n = stack.pop()
      for (const id of n.startMarkerIds) starting.add(id)
      for (const id of n.endMarkerIds) ending.add(id)
      if (n.left) stack.push(n.left)
      if (n.right) stack.push(n.right)
    }
  }

  _populateSpliceInvalidationSets (invalidated, startNode, endNode, startingInside, endingInside) {
    // Markers ending at spliceStart or starting at spliceEnd are touched
    for (const id of startNode.endMarkerIds) invalidated.touch.add(id)
    for (const id of endNode.startMarkerIds) invalidated.touch.add(id)

    // Markers spanning the entire splice (in rightMarkerIds of startNode or leftMarkerIds of endNode)
    for (const id of startNode.rightMarkerIds) {
      invalidated.touch.add(id)
      invalidated.inside.add(id)
    }
    for (const id of endNode.leftMarkerIds) {
      invalidated.touch.add(id)
      invalidated.inside.add(id)
    }

    // Markers that start inside the splice
    for (const id of startingInside) {
      invalidated.touch.add(id)
      invalidated.inside.add(id)
      invalidated.overlap.add(id)
      if (endingInside.has(id)) invalidated.surround.add(id)
    }

    // Markers that end inside the splice
    for (const id of endingInside) {
      invalidated.touch.add(id)
      invalidated.inside.add(id)
      invalidated.overlap.add(id)
    }
  }

  insert (id, start, end) {
    const startNode = this.iterator.insertMarkerStart(id, start, end)
    const endNode = this.iterator.insertMarkerEnd(id, start, end)

    this._nodePositionCache.set(startNode, start)
    this._nodePositionCache.set(endNode, end)

    startNode.startMarkerIds.add(id)
    endNode.endMarkerIds.add(id)

    if (startNode.priority === 0) {
      startNode.priority = this._generateRandomNumber()
      this._bubbleNodeUp(startNode)
    }

    if (endNode.priority === 0) {
      endNode.priority = this._generateRandomNumber()
      this._bubbleNodeUp(endNode)
    }

    this._startNodesById.set(id, startNode)
    this._endNodesById.set(id, endNode)
  }

  setExclusive (id, exclusive) {
    if (exclusive) {
      this._exclusiveMarkerIds.add(id)
    } else {
      this._exclusiveMarkerIds.delete(id)
    }
  }

  remove (id) {
    const startNode = this._startNodesById.get(id)
    const endNode = this._endNodesById.get(id)

    // Remove from right marker paths (going up from startNode)
    let node = startNode
    while (node) {
      node.rightMarkerIds.delete(id)
      node = node.parent
    }

    // Remove from left marker paths (going up from endNode)
    node = endNode
    while (node) {
      node.leftMarkerIds.delete(id)
      node = node.parent
    }

    startNode.startMarkerIds.delete(id)
    endNode.endMarkerIds.delete(id)

    if (!isMarkerEndpoint(startNode)) {
      this._deleteNode(startNode)
    }

    if (endNode !== startNode && !isMarkerEndpoint(endNode)) {
      this._deleteNode(endNode)
    }

    this._startNodesById.delete(id)
    this._endNodesById.delete(id)
  }

  has (id) {
    return this._startNodesById.has(id)
  }

  splice (start, oldExtent, newExtent) {
    this._nodePositionCache.clear()

    const invalidated = {
      touch: new Set(),
      inside: new Set(),
      overlap: new Set(),
      surround: new Set()
    }

    if (!this.root ||
        (oldExtent.row === 0 && oldExtent.column === 0 &&
         newExtent.row === 0 && newExtent.column === 0)) {
      return invalidated
    }

    const isInsertion = (oldExtent.row === 0 && oldExtent.column === 0)
    const startNode = this.iterator.insertSpliceBoundary(start, false)
    const endNode = this.iterator.insertSpliceBoundary(traverse(start, oldExtent), isInsertion)

    startNode.priority = -1
    this._bubbleNodeUp(startNode)
    endNode.priority = -2
    this._bubbleNodeUp(endNode)

    const startingInsideSplice = new Set()
    const endingInsideSplice = new Set()

    if (isInsertion) {
      // Move exclusive markers from splice start to splice end
      for (const id of [...startNode.startMarkerIds]) {
        if (this._exclusiveMarkerIds.has(id)) {
          startNode.startMarkerIds.delete(id)
          startNode.rightMarkerIds.delete(id)
          endNode.startMarkerIds.add(id)
          this._startNodesById.set(id, endNode)
        }
      }
      // Move non-exclusive end markers to splice end
      for (const id of [...startNode.endMarkerIds]) {
        if (!this._exclusiveMarkerIds.has(id) || endNode.startMarkerIds.has(id)) {
          startNode.endMarkerIds.delete(id)
          if (!endNode.startMarkerIds.has(id)) {
            startNode.rightMarkerIds.add(id)
          }
          endNode.endMarkerIds.add(id)
          this._endNodesById.set(id, endNode)
        }
      }
    } else {
      this._getStartingAndEndingMarkersInSubtree(startNode.right, startingInsideSplice, endingInsideSplice)

      for (const id of endingInsideSplice) {
        endNode.endMarkerIds.add(id)
        if (!startingInsideSplice.has(id)) {
          startNode.rightMarkerIds.add(id)
        }
        this._endNodesById.set(id, endNode)
      }

      for (const id of endNode.endMarkerIds) {
        if (this._exclusiveMarkerIds.has(id) && !endNode.startMarkerIds.has(id)) {
          endingInsideSplice.add(id)
        }
      }

      for (const id of startingInsideSplice) {
        endNode.startMarkerIds.add(id)
        this._startNodesById.set(id, endNode)
      }

      for (const id of [...startNode.startMarkerIds]) {
        if (this._exclusiveMarkerIds.has(id) && !startNode.endMarkerIds.has(id)) {
          startNode.startMarkerIds.delete(id)
          startNode.rightMarkerIds.delete(id)
          endNode.startMarkerIds.add(id)
          this._startNodesById.set(id, endNode)
          startingInsideSplice.add(id)
        }
      }
    }

    this._populateSpliceInvalidationSets(invalidated, startNode, endNode, startingInsideSplice, endingInsideSplice)

    // Delete subtree between startNode and endNode
    if (startNode.right) {
      this._deleteSubtree(startNode.right)
      startNode.right = null
    }

    endNode.leftExtent = traverse(start, newExtent)

    if (cmp(startNode.leftExtent, endNode.leftExtent) === 0) {
      // Merge start and end nodes
      for (const id of endNode.startMarkerIds) {
        startNode.startMarkerIds.add(id)
        startNode.rightMarkerIds.add(id)
        this._startNodesById.set(id, startNode)
      }
      for (const id of endNode.endMarkerIds) {
        startNode.endMarkerIds.add(id)
        if (endNode.leftMarkerIds.has(id)) {
          startNode.leftMarkerIds.add(id)
          endNode.leftMarkerIds.delete(id)
        }
        this._endNodesById.set(id, startNode)
      }
      this._deleteNode(endNode)
    } else if (isMarkerEndpoint(endNode)) {
      endNode.priority = this._generateRandomNumber()
      this._bubbleNodeDown(endNode)
    } else {
      this._deleteNode(endNode)
    }

    if (isMarkerEndpoint(startNode)) {
      startNode.priority = this._generateRandomNumber()
      this._bubbleNodeDown(startNode)
    } else {
      this._deleteNode(startNode)
    }

    return invalidated
  }

  getStart (id) {
    const node = this._startNodesById.get(id)
    if (!node) return {row: 0, column: 0}
    const p = this._getNodePosition(node)
    return {row: p.row, column: p.column}
  }

  getEnd (id) {
    const node = this._endNodesById.get(id)
    if (!node) return {row: 0, column: 0}
    const p = this._getNodePosition(node)
    return {row: p.row, column: p.column}
  }

  getRange (id) {
    return {start: this.getStart(id), end: this.getEnd(id)}
  }

  compare (id1, id2) {
    const startCmp = cmp(this.getStart(id1), this.getStart(id2))
    if (startCmp !== 0) return startCmp
    return cmp(this.getEnd(id2), this.getEnd(id1))
  }

  findIntersecting (start, end) {
    const result = new Set()
    this.iterator.findIntersecting(start, end, result)
    return result
  }

  findContaining (start, end) {
    const containingStart = new Set()
    this.iterator.findIntersecting(start, start, containingStart)
    if (cmp(end, start) === 0) return containingStart

    const containingEnd = new Set()
    this.iterator.findIntersecting(end, end, containingEnd)

    const result = new Set()
    for (const id of containingStart) {
      if (containingEnd.has(id)) result.add(id)
    }
    return result
  }

  findContainedIn (start, end) {
    const result = new Set()
    this.iterator.findContainedIn(start, end, result)
    return result
  }

  findStartingIn (start, end) {
    const result = new Set()
    this.iterator.findStartingIn(start, end, result)
    return result
  }

  findStartingAt (position) {
    return this.findStartingIn(position, position)
  }

  findEndingIn (start, end) {
    const result = new Set()
    this.iterator.findEndingIn(start, end, result)
    return result
  }

  findEndingAt (position) {
    return this.findEndingIn(position, position)
  }

  findBoundariesAfter (start, maxCount) {
    const result = {containingStart: [], boundaries: []}
    this.iterator.findBoundariesAfter(start, maxCount, result)
    return result
  }

  dump () {
    return this.iterator.dump()
  }
}

module.exports = {MarkerIndex}
