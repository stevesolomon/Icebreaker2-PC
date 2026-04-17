#!/usr/bin/env node
/**
 * extract_music.js — Extract music tracks from 3DO DataStream (ice.bigstream)
 *
 * The 3DO DataStream format multiplexes audio/video/control chunks.
 * Music is stored as SDX2-compressed stereo audio at 44100 Hz.
 * Each track is delimited by CTRL SYNC/GOTO markers.
 *
 * Usage: node tools/extract_music.js [input_bigstream] [output_dir]
 *   Defaults: iso_assets/IceFiles/Music/ice.bigstream → assets/Music/
 */

const fs = require('fs');
const path = require('path');

/* ── Configuration ───────────────────────────────────────────────────── */

const inputFile  = process.argv[2] || 'iso_assets/IceFiles/Music/ice.bigstream';
const outputDir  = process.argv[3] || 'assets/Music';

/* Track definitions: marker byte-offsets into the bigstream.
 * These come directly from the original PlayMusic.h */
const TRACKS = [
    { id:  1, name: 'track01', label: 'QUACK',              marker:    98304 },
    { id:  2, name: 'track02', label: 'CHECK_THIS_OUT_TALK', marker:   589824 },
    { id:  3, name: 'track03', label: 'MADONNA',            marker:  1376256 },
    { id:  4, name: 'track04', label: 'SPACE_AGE',          marker:  1835008 },
    { id:  5, name: 'track05', label: 'SOUND_OF_TALK',      marker: 54853632 },
    { id:  6, name: 'track06', label: 'LOTS_OF_PERC',       marker: 55738368 },
    { id:  7, name: 'track07', label: 'DRUNK_TRUMPET',      marker:  3997696 },
    { id:  8, name: 'track08', label: 'MONKEY',             marker:  4456448 },
    { id:  9, name: 'track09', label: 'THE_LONGER_ONE',     marker:  5373952 },
    { id: 10, name: 'track10', label: 'MORE_QUACK',         marker:  6160384 },
    { id: 11, name: 'track11', label: 'SEVENTIES2',         marker:  7143424 },
    { id: 12, name: 'track12', label: 'SHAFT',              marker:  7929856 },
    { id: 13, name: 'track13', label: 'HIT_ME',             marker:  9175040 },
    { id: 14, name: 'track14', label: 'WATER_WORKS',        marker: 10420224 },
    { id: 15, name: 'track15', label: 'FAST_HUNT',          marker: 11665408 },
    { id: 16, name: 'track16', label: 'G_BOUNCE',           marker: 12910592 },
    { id: 17, name: 'track17', label: 'SCHICK',             marker: 14155776 },
    { id: 18, name: 'track18', label: 'BALI',               marker: 15400960 },
    { id: 19, name: 'track19', label: 'ICE_OPEN_MUSIC',     marker: 16646144 },
];

/* ── 3DO DataStream chunk types ──────────────────────────────────────── */

const CHUNK_SNDS = 0x534E4453; // 'SNDS'
const CHUNK_CTRL = 0x4354524C; // 'CTRL'
const CHUNK_FILL = 0x46494C4C; // 'FILL'
const CHUNK_SHDR = 0x53484452; // 'SHDR' (stream header)
const CHUNK_DACQ = 0x44414351; // 'DACQ'

const SUB_SSMP   = 0x53534D50; // 'SSMP' (sample data)
const SUB_SHDR   = 0x53484452; // 'SHDR' (sound header)
const CTRL_GOTO  = 0x474F544F; // 'GOTO'
const CTRL_SYNC  = 0x53594E43; // 'SYNC'

/* ── SDX2 Decoder ────────────────────────────────────────────────────── */

/**
 * SDX2 DPCM lookup table: array[n+128] = n < 0 ? -(n*n*2) : (n*n*2)
 * Matches ffmpeg's dpcm_decode_init for AV_CODEC_ID_SDX2_DPCM.
 */
const SDX2_TABLE = new Int16Array(256);
for (let i = -128; i < 128; i++) {
    const square = i * i * 2;
    SDX2_TABLE[i + 128] = i < 0 ? -square : square;
}

