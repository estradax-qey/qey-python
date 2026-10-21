# qey-python

Python bindings for [qey](https://github.com/estradax-qey/qey), an embedded in-memory key-value store. The bindings are built with [nanobind](https://github.com/wjakob/nanobind), straight onto qey's C++ API, and compile qey from source.

```python
import qey

db = qey.Db("app.qey")          # durable; qey.Db() is in memory only
db.set("greeting", "hello", ttl_ms=60_000)
db.get("greeting")              # "hello"
db.incr("visits")

id = db.xadd("session:1", '{"role":"user","text":"hi"}')
db.xrange("session:1", start=id)  # [(id, payload), ...]
```

Keys and values are `str`. Errors raise `qey.Error`; using a closed database raises `ValueError`. IDs of stream entries are `"ms-seq"` strings.

Covered so far: strings, hashes, lists, sets and streams. Sorted sets, scans, write batches and TTL commands are not bound yet.

## Build

```sh
uv venv && uv pip install -e . pytest   # fetches qey v0.1.0 and builds it
pytest
# Build against a local checkout of qey instead:
uv pip install -e . -C cmake.define.QEY_SOURCE_DIR=../qey
```

Wheels are for Linux only (x86_64 and arm64), as qey is.
