// Builds res/app.ico from tools/icon/sizes/*.png
// 16..64 are stored as 32bpp BMP frames (best compatibility), 128/256 as PNG frames.
const fs = require('fs');
const path = require('path');
const zlib = require('zlib');

const root = path.resolve(__dirname, '..', '..');
const dir = path.join(__dirname, 'sizes');

function decodePng(file) {
  const b = fs.readFileSync(file);
  let off = 8, w = 0, h = 0, ct = 0; const idat = [];
  while (off < b.length) {
    const len = b.readUInt32BE(off), type = b.toString('latin1', off + 4, off + 8);
    const data = b.subarray(off + 8, off + 8 + len);
    if (type === 'IHDR') { w = data.readUInt32BE(0); h = data.readUInt32BE(4); ct = data[9]; }
    else if (type === 'IDAT') idat.push(data);
    else if (type === 'IEND') break;
    off += 12 + len;
  }
  const raw = zlib.inflateSync(Buffer.concat(idat));
  const bpp = ct === 6 ? 4 : 3;
  const stride = w * bpp;
  const px = Buffer.alloc(w * h * 4);
  let pos = 0, prev = Buffer.alloc(stride);
  for (let y = 0; y < h; y++) {
    const f = raw[pos++]; const line = raw.subarray(pos, pos + stride); pos += stride;
    const cur = Buffer.alloc(stride);
    for (let x = 0; x < stride; x++) {
      const a = x >= bpp ? cur[x - bpp] : 0, bb = prev[x], c = x >= bpp ? prev[x - bpp] : 0;
      let v = line[x];
      if (f === 1) v += a; else if (f === 2) v += bb; else if (f === 3) v += ((a + bb) >> 1);
      else if (f === 4) {
        const p = a + bb - c;
        const pa = Math.abs(p - a), pb = Math.abs(p - bb), pc = Math.abs(p - c);
        v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? bb : c);
      }
      cur[x] = v & 0xff;
    }
    for (let x = 0; x < w; x++) {
      const si = x * bpp, di = (y * w + x) * 4;
      px[di] = cur[si]; px[di + 1] = cur[si + 1]; px[di + 2] = cur[si + 2];
      px[di + 3] = bpp === 4 ? cur[si + 3] : 255;
    }
    prev = cur;
  }
  return { w, h, px };
}

function bmpFrame(img) {
  const { w, h, px } = img;
  const xorStride = w * 4;
  const maskStride = Math.ceil(w / 32) * 4;
  const header = Buffer.alloc(40);
  header.writeUInt32LE(40, 0);
  header.writeInt32LE(w, 4);
  header.writeInt32LE(h * 2, 8);
  header.writeUInt16LE(1, 12);
  header.writeUInt16LE(32, 14);
  header.writeUInt32LE(0, 16);
  header.writeUInt32LE(xorStride * h + maskStride * h, 20);
  const xor = Buffer.alloc(xorStride * h);
  for (let y = 0; y < h; y++) {
    const srcY = h - 1 - y;
    for (let x = 0; x < w; x++) {
      const s = (srcY * w + x) * 4, d = y * xorStride + x * 4;
      xor[d] = px[s + 2]; xor[d + 1] = px[s + 1]; xor[d + 2] = px[s]; xor[d + 3] = px[s + 3];
    }
  }
  const mask = Buffer.alloc(maskStride * h);
  for (let y = 0; y < h; y++) {
    const srcY = h - 1 - y;
    for (let x = 0; x < w; x++) {
      if (px[(srcY * w + x) * 4 + 3] === 0) mask[y * maskStride + (x >> 3)] |= 0x80 >> (x & 7);
    }
  }
  return Buffer.concat([header, xor, mask]);
}

const sizes = [16, 24, 32, 48, 64, 128, 256];
const entries = sizes.map((s) => {
  const file = path.join(dir, 'icon_' + s + '.png');
  const png = fs.readFileSync(file);
  const img = decodePng(file);
  const data = s >= 128 ? png : bmpFrame(img);
  return { s, data };
});

let offset = 6 + entries.length * 16;
const head = Buffer.alloc(6);
head.writeUInt16LE(0, 0); head.writeUInt16LE(1, 2); head.writeUInt16LE(entries.length, 4);
const dirBuf = Buffer.alloc(entries.length * 16);
entries.forEach((e, i) => {
  const o = i * 16;
  dirBuf[o] = e.s >= 256 ? 0 : e.s;
  dirBuf[o + 1] = e.s >= 256 ? 0 : e.s;
  dirBuf[o + 2] = 0; dirBuf[o + 3] = 0;
  dirBuf.writeUInt16LE(1, o + 4);
  dirBuf.writeUInt16LE(32, o + 6);
  dirBuf.writeUInt32LE(e.data.length, o + 8);
  dirBuf.writeUInt32LE(offset, o + 12);
  offset += e.data.length;
});

const out = path.join(root, 'res', 'app.ico');
fs.writeFileSync(out, Buffer.concat([head, dirBuf, ...entries.map((e) => e.data)]));
console.log('wrote ' + out + ' (' + entries.length + ' frames, ' + offset + ' bytes)');
