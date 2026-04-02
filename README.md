# AFLGo HTTP Target

This repository is primarily for testing AFLGo with a very small C++ HTTP/1.1-style request handler that contains many intentional deterministic crashes.

The main subject binary is:

- `build/mini_http_handler`

That binary uses the real shared handler logic in:

- `server/http_core.cpp`

The optional socket server in `server/main.cpp` exists only as a thin wrapper around the same core. It is not the main focus.

There is also a separate synthetic target under `fuzz/`, but the preferred AFLGo subject is the standalone handler built from `server/`.

## Layout

- `server/main.cpp`: optional socket wrapper around the shared handler.
- `server/http_core.cpp`: primary AFLGo target logic with intentional crash sites.
- `server/http_core.hpp`: shared declarations.
- `server/harness.cpp`: standalone executable for fuzzing the shared handler.
- `server/seeds/`: seed corpus for the primary AFLGo subject.
- `server/targets/README.md`: notes on selecting target lines in `server/http_core.cpp`.
- `fuzz/main.cpp`: entrypoint that reads one request from a file.
- `fuzz/http_target.cpp`: request parsing and route dispatch.
- `fuzz/http_target.hpp`: shared request/handler declarations.
- `fuzz/bugs.cpp`: intentionally crashing bug sinks.
- `fuzz/seeds/`: small seed corpus for AFL/AFLGo.
- `fuzz/targets/README.md`: how to select AFLGo targets.

## Build

On Linux or WSL:

```bash
make
```

This produces:

- `build/mini_http_handler`
- `build/mini_http_server`
- `build/mini_http_fuzz`

You can also build individually:

```bash
make server
make server-handler
make fuzz
```

## Primary Target

`server/harness.cpp` is the main AFLGo entrypoint. It reads one HTTP request from a file, calls the shared parser/handler in `server/http_core.cpp`, and prints an HTTP-like response unless one of the intentional bug paths crashes first.

This is the target to hand off, compile separately, and fuzz.

Build it with:

```bash
make server-handler
```

Run it with:

```bash
./build/mini_http_handler server/seeds/get_root.txt
./build/mini_http_handler server/seeds/post_echo.txt
```

The parser accepts both `CRLF` and plain `LF` line endings.

### What it parses

- request line: `METHOD SP PATH SP HTTP/1.1`
- headers
- optional body from `Content-Length`
- simple route dispatch based on path and a few headers

### Why this target is AFLGo-friendly

- file-driven and deterministic
- small control-flow graph
- stable target lines in one file: `server/http_core.cpp`
- multiple shallow and deep crash sites behind parser and dispatcher decisions
- no socket setup required for fuzzing

### Intentional crash paths

The shared handler contains deterministic crash sinks behind combinations of:

- path
- method
- `Authorization`
- duplicate `Host`
- `Content-Length`
- `Transfer-Encoding`
- `X-Debug`
- query markers
- body magic bytes

The actual crash lines live directly in `server/http_core.cpp`, which is the file AFLGo should target.

See:

- `server/targets/README.md`

## Fuzz Target

### Purpose

`fuzz/` is an extra synthetic target with similar ideas. It is no longer the preferred subject.

It does not open sockets. It reads a single input file and interprets it as one HTTP/1.1 request.

### Why this is better for AFLGo

- deterministic input model
- no network timing issues
- small control-flow graph
- stable line-based targets for `BBtargets.txt`
- easier seed corpus construction

### Run manually

```bash
./build/mini_http_fuzz fuzz/seeds/get_root.txt
./build/mini_http_fuzz fuzz/seeds/post_echo.txt
```

The parser accepts either canonical HTTP CRLF line endings or plain LF line endings in the seed corpus. This makes hand-written seeds and mutated inputs easier to use.

### What it parses

- request line: `METHOD SP PATH SP VERSION`
- headers
- optional body from `Content-Length`
- optional chunked path recognition for a tiny special-case parser

### Routes and bug paths

Routes include:

- `/echo`
- `/login`
- `/upload`
- `/admin`
- `/chunk`

Several intentional bugs live behind combinations of:

- path
- method
- duplicate headers
- `Authorization`
- `Content-Length`
- `Transfer-Encoding`
- `X-Debug`
- magic body bytes or query markers

The crashes are deterministic and chosen to be easy for AFL/AFLGo to detect.

## AFLGo Workflow

The local `aflgo/` checkout in this repository documents the standard instrumentation flow. The short version for the preferred target is:

1. Build `server/` once with AFLGo target extraction flags.
2. Generate `BBnames.txt` and `BBcalls.txt` into a temporary directory.
3. Compute `distance.cfg.txt` for selected target lines.
4. Rebuild `fuzz/` with `-distance=...`.
5. Run `afl-fuzz` against `build/mini_http_handler @@`.

### Important environment note

The bundled AFLGo tree is Linux-oriented. The intended place to run AFLGo is Linux or WSL, not native Windows PowerShell.

The optional socket wrapper in `server/main.cpp` uses POSIX sockets and is intended for Linux or WSL.

The standalone `server/harness.cpp` target does not use sockets and is the easiest way to apply AFLGo or regular AFL to the actual handler code.

### Example target selection process

Pick one or more crash lines in `server/http_core.cpp` and write them into `BBtargets.txt` as:

```text
server/http_core.cpp:LINE_NUMBER
```

Then follow the AFLGo flow from `aflgo/Readme.md` using `build/mini_http_handler` as the subject binary.

### Example Linux build for target extraction

This is the intended shape of the commands:

```bash
export AFLGO=$PWD/aflgo
export TMP_DIR=$PWD/temp
mkdir -p "$TMP_DIR"

export CC=$AFLGO/instrument/aflgo-clang
export CXX=$AFLGO/instrument/aflgo-clang++
export ADDITIONAL="-targets=$TMP_DIR/BBtargets.txt -outdir=$TMP_DIR -flto -fuse-ld=gold -Wl,-plugin-opt=save-temps"

make clean
make server-handler CXXFLAGS="-std=c++17 -O0 -g $ADDITIONAL"
```

After the first build, clean up `BBnames.txt` and `BBcalls.txt` as described in `aflgo/Readme.md`, compute distances, then rebuild with `-distance=$TMP_DIR/distance.cfg.txt`.

### Example fuzzing command

```bash
mkdir -p out
$AFLGO/afl-2.57b/afl-fuzz -m none -z exp -c 45m -i server/seeds -o out ./build/mini_http_handler @@
```

## Safety Note

The standalone handler and synthetic fuzz target are intentionally vulnerable and crashy. Do not expose them as network services.
