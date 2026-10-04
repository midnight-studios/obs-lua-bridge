"""Writes the seed corpus for registry-fuzz (tests/fuzz/corpus/).

Each seed is a sequence of operations: selector byte, then owner, name and json
separated by 0x01; operations separated by 0xFF (see registry-fuzz.cpp).

Run:  py tests/fuzz/make_corpus.py
"""

import pathlib

HERE = pathlib.Path(__file__).resolve().parent
CORPUS = HERE / "corpus"

REGISTER, SET_STATE, COMMAND, EMIT, HEARTBEAT, UNREGISTER, SNAPSHOT, PLAN, RATE, VALUE, STALE = range(11)

B7 = (
    '{"display_name":"Stopwatch","commands":[{"id":"start","label":"Start"},{"id":"reset","confirm":true},'
    '{"id":"add","args":{"seconds":"int"}},{"id":"flip","args":{"value":"bool"}}],"dock":['
    '{"type":"label","bind":"display","style":"large"},{"type":"row","items":[{"type":"button","command":"start",'
    '"label_bind":"start_label"},{"type":"button","command":"reset"}]},{"type":"number","id":"secs","min":1,"max":60},'
    '{"type":"button","command":"add","args_from":{"seconds":"secs"}},{"type":"toggle","bind":"on","command":"flip"},'
    '{"type":"text","id":"t","default":"x"},{"type":"separator"},{"type":"slider"}]}'
)


def op(selector, owner=b"", name=b"", json=b""):
    def b(x):
        return x if isinstance(x, bytes) else x.encode("utf-8")

    return bytes([selector]) + b(owner) + b"\x01" + b(name) + b"\x01" + b(json)


def seq(*ops):
    return b"\xff".join(ops)


SEEDS = {
    "lifecycle": seq(
        op(REGISTER, "sw", "", B7),
        op(SET_STATE, "sw", "", '{"display":"00:01","on":true,"start_label":"Starting…"}'),
        op(COMMAND, "sw", "add", '{"seconds":5}'),
        op(EMIT, "sw", "score.changed", '{"items":[{"id":1}]}'),
        op(HEARTBEAT, "sw"),
        op(STALE),
        op(PLAN),
        op(SNAPSHOT, "sw"),
        op(SET_STATE, "sw", "", '{"display":null}'),
        op(UNREGISTER, "sw"),
        op(PLAN),
    ),
    "unicode": seq(
        op(REGISTER, "u", "", '{"display_name":"café \U0001f3ae ‮RTL ﻿BOM ￿"}'),
        op(SET_STATE, "u", "", '{"k":"\\ud83c\\udfae","z":"\\u0000"}'),
        op(COMMAND, "u", "x", '{"a":"\\ud800"}'),
    ),
    "invalid_utf8": seq(op(REGISTER, b"a", b"", b'{"display_name":"\xff\xfe\xc3"}'), op(SET_STATE, b"a\x00b", b"", b"{\x00}")),
    "deep": seq(op(REGISTER, "d", "", '{"display_name":"d","x":' + "[" * 2000 + "]" * 2000 + "}")),
    "limits": seq(
        op(REGISTER, "o" * 64, "", '{"display_name":"' + "n" * 128 + '"}'),
        op(SET_STATE, "o" * 64, "", "{" + ",".join(f'"k{i}":{i}' for i in range(257)) + "}"),
        op(RATE, "o" * 64),
        op(RATE, "o" * 64),
    ),
    "values": seq(op(VALUE, "", "Start", '"x"'), op(VALUE, "", "", "2.50"), op(VALUE, "", "", '{"min":0,"max":0.5}')),
}


def main():
    CORPUS.mkdir(exist_ok=True)
    for name, data in SEEDS.items():
        (CORPUS / f"{name}.bin").write_bytes(data)
    print(f"wrote {len(SEEDS)} seeds to {CORPUS}")


if __name__ == "__main__":
    main()
