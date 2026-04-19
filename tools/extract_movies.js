#!/usr/bin/env node
/**
 * extract_movies.js — Extract movie clips from 3DO DataStream (ice.bigstream)
 *
 * Each movie is stored as interleaved FILM (Cinepak video) and SNDS (SDX2 audio)
 * chunks within the DataStream. This tool extracts each movie into a simple
 * binary format (.icm = Icebreaker Cinema Movie):
 *
 *   Header (32 bytes):
 *     +0:  magic "ICM1" (4 bytes)
 *     +4:  video width (uint16 BE)
 *     +6:  video height (uint16 BE)
 *     +8:  frame count (uint32 BE)
 *     +12: audio sample rate (uint32 BE)
 *     +16: audio channels (uint16 BE)
 *     +18: reserved (2 bytes, 0)
 *     +20: frame table offset (uint32 BE) — offset to frame index table
 *     +24: audio data offset (uint32 BE) — offset to raw PCM16-LE audio
 *     +28: audio data size (uint32 BE)
 *
 *   Frame Table (frameCount × 8 bytes):
 *     Per frame: offset (uint32 BE), size (uint32 BE) — pointing into Cinepak data section
 *
 *   Cinepak Data:
 *     Raw Cinepak frame data, concatenated
 *
 *   Audio Data:
 *     Raw PCM16-LE audio (decoded from SDX2)
 *
 * Usage: node tools/extract_movies.js [input_bigstream] [output_dir]
 */

const fs = require('fs');
const path = require('path');

const inputFile  = process.argv[2] || 'iso_assets/IceFiles/Music/ice.bigstream';
const outputDir  = process.argv[3] || 'assets/Movies';

/* ── Movie definitions ───────────────────────────────────────────────── */

const MOVIES = [
    { id: 0,  name: 'movie00_welcome',   marker: 22151168 },
    { id: 1,  name: 'movie01_pits',      marker: 26771456 },
    { id: 2,  name: 'movie02_purple',    marker: 27623424 },
    { id: 3,  name: 'movie03_pink',      marker: 28409856 },
    { id: 4,  name: 'movie04_rainbow',   marker: 29655040 },
    { id: 5,  name: 'movie05_cyanide',   marker: 33259520 },
    { id: 6,  name: 'movie06_ice',       marker: 34111488 },
    { id: 7,  name: 'movie07_limeys',    marker: 37421056 },
    { id: 8,  name: 'movie08_rocks',     marker: 38535168 },
    { id: 9,  name: 'movie09_concrete',  marker: 40665088 },
    { id: 10, name: 'movie10_lava',      marker: 42401792 },
    { id: 11, name: 'movie11_chameleon', marker: 43024384 },
    { id: 12, name: 'movie12_slime',     marker: 44171264 },
    { id: 13, name: 'movie13_lurkers',   marker: 46235648 },
    { id: 14, name: 'movie14_swamp',     marker: 47480832 },
    { id: 15, name: 'movie15_zombie',    marker: 49119232 },
    { id: 16, name: 'movie16_meany',     marker: 51019776 },
    { id: 17, name: 'movie17_victory',   marker: 52330496 },
];

/* ── 3DO DataStream chunk types ──────────────────────────────────────── */

const CHUNK_FILM = 0x46494C4D; // 'FILM'
const CHUNK_SNDS = 0x534E4453; // 'SNDS'
const CHUNK_CTRL = 0x4354524C; // 'CTRL'

const SUB_FHDR   = 0x46484452; // 'FHDR' — film header
const SUB_FRME   = 0x46524D45; // 'FRME' — film frame
const SUB_SSMP   = 0x53534D50; // 'SSMP' — sound sample data
const SUB_SHDR   = 0x53484452; // 'SHDR' — sound header
const CTRL_GOTO  = 0x474F544F; // 'GOTO'

/* ── SDX2 Decoder (same as extract_music.js) ─────────────────────────── */

const SDX2_TABLE = new Int16Array(256);
for (let i = -128; i < 128; i++) {
    const square = i * i * 2;
    SDX2_TABLE[i + 128] = i < 0 ? -square : square;
}

function decodeSdx2(sdx2Buf, numBytes, numChannels, accum) {
    const pcm = Buffer.alloc(numBytes * 2);
    let writeOff = 0;
    let ch = 0;
    for (let i = 0; i < numBytes; i++) {
        const raw = sdx2Buf[i];
        const n = raw > 127 ? raw - 256 : raw;
        if (!(n & 1)) accum[ch] = 0;
        accum[ch] += SDX2_TABLE[n + 128];
        if (accum[ch] > 32767)  accum[ch] = 32767;
        if (accum[ch] < -32768) accum[ch] = -32768;
        pcm.writeInt16LE(accum[ch], writeOff);
        writeOff += 2;
        ch ^= (numChannels - 1);
    }
    return pcm;
}

/* ── Movie Extractor ─────────────────────────────────────────────────── */

