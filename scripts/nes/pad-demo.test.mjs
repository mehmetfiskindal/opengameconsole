import { test } from 'node:test'
import assert from 'node:assert/strict'
import fs from 'node:fs'
import { assemble, buildPadDemoRom, romToC, ROM_SIZE, PRG_SIZE } from './pad-demo.mjs'

const appRoot = new URL('../../', import.meta.url)

test('assembler resolves forward and backward branches', () => {
  const { bytes, labels } = assemble(0xc000, (a) => {
    a.label('top')
    a.beq('ahead')
    a.inx()
    a.label('ahead')
    a.bne('top')
    a.jmp('top')
  })
  assert.deepEqual([...bytes], [0xf0, 0x01, 0xe8, 0xd0, 0xfb, 0x4c, 0x00, 0xc0])
  assert.equal(labels.get('ahead'), 0xc003)
})

test('assembler rejects unknown labels and long branches', () => {
  assert.throws(() => assemble(0xc000, (a) => a.jmp('nowhere')), /unknown label: nowhere/)
  assert.throws(() => assemble(0xc000, (a) => {
    a.label('far')
    a.bytes(...new Array(200).fill(0xea))
    a.bne('far')
  }), /branch to far out of range/)
})

test('pad demo is an NROM-128 iNES image', () => {
  const { rom } = buildPadDemoRom()
  assert.equal(rom.length, ROM_SIZE)
  assert.equal(ROM_SIZE, 24592)
  assert.deepEqual([...rom.subarray(0, 8)], [0x4e, 0x45, 0x53, 0x1a, 1, 1, 0x01, 0x00])
})

test('vectors point at the reset, nmi and irq handlers', () => {
  const { rom, labels } = buildPadDemoRom()
  const prg = rom.subarray(16, 16 + PRG_SIZE)
  const word = (offset) => prg[offset] | (prg[offset + 1] << 8)
  const at = (address) => prg[address - 0xc000]
  assert.equal(word(PRG_SIZE - 6), labels.get('nmi'))
  assert.equal(word(PRG_SIZE - 4), labels.get('reset'))
  assert.equal(word(PRG_SIZE - 2), labels.get('irq'))
  assert.equal(at(labels.get('reset')), 0x78)
  assert.equal(at(labels.get('nmi')), 0x48)
  assert.equal(at(labels.get('irq')), 0x40)
})

test('palettes and background colours match the spec', () => {
  const { rom, labels } = buildPadDemoRom()
  const prg = rom.subarray(16, 16 + PRG_SIZE)
  const palette = prg.subarray(labels.get('palette') - 0xc000, labels.get('palette') - 0xc000 + 32)
  assert.equal(palette[0], 0x10)
  assert.equal(palette[16], palette[0])
  assert.equal(palette[17], 0x16)
  assert.equal(palette[21], 0x2a)
  const bg = labels.get('bgColors') - 0xc000
  assert.deepEqual([...prg.subarray(bg, bg + 2)], [0x10, 0x21])
})

test('CHR tile 1 is solid colour 1 and tile 0 is empty', () => {
  const { rom } = buildPadDemoRom()
  const chr = rom.subarray(16 + PRG_SIZE)
  assert.ok(chr.subarray(0, 16).every((b) => b === 0))
  assert.ok(chr.subarray(16, 24).every((b) => b === 0xff))
  assert.ok(chr.subarray(24, 32).every((b) => b === 0))
})

test('C array holds the same bytes', () => {
  const { rom } = buildPadDemoRom()
  const c = romToC(rom)
  assert.match(c, /const unsigned char opengameconsole_pad_demo_rom\[24592\] = \{\n  0x4e, 0x45, 0x53, 0x1a,/)
  assert.match(c, /const size_t opengameconsole_pad_demo_rom_size = sizeof\(opengameconsole_pad_demo_rom\);/)
})

test('committed ROM files are up to date', () => {
  const { rom } = buildPadDemoRom()
  const nes = fs.readFileSync(new URL('assets/nes/pad-demo.nes', appRoot))
  assert.deepEqual(new Uint8Array(nes), rom, 'run npm run rom')
  const c = fs.readFileSync(new URL('native/pad_demo_rom.c', appRoot), 'utf8').replaceAll('\r\n', '\n')
  assert.equal(c, romToC(rom), 'run npm run rom')
})
