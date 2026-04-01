# Tiny HTTP/1.1 Server + AFLGo Target

This repository now contains two separate C++ programs:

- `server/`: a small real HTTP/1.1 server that listens on a TCP port.
- `fuzz/`: a file-driven HTTP/1.1 request parser with many intentional, deterministic crash bugs for AFL/AFLGo testing.

The `fuzz/` target is the primary AFLGo target. It is intentionally unsafe by design.

## Layout

- `server/main.cpp`: real socket-based HTTP/1.1 server.
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

- `build/mini_http_server`
- `build/mini_http_fuzz`

You can also build individually:

```bash
make server
make fuzz
```

## Real Server

### Purpose

`server/` is a tiny real HTTP/1.1 server. It is intentionally small and incomplete. It is meant to be understandable and easy to run, not production-safe.

### Expected behavior

It accepts one connection per threadless loop iteration and handles a single request per connection.

Supported behavior:

- Parses a request line in the form `METHOD PATH HTTP/1.1`
- Parses headers until `\r\n\r\n`
- Supports `GET` and `POST`
- Reads the body when `Content-Length` is present
- Requires `Host` for valid `HTTP/1.1`
- Returns simple text responses

Routes:

- `GET /`: returns a short banner
- `GET /health`: returns `ok`
- `POST /echo`: echoes the request body
- anything else: returns `404`

Known simplifications:

- no TLS
- no keep-alive reuse
- no chunked decoding
- no pipelining
- no persistent worker model
- no filesystem serving

### Run

```bash
./build/mini_http_server 8080
```

Then test it with:

```bash
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/health
curl -i -X POST http://127.0.0.1:8080/echo -d 'hello'
```

### How it should work internally

The server does this for each connection:

1. `accept()` a client.
2. Read from the socket into a string buffer.
3. Stop once headers are complete and the full `Content-Length` body is available.
4. Parse the request line and headers.
5. Route based on method and path.
6. Write one HTTP/1.1 response.
7. Close the socket.

This design keeps the code small and deterministic.

## Fuzz Target

### Purpose

`fuzz/` is the target that should be used for AFLGo. It is deliberately engineered to contain many reachable crashing bugs behind small HTTP-parsing and routing decisions.

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

The local `aflgo/` checkout in this repository documents the standard instrumentation flow. The short version for this target is:

1. Build `fuzz/` once with AFLGo target extraction flags.
2. Generate `BBnames.txt` and `BBcalls.txt` into a temporary directory.
3. Compute `distance.cfg.txt` for selected target lines.
4. Rebuild `fuzz/` with `-distance=...`.
5. Run `afl-fuzz` against `build/mini_http_fuzz @@`.

### Important environment note

The bundled AFLGo tree is Linux-oriented. The intended place to run AFLGo is Linux or WSL, not native Windows PowerShell.

The real `server/` target also uses POSIX sockets and is intended to be built and run on Linux or WSL.

### Example target selection process

Pick one or more crash lines in `fuzz/bugs.cpp` and write them into `BBtargets.txt` as:

```text
fuzz/bugs.cpp:LINE_NUMBER
```

Then follow the AFLGo flow from `aflgo/Readme.md` using `build/mini_http_fuzz` as the subject binary.

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
make fuzz CXXFLAGS="-std=c++17 -O0 -g $ADDITIONAL"
```

After the first build, clean up `BBnames.txt` and `BBcalls.txt` as described in `aflgo/Readme.md`, compute distances, then rebuild with `-distance=$TMP_DIR/distance.cfg.txt`.

### Example fuzzing command

```bash
mkdir -p out
$AFLGO/afl-2.57b/afl-fuzz -m none -z exp -c 45m -i fuzz/seeds -o out ./build/mini_http_fuzz @@
```

## Safety Note

The fuzz target is intentionally vulnerable and crashy. Do not expose it as a network service.
