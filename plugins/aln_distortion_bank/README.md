# ALN Distort

ALN Distort contains eight static circuit models learned from separate
ngspice sweeps:

- Silicon Soft
- Germanium
- LED Clip
- Asymmetric
- Op-amp Hard
- BJT Saturation
- CMOS Inverter
- Full-wave

## Controls

- **Input** — audio input bus.
- **Output** and **Output mode** — output bus and Replace/Add behavior.
- **Circuit** — selects one of the eight models; default Silicon Soft.
- **Drive** — 0-800%; default 100%. At 0% the wet transfer is identity. From
  0-100% it fades into the original learned response; above 100% it adds up to
  8x input gain before the circuit model.
- **Bias** — shifts the circuit operating point by up to +/-1 V; default 0 V.
- **Mix** — 0-100% dry/wet; default 100%.
- **Level** — 0-200%; default 100%.
- **Low-pass** — optional fixed 6 kHz one-pole output filter; default Off.

Circuit changes use a 10 ms crossfade. Each model has first-order antiderivative
antialiasing and a 5 Hz DC blocker, so this is an audio effect rather than a
precision DC/CV waveshaper. The plug-in contribution is limited to +/-5 V. In
Add mode, other algorithms can still push the final shared bus past that range.

The plug-in requires disting NT firmware 1.15 or later. Install
`aln_distortion_bank.o` under `/programs/plug-ins/`; it appears as **ALN
Distort**.
