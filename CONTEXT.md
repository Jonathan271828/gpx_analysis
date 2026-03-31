# gpx_analysis — Project Context

This file is intended to be read at the start of a new development session to
quickly restore full context about the project without re-reading all source
files from scratch.

---

## Purpose

`gpx_reader` is a C++17 command-line tool that parses GPX 1.1 files (e.g.
Garmin Connect exports) and computes:

- Overall track statistics (distance, elevation, speed, gradients, temperature)
- A table of all climbs detected on the track
- The fastest segment over a user-defined distance or time window

There is no GUI, no external runtime dependency (pugixml is fetched at build
time by CMake), and no file output — all results go to stdout.

---

## Repository layout

```
.
├── CMakeLists.txt       CMake build definition; fetches pugixml v1.14
├── README.md            User-facing documentation (shown on GitHub)
├── CONTEXT.md           This file — developer/AI session context
├── .gitignore           Excludes build/ and *.gpx
└── src/
    ├── gpx_reader.h     All public data types + GpxReader class declaration
    ├── gpx_reader.cpp   All algorithms — parsing, statistics, hill detection,
    │                    fastest-segment sliding window. No I/O.
    └── main.cpp         CLI argument parsing + all formatted console output.
                         Calls into GpxReader; never touches pugixml directly.
```

**Design principle:** `gpx_reader.cpp` contains zero I/O. `main.cpp` contains
zero algorithmic logic. All public types are in the header.

---

## Build system

```cmake
cmake_minimum_required(VERSION 3.16)
set(CMAKE_CXX_STANDARD 17)
# pugixml v1.14 fetched automatically via FetchContent
```

### Build commands

```bash
# Load compiler and CMake (example: HPC system with Lmod)
source /usr/share/lmod/8.7.49/init/bash
module load gcc/14.2.0 cmake/3.31.0

# Configure and build
mkdir build
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

Tested with:
- GCC 14.2.0 (Spack) on Rocky Linux 8 (x86_64)
- CMake 3.31.0
- pugixml 1.14

**Platform note:** `strptime` and `timegm` are used for timestamp parsing.
Both are POSIX / Linux/glibc only. On Windows, `timegm` does not exist and
would need to be replaced (e.g. with `_mkgmtime` on MSVC).

---

## Git / GitHub

- **Remote:** `git@github.com:Jonathan271828/gpx_analysis.git`
- **Branch:** `main`
- **SSH key:** `~/.ssh/github` (RSA), associated with `jonathan.lahnsteiner@gmx.at`
- **SSH config** (`~/.ssh/config`): `host github.com`, `user git`,
  `identityfile ~/.ssh/github` — already configured and tested working
- **`.gitignore`** excludes `build/` and `*.gpx` (GPX files are private data)
- **Git identity for this repo:**
  ```
  user.name  = Jonathan Lahnsteiner
  user.email = jonathan.lahnsteiner@gmx.at
  ```

---

## Data structures (`src/gpx_reader.h`)

### `TrackPoint`
One GPS sample from a `<trkpt>` element.

```cpp
struct TrackPoint {
    double      lat;        // latitude (degrees)
    double      lon;        // longitude (degrees)
    double      ele;        // elevation (metres)
    std::string time;       // ISO-8601 string, e.g. "2026-03-31T09:46:45.000Z"
    double      atemp;      // air temperature in °C (from ns3:atemp extension)
    bool        has_atemp;  // false if the extension was absent
};
```

### `Track`
One `<trk>` element. All `<trkseg>` children are flattened into a single
`points` vector.

```cpp
struct Track {
    std::string             name;    // from <name>
    std::string             type;    // from <type> (e.g. "gravel_cycling")
    std::vector<TrackPoint> points;
};
```

### `GpxData`
Top-level container returned by `GpxReader::data()`.

```cpp
struct GpxData {
    std::string        metadata_time;  // from <metadata><time>
    std::vector<Track> tracks;
};
```

### `TrackStats`
Computed by `GpxReader::compute_stats()`.

```cpp
struct TrackStats {
    std::size_t num_points;         // total track points
    double      total_distance_m;   // cumulative Haversine distance (m)
    double      elevation_gain_m;   // sum of positive ele deltas (m)
    double      elevation_loss_m;   // sum of negative ele deltas, positive value (m)
    double      min_ele_m;          // lowest elevation (m)
    double      max_ele_m;          // highest elevation (m)
    long        duration_s;         // first-to-last point elapsed time (s)
    double      avg_speed_kmh;      // total_distance / duration (km/h)
    double      avg_atemp;          // mean of all atemp values (°C)
    bool        has_atemp;          // false if no atemp data in track
    double      avg_climb_pct;      // mean gradient of uphill steps (%)
    double      avg_descent_pct;    // mean gradient of downhill steps (%, negative)
};
```

### `Hill`
One detected climb, produced by `GpxReader::detect_hills()`.

```cpp
struct Hill {
    std::size_t start_idx;      // index into track.points
    std::size_t end_idx;        // index of the peak point
    double      distance_m;     // horizontal distance from start to peak (m)
    double      gain_m;         // elevation: peak_ele - start_ele (m)
    double      start_ele_m;    // elevation at start (m)
    double      end_ele_m;      // elevation at peak (m)
    double      avg_grade_pct;  // gain_m / distance_m * 100 (%)
    std::string start_time;     // ISO-8601 timestamp at start
    std::string end_time;       // ISO-8601 timestamp at peak
};
```

### `BestSegment`
Result of `GpxReader::fastest_by_distance()` or `fastest_by_time()`.

```cpp
struct BestSegment {
    std::size_t start_idx;      // index into track.points
    std::size_t end_idx;
    double      distance_m;     // actual distance of the found window (m)
    long        duration_s;     // actual duration of the found window (s)
    double      avg_speed_kmh;  // distance_m/1000 / duration_s*3600 (km/h)
    std::string start_time;     // ISO-8601
    std::string end_time;       // ISO-8601
    double      start_lat;
    double      start_lon;
    double      end_lat;
    double      end_lon;
    bool        valid;          // false if window larger than the whole track
};
```

---

## GpxReader API (`src/gpx_reader.h`)

```cpp
class GpxReader {
public:
    bool              parse(const std::string& filepath);
    const GpxData&    data()          const;
    const std::string& error_message() const;

