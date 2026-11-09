"""Minimal KiCad S-expression reader/writer used by the schematic bootstrap.

Only what the generator needs: parse a file into nested lists, where atoms are
`Sym` (bare words) or `str` (quoted strings), and serialise them back.
"""

import re


class Sym(str):
    """A bare (unquoted) atom such as `pin`, `yes` or `1.27`."""


_TOKEN = re.compile(r'\s*(?:(\()|(\))|"((?:[^"\\]|\\.)*)"|([^\s()"]+))')


def parse(text):
    stack, cur = [], []
    pos = 0
    while pos < len(text):
        m = _TOKEN.match(text, pos)
        if not m:
            if text[pos:].strip() == "":
                break
            raise ValueError(f"bad token at {pos}: {text[pos : pos + 40]!r}")
        pos = m.end()
        if m.group(1):
            stack.append(cur)
            cur = []
        elif m.group(2):
            done = cur
            cur = stack.pop()
            cur.append(done)
        elif m.group(3) is not None:
            cur.append(m.group(3).replace('\\"', '"').replace("\\\\", "\\"))
        else:
            cur.append(Sym(m.group(4)))
    return cur


def dumps(node, indent=0):
    pad = "\t" * indent
    if not isinstance(node, list):
        if isinstance(node, Sym):
            return str(node)
        return '"' + node.replace("\\", "\\\\").replace('"', '\\"') + '"'
    simple = all(not isinstance(x, list) for x in node)
    if simple:
        return "(" + " ".join(dumps(x) for x in node) + ")"
    out = "(" + " ".join(dumps(x) for x in node if not isinstance(x, list))
    for x in node:
        if isinstance(x, list):
            out += "\n" + pad + "\t" + dumps(x, indent + 1)
    return out + "\n" + pad + ")"


def find(node, key):
    return [x for x in node if isinstance(x, list) and x and x[0] == key]


def first(node, key):
    r = find(node, key)
    return r[0] if r else None
