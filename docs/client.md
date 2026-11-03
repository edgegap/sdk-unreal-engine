# Matchmaking client

The `EdgegapClient` module is built into game clients and the editor, never into dedicated servers. Its Blueprint nodes are under **Edgegap > Matchmaker**.

## How Find Match works

`UEdgegapMatchmakerSubsystem` (a game instance subsystem) runs one search at a time:

1. **Beacons.**
   - It measures ping to every Edgegap beacon the matchmaker lists (UDP echo, with a TCP fallback), all in parallel.
   - It fills each latency attribute you asked for (`beacons` in the sample profile) with a `{ city: milliseconds }` map.
2. **Validation (development builds only).** It checks your attributes against the matchmaker's live OpenAPI description and logs any problem: an unknown profile, a missing or extra field, a wrong type. Shipping builds skip this.
3. **Queue.** It creates a Group Up group with the player as its only, ready member. A solo player is matched the same way as a party.
4. **Wait.** It polls the member every **Poll Interval Seconds** (default 2, minimum 1, since Edgegap rate-limits) until the matchmaker assigns a server.
5. **Travel.** When **Travel On Match** is on (the default), it calls `ClientTravel` to `ip:port?edgegap_mm_ticket_id=<ticket>`. The port is the one named **Game Port Name** (default `gameport`) in your app version, or the only port when there is just one.

### Starting a search

| Way | Use |
| --- | --- |
| `Find Match (<profile>)` | Generated per profile. It takes that profile's typed attributes struct and measures latency attributes for you. |
| `FindMatch(Profile, AttributesJson, MeasureLatencyInto)` | Any profile, with attributes as a JSON object. List the latency attributes to measure in `MeasureLatencyInto`. |
| `EdgegapMM.FindMatch <profile> [attributes JSON]` | Console command |
| `-EdgegapFindMatch=<profile>` | Launch option. It starts a search at startup. |

`CancelMatch` (or `EdgegapMM.Cancel`) leaves the queue and deletes the group. `IsSearching` tells whether a search is running.

### Events

| Event | Parameters |
| --- | --- |
| `OnStatus` | Progress text: measuring beacons, searching, waiting for a server... |
| `OnMatchFound` | `Assignment` (server IP, ports, location), `JoinUrl`, `TicketId` |
| `OnFailed` | `FEdgegapError` with `HttpStatus`, `Message` and `bRateLimited` |

The last measured latencies are kept in `BeaconLatencies`.

## Attributes

- **Typed (recommended):** the `Find Match (<profile>)` node takes a generated struct, for example `FEdgegapMMCasualAttributes`, with one field per attribute in your matchmaker rules.
- **Dynamic:** build a list with `Make String Attribute`, `Make Number Attribute`, `Make Boolean Attribute`, `Make String Array Attribute` and `Make Beacon Latencies Attribute`. Then turn it into the JSON that `FindMatch` takes with `Attributes To JSON`.
- **Check without searching:** `Validate Matchmaking Attributes` (an async node) runs the same live check as development builds and returns the list of problems.

## Beacons

`Measure Beacons` runs the latency measurement on its own and returns a `FEdgegapBeaconMeasurement` (one result per beacon, plus a city-to-milliseconds map). Settings under **Matchmaker > Beacons**:

| Setting | Default | Meaning |
| --- | --- | --- |
| Beacon Protocol | UDP | UDP echo first. TCP connect time is used when UDP gets no answer. |
| Beacon Ping Samples | 3 | Pings per beacon; the average is reported |
| Beacon Timeout Seconds | 1 | Wait per answer |

## Travel helpers

All in `UEdgegapMatchmakingLibrary`, under **Edgegap > Matchmaker > Travel**:

| Function | Does |
| --- | --- |
| `Make Assignment Address (Assignment, PortName)` | `ip:port` for a named port. It warns when the port is missing. |
| `Make Join Url (Assignment, TicketId)` | `ip:port?edgegap_mm_ticket_id=<ticket>`, which is what the server's ticket check expects |
| `Travel Player To Match Assignment (PlayerController, Assignment, TicketId)` | `ClientTravel` with the join URL |
| `Travel Player To Match (PlayerController, Address)` | `ClientTravel` to any address |
| `Travel Party To Match` / `Travel Party To Match Assignment` | From a listen-server party leader: sends every connected member, then the leader after a delay |
| `Travel All Connected Players To Match Assignment` | The same, from any host |

> [!IMPORTANT]
> With the server's ticket check on, every player needs their **own** ticket in the join URL. A party leader can't send other members' tickets. Have each member call `Find Match` or follow its own group membership, then travel with `Make Join Url`.

## Groups, tickets and backfill

Every matchmaker call is available as an async Blueprint node and as a static C++ function on `FEdgegapMMApi`, generated from your matchmaker's description. That covers:
- tickets and group tickets;
- Group Up groups and members;
- backfills (create, read, delete);
- beacons, the monitor, and the inspect endpoints.

Profile-specific bodies get typed variants, for example `Create Backfill (casual)`.

```cpp
FEdgegapMMApi::GetBackfill(BackfillId, [](const FEdgegapMMReadBackfillResponse& Backfill, const FEdgegapError& Error)
{
    if (Error.bFailed) { UE_LOG(LogTemp, Warning, TEXT("%s"), *Error.Message); return; }
    // ...
});
```

## Settings

**Project Settings > Plugins > Edgegap**, saved in `Config/DefaultGame.ini`:

| Setting | Meaning |
| --- | --- |
| Matchmaker Url / Matchmaker Token | From the dashboard. The token is meant for clients. |
| Poll Interval Seconds | Time between assignment checks (≥ 1) |
| Search Timeout Seconds | 0 = rely on the matchmaker's ticket expiration |
| Travel On Match | Connect as soon as a server is assigned |
| Game Port Name | The app version port to connect to |
| Request Timeout Seconds / Max Retries | HTTP behaviour (see [Edgegap API](edgegap-api.md#errors-and-retries)) |

The URL and token can also come from the command line (`-EdgegapMMUrl=`, `-EdgegapMMToken=`) or the environment (`EDGEGAP_MM_URL`, `EDGEGAP_MM_TOKEN`). The order of precedence is described in [Editor tools](editor.md#credentials). At runtime, `Set Connection` switches the URL and credential, for example per region or per environment.

## Launch options for testing

| Option | Effect |
| --- | --- |
| `-EdgegapFindMatch=<profile>` | Search at startup |
| `-EdgegapNoTravel` | Don't connect when matched |
| `-EdgegapQuitOnMatch` | Exit once a server is assigned |
