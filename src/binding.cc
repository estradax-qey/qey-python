#include <nanobind/nanobind.h>
#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>
#include <nanobind/stl/vector.h>

#include <chrono>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "qey/db.h"

namespace nb = nanobind;
using namespace nb::literals;

namespace {

nb::exception<std::runtime_error>* g_error = nullptr;

[[noreturn]] void Raise(const qey::Status& status) {
  PyErr_SetString(g_error->ptr(), status.message().c_str());
  throw nb::python_error();
}

template <typename T>
T Unwrap(qey::StatusOr<T> result) {
  if (!result.ok()) Raise(result.status());
  return std::move(result).value();
}

void Check(const qey::Status& status) {
  if (!status.ok()) Raise(status);
}

qey::StreamId ParseId(const std::string& text) {
  size_t dash = text.find('-');
  try {
    if (dash == std::string::npos) return {std::stoull(text), 0};
    return {std::stoull(text.substr(0, dash)), std::stoull(text.substr(dash + 1))};
  } catch (const std::exception&) {
    throw nb::value_error("a stream ID looks like \"1700000000000-0\"");
  }
}

using Entries = std::vector<std::pair<std::string, std::string>>;

Entries ToPython(const std::vector<qey::StreamEntry>& entries) {
  Entries out;
  out.reserve(entries.size());
  for (const auto& e : entries) out.emplace_back(e.id.ToString(), e.payload);
  return out;
}

// Owns the store; close() releases the file lock before the object dies.
class PyDb {
 public:
  PyDb(std::optional<std::string> path, const std::string& sync) {
    if (!path) {
      db_ = std::make_unique<qey::Db>();
      return;
    }
    qey::OpenOptions options;
    if (sync == "always") options.sync = qey::SyncPolicy::kAlways;
    else if (sync == "interval") options.sync = qey::SyncPolicy::kInterval;
    else if (sync == "manual") options.sync = qey::SyncPolicy::kManual;
    else throw nb::value_error("sync must be \"always\", \"interval\" or \"manual\"");
    db_ = Unwrap(qey::Db::Open(*path, options));
  }

  qey::Db& db() {
    if (!db_) throw nb::value_error("the database is closed");
    return *db_;
  }
  void Close() { db_.reset(); }

 private:
  std::unique_ptr<qey::Db> db_;
};

}  // namespace

