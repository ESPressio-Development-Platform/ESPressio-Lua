# Third-party notices

## Lua 5.5.1

Source: https://github.com/lua/lua/tree/v5.5.1
Commit: `7579fc9d7ed90240487251dfb69168f8e64e9294`.
Official language documentation: https://www.lua.org/manual/5.5/.

`vendor/lua` contains unchanged C sources and headers from that official tag.
`vendor/SHA256SUMS.json` records every vendored file's SHA-256. The ESPressio wrapper
selects the core and supported standard libraries and compiles them as C++.
Do not separately compile vendor `.c` files. Upstream tests/interpreter sources
are preserved for provenance but are not linked into the binding.

The Lua license below applies to these files. The repository's Apache 2.0
license applies to the ESPressio-authored binding, not to this upstream code.

```text
Copyright (C) 1994-2026 Lua.org, PUC-Rio.

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```
