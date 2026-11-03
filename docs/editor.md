# Editor tools

The `EdgegapSDKEditor` module adds a **Tools > Edgegap** panel. It takes a project from source to a running Edgegap deployment without leaving the editor, and every action also runs headless for CI.

## The panel

| Button | Does |
| --- | --- |
| **Verify API Token** | Lists your Edgegap applications with the API token |
| **Open Settings** | Opens **Project Settings > Plugins > Edgegap** |
| **Build and Push** | Packages the Linux server, builds and pushes its image, creates the app if needed, and registers the version |
| **Create Version Only** | Registers an image you already pushed. The tag must equal **Version Name**. |
| **Deploy Test Server** | Deploys the version near your public IP, waits until it is ready and prints the address to `open` |
| **Running deployments** | Lists deployments, each with a **Stop** button |
| **Copy API Token To Packaged Builds...** | See [Security](#api-token-in-packaged-builds) |

The log box mirrors the `LogEdgegapEditor` category in the Output Log.

### Build settings

Saved in `Config/DefaultEditor.ini`, so they are shared with your team:

| Setting | Default | Meaning |
| --- | --- | --- |
| App Name | | Edgegap application. It is created on first push. |
| Version Name | UTC `YYYYMMDD-HHMM` | Version name and image tag. Left empty, each build gets a new timestamp. |
| Req Cpu / Req Memory | 1024 / 2048 | vCPU units (1024 = 1 vCPU) and MB |
| Max Duration Minutes | 60 | Edgegap stops a deployment after this long. 0 = Edgegap's default. |
| Port Name / Port / Protocol | `gameport` / 7777 / UDP | The port the server listens on |
| Build Mode | Docker | **Docker:** builds in Epic's Linux image. **Local:** uses this machine's engine, which must be source-built with the Linux toolchain installed. |
| Unreal Image | `ghcr.io/epicgames/unreal-engine:dev-slim-<engine version>` | Image for Docker mode |
| Server Config | Development | Development or Shipping |
| Server Target | `<Project>Server` | Your server target |
| Server Map | The project's server default map | Map the server opens |
| Extra Server Arguments | | Added to the server command line in the image |
| Registry Url / Registry Project | `registry.edgegap.com` / | The image goes to `<url>/<project>/<app>:<version>` |

### What Build and Push runs

1. **Package.** `RunUAT BuildCookRun` for Linux runs inside the Unreal image, working in a Docker volume named `edgegap-build-<project>`. Building in a volume avoids slow and unreliable Windows file sharing. The packaged server is copied to `Saved/Edgegap/Docker/LinuxServer`.
2. **Dockerfile.** It writes a small Ubuntu 22.04 image definition:
   - The server runs as a non-root user, because Unreal refuses to run as root.
   - It starts with `exec`, so it receives Edgegap's stop signal.
   - It runs with `-log -port=<port>`.
3. **Image.** `docker build --platform linux/amd64`, then `docker login` and `docker push`. A failed login only warns, since an earlier `docker login` may still allow the push.
4. **App.** The app is created if it doesn't exist.
5. **Version.** It registers the image with the CPU, memory, port and registry credentials. Port checks are turned off for UDP, which Edgegap can't health-check.

The first Docker build downloads Epic's image, which is 15–25 GB compressed. Later builds reuse it and the volume.

## Headless (CI)

Every action is a console command. Run one with `-EdgegapQuitWhenDone` to exit when it finishes:

```bash
UnrealEditor-Cmd MyGame.uproject -ExecCmds="Edgegap.BuildAndPush" -EdgegapQuitWhenDone -unattended -nullrhi
```

| Command | Does |
| --- | --- |
| `Edgegap.VerifyToken` | List applications |
| `Edgegap.BuildAndPush` | Build, push and register the version |
| `Edgegap.BuildAndPush VersionOnly` | Register an already pushed image |
| `Edgegap.DeployTest` | Deploy the current version near this machine |
| `Edgegap.ListDeployments` | Log running deployments |
| `Edgegap.StopDeployment <request id>` | Stop one deployment |

`-EdgegapApp=<name>` and `-EdgegapVersion=<name>` override the app and version for this run.

## Credentials

Each value is taken from the first source that has it:

| Value | Order |
| --- | --- |
| Matchmaker URL / token | `Set Connection` → `-EdgegapMMUrl=` / `-EdgegapMMToken=` → `EDGEGAP_MM_URL` / `EDGEGAP_MM_TOKEN` → Project Settings |
| Edgegap API URL | `Set Connection` → `-EdgegapApiUrl=` → `EDGEGAP_API_URL` → Project Settings → `https://api.edgegap.com` |
| Edgegap API token | `Set Connection` → `EDGEGAP_API_TOKEN` → **Edgegap (my secrets)** (editor only) → the packaged token, when enabled |
| Server browser URL / client token | `-EdgegapSBUrl=` / `-EdgegapSBToken=` → `EDGEGAP_SB_URL` / `EDGEGAP_SB_CLIENT_TOKEN` → Project Settings |
| Server browser server token | `EDGEGAP_SB_SERVER_TOKEN` → **Edgegap (my secrets)** |
| Registry username / token | **Edgegap (my secrets)** → `EDGEGAP_REGISTRY_USERNAME` / `EDGEGAP_REGISTRY_TOKEN` |

- The API token can't be passed on the command line, because Unreal writes the command line to its log.
- The API token is sent as `token <value>`. Enter it with or without that prefix.
- **Edgegap (my secrets)** is saved in `Saved/Config/<Platform>/EditorPerProjectUserSettings.ini`. It is never committed, and never staged into packaged builds.

### API token in packaged builds

The API token controls your whole Edgegap organization. Game clients should only ever hold the matchmaker or server browser client token.

For internal tools that really need the Edgegap API in a packaged build:
1. Turn on **Include API Token In Packaged Builds (unsafe)**.
2. Click **Copy API Token To Packaged Builds...**, which writes the token to `Config/DefaultGame.ini`.

Anyone with that build can create, stop and delete everything in your organization, so keep such builds inside your team.
