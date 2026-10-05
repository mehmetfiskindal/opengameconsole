import { noPluginCapabilities } from '@geastack/compiler/plugin'

const preamble = ['#include "nes_host.h"']
const hostFunctions = [
  ['nesRefreshRoms', 'opengameconsole::nes::refreshRoms'],
  ['nesRomName', 'opengameconsole::nes::romName'],
  ['nesPlay', 'opengameconsole::nes::play'],
  ['nesSetButton', 'opengameconsole::nes::setButton'],
  ['nesStop', 'opengameconsole::nes::stop'],
]

export default {
  name: 'opengameconsole-nes-host',
  instantiate: () => ({
    producers: () => [],
    lower: () => false,
    capabilities: {
      ...noPluginCapabilities,
      hostFunctions: new Map(hostFunctions),
      hostPreambles: new Map(hostFunctions.map(([, spelling]) => [spelling, preamble])),
    },
  }),
}
