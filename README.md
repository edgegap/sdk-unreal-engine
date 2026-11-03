# Edgegap SDK for Unreal Engine

Matchmaking and dedicated servers on [Edgegap](https://edgegap.com) for Unreal Engine 5.0 to 5.8, in one plugin:

- **Client:** a `Find Match` node that measures ping to Edgegap's beacons, queues the player, waits for a server and connects to it.
- **Server:** only players the matchmaker sent to this server can join. Helpers read the match details Edgegap gives the server, and the deployment stops itself when the server exits or crashes.
- **Edgegap API:** every call (apps, versions, deployments, relays, fleets, sessions...) as C++ functions and Blueprint nodes.
- **Editor:** a `Tools > Edgegap` panel that builds the Linux server, pushes it, registers the version and starts a test deployment. The same actions run headless for CI.

> [!NOTE]
> This SDK is in beta (0.1.0). Its API can still change between versions.

The matchmaker and API code is **generated** from each service's own OpenAPI description, so your matchmaker's profiles become typed Blueprint nodes and structs. For example, the profile `casual` gives `Find Match (casual)`. Regenerate it whenever your matchmaker configuration changes; see [Generator](docs/generator.md).

## Requirements

- Unreal Engine 5.0 to 5.8 (C++ project, or a Blueprint project with a C++ class added so the plugin can compile).
- An Edgegap account, an application, and a matchmaker for matchmaking.
- For building the server from the editor: Docker Desktop, and a GitHub account linked to your Epic account (to pull Epic's Unreal images from `ghcr.io`).

## Install

1. Copy `Plugin/EdgegapSDK` into your project's `Plugins/` folder.
2. Regenerate your project files and build the editor.
3. Open **Edit > Project Settings > Plugins > Edgegap** and enter your matchmaker URL and token, from the Edgegap dashboard.

## Quick start

1. **Ship a server.** Open **Tools > Edgegap**, fill in your app name and registry project, then click **Build and Push**. See [Editor tools](docs/editor.md).
2. **Create a matchmaker** in the dashboard that points at that app version. [`HostProject/matchmaker-config.json`](HostProject/matchmaker-config.json) is a working example.
3. **Generate your matchmaker's code**, so its profiles become nodes:
   ```bash
   python generator/generate.py --service matchmaker --spec https://om-xxxxxxxxxx.edgegap.net --out Plugin/EdgegapSDK/Source
   ```
4. **Find a match** from your menu. Call `Find Match (<profile>)`, or `FindMatch` on the `Edgegap Matchmaker` subsystem. The player connects to the assigned server on its own. See [Matchmaking client](docs/client.md).
5. **Protect the server.** Use `EdgegapGameModeBase` (or `EdgegapGameMode`) as the server's game mode, or keep yours. Either way, only players with a ticket for this server get in. See [Dedicated server](docs/server.md).

[Getting started](docs/getting-started.md) walks through all of it.

## Documentation

| Page | What it covers |
| --- | --- |
| [Getting started](docs/getting-started.md) | From an empty project to two players matched on an Edgegap server |
| [Matchmaking client](docs/client.md) | Find Match, attributes, beacons, travel, groups and backfill calls, launch options |
| [Dedicated server](docs/server.md) | Ticket check, game modes, match details, self-stop |
| [Editor tools](docs/editor.md) | Build, push, version, deploy, headless commands, where credentials live |
| [Edgegap API](docs/edgegap-api.md) | Calling the generated API from C++ and Blueprints, errors and retries |
| [Generator](docs/generator.md) | Regenerating code for your matchmaker or a newer API |
| [Engine versions](docs/compatibility.md) | Supported versions and how each one was tested |

## Repository layout

| Path | Contents |
| --- | --- |
| `Plugin/EdgegapSDK` | The plugin. Copy this folder into your project. |
| `Plugin/EdgegapSDK/Source/EdgegapSDK` | Shared runtime module: HTTP, JSON, settings, and the generated matchmaker (`EdgegapMM*`) and Edgegap API (`EdgegapAPI*`) code |
| `Plugin/EdgegapSDK/Source/EdgegapClient` | Client module. It isn't built into dedicated servers. |
| `Plugin/EdgegapSDK/Source/EdgegapServer` | Server module. It isn't built into game clients. |
| `Plugin/EdgegapSDK/Source/EdgegapSDKEditor` | Editor panel, console commands and automation tests |
| `generator/` | The spec-to-Unreal code generator (Python 3, no dependencies) |
| `specs/` | The specs the checked-in code was generated from |
| `HostProject/` | A minimal project for building and testing the plugin, with a sample matchmaker configuration and an end-to-end ticket test |
| `tools/` | Engine-version checks (see [Engine versions](docs/compatibility.md)) |

## Security

- **Matchmaker token:** meant for game clients. It can only queue players.
- **API token:** it controls your whole Edgegap organization, so it never goes in a client build by default.
  - The editor keeps it in your per-user settings (`Saved/`, not committed) or reads `EDGEGAP_API_TOKEN`.
  - Servers don't need it: self-stop uses the deployment's own delete token, which Edgegap injects.
  - Shipping it in a packaged build is an explicit, opt-in setting.
- **Server browser:** its client token works the same way as the matchmaker token, and its server token stays on the server.

## License

[MIT](LICENSE)
