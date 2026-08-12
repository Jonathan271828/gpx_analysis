# gpx_reader

A C++17 command-line tool that parses GPX files and computes track statistics,
detects hills, and finds the fastest segment over a user-defined distance or
time window.

## Features

- Parse GPX 1.1 files (Garmin Connect and compatible devices)
- Per-track statistics: distance, elevation gain/loss, duration, average speed,
  average temperature, heart rate, cadence and power, average climb and descent
  gradient
- Hill detection table: distance, elevation gain, average grade and average
  estimated power per climb
- Estimated power (Strava-style physics model) with optional comparison against
  measured power, and a time-vs-power CSV export
- Optional historical wind (Open-Meteo) folded into the aerodynamic term for a
  more accurate power estimate
- Fastest segment finder: sliding-window search by distance or time
- Autocorrelation function and power spectrum of each time-dependent channel
  (velocity, power, heart rate, cadence), exported as 4-column text files
- Multiple `--dist` and `--time` queries supported in a single run

## Requirements

- C++17-compatible compiler (e.g. GCC >= 8)
- CMake >= 3.16
- Internet access at first configure — [pugixml](https://github.com/zeux/pugixml)
  and [nlohmann/json](https://github.com/nlohmann/json) are fetched
  automatically via CMake `FetchContent`, no manual install needed
- The `curl` command-line tool on `PATH` — only needed at runtime for the
  optional `--wind` fetch (no libcurl dev package required)

## Building

```bash
mkdir build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

The binary is placed at `build/gpx_reader`.

## Usage

```
./build/gpx_reader <file.gpx> [options]

Options:
  --points N       print first N track points (default: 10, use 0 to suppress)
  --dist  D        find fastest segment of D km    (e.g. --dist 5.0)
  --time  T        find fastest segment of T s     (e.g. --time 300)

Power estimation (runs by default):
  --mass  M        total rider+bike mass in kg (default: 80)
  --rider R        rider mass in kg (summed with --bike if --mass unset)
  --bike  B        bike mass in kg  (summed with --rider if --mass unset)
  --crr   C        rolling resistance coefficient (default: 0.005)
  --cda   A        aerodynamic drag area CdA in m^2 (default: 0.32)
  --drivetrain E   drivetrain efficiency 0..1 (default: 0.977)
  --smooth S       GPS speed smoothing window in s, tames spikes (default: 5; 0 off)
  --max-accel A    clamp on |acceleration| in m/s^2 (default: 3)
  --max-speed V    cap on raw step speed in m/s, drops GPS teleports (default: 30)
  --max-grade G    clamp on |grade| as a fraction (default: 0.30)
  --max-gap  S     steps longer than S seconds count as a stop (default: 10)
  --power-csv F    write a time-vs-power CSV to file F
  --xy F           write all per-point data as a #-commented XY table to F
  --power-curve F  write mean-maximal power curve (duration vs W) to F
  --power-hist F   write power histogram (time in each power band) to F
  --hist-bin W     histogram bin width in watts (default: 25)

Autocorrelation & power spectrum (4-col: lag, acf, freq, psd):
  --acf-velocity F       velocity autocorrelation + spectrum -> F
  --acf-power F          estimated power                     -> F
  --acf-power-measured F measured <power>                    -> F
  --acf-hr F             heart rate                          -> F
  --acf-cadence F        cadence (rpm)                       -> F
  --acf-dt S             uniform resample interval in s (default: auto = median)

Wind (Open-Meteo historical API; improves the aero term):
  --wind           fetch historical wind and apply it
  --wind-cache F   like --wind, but cache to / read from file F
  --wind-file F    apply wind from a local JSON file F (offline)
```

Multiple `--dist` and `--time` flags are supported and each is reported
separately.

### Examples

```bash
# Basic run — points, statistics, hills (with per-climb power) and power estimate
./build/gpx_reader ride.gpx

# Custom mass and aerodynamics, plus a time-vs-power CSV
./build/gpx_reader ride.gpx --points 0 --mass 78 --cda 0.30 --power-csv power.csv

# Suppress point listing; find fastest 1 km, 5 km, 5-minute and 10-minute segments
./build/gpx_reader ride.gpx --points 0 --dist 1.0 --dist 5.0 --time 300 --time 600

# Write a mean-maximal power curve and a power histogram (50 W bins)
./build/gpx_reader ride.gpx --points 0 --power-curve curve.txt --power-hist hist.txt --hist-bin 50

# Autocorrelation + power spectrum for selected channels (one flag each)
./build/gpx_reader ride.gpx --points 0 --acf-hr hr.txt --acf-cadence cad.txt
```

The power estimate is de-spiked before use: GPS speed is smoothed over a short
window (`--smooth`), and acceleration, speed, grade and long stops are bounded
(`--max-accel`, `--max-speed`, `--max-grade`, `--max-gap`) so position/elevation
noise can't produce physically impossible power. Loosen or disable these
(`--smooth 0`) to see the raw model output.

## Sample output

```
GPX file   : activity_22358886983.gpx
Recorded   : 2026-03-31T09:46:45.000Z
Tracks     : 1

--- Track points (showing first 3 of 5749) ---
[    1] lat=    48.227751  lon=     16.304489  ele=  239.80 m  time=2026-03-31T09:46:45.000Z  temp=21.0 C
[    2] lat=    48.227753  lon=     16.304492  ele=  238.00 m  time=2026-03-31T09:46:46.000Z  temp=21.0 C
[    3] lat=    48.227544  lon=     16.304466  ele=  237.80 m  time=2026-03-31T09:47:05.000Z  temp=20.0 C
  ... (5746 more points not shown)

=== Statistics for track: "Vienna Gravel/Unpaved Cycling" ===
  Type           : gravel_cycling
  Points         : 5749
  Total distance : 22.74 km
  Elevation gain : 729.2 m
  Elevation loss : 727.4 m
  Min elevation  : 232.4 m
  Max elevation  : 523.6 m
  Duration       : 1h 37m 19s
  Avg speed      : 14.0 km/h
  Avg temperature: 5.4 C
  Avg climb grade: +8.6 %
  Avg desc grade : -9.4 %


--- Hills (min grade: 1%, min gain: 10 m, gap tolerance: 20 m) ---
   #    Distance      Gain    Avg grade   Start ele     End ele  Start time
 ---  ----------  --------  -----------  ----------  ----------  ------------------------
   1      5.00 km   243.6 m  +    4.9 %     232.4 m     476.0 m  2026-03-31T09:47:36.000Z
   2      0.33 km    12.6 m  +    3.9 %     438.6 m     451.2 m  2026-03-31T10:15:12.000Z
   3      5.91 km   242.0 m  +    4.1 %     281.6 m     523.6 m  2026-03-31T10:22:51.000Z
   4      2.96 km   149.8 m  +    5.1 %     299.0 m     448.8 m  2026-03-31T11:00:59.000Z

Total: 4 hills


=== Fastest 5.00 km segment ===
  Avg speed  : 16.3 km/h
  Distance   : 5.002 km
  Duration   : 18m 24s
  Start      : 2026-03-31T10:13:48.000Z  (lat=48.255101, lon=16.265314)
  End        : 2026-03-31T10:32:12.000Z  (lat=48.258232, lon=16.245833)
  Point idx  : 1592 -> 2661


=== Fastest 5m 0s segment ===
  Avg speed  : 26.9 km/h
  Distance   : 2.242 km
  Duration   : 5m 0s
  Start      : 2026-03-31T11:18:54.000Z  (lat=48.224949, lon=16.277718)
  End        : 2026-03-31T11:23:54.000Z  (lat=48.227416, lon=16.304194)
  Point idx  : 5438 -> 5738
```

## Output reference

### File header

| Field | Description |
|---|---|
| `GPX file` | Path to the input file |
| `Recorded` | Timestamp from the `<metadata><time>` element |
| `Tracks` | Number of `<trk>` elements found in the file |

### Track points listing

One line per point showing index, latitude, longitude, elevation, timestamp
and (if present) air temperature, heart rate, cadence and power. Controlled by
`--points N`.

### Statistics block

| Field | Description |
|---|---|
| Type | Activity type from `<type>` element |
| Points | Total number of track points |
| Total distance | Cumulative Haversine distance in km |
| Elevation gain | Sum of all positive elevation deltas in m |
| Elevation loss | Sum of all negative elevation deltas (as a positive value) in m |
| Min / Max elevation | Lowest and highest recorded elevation in m |
| Duration | Time from first to last point (`HhMmSs`) |
| Avg speed | Total distance divided by total duration in km/h |
| Avg temperature | Mean air temperature from `ns3:atemp` extension in °C |
| Avg heart rate | Mean / min / max heart rate from `ns3:hr` in bpm |
| Avg cadence | Mean / min / max cadence from `ns3:cad` in rpm |
| Avg power | Mean / min / max power from `<power>` in watts |
| Avg climb grade | Mean gradient of all uphill steps (%) |
| Avg desc grade | Mean gradient of all downhill steps (%, negative value) |

### Hills table

One row per detected climb, listed in chronological order.

| Column | Description |
|---|---|
| `#` | Hill number (chronological order) |
| Distance | Horizontal distance of the climb in km |
| Gain | Elevation gained from start to peak in m |
| Avg grade | Mean gradient over the climb distance (%) |
| Start ele | Elevation at the start of the climb in m |
| End ele | Elevation at the peak in m |
| Avg power | Mean estimated power over the climb in watts |
| Start time | ISO-8601 timestamp at the start of the climb |

### Estimated power block

| Field | Description |
|---|---|
| Model | Mass, Crr, CdA and drivetrain efficiency used |
| Avg / Max est. power | Mean / maximum estimated power in watts |
| Work done | Total mechanical work in kJ |
| Measured avg | Mean measured power (only if `<power>` present) |
| Mean abs error / bias | Estimate-vs-measured error (only if `<power>` present) |

### Fastest segment block

| Field | Description |
|---|---|
| Avg speed | Average speed over the found segment in km/h |
| Distance | Actual distance covered by the segment in km |
| Duration | Elapsed time of the segment (`HhMmSs`) |
| Start / End | ISO-8601 timestamp and coordinates (lat/lon) |
| Point idx | Index range into the track points array |

## Hill detection

Hills are detected using a state machine that scans the track points in order.
A step is classified as uphill when its gradient meets or exceeds
`MIN_GRADE_PCT`. Short flat or downhill sections within a climb are tolerated
up to `GAP_TOLERANCE_M` of cumulative descent before the hill is considered
finished. A completed climb is only recorded if its total elevation gain
reaches `MIN_GAIN_M`.

| Parameter | Default | Description |
|---|---|---|
| `MIN_GRADE_PCT` | 1.0 % | Minimum step gradient to enter climbing state |
| `MIN_GAIN_M` | 10.0 m | Minimum total gain for a climb to be recorded |
| `GAP_TOLERANCE_M` | 20.0 m | Cumulative descent absorbed before ending a hill |

## Fastest segment algorithm

Both the distance-based and time-based searches use an **O(N) two-pointer
sliding window**. A cumulative distance and timestamp table is built once per
query, then the right pointer advances until the window satisfies the requested
size. The window with the highest average speed (`distance / duration`) is
kept. If the requested window is larger than the entire track, the result is
marked invalid and a notice is printed.

## Power estimation

Power is estimated from the track kinematics using the same physics model
Strava uses (the Martin et al. 1998 road-cycling model). For each step, power at
the pedals is the sum of four resistive forces multiplied by ground speed,
divided by drivetrain efficiency:

```
P = (1/η) · [ P_gravity + P_rolling + P_aero + P_accel ]

P_gravity = m · g · sin(θ) · v          (climbing)
P_rolling = m · g · cos(θ) · Crr · v    (rolling resistance)
P_aero    = ½ · ρ · CdA · (v + v_hw)² · v   (air resistance)
P_accel   = m · a · v                   (acceleration)
```

where `θ = atan(grade)`, `v` = ground speed, `a` = acceleration and `v_hw` = the
headwind component. `v` is the rider's speed over the ground; `(v + v_hw)` is the
airspeed the drag force depends on. Wind is not yet modelled, so `v_hw = 0` and
the aero term reduces to `½·ρ·CdA·v³`.

| Parameter | Default | Notes |
|---|---|---|
| Mass (rider+bike) | 80 kg | `--mass`, or `--rider` + `--bike` |
| `Crr` | 0.005 | quality road tyres on asphalt (`--crr`) |
| `CdA` | 0.32 m² | rider on the hoods (`--cda`) |
| Drivetrain efficiency | 0.977 | ≈2.3 % loss (`--drivetrain`) |
| Air density `ρ` | computed | from elevation + air temperature; 1.225 fallback |

Air density is computed per point from elevation (International Standard
Atmosphere barometric pressure) and, when present, the recorded air temperature.
Coasting/downhill samples that yield negative power are clamped to 0 W.

The estimate is reported as average/max power and total work (kJ), each climb in
the hills table gets an **average power** column, and — when the GPX carries a
measured `<power>` channel — the summary also prints the measured average, mean
absolute error and mean bias so the estimate can be validated. `--power-csv F`
writes a `time,elapsed_s,est_power_w[,measured_power_w]` row per track point.

**Accuracy caveats:** like Strava, the model cannot see wind, exact tyre/surface
`Crr`, or real rider aerodynamics, so absolute numbers are approximate — most
accurate on sustained climbs where gravity dominates.

### Wind

Wind is fetched from the **[Open-Meteo Historical Weather API](https://open-meteo.com/en/docs/historical-weather-api)**
(free, no API key, ERA5 reanalysis) and folded into the aerodynamic term via the
headwind component `v_hw`:

```
v_hw = wind_speed · cos(wind_from_direction − rider_heading)
```

The rider heading is the GPS bearing between consecutive points; the wind
direction is Open-Meteo's meteorological "blows from" bearing. A wind from ahead
raises estimated power, a tailwind lowers it. One hourly value is requested at
the track centroid for the ride's date(s) and matched to each point by nearest
hour. HTTP is performed by invoking the `curl` command-line tool.

| Flag | Behaviour |
|---|---|
| `--wind` | fetch from Open-Meteo and apply |
| `--wind-cache F` | fetch once, cache to `F`, reuse on later runs (offline-friendly) |
| `--wind-file F` | apply wind from a local JSON file (Open-Meteo shape); no network |

When wind is applied the estimated-power block reports the average headwind and
the CSV gains a `headwind_ms` column.

**Caveats:** ERA5 is a ~25 km / hourly reanalysis with a ~5-day delay, so wind is
a regional hourly estimate — it captures the prevailing wind of the day, not
gusts or local terrain effects (valleys, tree cover). For rides in the last few
days the archive may not yet have data.

## Autocorrelation & power spectrum

Each time-dependent channel has its own flag, and each names the file it writes
to — `--acf-velocity F`, `--acf-power F` (estimated), `--acf-power-measured F`
(the file's `<power>`), `--acf-hr F` and `--acf-cadence F`. Only the channels you
ask for are computed; a requested channel the track doesn't carry (or a constant
signal) is skipped with a message. With more than one track the output name gains
a `.N` suffix per track.

Each file is a 4-column, `#`-commented table:

| Column | Meaning |
|---|---|
| 1 `lag_s` | time lag τ of the autocorrelation (s) |
| 2 `autocorr` | normalized autocorrelation at that lag (`acf[0] = 1`) |
| 3 `freq_hz` | frequency (Hz), from 0 to the Nyquist frequency `1/(2·dt)` |
| 4 `psd` | one-sided power spectral density at that frequency (`unit²/Hz`) |

**Method.** GPX samples are irregular in time, so each channel is first linearly
resampled onto a uniform grid of spacing `dt` (`--acf-dt`, default = the median
sample interval, at least 1 s). The mean is removed, and the
[Wiener–Khinchin theorem](https://en.wikipedia.org/wiki/Wiener%E2%80%93Khinchin_theorem)
is applied via an FFT: the power spectrum is `|FFT(signal)|²` and the
autocorrelation is the inverse FFT of that spectrum.

The lag/ACF pair (time domain) and the freq/PSD pair (frequency domain) have
different natural lengths, so the shorter columns are padded with `NaN` to keep
the file rectangular — plot columns 1:2 for the autocorrelation and 3:4 for the
spectrum:

```bash
gnuplot -p -e "plot 'power.txt' using 1:2 with lines"                 # ACF
gnuplot -p -e "set logscale xy; plot 'power.txt' using 3:4 with lines" # PSD
```

## Interpreting the results

The two curves answer different questions. The **autocorrelation function (ACF)**
tells you about *timescales* — "how long does this signal stay similar to
itself?" The **power spectrum (PSD)** tells you about *periodicity and where the
variability lives* — "is the ride dominated by slow drifts or fast surges, and is
anything rhythmic?" They are two views of the same information (one is the
Fourier transform of the other), so use whichever makes a given feature obvious.

### Reading the two curves

**Autocorrelation (column 2 vs lag, column 1).** Always starts at `1.0` at lag 0
and decays as the lag grows.

- **Correlation time** `τc` — the lag where the ACF first drops to about `1/e`
  (≈ 0.37). This is the signal's *memory*: how long, on average, it holds its
  value. A large `τc` (minutes) means smooth, steady, persistent; a small `τc`
  (seconds) means jumpy and rapidly changing.
- **Oscillation** — if the ACF dips below zero and swings back up periodically,
  the signal is *periodic*, and the spacing between ACF peaks is the period.
- **A raised, slowly-decaying tail** — a long trend/drift over the ride (e.g.
  heart-rate drift).

**Power spectrum (column 4 vs frequency, column 3).** Shows how the signal's
variance is spread across frequencies (period = `1 / frequency`).

- **A sharp peak** at frequency `f` means a repeating pattern with period `1/f`
  (structured intervals, circuit laps, regular rollers).
- **Energy piled at low frequency** (steep fall-off) = slow changes dominate =
  steady riding. **A fat high-frequency tail** = lots of rapid fluctuation =
  surgy riding.
- The DC bin (`f = 0`) is ≈ 0 by construction — the mean is removed, so the
  spectrum shows *fluctuations about your average*, not the average itself.

### What each channel tells you

#### Power (`--acf-power`, `--acf-power-measured`) — the most useful for training

This is the spectral view of how evenly you paced.

| You see | It means | To improve |
|---|---|---|
| ACF stays high for minutes; PSD steeply low-frequency | Steady, even effort — ideal for a time trial, sustained climb or endurance block | Already good; this is the target for steady efforts |
| ACF collapses within seconds; fat high-frequency PSD tail | Surgy, stop-and-go effort — crit, group ride, technical terrain, or ragged pacing | Anticipate hills/corners, feather rather than stab the pedals; smoothing power lowers the high-frequency tail and usually raises sustainable output |
| ACF oscillates with period `T`; sharp PSD peak at `1/T` | Structured intervals with cycle length `T` (e.g. 40 s on / 20 s off → `T` = 60 s → peak at ≈ 0.017 Hz) | Confirms you actually held the prescribed work/rest structure — a fuzzy or absent peak means the intervals drifted |
| Estimated vs measured PSD differ mostly at high frequency | Real power meter captures micro-surges the physics estimate smooths out | Those micro-surges above threshold cost disproportionate energy — flattening them is "free" endurance |

The correlation time of power is essentially a Variability-Index-style measure:
**longer `τc` and less high-frequency energy = more even, more sustainable
riding.**

#### Heart rate (`--acf-hr`)

Heart rate is a slow, inertial response to effort (cardiovascular time constant
of roughly 30–60 s), so its curves look very different from power — *by design,
not because the data is bad*.

- Expect a **long correlation time (minutes)** and a PSD that collapses almost
  entirely to the lowest frequencies.
- **Compare the HR spectrum with the power spectrum.** Above some frequency the
  HR spectrum falls well below the power spectrum: that is your body's low-pass
  cutoff. Efforts shorter than ~1–2 minutes barely move HR — which is exactly why
  **HR is a poor guide for short/interval efforts; trust power there** and use HR
  for steady aerobic pacing.
- A rising lowest-frequency component (beyond the effort structure) is **cardiac
  drift** — heat, dehydration or fatigue pushing HR up at constant power.
  Actionable: cool, fuel, hydrate, or start steadier.

#### Velocity (`--acf-velocity`)

- Correlation time = how long you hold a steady speed — long on open roads,
  short in traffic or on twisty descents.
- **PSD peaks reveal repeating route structure:** laps of a circuit show a peak
  at `1 / lap-time`; evenly spaced rollers show a peak at their spacing.
- Comparing velocity with power: on the flat, velocity tracks power closely; on
  rolling terrain, velocity carries extra low-frequency content from the gradient
  even when your power is constant — a quick way to see how terrain-driven the
  ride was.

#### Cadence (`--acf-cadence`)

- Correlation time ≈ how long you stay in a gear before shifting. Long and smooth
  on a flat road or trainer; choppy on rolling terrain or in stop-and-go riding.
- Frequent coasting shows up as cadence dropping to zero, adding both low- and
  high-frequency content.
- **A tight, high-`τc` cadence** means consistent pedalling; a jumpy cadence with
  lots of high-frequency energy means frequent shifts/coasting. For steady
  efforts, holding cadence steady tends to steady your power too.

### Reading a whole ride at a glance

- **Well-paced time trial / climb:** power and velocity ACFs decay slowly, their
  PSDs are steep and low-frequency, HR sits high with a long `τc`. Clean and
  boring is the goal.
- **Interval workout:** clear PSD peaks in power (and often HR/velocity) at the
  interval-cycle frequency; oscillating ACFs. Missing/blurry peaks mean the
  structure slipped.
- **Group ride / crit:** broad, high-frequency-heavy PSDs and short correlation
  times across power, velocity and cadence — lots of surging and coasting.
- **Fatigue / heat:** power structure unchanged, but the HR spectrum grows a
  stronger very-low-frequency component (drift).

### Caveats (so you don't over-read the plots)

- **Nyquist limit.** At the default 1 s grid the highest visible frequency is
  0.5 Hz (period 2 s). This means the **pedal-stroke rate is invisible**: 90 rpm
  is a 1.5 Hz crank rate, far above Nyquist. The cadence channel is your
  *reported average rpm over time*, not the crank-rotation signal — don't look
  for a "pedalling" peak.
- **Gaps and stops.** Resampling interpolates across dropouts and stops, which
  injects mostly spurious low-frequency energy. A ride with long stops will look
  more low-frequency than it really was.
- **Non-stationarity.** A whole ride mixes regimes (warm-up, climbs, fatigue), so
  one global spectrum blends them. For a specific claim — "were my intervals
  on-period?" — analyse just that segment of the ride.
- **Trend leakage & estimator bias.** The mean is removed but a linear drift is
  not, so slow trends land in the lowest bins; and the ACF is slightly attenuated
  at large lags (standard biased estimator). Read the *shape* and *peak
  locations*, not the absolute values of the far tail.

## GPX format support

The parser reads GPX 1.1 files. The following elements are extracted:

| Element / Attribute | Field |
|---|---|
| `<trkpt lat="..." lon="...">` | Latitude and longitude |
| `<ele>` | Elevation in metres |
| `<time>` | ISO-8601 timestamp (UTC) |
| `<ns3:TrackPointExtension><ns3:atemp>` | Air temperature in °C |
| `<ns3:TrackPointExtension><ns3:hr>` | Heart rate in bpm |
| `<ns3:TrackPointExtension><ns3:cad>` | Cadence in rpm |
| `<power>` (or `<ns3:power>`) | Power in watts |
| `<trk><name>` | Track name |
| `<trk><type>` | Activity type |
| `<metadata><time>` | Recording start time |

Files with multiple `<trk>` elements are fully supported; statistics, hill
detection and fastest-segment queries are run independently for each track.

## Implementation notes

- **XML parsing** — [pugixml](https://github.com/zeux/pugixml) v1.14, fetched
  automatically by CMake `FetchContent` at configure time.
- **Distance** — Haversine formula using the WGS-84 Earth radius (6 371 000 m).
  All distances are horizontal (2D); elevation is not included in distance
  calculations.
- **Gradient noise filter** — Steps with a horizontal distance smaller than 1 m
  are excluded from gradient calculations to avoid division by near-zero values
  from GPS noise.
- **Timestamps** — Parsed with `strptime` / `timegm` (UTC). Requires a
  POSIX-compatible standard library (Linux / glibc).

## Project structure

```
.
├── CMakeLists.txt       Build definition; fetches pugixml v1.14 via FetchContent
└── src/
    ├── types.hpp       Project-wide scalar type aliases (Real, Int, Size, ...)
    ├── arg_parser.hpp   Options struct + parse()/print_usage() (namespace arg_parser)
    ├── arg_parser.cpp   Command-line parsing and usage text
    ├── gpx_reader.hpp   Data structs (TrackPoint, Track, GpxData, TrackStats,
    │                    Hill, BestSegment) and GpxReader class declaration
    ├── gpx_reader.cpp   GPX parsing (pugixml), statistics, hill detection and
    │                    fastest-segment sliding-window algorithms
    ├── signal.hpp       SpectralResult struct and compute_acf_psd() declaration
    ├── signal.cpp       Resampling, FFT and Wiener–Khinchin autocorrelation/PSD
    ├── wind.hpp         WindData helpers (fetch / load / save) declarations
    ├── wind.cpp         Open-Meteo wind fetch (curl) and JSON cache I/O
    ├── io_base.hpp      Shared I/O helpers (format_duration) — namespace io
    ├── io_base.cpp      Shared I/O helper implementations
    ├── screen_output.hpp  print_* declarations for the stdout report (namespace io)
    ├── screen_output.cpp  Formatted console output
    ├── file_output.hpp  write_* declarations for the data-file exports (namespace io)
    ├── file_output.cpp  CSV / whitespace-table writers
    └── main.cpp         Program flow (parse → analyse → report); data acquisition helpers
```
