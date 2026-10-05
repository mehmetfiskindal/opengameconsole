import fs from 'node:fs'
import { buildPadDemoRom, romToC } from './pad-demo.mjs'

const appRoot = new URL('../../', import.meta.url)
const { rom } = buildPadDemoRom()
fs.mkdirSync(new URL('assets/nes/', appRoot), { recursive: true })
fs.mkdirSync(new URL('native/', appRoot), { recursive: true })
fs.writeFileSync(new URL('assets/nes/pad-demo.nes', appRoot), rom)
fs.writeFileSync(new URL('native/pad_demo_rom.c', appRoot), romToC(rom))
console.log(`pad-demo.nes ${rom.length} bytes`)
