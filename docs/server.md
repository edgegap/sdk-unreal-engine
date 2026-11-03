# Dedicated server

The `EdgegapServer` module is built into dedicated servers and the editor, never into game clients. Its Blueprint nodes are under **Edgegap > Server**.

## Ticket check

When a matchmaker starts your server, Edgegap tells the server which tickets were matched to it (`MM_TICKET_IDS`). `UEdgegapTicketGate`, a game instance subsystem, admits a joining player only when:

- the travel URL has `?edgegap_mm_ticket_id=<ticket>` (the client module adds it), and
- that ticket was assigned to **this** server (the match is exact, not a prefix), and
- no other connected player is using that ticket.

A ticket is held by the network connection that joined with it, not by the player controller, so it stays in use if your game replaces that player's controller (as Lyra does). When that connection closes, the ticket is free again, so a player who lost connection can rejoin with the same ticket.

Refused joins give one of these reasons:

| Reason | Cause |
| --- | --- |
| `no matchmaking ticket` | The URL has no ticket option |
| `ticket not assigned to this server` | The ticket is unknown here |
| `ticket already in use` | A connected player already holds this ticket |

The check is **off** in these cases, so local tests and direct deployments still work:
- `MM_TICKET_IDS` isn't set, because the server wasn't started by a matchmaker;
- **Require Matchmaker Ticket** is turned off in **Project Settings > Plugins > Edgegap > Dedicated Server**.

### Choosing how players are refused

| Setup | Result |
| --- | --- |
| `AEdgegapGameModeBase` or `AEdgegapGameMode` (or a subclass) | Refused in `PreLogin` with `403: Unauthorized (<reason>)`. The player never spawns, and the client sees a connection failure. |
| Your own game mode calling `EdgegapGameMode::CheckPreLogin` after `Super::PreLogin` | Same as above |
| Any other game mode | The player is kicked right after login |

To use the Edgegap game mode for every server map, add this to `Config/DefaultEngine.ini`:

```ini
[/Script/EngineSettings.GameMapsSettings]
GlobalDefaultServerGameMode=/Script/EdgegapServer.EdgegapGameModeBase
```

Both Edgegap game modes call the Blueprint event `Validate Join` after the ticket check passes. Set `ErrorMessage` there to refuse a player for your own reasons, such as a ban or the wrong client version.

For your own game mode, call the check from `PreLogin`:

```cpp
#include "EdgegapGameMode.h"

void AMyGameMode::PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
    EdgegapGameMode::CheckPreLogin(this, Options, Address, ErrorMessage);
}
```

### Gate functions

| Function | Use |
| --- | --- |
| `Is Enforcing` / `Set Enforcing` | Read or switch the check at runtime |
| `Allow Ticket` | Admit a ticket assigned later, for example by backfill |
| `Allow Backfill Tickets` | Admit every ticket in a backfill response from the matchmaker |
| `Revoke Ticket` | Stop admitting a ticket |
| `Get Allowed Tickets` | Every admitted ticket |
| `Get Player Ticket` | The ticket a connected player joined with |
| `On Join Rejected` | Event with the address, ticket and reason of each refused join |

## Match details

`UEdgegapServerLibrary` reads what Edgegap and the matchmaker give the server:

| Function | Source | Returns |
| --- | --- | --- |
| `Is Running On Edgegap` | `ARBITRIUM_REQUEST_ID` | Whether this is an Edgegap deployment |
| `Is Matchmaker Deployment` | `MM_MATCH_ID` | Whether a matchmaker started it |
| `Get Deployment Id` | `ARBITRIUM_REQUEST_ID` | The deployment's request id |
| `Get External Port` / `Get Internal Port` | `ARBITRIUM_PORT_<NAME>_EXTERNAL` / `_INTERNAL` | A named port |
| `Get Match Id`, `Get Match Profile`, `Get Expansion Stage` | `MM_MATCH_ID`, `MM_MATCH_PROFILE`, `MM_EXPANSION` | Match details |
| `Get Ticket Ids` | `MM_TICKET_IDS` | Every ticket in the match |
| `Get Ticket Json` | `MM_TICKET_<id>` | One ticket's data, including its attributes |
| `Get Group Ticket Mapping` | `MM_GROUPS` | Group id to ticket ids |
| `Get Team Group Mapping` | `MM_TEAMS` | Team id to group ids |
| `Get Equality Json`, `Get Intersection Json` | `MM_EQUALITY`, `MM_INTERSECTION` | Values the matchmaker matched players on |
| `Get Environment Variable`, `Json Array To String Array` | | General helpers |

## Self-stop

An Edgegap deployment runs, and is billed, until it is stopped.

- **`Self Stop`** asks Edgegap to stop this deployment. It uses the delete URL and token Edgegap injects (`ARBITRIUM_DELETE_URL`/`ARBITRIUM_DELETE_TOKEN`), so no API token is needed on the server.
- **`UEdgegapServerSubsystem`** calls it on its own when the server process exits or crashes. Turn this off with **Self Stop On Exit** in **Project Settings > Plugins > Edgegap > Dedicated Server**.

End your match by calling `Self Stop`, or simply by exiting the process.

## Port and network driver

Unreal's default IP network driver uses UDP port 7777. The app version that **Build and Push** creates exposes it as a port named `gameport`, and clients connect to the port with that name. If you use a different driver (WebSockets, for example) or port, set **Port**, **Protocol** and **Port Name** in **Tools > Edgegap**, and the matching **Game Port Name** in the client settings.
