// Development helper: crop + upscale a region of a PNG so the UI can be
// inspected at pixel level.  usage: node tools/pngcrop.mjs in.png out.png x y w h zoom
import { readFileSync, writeFileSync } from 'node:fs';
import { inflateSync, deflateSync } from 'node:zlib';

function decode(file) {
  const b = readFileSync(file);
  let o = 8, W, H, bd, ct, inter;
  const idat = [];
  while (o < b.length) {
    const len = b.readUInt32BE(o);
    const type = b.toString('ascii', o + 4, o + 8);
    const d = b.subarray(o + 8, o + 8 + len);
    if (type === 'IHDR') { W = d.readUInt32BE(0); H = d.readUInt32BE(4); bd = d[8]; ct = d[9]; inter = d[12]; }
    else if (type === 'IDAT') idat.push(d);
    else if (type === 'IEND') break;
    o += 12 + len;
  }
  if (bd !== 8 || inter !== 0) throw new Error('only 8-bit non-interlaced PNGs');
  const ch = ct === 6 ? 4 : ct === 2 ? 3 : 1;
  const raw = inflateSync(Buffer.concat(idat));
  const stride = W * ch;
  const out = Buffer.alloc(W * H * ch);
  let p = 0;
  for (let y = 0; y < H; y++) {
    const ft = raw[p++];
    const line = raw.subarray(p, p + stride); p += stride;
    const cur = out.subarray(y * stride, (y + 1) * stride);
    const prev = y > 0 ? out.subarray((y - 1) * stride, y * stride) : Buffer.alloc(stride);
    for (let x = 0; x < stride; x++) {
      const a = x >= ch ? cur[x - ch] : 0, bb = prev[x], c = x >= ch ? prev[x - ch] : 0;
      let v = line[x];
      if (ft === 1) v += a; else if (ft === 2) v += bb; else if (ft === 3) v += (a + bb) >> 1;
      else if (ft === 4) { const pa = Math.abs(bb - c), pb = Math.abs(a - c), pc = Math.abs(a + bb - 2 * c); v += (pa <= pb && pa <= pc) ? a : (pb <= pc ? bb : c); }
      cur[x] = v & 255;
    }
  }
  return { W, H, ch, out };
}

function encode(W, H, rgba) {
  const stride = W * 4;
  const raw = Buffer.alloc((stride + 1) * H);
  for (let y = 0; y < H; y++) {
    raw[y * (stride + 1)] = 0;
    rgba.copy(raw, y * (stride + 1) + 1, y * stride, (y + 1) * stride);
  }
  const chunks = [];
  const chunk = (type, data) => {
    const len = Buffer.alloc(4); len.writeUInt32BE(data.length);
    const body = Buffer.concat([Buffer.from(type, 'ascii'), data]);
    const crc = Buffer.alloc(4); crc.writeUInt32BE(crc32(body) >>> 0);
    chunks.push(len, body, crc);
  };
  const ihdr = Buffer.alloc(13);
  ihdr.writeUInt32BE(W, 0); ihdr.writeUInt32BE(H, 4); ihdr[8] = 8; ihdr[9] = 6;
  chunk('IHDR', ihdr);
  chunk('IDAT', deflateSync(raw, { level: 9 }));
  chunk('IEND', Buffer.alloc(0));
  return Buffer.concat([Buffer.from([137, 80, 78, 71, 13, 10, 26, 10]), ...chunks]);
}

let crcTable = null;
function crc32(buf) {
  if (!crcTable) {
    crcTable = new Int32Array(256);
    for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1; crcTable[n] = c; }
  }
  let c = -1;
  for (let i = 0; i < buf.length; i++) c = crcTable[(c ^ buf[i]) & 255] ^ (c >>> 8);
  return c ^ -1;
}

const [inFile, outFile, sx, sy, sw, sh, zoom] = process.argv.slice(2);
const X = +sx, Y = +sy, W = +sw, H = +sh, Z = +(zoom || 1);
const im = decode(inFile);
const ow = W * Z, oh = H * Z;
const out = Buffer.alloc(ow * oh * 4);
for (let y = 0; y < oh; y++) {
  for (let x = 0; x < ow; x++) {
    const sxi = Math.min(im.W - 1, X + Math.floor(x / Z));
    const syi = Math.min(im.H - 1, Y + Math.floor(y / Z));
    const si = (syi * im.W + sxi) * im.ch;
    const di = (y * ow + x) * 4;
    out[di] = im.out[si]; out[di + 1] = im.out[si + 1]; out[di + 2] = im.out[si + 2];
    out[di + 3] = im.ch === 4 ? im.out[si + 3] : 255;
  }
}
writeFileSync(outFile, encode(ow, oh, out));
console.log(`wrote ${outFile} (${ow}x${oh})`);
