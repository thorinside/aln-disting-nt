# ALN effects for disting NT

Two disting NT audio effects built from compact decision trees fitted to analog
circuit simulations:

| Plug-in | GUID | What it does | Release file |
| --- | --- | --- | --- |
| [ALN Fold Wavefolder](plugins/aln_fold_wavefolder/README.md) | `ThWf` | Selectable Buchla 259-style and extended 16-fold wavefolders | `aln_fold_wavefolder.o` |
| [ALN Distortion Bank](plugins/aln_distortion_bank/README.md) | `ThDb` | Eight circuit-inspired distortion models | `aln_distortion_bank.o` |

The learned tables are checked into this repository. The disting NT does not
train a model or run Python: each audio sample follows a small fixed decision
tree and evaluates one affine line. Both effects add first-order antiderivative
antialiasing around that static nonlinearity.

## Install

These plug-ins use disting NT C++ API v13 and require firmware 1.15 or later.
Download either the `.o` file or its matching ZIP from the latest GitHub
release. Copy the object to:

```text
/programs/plug-ins/
```

Rescan plug-ins or restart the module, then add the algorithm by name. Begin
monitoring at a low level when trying a new nonlinear effect.

Both plug-ins have been loaded and auditioned on physical disting NT hardware.
The automated checks cover their native behavior, ARM build, exported entry
point, ELF format, and unresolved symbols; those checks are not a substitute
for listening on a module.

## Releases

One version tag builds both plug-ins. Each GitHub release contains four assets:

- `aln_fold_wavefolder.o`
- `aln-fold-wavefolder.zip`
- `aln_distortion_bank.o`
- `aln-distortion-bank.zip`

The assets are separate so the two effects can have separate NT Gallery entries
while sharing one source repository and one tested build.

See [DEVELOPING.md](DEVELOPING.md) for local build and release details.

## Source boundary

This repository contains the real-time plug-in code and the generated model
tables needed to reproduce its release binaries. It intentionally does not
contain the research/training workspace or the separately supplied ALN trainer
used during the experiments.

The experimental trainer was an affine-leaf regression tree, not the canonical
Armstrong/Thomas MIN/MAX ALN implementation. After fitting, its breakpoint
locations were distilled into the interval decision trees checked in here. The
plug-in names retain “ALN” because that is the experiment they came from, not
because the module runs a canonical ALN evaluator.

## License

MIT. The pinned official `distingNT_API` submodule has its own MIT license.
