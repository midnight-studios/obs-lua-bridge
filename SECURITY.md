# Security policy

## Reporting a vulnerability
Please **don't** open a public issue for a security problem. Report it privately through GitHub:
1. Open the repository's **[Security](https://github.com/midnight-studios/obs-lua-bridge/security)** tab.
2. Click **Report a vulnerability** (GitHub's private vulnerability reporting).
3. Describe the problem, the affected version (from the OBS log line
   `[lua-bridge] plugin loaded successfully (version …)`), and how to reproduce it.

Only the maintainers can see the report. We aim to acknowledge it within a week, and we'll keep you
updated until it's fixed. We're happy to credit you in the release notes if you want.

## Scope
**In scope:**
- the Lua Bridge plugin;
- its obs-websocket vendor requests (`LuaBridge`), for example input that crashes or hangs OBS, or bypasses the
  rate limit;
- the Lua helper `luabridge.lua`.

**Out of scope:**
- OBS Studio itself, obs-websocket, and third-party scripts that use the bridge. Report those to their own
  projects.
- Anything that needs a script you chose to run: Lua scripts in OBS already run with your user's permissions.

## Supported versions
During the beta, only the latest release gets fixes.