/**
 * Decode SDX2-compressed audio to 16-bit PCM (DPCM accumulator model).
 *
 * Algorithm (from ffmpeg dpcm.c, AV_CODEC_ID_SDX2_DPCM):
 *   - Each byte is a signed delta, decoded via: delta = byte * |byte| * 2
 *   - If the byte is EVEN (low bit = 0), reset the channel accumulator to 0
 *   - Accumulate: sample[ch] += delta
 *   - Clamp to int16 range
 *   - Stereo: channels interleaved byte-by-byte (L, R, L, R, ...)
 *
 * @param {Buffer} sdx2Buf   Raw SDX2 bytes
 * @param {number} numBytes  Number of bytes to decode
 * @param {number} numChannels  1 = mono, 2 = stereo
 * @param {Int32Array} accum  Per-channel accumulators (carried across chunks)
 * @returns {Buffer} PCM16-LE output (numBytes * 2 bytes)
 */
function decodeSdx2(sdx2Buf, numBytes, numChannels, accum) {
    const pcm = Buffer.alloc(numBytes * 2);
    let writeOff = 0;
    let ch = 0;

    for (let i = 0; i < numBytes; i++) {
        const raw = sdx2Buf[i];
        /* Interpret as signed int8 */
        const n = raw > 127 ? raw - 256 : raw;

        /* If low bit is 0 (even byte), reset accumulator for this channel */
        if (!(n & 1)) {
            accum[ch] = 0;
        }

        /* Accumulate the delta */
        accum[ch] += SDX2_TABLE[n + 128];

        /* Clamp to int16 range */
        if (accum[ch] > 32767)  accum[ch] = 32767;
        if (accum[ch] < -32768) accum[ch] = -32768;

        pcm.writeInt16LE(accum[ch], writeOff);
        writeOff += 2;

        /* Toggle channel for stereo */
        ch ^= (numChannels - 1);
    }

    return pcm;
}

/* ── WAV Writer ──────────────────────────────────────────────────────── */

function writeWav(filename, pcmBuffers, sampleRate, numChannels, bitsPerSample) {
    const pcm = Buffer.concat(pcmBuffers);
    const byteRate = sampleRate * numChannels * (bitsPerSample / 8);
    const blockAlign = numChannels * (bitsPerSample / 8);
    const dataSize = pcm.length;
    const headerSize = 44;

    const wav = Buffer.alloc(headerSize + dataSize);
    let off = 0;

    /* RIFF header */
    wav.write('RIFF', off); off += 4;
    wav.writeUInt32LE(36 + dataSize, off); off += 4;
    wav.write('WAVE', off); off += 4;

    /* fmt sub-chunk */
    wav.write('fmt ', off); off += 4;
    wav.writeUInt32LE(16, off); off += 4;           // sub-chunk size
    wav.writeUInt16LE(1, off); off += 2;            // PCM format
    wav.writeUInt16LE(numChannels, off); off += 2;
    wav.writeUInt32LE(sampleRate, off); off += 4;
    wav.writeUInt32LE(byteRate, off); off += 4;
    wav.writeUInt16LE(blockAlign, off); off += 2;
    wav.writeUInt16LE(bitsPerSample, off); off += 2;

    /* data sub-chunk */
    wav.write('data', off); off += 4;
    wav.writeUInt32LE(dataSize, off); off += 4;
    pcm.copy(wav, off);

    fs.writeFileSync(filename, wav);
    const durationSec = (dataSize / byteRate).toFixed(1);
    console.log(`  → ${filename} (${(wav.length / 1024 / 1024).toFixed(1)} MB, ${durationSec}s)`);
}

/* ── Stream Parser ───────────────────────────────────────────────────── */

/**
 * Extract one track's audio data from the bigstream.
 * Walks from `startOffset`, collects SNDS/SSMP chunks on the audio channel,
 * and stops when it hits a CTRL/GOTO chunk pointing back to startOffset.
 */
