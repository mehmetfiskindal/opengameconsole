import { test } from 'node:test'
import assert from 'node:assert/strict'
import { fileURLToPath } from 'node:url'
import { loadCliPlugins } from '@geastack/compiler/dist/plugins/load.js'

const pluginPath = fileURLToPath(new URL('./nes-host-plugin.mjs', import.meta.url))

test('geatsc accepts the nes host plugin and maps the three calls', async () => {
  const plugins = await loadCliPlugins([pluginPath])
  const plugin = plugins.find((item) => item.name === 'opengameconsole-nes-host')
  assert.ok(plugin, 'plugin loaded')
  const { capabilities } = plugin.instantiate(new Map())
  assert.deepEqual([...capabilities.hostFunctions], [
    ['nesPlay', 'opengameconsole::nes::play'],
    ['nesSetButton', 'opengameconsole::nes::setButton'],
    ['nesStop', 'opengameconsole::nes::stop'],
  ])
  for (const spelling of capabilities.hostFunctions.values()) {
    assert.deepEqual(capabilities.hostPreambles.get(spelling), ['#include "nes_host.h"'])
  }
})
