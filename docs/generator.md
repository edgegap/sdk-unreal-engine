# Generator

`generator/generate.py` turns an OpenAPI 3 or Swagger 2 description into Unreal C++. It needs Python 3 and nothing else.

## Commands

```bash
# Your matchmaker: run again whenever you change profiles or rules
python generator/generate.py --service matchmaker --spec https://om-xxxxxxxxxx.edgegap.net --out Plugin/EdgegapSDK/Source

# The Edgegap API (v1 and v2 merged)
python generator/generate.py --service api --spec https://api.edgegap.com/swagger.json --spec https://api.edgegap.com/v2/swagger.json --out Plugin/EdgegapSDK/Source

# Your server browser
python generator/generate.py --service serverbrowser --spec https://<your server browser>/swagger/v1/swagger.json --out Plugin/EdgegapSDK/Source
```

| Argument | Meaning |
| --- | --- |
| `--service` | `matchmaker`, `api` or `serverbrowser`. It sets the class prefix and Blueprint category. |
| `--spec` | A URL or a file. For a URL that doesn't end in `.json`, `/swagger/v1/swagger.json` is added, which is where Edgegap matchmakers serve their description. Repeat the argument to merge several descriptions. |
| `--out` | The plugin's `Source` folder. When the plugin is in your project: `Plugins/EdgegapSDK/Source`. |

Only files named `<Prefix>*` under `Public/Generated` and `Private/Generated` are written, so your own code is never touched. Rebuild the editor afterwards.

## What it generates

Prefixes: `EdgegapMM` for the matchmaker, `EdgegapAPI` for the Edgegap API, `EdgegapSB` for the server browser.

| File | Contents |
| --- | --- |
| `<Prefix>Models.h/.cpp` | A `USTRUCT` per schema, with `ReadJson`/`WriteJson`, and a `UENUM` per enum |
| `<Prefix>Api.h/.cpp` | `F<Prefix>Api`: a static C++ function per operation |
| `<Prefix>Nodes.h/.cpp` | An async Blueprint node per operation |
| `<Prefix>JsonLibrary.h/.cpp` | Blueprint conversions: each struct to and from a JSON string |
| `EdgegapMMProfiles.h/.cpp` (matchmaker only, in `EdgegapClient`) | `Find Match (<profile>)` per profile, and `Get Profile Names` |

### Matchmaker profiles

An Edgegap matchmaker's description has one request variant per profile, with that profile's attributes typed. For each profile the generator creates:
- an attributes struct, for example `FEdgegapMMCasualAttributes`;
- typed request variants, for example `Create Backfill (casual)`;
- a `Find Match (<profile>)` node that also fills latency attributes from the beacons.

So after a configuration change, the compiler and Blueprint editor flag any attribute your game no longer sends.

### Mapping rules

- Optional booleans and numbers are sent only when they differ from the description's default, which is false or 0 when none is given.
- A field that can only have one value becomes a `Fixed<Name>()` constant.
- Schemas without a fixed shape become JSON strings (`FString`).
- Name clashes between merged descriptions get a `V2` suffix.

## Spec quirks it handles

- Matchmaker latency maps are declared as `string` with numeric `minimum`/`maximum`, but the values are numbers. A string with numeric bounds is generated as `double`.
- Responses that differ per profile (backfills) use the generic type, so their attributes arrive as JSON text.
- The Edgegap API's streaming logs endpoint is skipped: it streams, which a request/response call can't represent.

Each generated file starts with the description it came from and its hash, so you can tell which version of your matchmaker the code matches. If you generate from a URL, that line records your matchmaker's hostname. The URL isn't secret, but regenerate from a saved file if you'd rather not commit it.
