import pytest
import qey


def test_strings():
    db = qey.Db()
    db.set("a", "1")
    assert db.get("a") == "1"
    assert db.incr("a", 4) == 5
    assert db.get("missing") is None
    assert db.delete("a") is True
    assert not db.exists("a")


def test_collections():
    db = qey.Db()
    db.hset("h", "f", "v")
    assert db.hgetall("h") == {"f": "v"}
    db.rpush("l", ["a", "b", "c"])
    assert db.lrange("l") == ["a", "b", "c"]
    assert db.lpop("l") == ["a"]
    db.sadd("s", ["x", "y"])
    assert sorted(db.smembers("s")) == ["x", "y"]


def test_stream_resume():
    db = qey.Db()
    ids = [db.xadd("s", f'{{"n":{i}}}') for i in range(3)]
    assert db.xlen("s") == 3
    entries = db.xrange("s")
    assert [e[0] for e in entries] == ids
    assert db.xrevrange("s", 1)[0][0] == ids[-1]


def test_wrong_type_raises():
    db = qey.Db()
    db.set("k", "v")
    with pytest.raises(qey.Error):
        db.rpush("k", ["a"])


def test_persistence(tmp_path):
    path = str(tmp_path / "t.qey")
    with qey.Db(path) as db:
        db.set("k", "v")
        db.xadd("s", "p")
    with qey.Db(path) as db:
        assert db.get("k") == "v"
        assert db.xlen("s") == 1
    with pytest.raises(ValueError):
        db.get("k")
