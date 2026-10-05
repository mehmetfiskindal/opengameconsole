export const PRG_SIZE = 16384
export const CHR_SIZE = 8192
export const ROM_SIZE = 16 + PRG_SIZE + CHR_SIZE
const PRG_ORIGIN = 0xc000

const BUTTONS = 0x00
const X_POS = 0x01
const Y_POS = 0x02
const SPRITE_PALETTE = 0x03
const BG_INDEX = 0x04
const PREV_BUTTONS = 0x05
const NMI_FLAG = 0x06

const BUTTON_A = 0x80
const BUTTON_B = 0x40
const BUTTON_START_OR_SELECT = 0x30
const BUTTON_UP = 0x08
const BUTTON_DOWN = 0x04
const BUTTON_LEFT = 0x02
const BUTTON_RIGHT = 0x01

export function assemble(origin, program) {
  const bytes = []
  const labels = new Map()
  const fixups = []
  const emit = (...values) => { for (const value of values) bytes.push(value & 0xff) }
  const absolute = (opcode, target) => {
    if (typeof target === 'string') {
      fixups.push({ at: bytes.length + 1, label: target, kind: 'abs' })
      emit(opcode, 0, 0)
    } else {
      emit(opcode, target, target >> 8)
    }
  }
  const relative = (opcode, label) => {
    fixups.push({ at: bytes.length + 1, label, kind: 'rel' })
    emit(opcode, 0)
  }
  const asm = {
    label(name) {
      if (labels.has(name)) throw new Error(`duplicate label: ${name}`)
      labels.set(name, origin + bytes.length)
    },
    bytes: (...values) => emit(...values),
    sei: () => emit(0x78), cld: () => emit(0xd8), clc: () => emit(0x18),
    txs: () => emit(0x9a), txa: () => emit(0x8a), tya: () => emit(0x98), tax: () => emit(0xaa), tay: () => emit(0xa8),
    inx: () => emit(0xe8), dex: () => emit(0xca), dey: () => emit(0x88),
    pha: () => emit(0x48), pla: () => emit(0x68), lsr: () => emit(0x4a), rti: () => emit(0x40),
    ldaImm: (v) => emit(0xa9, v), ldxImm: (v) => emit(0xa2, v), ldyImm: (v) => emit(0xa0, v),
    andImm: (v) => emit(0x29, v), eorImm: (v) => emit(0x49, v), adcImm: (v) => emit(0x69, v), cpxImm: (v) => emit(0xe0, v),
    ldaZp: (z) => emit(0xa5, z), ldxZp: (z) => emit(0xa6, z), staZp: (z) => emit(0x85, z), andZp: (z) => emit(0x25, z),
    incZp: (z) => emit(0xe6, z), decZp: (z) => emit(0xc6, z), rolZp: (z) => emit(0x26, z),
    ldaAbs: (a) => absolute(0xad, a), staAbs: (a) => absolute(0x8d, a), stxAbs: (a) => absolute(0x8e, a),
    bitAbs: (a) => absolute(0x2c, a), ldaAbsX: (a) => absolute(0xbd, a), staAbsX: (a) => absolute(0x9d, a),
    jmp: (a) => absolute(0x4c, a),
    bpl: (label) => relative(0x10, label), bne: (label) => relative(0xd0, label), beq: (label) => relative(0xf0, label),
  }
  program(asm)
  for (const fix of fixups) {
    const target = labels.get(fix.label)
    if (target === undefined) throw new Error(`unknown label: ${fix.label}`)
    if (fix.kind === 'abs') {
      bytes[fix.at] = target & 0xff
      bytes[fix.at + 1] = target >> 8
    } else {
      const offset = target - (origin + fix.at + 1)
      if (offset < -128 || offset > 127) throw new Error(`branch to ${fix.label} out of range: ${offset}`)
      bytes[fix.at] = offset & 0xff
    }
  }
  return { bytes: Uint8Array.from(bytes), labels }
}

