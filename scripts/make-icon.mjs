import sharp from 'sharp'
import { fileURLToPath } from 'node:url'

const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
  <rect width="512" height="512" rx="96" fill="#101418"/>
  <rect x="80" y="160" width="352" height="192" rx="64" fill="#2dd4bf"/>
  <rect x="136" y="244" width="88" height="24" rx="4" fill="#101418"/>
  <rect x="168" y="212" width="24" height="88" rx="4" fill="#101418"/>
  <circle cx="336" cy="236" r="20" fill="#b91c1c"/>
  <circle cx="380" cy="276" r="20" fill="#b91c1c"/>
</svg>`

await sharp(Buffer.from(svg)).png().toFile(fileURLToPath(new URL('../assets/icon.png', import.meta.url)))
console.log('assets/icon.png 512x512')