NB_MODULE(_qey, m) {
  m.attr("__version__") = "0.1.0";
  static nb::exception<std::runtime_error> error(m, "Error");
  g_error = &error;

  nb::class_<PyDb>(m, "Db")
      .def(nb::init<std::optional<std::string>, const std::string&>(),
           "path"_a = nb::none(), "sync"_a = "always")
      .def("close", &PyDb::Close)
      .def("__enter__", [](nb::handle self) { return self; })
      .def("__exit__", [](PyDb& d, nb::args) { d.Close(); })
      // Strings
      .def("set",
           [](PyDb& d, const std::string& key, const std::string& value,
              std::optional<int64_t> ttl_ms) {
             std::optional<std::chrono::milliseconds> ttl;
             if (ttl_ms) ttl = std::chrono::milliseconds(*ttl_ms);
             Check(d.db().Set(key, value, ttl));
           },
           "key"_a, "value"_a, "ttl_ms"_a = nb::none())
      .def("get", [](PyDb& d, const std::string& k) { return d.db().Get(k); })
      .def("delete", [](PyDb& d, const std::string& k) { return Unwrap(d.db().Del(k)); })
      .def("exists", [](PyDb& d, const std::string& k) { return d.db().Exists(k); })
      .def("append", [](PyDb& d, const std::string& k, const std::string& s) {
        return Unwrap(d.db().Append(k, s));
      })
      .def("incr", [](PyDb& d, const std::string& k, int64_t by) {
        return Unwrap(d.db().Incr(k, by));
      }, "key"_a, "by"_a = 1)
      .def("decr", [](PyDb& d, const std::string& k, int64_t by) {
        return Unwrap(d.db().Decr(k, by));
      }, "key"_a, "by"_a = 1)
      .def("keys", [](PyDb& d, const std::string& p) { return d.db().Keys(p); },
           "prefix"_a = "")
      .def("__len__", [](PyDb& d) { return d.db().Size(); })
      .def("sync", [](PyDb& d) { Check(d.db().Sync()); })
      .def("compact", [](PyDb& d) { Check(d.db().Compact()); })
      // Hashes
      .def("hset", [](PyDb& d, const std::string& k, const std::string& f,
                      const std::string& v) { return Unwrap(d.db().HSet(k, f, v)); })
      .def("hget", [](PyDb& d, const std::string& k, const std::string& f) {
        return Unwrap(d.db().HGet(k, f));
      })
      .def("hgetall", [](PyDb& d, const std::string& k) {
        nb::dict out;
        for (auto& [f, v] : Unwrap(d.db().HGetAll(k))) out[nb::str(f.c_str())] = nb::str(v.c_str());
        return out;
      })
      .def("hdel", [](PyDb& d, const std::string& k, const std::vector<std::string>& fs) {
        return Unwrap(d.db().HDel(k, fs));
      })
      // Lists
      .def("lpush", [](PyDb& d, const std::string& k, const std::vector<std::string>& v) {
        return Unwrap(d.db().LPush(k, v));
      })
      .def("rpush", [](PyDb& d, const std::string& k, const std::vector<std::string>& v) {
        return Unwrap(d.db().RPush(k, v));
      })
      .def("lpop", [](PyDb& d, const std::string& k, size_t n) {
        return Unwrap(d.db().LPop(k, n));
      }, "key"_a, "count"_a = 1)
      .def("rpop", [](PyDb& d, const std::string& k, size_t n) {
        return Unwrap(d.db().RPop(k, n));
      }, "key"_a, "count"_a = 1)
      .def("llen", [](PyDb& d, const std::string& k) { return Unwrap(d.db().LLen(k)); })
      .def("lrange", [](PyDb& d, const std::string& k, int64_t a, int64_t b) {
        return Unwrap(d.db().LRange(k, a, b));
      }, "key"_a, "start"_a = 0, "stop"_a = -1)
      // Sets
      .def("sadd", [](PyDb& d, const std::string& k, const std::vector<std::string>& v) {
        return Unwrap(d.db().SAdd(k, v));
      })
      .def("srem", [](PyDb& d, const std::string& k, const std::vector<std::string>& v) {
        return Unwrap(d.db().SRem(k, v));
      })
      .def("smembers", [](PyDb& d, const std::string& k) {
        return Unwrap(d.db().SMembers(k));
      })
      // Streams. IDs are "ms-seq" strings.
      .def("xadd", [](PyDb& d, const std::string& k, const std::string& payload,
                      std::optional<size_t> max_len) {
        return Unwrap(d.db().XAdd(k, payload, max_len)).ToString();
      }, "key"_a, "payload"_a, "max_len"_a = nb::none())
      .def("xlen", [](PyDb& d, const std::string& k) { return Unwrap(d.db().XLen(k)); })
      .def("xrange", [](PyDb& d, const std::string& k, const std::string& start,
                        size_t count) {
        return ToPython(Unwrap(d.db().XRange(k, ParseId(start), count)));
      }, "key"_a, "start"_a = "0-0", "count"_a = 100)
      .def("xrevrange", [](PyDb& d, const std::string& k, size_t count) {
        return ToPython(Unwrap(d.db().XRevRange(k, count)));
      }, "key"_a, "count"_a = 100)
      .def("xtrim", [](PyDb& d, const std::string& k, size_t max_len) {
        return Unwrap(d.db().XTrimMaxLen(k, max_len));
      });
}