function padDemoProgram(a) {
  a.label('reset')
  a.sei(); a.cld()
  a.ldxImm(0x40); a.stxAbs(0x4017)
  a.ldxImm(0xff); a.txs()
  a.inx()
  a.stxAbs(0x2000); a.stxAbs(0x2001); a.stxAbs(0x4010)
  a.label('vblank1'); a.bitAbs(0x2002); a.bpl('vblank1')

  a.label('clearMemory')
  a.ldaImm(0)
  for (const page of [0x0000, 0x0100, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700]) a.staAbsX(page)
  a.ldaImm(0xff); a.staAbsX(0x0200)
  a.inx(); a.bne('clearMemory')
  a.label('vblank2'); a.bitAbs(0x2002); a.bpl('vblank2')

  a.ldaImm(124); a.staZp(X_POS)
  a.ldaImm(112); a.staZp(Y_POS)

  a.ldaAbs(0x2002)
  a.ldaImm(0x3f); a.staAbs(0x2006); a.ldaImm(0x00); a.staAbs(0x2006)
  a.ldxImm(0)
  a.label('paletteLoop'); a.ldaAbsX('palette'); a.staAbs(0x2007); a.inx(); a.cpxImm(32); a.bne('paletteLoop')

  a.ldaImm(0x20); a.staAbs(0x2006); a.ldaImm(0x00); a.staAbs(0x2006)
  a.ldaImm(0); a.ldyImm(4); a.ldxImm(0)
  a.label('nametableLoop'); a.staAbs(0x2007); a.inx(); a.bne('nametableLoop'); a.dey(); a.bne('nametableLoop')

  a.ldaImm(0); a.staAbs(0x2005); a.staAbs(0x2005)
  a.ldaImm(0x80); a.staAbs(0x2000)
  a.ldaImm(0x1e); a.staAbs(0x2001)

  a.label('main')
  a.label('waitNmi'); a.ldaZp(NMI_FLAG); a.beq('waitNmi')
  a.ldaImm(0); a.staZp(NMI_FLAG)

  a.ldaImm(1); a.staAbs(0x4016); a.ldaImm(0); a.staAbs(0x4016)
  a.ldxImm(8)
  a.label('readPad'); a.ldaAbs(0x4016); a.lsr(); a.rolZp(BUTTONS); a.dex(); a.bne('readPad')

  a.ldaZp(BUTTONS); a.andImm(BUTTON_UP); a.beq('noUp'); a.decZp(Y_POS)
  a.label('noUp'); a.ldaZp(BUTTONS); a.andImm(BUTTON_DOWN); a.beq('noDown'); a.incZp(Y_POS)
  a.label('noDown'); a.ldaZp(BUTTONS); a.andImm(BUTTON_LEFT); a.beq('noLeft'); a.decZp(X_POS)
  a.label('noLeft'); a.ldaZp(BUTTONS); a.andImm(BUTTON_RIGHT); a.beq('noRight'); a.incZp(X_POS)
  a.label('noRight'); a.ldaZp(BUTTONS); a.andImm(BUTTON_A); a.beq('noA'); a.ldaImm(0); a.staZp(SPRITE_PALETTE)
  a.label('noA'); a.ldaZp(BUTTONS); a.andImm(BUTTON_B); a.beq('noB'); a.ldaImm(1); a.staZp(SPRITE_PALETTE)
  a.label('noB'); a.ldaZp(PREV_BUTTONS); a.eorImm(0xff); a.andZp(BUTTONS); a.andImm(BUTTON_START_OR_SELECT); a.beq('noToggle')
  a.ldaZp(BG_INDEX); a.eorImm(1); a.staZp(BG_INDEX)
  a.label('noToggle'); a.ldaZp(BUTTONS); a.staZp(PREV_BUTTONS)

  a.ldaZp(Y_POS); a.staAbs(0x0200); a.staAbs(0x0204)
  a.clc(); a.adcImm(8); a.staAbs(0x0208); a.staAbs(0x020c)
  a.ldaImm(1); a.staAbs(0x0201); a.staAbs(0x0205); a.staAbs(0x0209); a.staAbs(0x020d)
  a.ldaZp(SPRITE_PALETTE); a.staAbs(0x0202); a.staAbs(0x0206); a.staAbs(0x020a); a.staAbs(0x020e)
  a.ldaZp(X_POS); a.staAbs(0x0203); a.staAbs(0x020b)
  a.clc(); a.adcImm(8); a.staAbs(0x0207); a.staAbs(0x020f)
  a.jmp('main')

  a.label('nmi')
  a.pha(); a.txa(); a.pha(); a.tya(); a.pha()
  a.ldaImm(0x00); a.staAbs(0x2003); a.ldaImm(0x02); a.staAbs(0x4014)
  a.ldaAbs(0x2002)
  a.ldaImm(0x3f); a.staAbs(0x2006); a.ldaImm(0x00); a.staAbs(0x2006)
  a.ldxZp(BG_INDEX); a.ldaAbsX('bgColors'); a.staAbs(0x2007)
  a.ldaImm(0); a.staAbs(0x2005); a.staAbs(0x2005)
  a.ldaImm(0x80); a.staAbs(0x2000)
  a.ldaImm(1); a.staZp(NMI_FLAG)
  a.pla(); a.tay(); a.pla(); a.tax(); a.pla()
  a.rti()

  a.label('irq')
  a.rti()

  a.label('palette')
  a.bytes(0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00)
  a.bytes(0x10, 0x16, 0x27, 0x30, 0x10, 0x2a, 0x27, 0x30, 0x10, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00)
  a.label('bgColors')
  a.bytes(0x10, 0x21)
}

export function buildPadDemoRom() {
  const { bytes: code, labels } = assemble(PRG_ORIGIN, padDemoProgram)
  if (code.length > PRG_SIZE - 6) throw new Error('pad demo program does not fit in PRG')
  const prg = new Uint8Array(PRG_SIZE).fill(0xff)
  prg.set(code, 0)
  const vector = (offset, name) => {
    const address = labels.get(name)
    prg[offset] = address & 0xff
    prg[offset + 1] = address >> 8
  }
  vector(PRG_SIZE - 6, 'nmi')
  vector(PRG_SIZE - 4, 'reset')
  vector(PRG_SIZE - 2, 'irq')
  const chr = new Uint8Array(CHR_SIZE)
  chr.fill(0xff, 16, 24)
  const rom = new Uint8Array(ROM_SIZE)
  rom.set([0x4e, 0x45, 0x53, 0x1a, 1, 1, 0x01, 0x00], 0)
  rom.set(prg, 16)
  rom.set(chr, 16 + PRG_SIZE)
  return { rom, labels }
}

export function romToC(rom) {
  const rows = []
  for (let offset = 0; offset < rom.length; offset += 16) {
    const row = Array.from(rom.subarray(offset, offset + 16), (b) => `0x${b.toString(16).padStart(2, '0')}`)
    rows.push(`  ${row.join(', ')},`)
  }
  return `/* Generated by scripts/nes/write-pad-demo.mjs. Do not edit. */\n#include <stddef.h>\n\n` +
    `const unsigned char opengameconsole_pad_demo_rom[${rom.length}] = {\n${rows.join('\n')}\n};\n` +
    `const size_t opengameconsole_pad_demo_rom_size = sizeof(opengameconsole_pad_demo_rom);\n`
}
