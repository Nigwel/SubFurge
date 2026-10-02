# SubForge — VST3 sub bass instrument (JG BeatsLab)

Loads a sub bass sample, plays it chromatically, layers a synth sine/saw/etc. engine, and auto-fits any key + scale
so the HOME key (C2, MIDI 36) plays the tonic in its best sub octave (E1..D#2, 41-78 Hz).
Note: Ableton labels MIDI 36 as "C1"; SubForge uses scientific naming (C2 = MIDI 36).

## Get the VST3 (no compiler needed): GitHub Actions
1. Create a new GitHub repo, upload this whole folder (keep `.github/workflows/build.yml`).
2. Repo > Actions > "Build SubForge VST3" > Run workflow (or it runs on push).
3. Download the artifact: SubForge-Windows-VST3 or SubForge-macOS-VST3 (universal arm64+x86_64).

## Or build locally
Needs CMake 3.22+, Git, and Visual Studio 2022 (Windows) or Xcode CLT (macOS).
    cmake -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build --config Release
Output: build/SubForge_artefacts/Release/VST3/SubForge.vst3
Add -DSUBFORGE_INSTALL=ON to auto-copy into the system VST3 folder (admin may be needed).

## Install in Ableton Live 11 Standard
- Windows: copy SubForge.vst3 to C:\Program Files\Common Files\VST3\
- macOS: copy to /Library/Audio/Plug-Ins/VST3/ then run: xattr -cr /Library/Audio/Plug-Ins/VST3/SubForge.vst3
- Live > Preferences > Plug-ins > turn ON "Use VST3 System Folders" > Rescan.
- Drop SubForge onto a MIDI track (Plug-ins browser > Instruments).

## Quick start
1. Drag a sub bass WAV onto the window (root pitch is auto-detected; fix Sample Root if wrong).
2. Pick Key + Scale. Key Mode: Scale Snap (default), Key Home, Degree Map (white keys = scale degrees from C2), Free.
3. Press C2 (Ableton shows C1): you get the tonic. Readout top-right shows the exact note and Hz.
4. Layer Osc/Sub, add Pitch Drop for 808 punch, Harmonics for small-speaker translation.

## Novice workflow (3 steps)
1. Drag in any sub bass / 808 sample (pitch auto-detected, DC removed, normalised).
2. Type the song key in the header box: "Eb Min9", "F#m7", "Bb major", "G dorian" -> Enter.
   (min/min7/min9/min11 -> Natural Minor, min6/min13 -> Dorian, maj/maj7/maj9 -> Major, 7/9/13 -> Mixolydian.)
3. Play. The Home key (C2) plays the tonic in its best octave; Scale Snap keeps every other key in the scale.

## Research basis (what is codified, and where it came from)
Sources are producer/engineering guides + psychoacoustics references, not formal Afrobeats studies.
- 808/sub basses are typically tuned between C1 (32.7 Hz) and E2 (82.4 Hz); sub-bass sits below ~60-80 Hz.
  -> Tonic window = E1..D#2 (41-78 Hz). Sub Floor default = C1 (MIDI 24); lower notes fold up an octave.
- Many headphones/small speakers roll off below ~40 Hz; laptop/phone speakers reproduce no true sub; ear sensitivity
  drops steeply below ~100 Hz (equal-loudness / ISO 226). -> Harmonics enhancer adds 2nd-5th harmonics of the sub
  band so the bass translates; Sub HPF (20 Hz) removes inaudible rumble/DC.
- Always tune the sub to the song key; follow chord roots; set the first 808 note to the progression root.
  -> Key/scale auto-fit; Scale Snap.
- Sample-aware octave: the tonic octave (C1..E2) needing the least pitch-shift of your sample wins, with a penalty
  outside 41-78 Hz. (Sample F1 + key Eb -> Eb1 38.9 Hz; sample at C2 or higher + key Eb -> Eb2 77.8 Hz.)
- Afrobeats: ~100-118 BPM (105-110 common), simple 3-4 chord loops, root notes locked to chord changes, log-drum or
  melodic 808, rounder/warmer than trap 808s (don't over-distort), leave room for the kick, glide on only 1-2 notes
  per bar via mono/legato overlap, sidechain bass to kick.
  -> Presets "Afro 808 Round", "Afro Log Drum", "Afro House Sub" (low drive, legato glide, round tone).
  Sidechain: use Ableton's Compressor with the kick as sidechain source on the SubForge track.
Sources: output.com/blog/best-vst-plugins-for-afrobeats, violetrecording.com (808 patterns, afrobeat beats),
beatportal.com (Afro House bass), beatkitchen.io (bass design), theaudiostuff.com (sub-bass), Wikipedia equal-loudness.
