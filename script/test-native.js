#!/usr/bin/env node

const fs = require('fs')
const path = require('path')
const {spawnSync} = require('child_process')

const isWindows = process.platform === 'win32'

// MSVC's Debug configuration enables checked iterators
// (`_ITERATOR_DEBUG_LEVEL=2`) and `/RTC1` runtime checks. For a suite this
// STL-heavy that is orders of magnitude slower — tens of minutes on CI, versus
// under two seconds elsewhere. Clang's `-O0` does none of that, so Debug stays
// the default on other platforms, where it costs nothing and keeps the binary
// friendly to `lldb`.
//
// This does not weaken what CI checks: node-gyp never defines `NDEBUG`, so the
// `assert()` calls throughout `src/core` stay live in Release too.
//
// Set SUPERSTRING_TEST_CONFIG to 'Debug' or 'Release' to override — you want
// 'Debug' if you're about to attach a debugger on Windows.
const configuration = process.env.SUPERSTRING_TEST_CONFIG ||
  (isWindows ? 'Release' : 'Debug')
const isDebug = configuration === 'Debug'

const testsPath = path.resolve(
  __dirname, '..', 'build', configuration, isWindows ? 'tests.exe' : 'tests'
)
const dotPath = path.resolve(__dirname, '..', 'build', 'debug.dot')
const htmlPath = path.join(__dirname, '..', 'build', 'debug.html')

if (fs.existsSync(testsPath)) {
  run('node-gyp', isDebug ? ['build', '--debug'] : ['build'])
} else {
  run('node-gyp', isDebug
    ? ['rebuild', '--debug', '--tests']
    : ['rebuild', '--tests'])
}

const args = process.argv.slice(2)

switch (args[0]) {
  case '-d':
  case '--debug':
    args.shift()
    run('lldb', [testsPath, '--', ...args])
    break

  case '-v':
  case '--valgrind':
    args.shift()
    run('valgrind', ['--leak-check=full', testsPath, args[0]])
    break

  case '-s':
  case '--svg':
    args.shift()

    let dotFile = fs.openSync(dotPath, 'w')
    const {status} = spawnSync(testsPath, args, {stdio: ['ignore', 1, dotFile]})
    fs.closeSync(dotFile)

    dotFile = fs.openSync(dotPath, 'r')
    let htmlFile = fs.openSync(htmlPath, 'w')
    fs.writeSync(htmlFile, '<!doctype HTML>\n<style>svg {width: 100%;}</style>\n')
    spawnSync('dot', ['-Tsvg'], {stdio: [dotFile, htmlFile, 2]})
    spawnSync('open', [htmlPath])

    process.exit(status)
    break

  default:
    run(testsPath, args)
    break
}

function run(command, args = [], options = {stdio: 'inherit'}) {
  // `shell` is needed on Windows so that `node-gyp` resolves to `node-gyp.cmd`;
  // CreateProcess cannot launch a batch file directly.
  const {status, error} = spawnSync(command, args, {shell: isWindows, ...options})

  // A failure to spawn at all reports `status: null`, which is not `0` — so the
  // old check handed it to `process.exit`, where Node coerced it to 0 and the
  // run was reported as a success.
  if (error) {
    console.error(`Failed to run ${command}: ${error.message}`)
    process.exit(1)
  }
  if (status !== 0) process.exit(status === null ? 1 : status)
}