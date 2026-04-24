# MCP (Model Context Protocol) server

qBittorrent exposes an optional Model Context Protocol endpoint at `/mcp/`,
suitable for driving the application from LLM clients (Claude Desktop,
Cursor, `mcp-cli`, etc.).

## Enabling

Either:

- Toggle **Preferences > WebUI > MCP (Model Context Protocol) server** (checkable group box).
- Pass `--enable-mcp` on the command line at startup. The flag persists the preference.

MCP is disabled by default.

## Authentication

MCP uses the same authentication chain as the WebUI. **Bearer API key is recommended**:

1. Log into the WebUI at http://localhost:8080.
2. Call `POST /api/v2/app/rotateAPIKey` (authenticated) to generate a key.
3. Configure your MCP client to send the key as `Authorization: Bearer <key>`.

Cookie sessions and HTTP Basic are also accepted — they work the same way they do against the WebUI API.

## Client configuration

Most MCP clients accept an HTTP transport URL with custom headers. Example (Claude Desktop, `.claude/mcp_config.json` or equivalent):

```json
{
    "mcpServers": {
        "qbittorrent": {
            "transport": {
                "type": "http",
                "url": "http://localhost:8080/mcp/",
                "headers": {
                    "Authorization": "Bearer YOUR_API_KEY"
                }
            }
        }
    }
}
```

Clients that only speak MCP over stdio can use a bridge like `mcp-proxy` to forward stdio to the HTTP endpoint.

## Origin validation (browser clients)

When an MCP client sends an `Origin` header (typical for browser-based clients), qBittorrent validates it:

- **Same-origin** (Origin matches the request's Host) → allowed.
- **Non-browser clients** (no Origin header) → allowed.
- **Listed in `Preferences > WebUI > Allowed MCP origins`** (comma-separated) → allowed.
- **Other** → HTTP 403.

Add explicit origins if you put qBittorrent behind a reverse proxy or use a browser extension.

## Spec compliance

- Protocol version: **2025-06-18** (with backward-compat support for `2025-03-26`).
- Transport: **Streamable HTTP**, JSON responses only (no SSE in v1).
- Authorization: Bearer / Basic / Cookie. OAuth 2.1 is not implemented.
- Notifications: accepted with HTTP 202.
- Session termination: HTTP DELETE to `/mcp/` with `Mcp-Session-Id` header.

## Tools

`tools/list` returns the full catalog. Every MCP tool maps 1:1 to a WebUI action — see the [WebAPI documentation](https://github.com/qbittorrent/qBittorrent/wiki/WebAPI-Documentation) for per-action semantics.

Categories exposed: `app`, `torrents`, `transfer`, `sync`, `log`, `rss`, `search` (~113 tools total). Tools in the `auth`, `clientdata`, and `torrentcreator` categories are not exposed over MCP in this version.

## Session model

- Sessions are created by the `initialize` handshake.
- 30-minute idle timeout.
- 10 concurrent sessions per client IP, 100 global cap.
- Sessions do not persist across qBittorrent restarts.

## Security notes

- MCP inherits qBittorrent's WebUI "Bypass authentication for clients on localhost" setting. If this is enabled, any local process can drive qBittorrent via MCP without credentials. This is the same risk as the existing WebUI API and is not new.
- Origin validation is enforced only for `/mcp/` — existing WebUI endpoints are unchanged.
