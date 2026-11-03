# Engine versions

The SDK supports **Unreal Engine 5.0 to 5.8**. UE 5.0 is the floor because it is where Blueprints got `double` and the engine got `FTSTicker`, and the SDK relies on both. UE 4.27 would need a separate float-only code path.

## How each version was tested

| Version | Compiled | Automation tests | Live on Edgegap |
| --- | --- | --- | --- |
| 5.8 | Win64 (MSVC) and Linux (clang, editor and server targets) | 6/6 pass | Matchmaking, ticket check, build/push/deploy |
| 5.5.4 | Linux (clang, editor target) | 6/6 pass | |
| 5.3.2 | Linux (clang, editor target) | 6/6 pass | |
| 5.0.3 | Linux (clang, editor target) | 6/6 pass | |
| 5.1, 5.2, 5.4, 5.6, 5.7 | Engine API check only (see below) | | |

The editor target compiles all four modules. The live test on UE 5.8 checked that:
- two clients were matched on a real Edgegap matchmaker and both joined the server it started;
- the server refused a missing ticket, a made-up ticket, a prefix of a real ticket, and a ticket already in use;
- a ticket rejoined after its connection closed;
- the editor commands built, pushed, registered, deployed, listed and stopped a deployment.

## Checking a version yourself

### Engine API check (no engine needed)

`tools/check_engine_api.py` reads the engine headers at each release tag on GitHub. It confirms that every engine API the SDK depends on exists there. It needs the GitHub CLI logged in with an account that can see Epic's `UnrealEngine` repository.

```bash
python tools/check_engine_api.py
```

```bash
python tools/check_engine_api.py --tags 5.4.4-release 5.6.1-release
```

This catches removed or renamed APIs, not every compile problem.

### Real compile and tests in Docker

`tools/compile-on-ue.sh` compiles the plugin and runs its automation tests inside Epic's Linux image for a given version. Run it from Git Bash or WSL.

1. Copy `HostProject/` to a folder outside cloud-synced storage.
2. Put `Plugin/EdgegapSDK` in its `Plugins/` folder.
3. Run:

```bash
tools/compile-on-ue.sh 5.3.2 C:/EdgegapMMHost
```

- It needs Docker and `docker login ghcr.io` with a GitHub account linked to Epic.
- The compile output is written to `compat-logs/ue-<version>.log`.
- For UE 5.0, it also adjusts the host project's target files, because 5.0 doesn't know the newer build settings.

> [!WARNING]
> Each image takes 45–60 GB of disk once unpacked, and Docker Desktop's disk file doesn't shrink on its own. Test one version at a time and remove its image afterwards:
>
> ```bash
> docker rmi ghcr.io/epicgames/unreal-engine:dev-slim-5.3.2
> ```

## Version differences handled in the code

`EdgegapCompat.h` holds the version switches:

| Difference | Handling |
| --- | --- |
| Automation test flags changed in 5.5 | `EDGEGAP_TEST_FLAGS` picks the right flags per version |
| `UToolMenu::FindOrAddSection(Name, Label)` arrived in 5.1 | `FindSection`, then `AddSection` |
| JSON object keys are `FString` before 5.8 and a shared string type in 5.8 | Keys are read through a conversion that works on both |
| The condensed JSON writer header isn't included indirectly in 5.0 | Included explicitly |
