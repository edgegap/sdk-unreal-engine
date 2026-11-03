# Getting started

This guide takes an Unreal project from nothing to two players matched onto the same Edgegap server. It uses the `casual` profile from the sample matchmaker configuration. Use your own profile names in your game.

## 1. Add the plugin

1. Copy `Plugin/EdgegapSDK` into `<YourProject>/Plugins/`.
2. Right-click your `.uproject` and choose **Generate Visual Studio project files**, then build the `<YourProject>Editor` target.
3. Your project needs a dedicated server target, `Source/<YourProject>Server.Target.cs`, so the editor can build the server. [`HostProject/Source/MMHostServer.Target.cs`](../HostProject/Source/MMHostServer.Target.cs) is an example.

A Blueprint-only project needs one C++ class added first (**Tools > New C++ Class**), so Unreal can compile plugins and server targets.

## 2. Build and push the server

1. Start Docker Desktop.
2. Log in to GitHub's registry with a GitHub account linked to your Epic account (Epic's Unreal images are private to Epic-linked accounts):

   ```bash
   docker login ghcr.io
   ```

3. In the editor, open **Tools > Edgegap** and fill in:
   - **App Name:** your Edgegap application. It is created if it doesn't exist.
   - **Registry Project:** from **Dashboard > Container Registry**, for example `edgegap-abcd1234`.
   - **Port / Protocol:** `7777` / `UDP` for Unreal's default network driver.
4. In **Project Settings > Plugins > Edgegap (my secrets)**, enter your API token and your registry username and token. These are saved in your `Saved/` folder, so they are never committed. Environment variables work too; see [Editor tools](editor.md#credentials).
5. Click **Build and Push**. The log shows each step:
   1. It packages the Linux server in Epic's container image.
   2. It builds and pushes your server image.
   3. It creates the app if it doesn't exist.
   4. It registers a version named with the UTC time (`YYYYMMDD-HHMM`).

To check the server before involving a matchmaker, click **Deploy Test Server**. It deploys near you and prints the address to `open`.

## 3. Create a matchmaker

1. In the Edgegap dashboard, create a matchmaker. Use [`HostProject/matchmaker-config.json`](../HostProject/matchmaker-config.json) as a starting point, and set `application.name` and `application.version` to the app and version you just pushed. The sample matches 2 players with less than 150 ms to the server.
2. Start the matchmaker. Copy its URL (`https://om-xxxxxxxxxx.edgegap.net`) and auth token.
3. In **Project Settings > Plugins > Edgegap**, set **Matchmaker Url** and **Matchmaker Token**.

## 4. Generate code for your matchmaker

```bash
python generator/generate.py --service matchmaker --spec https://om-xxxxxxxxxx.edgegap.net --out Plugins/EdgegapSDK/Source
```

The generator reads the matchmaker's OpenAPI description. It creates one typed attributes struct and one `Find Match (<profile>)` node per profile. Rebuild the editor afterwards. The checked-in code comes from the sample configuration (profile `casual`), so it compiles as is.

## 5. Find a match

**Blueprint:**
1. From your menu, call **Find Match (casual)**.
2. Leave the `Beacons` attribute empty: the SDK pings Edgegap's beacons and fills it in.
3. Bind the `Edgegap Matchmaker` subsystem's events:
   - **On Status:** progress text for your UI.
   - **On Match Found:** the assigned server, its join URL and the player's ticket.
   - **On Failed:** the reason, as an `Edgegap Error`.

**C++:**

```cpp
#include "EdgegapMatchmakerSubsystem.h"

UEdgegapMatchmakerSubsystem* Matchmaker = GetGameInstance()->GetSubsystem<UEdgegapMatchmakerSubsystem>();
Matchmaker->OnFailed.AddDynamic(this, &UMyMenu::HandleMatchmakingFailed);
Matchmaker->FindMatch(TEXT("casual"), TEXT("{}"), { TEXT("beacons") });
```

When a server is assigned, the player travels to `ip:port?edgegap_mm_ticket_id=<ticket>`. To travel yourself, turn off **Travel On Match** and use `Travel Player To Match Assignment` from **On Match Found**. See [Matchmaking client](client.md).

## 6. Let only matched players in

The server checks every joining player's ticket against the tickets the matchmaker assigned to it. This is on by default for servers the matchmaker starts. Pick one way of refusing players:

- **Before login (recommended).** Set your server game mode to `EdgegapGameModeBase` or `EdgegapGameMode`, or derive from them. Refused players get `403: Unauthorized (<reason>)` and never spawn.
- **Any game mode.** Keep your own game mode. Players without a valid ticket are kicked right after login.

In `Config/DefaultEngine.ini`:

```ini
[/Script/EngineSettings.GameMapsSettings]
GlobalDefaultServerGameMode=/Script/EdgegapServer.EdgegapGameModeBase
```

See [Dedicated server](server.md).

## 7. Try it

1. Package a client, or launch two standalone game instances.
2. In each one, run the console command `EdgegapMM.FindMatch casual` (or press your Find Match button).
3. The matchmaker starts one deployment, and both players join it.

To repeat this from a script, [`HostProject/e2e-tickets.ps1`](../HostProject/e2e-tickets.ps1) checks that:
- two clients are matched and join;
- a made-up ticket is refused;
- a ticket that is already playing is refused.

```powershell
$env:EDGEGAP_MM_URL = "https://om-xxxxxxxxxx.edgegap.net"
$env:EDGEGAP_MM_TOKEN = "<matchmaker token>"
.\HostProject\e2e-tickets.ps1 -Project C:\Path\To\YourProject.uproject
```

When you are done, stop leftover deployments from **Tools > Edgegap > Running deployments**, or from the dashboard.
