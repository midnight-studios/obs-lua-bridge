# Third-party notices

Lua Bridge for OBS is licensed under the GNU General Public License, version 2
or later (see `LICENSE`). The Lua helper and example scripts in `lua/` are
MIT-licensed (see `lua/LICENSE`).

It includes or is built with the following third-party code.

## obs-websocket API header
`src/third-party/obs-websocket-api.h`, from https://github.com/obsproject/obs-websocket

Copyright (C) 2016-2021 Stephane Lepin, Copyright (C) 2020-2022 Kyle Manning.
Licensed under the GNU General Public License, version 2 or later (the same
license as this plugin; see `LICENSE`).

## JSON for Modern C++ (nlohmann/json) 3.11.3
https://github.com/nlohmann/json, compiled into the plugin.

```
MIT License

Copyright (c) 2013-2023 Niels Lohmann

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

## Not shipped with this plugin
libobs, the OBS frontend API and Qt 6 are provided by OBS Studio at run time
and are not included in Lua Bridge packages.
