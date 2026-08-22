# ALN Saturation

ALN Saturation is an audio-rate tube-stage saturator learned from two complete
ngspice common-cathode circuit simulations. It uses compact decision trees with
affine leaves on the disting NT; it does not run a circuit solver or train on
the module.

## Stages

- **AX7 Data** — a 250 V common-cathode high-mu stage with a 100 kΩ plate load,
  1.5 kΩ cathode resistor, 68 kΩ grid source, and 1 MΩ following load.
- **AX7 Hot** — a 300 V variation with stronger input scaling and a 100 kΩ grid
  source for earlier, more asymmetric saturation.
- **Bias Memory** — the AX7 Hot learned transfer plus an explicit fast-charge,
  slow-release bias-shift state. This adds level-history-dependent compression
  without making the learned tree recursive.

The source netlists are under `models/`. They use a clean-room behavioral
triode current element based on the Child-Langmuir 3/2-power relation, plus
explicit supply, plate, cathode, grid-conduction, clamp, source, and load
components. These are tube-circuit-inspired simulations, not measurements of a
particular physical 12AX7 and not third-party SPICE macro-models.

## Controls

- **Input** — audio input bus; intended for Eurorack-level signals up to 10 Vpp.
- **Output** and **Output mode** — output bus and Replace/Add behavior.
- **Stage** — AX7 Data, AX7 Hot, or Bias Memory; default AX7 Data.
- **Drive** — 0-800%; default 100%. Drive 0% is transparent. Above 100% adds up
  to 8x excitation before the learned circuit.
- **Bias** — shifts the circuit input by up to +/-1 V; default 0 V.
- **Memory** — 0-100%; default 50%. It controls the dynamic bias shift in Bias
  Memory and has no effect on the two static stages.
- **Mix** — 0-100% dry/wet; default 100%. Mix 0% is transparent.
- **Level** — 0-200%; default 100%.
- **Low-pass** — optional fixed 6 kHz one-pole output filter; default Off.

Stage changes crossfade over 10 ms. The learned static transfers use
first-order antiderivative antialiasing. A limiter-safe 5 Hz DC rejector removes
offset from asymmetric saturation while keeping this plug-in's contribution
within +/-5 V. In Add mode, other algorithms can still push the final shared
bus outside that range. Because DC is rejected, this is an audio effect rather
than a precision CV processor.

## Model and test evidence

Each simulated stage was swept over -5 V to +5 V at 1.25 mV spacing. The ALN
was trained on 4,001 alternating voltage samples and evaluated on the other
4,000 held-out samples. Each exported model has 255 decision nodes, 256 affine
leaves, and a maximum depth of eight. Exact held-out errors and simulated plate
voltages are recorded in `models/model_manifest.json`.

The host audio test drives the public plug-in entry point in 128-sample blocks.
It measures all three stages across Drive, Bias, Memory, frequency, bypass,
level, low-pass, and Add/Replace paths, recording DC, RMS, peak, fundamental,
residual, and THD+N metrics under `build/`.

The plug-in requires disting NT firmware 1.15 or later. Install
`aln_saturation.o` under `/programs/plug-ins/`; it appears as **ALN
Saturation**.
