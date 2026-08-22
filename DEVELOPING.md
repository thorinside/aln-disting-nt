# Developing

Clone with the pinned disting NT API:

```sh
git clone --recurse-submodules https://github.com/thorinside/aln-disting-nt.git
cd aln-disting-nt
```

The build needs a C++ compiler, `gcc-arm-none-eabi`, `zip`, and `unzip`.

```sh
make test       # native API and DSP behavior tests
make audio-test # settled audio-path matrix and build/*.csv measurements
make hardware   # build all ARM relocatable objects
make inspect    # verify ELF format, pluginEntry, size, and unresolved symbols
make verify     # test + hardware + inspect
make package    # verify and create six files under release/
```

`distingNT_API` is pinned to the API v13 revision used for these plug-ins. An
alternate checkout can be supplied with `make API_DIR=/path/to/distingNT_API`.

The ALN Distort and ALN Saturation audio tests render 128-sample blocks through
their public plug-in entry points, assert the DC and output-level safety gates,
and write separate CSV files under `build/`. The CSVs include RMS, peak,
fundamental, residual, and THD+N snapshots so character changes can be compared
between revisions without turning the intended nonlinearities into exact-output
tests.

The generated headers under each plug-in's `generated/` directory are build
inputs, not temporary files. A release build does not retrain the models or run
ngspice; it compiles those reviewed tables into the plug-ins.

Pushing a tag matching `v*` runs the same verification on GitHub Actions and
creates a release containing separate raw objects and MicroSD-layout ZIPs for
all Gallery entries. A manually dispatched workflow performs the build and
uploads the same files as a workflow artifact without creating a release.