    TrackStats        compute_stats(std::size_t track_index = 0)       const;
    BestSegment       fastest_by_distance(double window_m,
                                          std::size_t track_index = 0) const;
    BestSegment       fastest_by_time(long window_s,
                                      std::size_t track_index = 0)     const;
    std::vector<Hill> detect_hills(std::size_t track_index = 0)        const;
};
```

All methods are `const` — parsing is the only mutating operation. The
`track_index` parameter selects which `<trk>` element to operate on (default
`0`); all methods return an empty/invalid result gracefully if the index is out
of range.

---

## Key algorithms (`src/gpx_reader.cpp`)

### Haversine distance (`haversine()`, anonymous namespace)

```cpp
double haversine(double lat1, double lon1, double lat2, double lon2);
```

Returns the great-circle distance in **metres** between two WGS-84 points.
Uses Earth radius R = 6 371 000 m. All distances in the project are
**horizontal / 2D** — elevation is never included in distance calculations.

### Timestamp parsing (`parse_iso8601()`, anonymous namespace)

```cpp
std::time_t parse_iso8601(const std::string& s);  // returns -1 on failure
```

Uses `strptime` with format `"%Y-%m-%dT%H:%M:%S"` then `timegm` for UTC
interpretation. Fractional seconds and the trailing `Z` are ignored.

---

### `GpxReader::parse()` — GPX parsing (lines 51–111)

Uses pugixml. Key implementation details:
- `doc.load_file()` for file I/O
- All `<trkseg>` children of a `<trk>` are iterated and their points appended
  to the same `Track::points` vector (segments are flattened)
- The Garmin temperature extension is accessed as `ns3:TrackPointExtension` /
  `ns3:atemp` — pugixml treats the namespace prefix as a literal part of the
  element name, so no special namespace handling is needed

---

### `GpxReader::compute_stats()` — statistics (lines 117–204)

Single forward pass over `track.points`. Key details:

- **Gradient noise filter:** gradient is only computed for steps where
  `horiz_dist >= 1.0 m` to avoid GPS noise inflating gradient averages
- **Gradient classification:** `delta > 0` → climb accumulator;
  `delta < 0` → descent accumulator; `delta == 0` or filtered → ignored
- **Duration** is computed from `parse_iso8601(pts.front().time)` to
  `parse_iso8601(pts.back().time)` after the main loop

---

### `fastest_by_distance()` and `fastest_by_time()` — sliding window (lines 265–354)

Both use the same two-helper setup:

**`build_tables(pts, cum_dist, timestamps)`** (anonymous namespace, line 215)
- Builds `cum_dist[i]` = cumulative Haversine distance from point 0 to point i
- Builds `timestamps[i]` = `parse_iso8601(pts[i].time)`, or -1 on failure

**`make_segment(lo, hi, pts, cum_dist, timestamps)`** (anonymous namespace, line 234)
- Constructs a `BestSegment` from indices `lo`..`hi` using the precomputed tables

**Algorithm (O(N) two-pointer):**
```
hi = 1
for lo = 0 .. N-2:
    advance hi until window condition is met (dist >= window_m  OR  time >= window_s)
    compute speed = distance / duration
    if speed > best so far → update best
