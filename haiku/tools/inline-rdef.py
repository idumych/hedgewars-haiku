#!/usr/bin/env python3
"""Copies .rdef files with every `import "file"` replaced by the file's bytes as a
hex literal ($"..."). VitruvianOS's rc finds imported files but can't read them on a
plain Linux build machine; inline data needs no file access.

    python3 haiku/tools/inline-rdef.py OUT.rdef IN.rdef...   (paths relative to the tree)
"""
import re
import sys

out, sources = sys.argv[1], sys.argv[2:]
text = ''.join(open(s).read() + '\n' for s in sources)


def inline(m):
    data = open(m.group(1), 'rb').read()
    return '$"' + data.hex().upper() + '"'


open(out, 'w').write(re.sub(r'(?:array\s+)?import\s+"([^"]+)"', inline, text))
