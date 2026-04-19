/*
 * Patch level files that contain empty seeker difficulty sections.
 *
 * 3DO Icebreaker 2 (and a handful of IB1 levels) ship with one or more
 * empty difficulty sections after the talk-file marker.  An empty section
 * means zero seekers spawn at that difficulty.  Combined with the engine's
 * "no seekers => level ended" exit, those levels would either instantly
 * finish (no seekers, no pyramid-clearing victory either if green pyramids
 * are present) or be unwinnable.
 *
 * This script scans the configured level directories, finds each empty
 * difficulty section, and inserts a single seeker line copied from the
 * nearest populated section.  Files are rewritten in-place using the
 * original CR-only line ending.
 *
 * Run from the repo root:
 *     node tools/patch_empty_seekers.js
 */

const fs = require('fs');
const path = require('path');

const REPO = path.resolve(__dirname, '..');
const LEVEL_DIRS = [
    path.join(REPO, 'assets', 'newlevels'),
    path.join(REPO, 'assets', 'NewLevels'),
    path.join(REPO, 'assets', 'Levels'),
];

const SEEKER_RX = /^(LTBLUE_SEEKER|YELLOW_SEEKER|PINK_SEEKER|LIME_SEEKER|CHAMELEON|BUMMER|REDCOAT|ZOMBIE|LURKER|MEANY|JUGGERNAUT|PSYCHO|GRUMPY|NASTY|PHANTOM|DORMANT)/;
const SEP_RX = /^\*+$/;
const DIFF_NAMES = ['EASY', 'MEDIUM', 'HARD', 'INSANE'];

function patchFile(filePath) {
    const raw = fs.readFileSync(filePath);
    if (!raw.includes(0x0D)) return null;

    const lines = [];
    let start = 0;
    for (let i = 0; i < raw.length; i++) {
        if (raw[i] === 0x0D) {
            lines.push(raw.slice(start, i));
            start = i + 1;
        }
    }
    if (start <= raw.length) lines.push(raw.slice(start));

    const sepIdx = [];
    for (let i = 0; i < lines.length; i++) {
        if (SEP_RX.test(lines[i].toString('ascii'))) sepIdx.push(i);
    }
    if (sepIdx.length < 8) return null;

    const sections = [];
    for (let d = 0; d < 4; d++) {
        const sStart = sepIdx[3 + d] + 1;
        const sEnd = sepIdx[4 + d];
        const rows = [];
        for (let r = sStart; r < sEnd; r++) {
            const text = lines[r].toString('ascii').replace(/^\s+/, '');
            if (SEEKER_RX.test(text)) rows.push(lines[r]);
        }
        sections.push(rows);
    }

    const empty = [];
    for (let d = 0; d < 4; d++) if (sections[d].length === 0) empty.push(d);
    if (empty.length === 0) return null;

    const donors = {};
    for (const d of empty) {
        let donor = null;
        for (let off = 1; off < 4 && donor === null; off++) {
            for (const cand of [d + off, d - off]) {
                if (cand >= 0 && cand < 4 && sections[cand].length > 0) {
                    donor = sections[cand][0];
                    break;
                }
            }
        }
        if (donor === null) return '  no donor found';
        donors[d] = donor;
    }

    /* Insert from highest section index downward so earlier sepIdx values
       remain valid as we mutate `lines`. */
    const sortedEmpty = [...empty].sort((a, b) => b - a);
    for (const d of sortedEmpty) {
        const endSep = sepIdx[4 + d];
        lines.splice(endSep, 0, donors[d]);
    }

    /* Rejoin with CR-only line endings. */
    const parts = [];
    for (let i = 0; i < lines.length; i++) {
        parts.push(lines[i]);
        if (i < lines.length - 1) parts.push(Buffer.from([0x0D]));
    }
    const out = Buffer.concat(parts);
    if (out.equals(raw)) return null;
    fs.writeFileSync(filePath, out);

    const filled = empty.map(d => DIFF_NAMES[d]).join(',');
    const inserted = empty.map(d => donors[d].toString('ascii').trim()).join(' | ');
    return `  filled ${filled} with: ${inserted}`;
}

function main() {
    const seen = new Set();
    let total = 0;
    for (const dir of LEVEL_DIRS) {
        if (!fs.existsSync(dir)) continue;
        for (const name of fs.readdirSync(dir).sort()) {
            const key = name.toLowerCase();
            if (seen.has(key)) continue;
            seen.add(key);
            const fp = path.join(dir, name);
            if (!fs.statSync(fp).isFile()) continue;
            const result = patchFile(fp);
            if (result !== null) {
                console.log(fp);
                console.log(result);
                total++;
            }
        }
    }
    console.log(`\nPatched ${total} file(s).`);
}

main();