```

Early exit if the entire track is shorter than the requested window
(`best.valid` stays `false`).

---

### `GpxReader::detect_hills()` — state machine (lines 360–456)

**Thresholds (compile-time constants):**

| Constant | Value | Meaning |
|---|---|---|
| `MIN_GRADE_PCT` | 1.0 % | Minimum step gradient to enter CLIMBING state |
| `MIN_GAIN_M` | 10.0 m | Minimum total gain for a hill to be recorded |
| `GAP_TOLERANCE_M` | 20.0 m | Cumulative descent absorbed before ending a hill |

**State machine — two states: `FLAT` and `CLIMBING`:**

```
FLAT → CLIMBING   when step grade >= MIN_GRADE_PCT
CLIMBING → FLAT   when gap_loss > GAP_TOLERANCE_M
```

`gap_loss` accumulates negative elevation deltas since the last local
high-point (`peak_idx` / `peak_ele`). When a new high-point is found,
`peak_idx`, `peak_ele` and `gap_loss` are all reset.

**`commit_hill(end_idx)` lambda:**
- Computes `gain = pts[end_idx].ele - pts[hill_start].ele`
- Discards if `gain < MIN_GAIN_M` or `dist < 1.0`
- Otherwise pushes a `Hill` onto the result vector

**End-of-track:** if the state machine is still `CLIMBING` when the loop
finishes, `commit_hill(peak_idx)` is called to flush the open hill.

---

## CLI (`src/main.cpp`)

```
./build/gpx_reader <file.gpx> [options]

  --points N   print first N track points  (default: 10; 0 = suppress)
  --dist  D    fastest segment of D km     (e.g. --dist 5.0)
  --time  T    fastest segment of T s      (e.g. --time 300)
```

Multiple `--dist` and `--time` flags are supported; each produces a separate
output block. Unrecognised flags print usage and exit with `EXIT_FAILURE`.

**Output order per track:**
1. Track point listing (`print_track_points`)
2. Statistics block (`print_stats`)
3. Hills table (`print_hills`) — always printed
4. One fastest-segment block per `--dist` value (`print_best_segment`)
5. One fastest-segment block per `--time` value (`print_best_segment`)

---

## GPX format assumptions

The parser expects GPX 1.1 as exported by Garmin Connect. Fields extracted:

| GPX element / attribute | Maps to |
|---|---|
| `<trkpt lat="..." lon="...">` | `TrackPoint::lat`, `TrackPoint::lon` |
| `<trkpt><ele>` | `TrackPoint::ele` |
| `<trkpt><time>` | `TrackPoint::time` |
| `<extensions><ns3:TrackPointExtension><ns3:atemp>` | `TrackPoint::atemp` |
| `<trk><name>` | `Track::name` |
| `<trk><type>` | `Track::type` |
| `<metadata><time>` | `GpxData::metadata_time` |

Missing fields degrade gracefully: missing `<ele>` → 0.0, missing `<time>` →
empty string (timestamps return -1 and are skipped in calculations), missing
`ns3:atemp` → `has_atemp = false`.

Multiple `<trk>` elements are fully supported. Multiple `<trkseg>` elements
within a track are flattened into one `points` vector.

---

## Known limitations and possible extensions

| Area | Current state | Possible extension |
|---|---|---|
| Heart rate | Not parsed | Add `ns3:hr` field to `TrackPoint` |
| Cadence | Not parsed | Add `ns3:cad` field to `TrackPoint` |
| Power | Not parsed | Add `ns3:watts` or similar |
| Hill thresholds | Compile-time constants | Expose `--min-grade`, `--min-gain`, `--gap-tolerance` CLI flags |
| Gradient distance | Horizontal (2D) only | True slope distance = `sqrt(horiz² + delta_ele²)` |
| Output format | Plain text to stdout | Add `--csv` or `--json` export flag |
| Windows support | Broken (`timegm` missing) | Replace `timegm` with `_mkgmtime` or a portable implementation |
| Multiple files | Not supported | Accept a list of GPX files, aggregate or compare |