function extractMovie(buf, startOffset, nextOffset) {
    const frames = [];         // Array of { data: Buffer } — raw Cinepak frame data
    let videoWidth = 288;
    let videoHeight = 216;
    const pcmBuffers = [];
    let sampleRate = 44100;
    let numChannels = 2;
    const accum = new Int32Array(2);

    let pos = startOffset;
    const endPos = nextOffset || buf.length;
    let foundFirstCtrl = false;

    while (pos + 8 <= endPos) {
        const type = buf.readUInt32BE(pos);
        const size = buf.readUInt32BE(pos + 4);

        if (size < 8 || size > 0x200000) {
            // Skip to next 32KB boundary
            const nextBlock = ((pos >> 15) + 1) << 15;
            if (nextBlock <= pos || nextBlock >= endPos) break;
            pos = nextBlock;
            continue;
        }

        // Stop at CTRL/GOTO that loops back (end of movie)
        if (type === CHUNK_CTRL && size >= 20) {
            const ctrlSubType = buf.readUInt32BE(pos + 16);
            if (ctrlSubType === CTRL_GOTO && (frames.length > 0 || pcmBuffers.length > 0)) {
                break;
            }
        }

        // FILM chunks — video
        if (type === CHUNK_FILM && size >= 20) {
            const subType = buf.readUInt32BE(pos + 16);

            if (subType === SUB_FHDR && size >= 40) {
                // Film header: +24='cvid', +28=height, +32=width
                videoHeight = buf.readUInt32BE(pos + 28) & 0xFFFF;
                videoWidth  = buf.readUInt32BE(pos + 32) & 0xFFFF;
            } else if (subType === SUB_FRME && size >= 32) {
                // Film frame: Cinepak data starts at +28
                const cpakDataStart = pos + 28;
                const cpakDataEnd = pos + size;
                if (cpakDataEnd <= buf.length && cpakDataEnd > cpakDataStart) {
                    frames.push(buf.slice(cpakDataStart, cpakDataEnd));
                }
            }
        }

        // SNDS chunks — audio
        if (type === CHUNK_SNDS && size >= 24) {
            const subType = buf.readUInt32BE(pos + 16);

            if (subType === SUB_SHDR && size >= 56) {
                sampleRate  = buf.readUInt32BE(pos + 44);
                numChannels = buf.readUInt32BE(pos + 48);
            } else if (subType === SUB_SSMP) {
                const actualSize = buf.readUInt32BE(pos + 20);
                const dataStart = pos + 24;
                const dataEnd = Math.min(dataStart + actualSize, pos + size);
                if (dataEnd > dataStart) {
                    const sdx2Data = buf.slice(dataStart, dataEnd);
                    pcmBuffers.push(decodeSdx2(sdx2Data, sdx2Data.length, numChannels, accum));
                }
            }
        }

        pos += size;
        pos = (pos + 3) & ~3;
    }

    return { frames, videoWidth, videoHeight, pcmBuffers, sampleRate, numChannels };
}

/* ── ICM File Writer ─────────────────────────────────────────────────── */

function writeIcm(filename, movie) {
    const { frames, videoWidth, videoHeight, pcmBuffers, sampleRate, numChannels } = movie;
    const pcmData = Buffer.concat(pcmBuffers);

    // Calculate sizes
    const headerSize = 32;
    const frameTableSize = frames.length * 8;
    const frameTableOffset = headerSize;

    // Cinepak data follows frame table
    let cpakOffset = headerSize + frameTableSize;
    const frameEntries = [];
    for (const frame of frames) {
        frameEntries.push({ offset: cpakOffset, size: frame.length });
        cpakOffset += frame.length;
    }

    const audioDataOffset = cpakOffset;
    const audioDataSize = pcmData.length;
    const totalSize = audioDataOffset + audioDataSize;

    const out = Buffer.alloc(totalSize);
    let off = 0;

    // Header
    out.write('ICM1', off); off += 4;
    out.writeUInt16BE(videoWidth, off); off += 2;
    out.writeUInt16BE(videoHeight, off); off += 2;
    out.writeUInt32BE(frames.length, off); off += 4;
    out.writeUInt32BE(sampleRate, off); off += 4;
    out.writeUInt16BE(numChannels, off); off += 2;
    out.writeUInt16BE(0, off); off += 2; // reserved
    out.writeUInt32BE(frameTableOffset, off); off += 4;
    out.writeUInt32BE(audioDataOffset, off); off += 4;
    out.writeUInt32BE(audioDataSize, off); off += 4;

    // Frame table
    for (const entry of frameEntries) {
        out.writeUInt32BE(entry.offset, off); off += 4;
        out.writeUInt32BE(entry.size, off); off += 4;
    }

    // Cinepak data
    for (const frame of frames) {
        frame.copy(out, off);
        off += frame.length;
    }

    // Audio data
    pcmData.copy(out, off);

    fs.writeFileSync(filename, out);

    const durationSec = (pcmData.length / (sampleRate * numChannels * 2)).toFixed(1);
    console.log(`  → ${filename} (${(totalSize / 1024).toFixed(0)} KB, ${frames.length} frames, ${durationSec}s audio)`);
}

/* ── Main ────────────────────────────────────────────────────────────── */

function main() {
    console.log(`Reading: ${inputFile}`);
    const buf = fs.readFileSync(inputFile);
    console.log(`File size: ${buf.length} bytes (${(buf.length / 1024 / 1024).toFixed(1)} MB)\n`);

    fs.mkdirSync(outputDir, { recursive: true });

    console.log(`Extracting ${MOVIES.length} movies...\n`);

    // Sort by marker to determine end boundaries
    const sorted = [...MOVIES].sort((a, b) => a.marker - b.marker);

    for (let i = 0; i < sorted.length; i++) {
        const movie = sorted[i];
        const nextOffset = (i + 1 < sorted.length) ? sorted[i + 1].marker : buf.length;

        if (movie.marker >= buf.length) {
            console.log(`  SKIP ${movie.name}: marker beyond file end`);
            continue;
        }

        process.stdout.write(`  [${movie.id.toString().padStart(2)}] ${movie.name.padEnd(24)}`);

        const result = extractMovie(buf, movie.marker, nextOffset);

        if (result.frames.length === 0) {
            console.log(' — NO VIDEO DATA FOUND');
            continue;
        }

        const outPath = path.join(outputDir, `${movie.name}.icm`);
        writeIcm(outPath, result);
    }

    console.log('\nDone!');
}

main();