function extractTrackAudio(buf, startOffset) {
    const pcmBuffers = [];
    let offset = startOffset;
    let sampleRate = 44100;
    let numChannels = 2;
    let foundAudio = false;
    /* Per-channel DPCM accumulators, carried across SSMP chunks */
    const accum = new Int32Array(2);

    while (offset + 8 <= buf.length) {
        const chunkType = buf.readUInt32BE(offset);
        const chunkSize = buf.readUInt32BE(offset + 4);

        if (chunkSize < 8 || chunkSize > 0x100000 || offset + chunkSize > buf.length) {
            /* Invalid chunk — skip forward to next 32KB block boundary */
            const nextBlock = ((offset >> 15) + 1) << 15;
            if (nextBlock <= offset || nextBlock >= buf.length) break;
            offset = nextBlock;
            continue;
        }

        if (chunkType === CHUNK_CTRL && chunkSize >= 20) {
            const ctrlSubType = buf.readUInt32BE(offset + 16);
            if (ctrlSubType === CTRL_GOTO) {
                /* GOTO marker — this is the loop point (end of track) */
                const gotoTarget = buf.readUInt32BE(offset + 20);
                if (foundAudio) {
                    /* If the GOTO points back to our start, we're done */
                    if (gotoTarget === startOffset) break;
                    /* Also stop if we've collected data and hit any GOTO */
                    break;
                }
            }
        }

        if (chunkType === CHUNK_SNDS && chunkSize >= 24) {
            const channel = buf.readUInt32BE(offset + 12);
            const subType = buf.readUInt32BE(offset + 16);

            if (subType === SUB_SHDR && chunkSize >= 56) {
                /* SNDS header — extract audio format info */
                sampleRate  = buf.readUInt32BE(offset + 44);
                numChannels = buf.readUInt32BE(offset + 48);
                // console.log(`    Audio: ${sampleRate} Hz, ${numChannels} ch`);
            } else if (subType === SUB_SSMP) {
                /* SNDS sample data */
                const actualSampleSize = buf.readUInt32BE(offset + 20);
                const dataStart = offset + 24;
                const dataEnd = Math.min(dataStart + actualSampleSize, offset + chunkSize);

                if (dataEnd > dataStart) {
                    const sdx2Data = buf.slice(dataStart, dataEnd);
                    const pcm = decodeSdx2(sdx2Data, sdx2Data.length, numChannels, accum);
                    pcmBuffers.push(pcm);
                    foundAudio = true;
                }
            }
        }

        offset += chunkSize;
        offset = (offset + 3) & ~3; // align to 4 bytes
    }

    return { pcmBuffers, sampleRate, numChannels };
}

/* ── Main ────────────────────────────────────────────────────────────── */

function main() {
    console.log(`Reading: ${inputFile}`);
    const buf = fs.readFileSync(inputFile);
    console.log(`File size: ${buf.length} bytes (${(buf.length / 1024 / 1024).toFixed(1)} MB)\n`);

    /* Verify stream header */
    const hdrType = buf.toString('ascii', 0, 4);
    if (hdrType !== 'SHDR') {
        console.error(`Error: expected SHDR header, got '${hdrType}'`);
        process.exit(1);
    }

    /* Create output directory */
    fs.mkdirSync(outputDir, { recursive: true });

    console.log(`Extracting ${TRACKS.length} music tracks...\n`);

    for (const track of TRACKS) {
        if (track.marker >= buf.length) {
            console.log(`  SKIP ${track.name} (${track.label}): marker ${track.marker} beyond file size`);
            continue;
        }

        process.stdout.write(`  [${track.id.toString().padStart(2)}] ${track.label.padEnd(22)}`);

        const { pcmBuffers, sampleRate, numChannels } = extractTrackAudio(buf, track.marker);

        if (pcmBuffers.length === 0) {
            console.log(' — NO AUDIO DATA FOUND');
            continue;
        }

        const outPath = path.join(outputDir, `${track.name}.wav`);
        writeWav(outPath, pcmBuffers, sampleRate, numChannels, 16);
    }

    console.log('\nDone!');
}

main();
