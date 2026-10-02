#pragma once
#include <juce_core/juce_core.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace subforge
{
struct ScaleDef { const char* name; int count; int iv[12]; };

inline const ScaleDef* scaleTable()
{
    static const ScaleDef t[] = {
        { "Major (Ionian)",    7, { 0, 2, 4, 5, 7, 9, 11 } },
        { "Natural Minor",     7, { 0, 2, 3, 5, 7, 8, 10 } },
        { "Dorian",            7, { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",          7, { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",            7, { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",        7, { 0, 2, 4, 5, 7, 9, 10 } },
        { "Locrian",           7, { 0, 1, 3, 5, 6, 8, 10 } },
        { "Harmonic Minor",    7, { 0, 2, 3, 5, 7, 8, 11 } },
        { "Melodic Minor",     7, { 0, 2, 3, 5, 7, 9, 11 } },
        { "Pentatonic Major",  5, { 0, 2, 4, 7, 9 } },
        { "Pentatonic Minor",  5, { 0, 3, 5, 7, 10 } },
        { "Blues",             6, { 0, 3, 5, 6, 7, 10 } },
        { "Phrygian Dominant", 7, { 0, 1, 4, 5, 7, 8, 10 } },
        { "Chromatic",        12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    };
    return t;
}
inline constexpr int numScales = 14;
inline const char* const keyNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

enum PlayMode { Free = 0, KeyHome, ScaleSnap, DegreeMap };

inline int posMod(int a, int b) { const int m = a % b; return m < 0 ? m + b : m; }
inline int floorDiv(int a, int b) { int q = a / b; if ((a % b != 0) && ((a < 0) != (b < 0))) --q; return q; }

// Scientific pitch naming: MIDI 36 = C2 (Ableton displays MIDI 36 as C1)
inline juce::String noteName(int midi) { return juce::String(keyNames[posMod(midi, 12)]) + juce::String(floorDiv(midi, 12) - 1); }

struct MapCfg
{
    int mode = ScaleSnap, key = 0, scale = 0, home = 36, octave = 0, semi = 0, floorNote = 24;
    bool autoOct = true;
    int sampleRoot = -1;      // MIDI root of the loaded sample (-1 = none loaded)
};

// BEST-OCTAVE RULE (research-based, see README):
//  * 808/sub basses are tuned between C1 (32.7 Hz) and E2 (82.4 Hz); sub-bass lives below ~60-80 Hz.
//  * Many headphones/small speakers roll off below ~40 Hz, ear sensitivity falls steeply below ~100 Hz,
//    so the tonic should sit in the sweet zone E1..D#2 (41-78 Hz): one octave, every pitch class once.
//  * With a sample loaded, pick the tonic octave (C1..E2) that needs the least pitch-shift of the sample,
//    penalising anything outside the sweet zone. C always lands on C2 (home).
inline int tonicNote(const MapCfg& c)
{
    if (! c.autoOct) return c.home + c.key;
    const int sweetLo = c.home - 8, sweetHi = c.home + 3;
    if (c.sampleRoot < 0) return sweetLo + posMod(c.key - sweetLo, 12);

    int best = sweetLo + posMod(c.key - sweetLo, 12);
    double bestScore = 1e9;
    for (int t = c.home - 12; t <= c.home + 4; ++t)
    {
        if (posMod(t - c.key, 12) != 0) continue;
        const int outside = std::max(0, sweetLo - t) + std::max(0, t - sweetHi);
        const double score = 1.5 * outside + std::abs(t - c.sampleRoot);
        if (score < bestScore - 1e-9 || (std::abs(score - bestScore) < 1e-9 && std::abs(t - c.home) < std::abs(best - c.home)))
        { bestScore = score; best = t; }
    }
    return best;
}

inline int degInterval(const ScaleDef& s, int d) { return s.iv[d % s.count] + 12 * (d / s.count); }

inline int mapNote(int n, const MapCfg& c)
{
    if (c.mode == Free) return n;
    const int tonic = tonicNote(c);
    const int rel = n - c.home;
    if (c.mode == KeyHome) return tonic + rel;

    const ScaleDef& sc = scaleTable()[juce::jlimit(0, numScales - 1, c.scale)];
    const int oct = floorDiv(rel, 12), pc = posMod(rel, 12);

    if (c.mode == ScaleSnap)
    {
        int best = 0, bestDist = 99;
        for (int i = 0; i <= sc.count; ++i)
        {
            const int iv = (i == sc.count) ? 12 : sc.iv[i];
            const int dist = std::abs(iv - pc);
            if (dist < bestDist) { bestDist = dist; best = iv; }
        }
        return tonic + oct * 12 + best;
    }

    // Degree Map: white keys = scale degrees from home (C = root), black keys = chromatic passing tones
    static const int lowerDeg[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
    static const bool isBlack[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
    const int d = lowerDeg[pc];
    const int lo = degInterval(sc, d);
    int iv = lo;
    if (isBlack[pc]) { const int hi = degInterval(sc, d + 1); iv = (lo + 1 < hi) ? lo + 1 : lo; }
    return tonic + oct * 12 + iv;
}

inline int mapFull(int n, const MapCfg& c)
{
    int s = mapNote(n, c) + 12 * c.octave + c.semi;
    while (s < c.floorNote) s += 12;     // Sub Safe: fold anything below the floor up by octaves
    return std::min(s, 127);
}
// Parse a song key / chord symbol such as "Eb Min9", "F#m7", "Bb major", "G dorian", "Am pentatonic".
// Chord extensions map to their parent scale: min/min7/min9/min11 -> Natural Minor, min6/min13 -> Dorian,
// maj/maj7/maj9 -> Major, 7/9/13 (dominant) -> Mixolydian.
struct ParsedKey { bool ok = false; int key = 0; int scale = 0; };

inline ParsedKey parseKeySymbol(const juce::String& text)
{
    ParsedKey r;
    auto t = text.trim().replace(juce::String::charToString(0x266F), "#").replace(juce::String::charToString(0x266D), "b");
    t = t.removeCharacters(" ._-/");
    if (t.isEmpty()) return r;
    const auto letter = juce::CharacterFunctions::toUpperCase(t[0]);
    static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
    if (letter < 'A' || letter > 'G') return r;
    int pc = base[letter - 'A'];
    int i = 1;
    if (t[i] == '#') { ++pc; ++i; }
    else if (t[i] == 'b' && ! t.substring(i).startsWithIgnoreCase("bl")) { --pc; ++i; }
    r.key = posMod(pc, 12);

    const auto q = t.substring(i).toLowerCase();
    auto has = [&](const char* s) { return q.contains(s); };
    int sc = 0;
    if (has("phryg") && has("dom")) sc = 12;
    else if (has("phryg"))          sc = 3;
    else if (has("dorian"))         sc = 2;
    else if (has("lydian"))         sc = 4;
    else if (has("mixo"))           sc = 5;
    else if (has("locr") || has("dim")) sc = 6;
    else if (has("harm"))           sc = 7;
    else if (has("mel"))            sc = 8;
    else if (has("blues"))          sc = 11;
    else if (has("pent"))           sc = (has("maj") ? 9 : 10);
    else if (has("chrom"))          sc = 13;
    else if (has("maj") || has("ion")) sc = 0;
    else if (has("min") || has("aeol") || q.startsWith("m") || q.startsWith("-"))
        sc = (has("6") || has("13")) ? 2 : 1;
    else if (has("7") || has("9") || has("13") || has("dom")) sc = 5;
    r.scale = sc; r.ok = true;
    return r;
}
}
