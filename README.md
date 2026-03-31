# gpx_reader

A C++17 command-line tool that parses GPX files and computes track statistics,
detects hills, and finds the fastest segment over a user-defined distance or
time window.

## Features

- Parse GPX 1.1 files (Garmin Connect and compatible devices)
- Per-track statistics: distance, elevation gain/loss, duration, average speed,
  average temperature, average climb and descent gradient
- Hill detection table: distance, elevation gain and average grade per climb
- Fastest segment finder: sliding-window search by distance or time
- Multiple `--dist` and `--time` queries supported in a single run

## Requirements

- C++17-compatible compiler (e.g. GCC >= 8)
- CMake >= 3.16
- Internet access at first configure — [pugixml](https://github.com/zeux/pugixml)
  is fetched automatically via CMake `FetchContent`, no manual install needed

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
  --points N   print first N track points (default: 10, use 0 to suppress)
  --dist  D    find fastest segment of D km    (e.g. --dist 5.0)
  --time  T    find fastest segment of T s     (e.g. --time 300)
```

Multiple `--dist` and `--time` flags are supported and each is reported
separately.

### Examples

```bash
# Basic run — show first 10 track points and overall statistics
./build/gpx_reader ride.gpx

# Suppress point listing; find fastest 1 km, 5 km, 5-minute and 10-minute segments
./build/gpx_reader ride.gpx --points 0 --dist 1.0 --dist 5.0 --time 300 --time 600
```

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
and (if present) air temperature. Controlled by `--points N`.

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
| Start time | ISO-8601 timestamp at the start of the climb |

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

## GPX format support

The parser reads GPX 1.1 files. The following elements are extracted:

| Element / Attribute | Field |
|---|---|
| `<trkpt lat="..." lon="...">` | Latitude and longitude |
| `<ele>` | Elevation in metres |
| `<time>` | ISO-8601 timestamp (UTC) |
| `<ns3:TrackPointExtension><ns3:atemp>` | Air temperature in °C |
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
    ├── gpx_reader.h     Data structs (TrackPoint, Track, GpxData, TrackStats,
    │                    Hill, BestSegment) and GpxReader class declaration
    ├── gpx_reader.cpp   GPX parsing (pugixml), statistics, hill detection and
    │                    fastest-segment sliding-window algorithms
    └── main.cpp         CLI argument parsing and all formatted console output
```
