# ALN Fold

ALN Fold has two selectable transfer curves: a Buchla 259-style
five-cell circuit model and an extended eight-cell model. The first produces
eight turning points across a -5 V to +5 V sweep; the second produces sixteen.

At Fold 0%, the signal is unchanged before Mix and the optional output filter.
At Fold 100%, a 10 Vpp input covers the complete learned circuit range. Both
models are normalized to approximately 10 Vpp output. Switching models uses a
10 ms crossfade.

Coefficient rotation keeps each model's breakpoint positions fixed while
cyclically shifting its learned slope-change coefficients. Intermediate values
interpolate between shifts, producing a continuous family of related transfer
curves rather than switching among presets. Depending on the setting, the
result ranges from a reordered wavefolder to asymmetric-looking changes in
local gain while retaining odd symmetry overall. Audibly, expect moving
harmonic emphasis, changes in the number and strength of folds, and animated
timbres when the control is modulated.

## Controls

- **Input** — audio input bus.
- **Output** and **Output mode** — output bus and Replace/Add behavior.
- **Fold** — 0-100%; default 100%. Moves continuously from identity to the full
  learned response.
- **Mix** — 0-100% dry/wet; default 100%.
- **Low-pass** — optional fixed 1,326 Hz one-pole output filter; default Off.
- **Model** — Buchla 259 or 16-fold; default 16-fold.
- **Coeff rotate** — 0.0-100.0%; default 0.0%. Cyclically rotates the learned
  slope-change coefficients. The endpoints are identical, making a full sweep
  loop cleanly. This parameter can be mapped for CV modulation.
- **Level comp** — Off or On; default On. Normalizes rotated curves toward the
  original 10 Vpp folded-branch level. Off preserves the raw level differences
  caused by rotation; the plug-in contribution remains bounded to +/-5 V.

Input is limited to the learned -5 V to +5 V domain on the wet path. Fold is
smoothed at 10 Hz, coefficient rotation at 2 Hz, and model changes are
crossfaded. The folded branch uses first-order antiderivative antialiasing.

For a first listen, use a simple sine or triangle wave, set Fold and Mix to
100%, leave Level comp On, then sweep Coeff rotate slowly. Turn Level comp Off
to hear which rotations naturally become quieter or louder. Begin monitoring
at a low level when modulating any nonlinear transfer.

The plug-in requires disting NT firmware 1.15 or later. Install
`aln_fold_wavefolder.o` under `/programs/plug-ins/`; it appears as **ALN Fold**.
