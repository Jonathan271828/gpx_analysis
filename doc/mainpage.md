# GPXAna — API reference {#mainpage}

GPXAna parses cycling GPX files and computes track statistics, a physics-based
power estimate, training metrics, and the autocorrelation and power spectrum of
every recorded channel. It ships as a command-line tool (`gpx_reader`) and an
optional desktop GUI (`gpxana_gui`).

This is the generated API reference. For **installation, usage, the option
list, worked examples and the equations behind every model**, see
[README.md](https://github.com/Jonathan271828/gpx_analysis#readme) in the
repository root — it is written for GitHub, whose maths rendering Doxygen does
not share.

## How the pieces fit

The analysis is a library; both front-ends are thin.

- `GpxReader` parses the file and owns the raw data. Everything downstream takes
  a `Track` and a `PowerAnalysis` and returns a plain struct — no I/O, no
  globals, no ownership.
- Each metric lives in its own namespace: @ref metrics, @ref zones,
  @ref splits, @ref peaks, @ref durability, @ref quadrant, @ref cp,
  @ref trends, @ref channels, @ref signal, @ref wind.
- Output is separated from computation. @ref io holds the screen report and the
  data-file writers; nothing in the analysis prints.
- @ref app drives one file, or several for the multi-ride trend.
- @ref gui is the GUI, built only with `-DGPXANA_BUILD_GUI=ON`. It reimplements
  no analysis: it hands a synthesised `argv` to the same `arg_parser`, calls the
  same `app::run()`, and captures the report it prints.

## Where to start

| To follow | Read |
|---|---|
| Parsing and the physics model | `GpxReader::estimate_power` in gpx_reader.hpp |
| A metric's definition | its namespace header, e.g. metrics.hpp, durability.hpp |
| The spectral transform | signal.hpp, and `signal::compute_acf_psd` |
| How a ride reaches the screen | app.hpp, then screen_output.hpp |
| The GUI's structure | gui/src/app_window.hpp, then gui/src/file_view.hpp |

## Conventions

- Scalar aliases (`Real`, `Int`, `Size`, `Long`, `Bool`) are in types.hpp, so a
  precision change is one edit.
- Headers carry the documentation; `.cpp` files carry implementation comments
  explaining *why*, not *what*.
- Every result struct has a `valid` flag rather than throwing or returning
  sentinels; callers check it and print nothing when the data does not support
  the metric.
