#!/usr/bin/env node
/**
 * convert_wav_to_ogg.js — Converts all WAV music tracks to OGG Vorbis.
 *
 * Usage:  node tools/convert_wav_to_ogg.js
 *
 * Requires ffmpeg-static (npm install -g ffmpeg-static).
 * Reads from assets/Music/track*.wav, writes assets/Music/track*.ogg.
 */

const { execFileSync } = require('child_process');
const path = require('path');
const fs = require('fs');

const FFMPEG = 'C:\\Users\\steve\\AppData\\Roaming\\npm\\node_modules\\ffmpeg-static\\ffmpeg.exe';
const MUSIC_DIR = path.join(__dirname, '..', 'assets', 'Music');

const wavFiles = fs.readdirSync(MUSIC_DIR)
    .filter(f => f.match(/^track\d+\.wav$/))
    .sort();

if (wavFiles.length === 0) {
    console.error('No WAV files found in', MUSIC_DIR);
    process.exit(1);
}

console.log(`Converting ${wavFiles.length} WAV files to OGG Vorbis...\n`);

for (const wav of wavFiles) {
    const ogg = wav.replace('.wav', '.ogg');
    const inPath = path.join(MUSIC_DIR, wav);
    const outPath = path.join(MUSIC_DIR, ogg);

    process.stdout.write(`  ${wav} -> ${ogg} ... `);
    try {
        execFileSync(FFMPEG, [
            '-y', '-i', inPath,
            '-c:a', 'libvorbis', '-q:a', '5',   // quality 5 ≈ ~160 kbps VBR
            outPath
        ], { stdio: 'pipe' });

        const inSize = fs.statSync(inPath).size;
        const outSize = fs.statSync(outPath).size;
        const ratio = ((1 - outSize / inSize) * 100).toFixed(1);
        console.log(`OK  (${(inSize / 1048576).toFixed(1)} MB -> ${(outSize / 1048576).toFixed(1)} MB, ${ratio}% smaller)`);
    } catch (err) {
        console.log('FAILED');
        console.error(err.stderr ? err.stderr.toString() : err.message);
    }
}

console.log('\nDone!');
