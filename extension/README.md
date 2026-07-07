# Claude Desktop extension (MCP Bundle)

This folder holds the [MCP Bundle](https://github.com/anthropics/mcpb) manifest
that packages RouteMIDI as a Claude Desktop extension, so a user can install it
by double-clicking a single `.mcpb` file — no configuration file to edit.

`manifest.json` describes RouteMIDI as a `binary` MCP server launched with
`--mcp`. Its `version` is a `0.0.0` placeholder; the real version is stamped in
at pack time from the binary's own `--version`, so there is nothing to keep in
sync by hand.

Build a bundle from a compiled binary with:

```
Scripts/build-mcpb.sh <path-to-routemidi-binary> [output.mcpb]
```

The bundle carries the one binary you pass, so build it per platform you
publish. Before publishing, validate and (re)pack with the official tool, which
also checks the manifest against the current schema:

```
npx @anthropic-ai/mcpb pack extension
```

For the other, lighter ways to connect RouteMIDI to an MCP client
(`--install-mcp`, `--print-mcp-config`, `claude mcp add`), see
[AI.md](../AI.md).
