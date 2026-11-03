# Edgegap API

The whole Edgegap API (v1 and v2) is generated into the shared `EdgegapSDK` module:
- **Apps:** applications and versions.
- **Deployments:** deploy, status, logs, metrics, bulk stop, tags.
- **Infrastructure:** relays, locations and beacons, fleets and private fleets, endpoint storage, the container registry, sessions, and IP lookup.

That's 87 operations in total.

Each operation exists in two forms:

- **C++:** a static function on `FEdgegapAPIApi`, with a typed request struct and a callback.
- **Blueprint:** an async node under **Edgegap > API** with **On Success** and **On Failure** pins.

Request and response types are `FEdgegapAPI*` structs. Read each struct's header for its fields; each field's comment is copied from the API description.

The API needs the organization **API token**. That's fine in the editor, tools and CI. It doesn't belong in game clients; see [Editor tools](editor.md#credentials).

## C++

```cpp
#include "Generated/EdgegapAPIApi.h"

FEdgegapAPIDeployModel Deploy;
Deploy.AppName = TEXT("my-game");
Deploy.VersionName = TEXT("20261103-1200");
Deploy.IpList = { TEXT("203.0.113.10") };  // players' public IPs: Edgegap deploys near them

FEdgegapAPIApi::Deploy(Deploy, [](const FEdgegapAPIRequest& Request, const FEdgegapError& Error)
{
    if (Error.bFailed)
    {
        UE_LOG(LogTemp, Warning, TEXT("Deploy failed (%d): %s"), Error.HttpStatus, *Error.Message);
        return;
    }
    UE_LOG(LogTemp, Log, TEXT("Deployment %s starting"), *Request.RequestId);
});
```

Callbacks always run on the game thread, exactly once.

## Blueprint

Search for the operation's name, such as **Deploy**, **Get Deployment Status** or **List Locations**. The node fires **On Success** with the response struct, or **On Failure** with an `Edgegap Error`, which converts to a string for printing.

## Matchmaker and server browser

The matchmaker and the server browser are generated the same way, from their own descriptions:
- matchmaker: `FEdgegapMMApi`, nodes under **Edgegap > Matchmaker**;
- server browser: `FEdgegapSBApi` once generated.

Each uses its own URL and token. `Set Connection (Service, Url, Credential)` changes either one at runtime.

## Errors and retries

`FEdgegapError` has these fields:

| Field | Meaning |
| --- | --- |
| `bFailed` | The call failed |
| `HttpStatus` | HTTP status, or 0 when no answer came back |
| `bRateLimited` | Edgegap kept answering 429 (too many requests) after every retry |
| `Message` | Edgegap's error message when it sent one, or a description |

Each request is retried up to **Max Retries** (default 3) with a growing delay, waiting **Request Timeout Seconds** (default 10) per attempt:

- **429:** always retried, waiting as long as the `Retry-After` header asks (at most 60 seconds).
- **Network errors and 5xx:** retried, except for POST. A POST that timed out may have succeeded, and sending it again could, for example, start a second deployment.
- **Other 4xx:** never retried; the error goes straight to your callback.
