const {TextBuffer} = require('./src/js/text-buffer')
const {Patch} = require('./src/js/patch')
const {MarkerIndex} = require('./src/js/marker-index')

// The pure-JS Patch.prototype.splice returns false on failure.
// Public callers expect a throw, so wrap it here.
// combine() internally calls _spliceInternal so it is unaffected.
const {splice} = Patch.prototype
Patch.prototype.splice = Object.assign(function () {
  if (!splice.apply(this, arguments)) {
    throw new Error('Patch does not apply')
  }
}, splice)

module.exports = {TextBuffer, Patch, MarkerIndex}
