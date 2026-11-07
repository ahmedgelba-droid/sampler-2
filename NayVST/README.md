# Gelbo Sampler (VST3, JUCE)

Monophonic/polyphonic sampler for nay / mizmar with per-note fine tune, legato, delay, reverb and switchable instruments.

## Build (GitHub Actions, Windows)
Push this folder, open Actions, run "Build Gelbo Sampler VST3 (Windows)", download the artifact.
Locally: cmake -B build && cmake --build build --config Release

## Instruments
Each instrument = a folder of samples named by note (C4.wav, D#4.wav, Bb3.wav or 60.wav).
"Add Folder...": a folder with samples -> one instrument named after it; a folder of folders -> one instrument per sub-folder.
Use Prev/Next, or click the display to pick from a list. "Rename" changes the shown name.

## Controls
- 12 number boxes: fine tune per note in cents (drag; double-click = quarter-tone flat -50 / back to 0)
- MONO/POLY button (blue = mono; 8 voices in poly)
- LEGATO (mono): lit = glide between notes without retrigger (speed = Glide knob), works with the cents offsets
- Reverb / Delay level in dB (Off at the bottom), Rev Size, Dly Time, Dly FB
- Vib Depth/Speed (mod wheel adds depth), Attack, Release, Gain
