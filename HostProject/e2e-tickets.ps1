# End-to-end ticket check on Edgegap (matchmaker profile "casual" must deploy this host project's server).
#   1. Two clients Find Match, travel with their tickets: both must be let in ("Welcomed by server" = PreLogin passed).
#   2. A client opens the same server with a made-up ticket: must be refused (403).
#   3. A client opens it with a real ticket that is already playing: must be refused (403).
# Needs EDGEGAP_MM_URL and EDGEGAP_MM_TOKEN in the environment. Usage: .\e2e-tickets.ps1 [-Project C:\EdgegapMMHost\MMHost.uproject]
param(
    [string]$Project = "C:\EdgegapMMHost\MMHost.uproject",
    [string]$Editor = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
)
$ErrorActionPreference = "Stop"
if (-not $env:EDGEGAP_MM_URL -or -not $env:EDGEGAP_MM_TOKEN) { throw "Set EDGEGAP_MM_URL and EDGEGAP_MM_TOKEN." }
$logs = Join-Path (Split-Path $Project) "Saved\Logs\e2e"
New-Item -ItemType Directory -Force $logs | Out-Null
Remove-Item "$logs\*.log" -ErrorAction SilentlyContinue

function Start-Client($name, [string[]]$extra) {
    $argList = @($Project, "-game", "-nullrhi", "-nosound", "-unattended", "-nosplash", "-abslog=$logs\$name.log") + $extra
    Start-Process -FilePath $Editor -ArgumentList $argList -PassThru -WindowStyle Hidden
}
function Wait-Log($name, $pattern, $seconds) {
    $end = (Get-Date).AddSeconds($seconds)
    while ((Get-Date) -lt $end) {
        if (Test-Path "$logs\$name.log") {
            $hit = Select-String -Path "$logs\$name.log" -Pattern $pattern | Select-Object -First 1
            if ($hit) { return $hit.Line }
        }
        Start-Sleep -Seconds 2
    }
    return $null
}

"1. Two clients find a match"
$a = Start-Client "clientA" @("-EdgegapFindMatch=casual")
$b = Start-Client "clientB" @("-EdgegapFindMatch=casual")
$matchA = Wait-Log "clientA" "MATCH FOUND: (\S+)" 180
$matchB = Wait-Log "clientB" "MATCH FOUND: (\S+)" 60
if (-not $matchA -or -not $matchB) { throw "No match found (see $logs)." }
$urlA = ([regex]"MATCH FOUND: (\S+)").Match($matchA).Groups[1].Value
"   A -> $urlA"
"   B -> " + ([regex]"MATCH FOUND: (\S+)").Match($matchB).Groups[1].Value
$joinA = Wait-Log "clientA" "Welcomed by server" 120
$joinB = Wait-Log "clientB" "Welcomed by server" 60
"   A joined: " + [bool]$joinA
"   B joined: " + [bool]$joinB

$server = $urlA.Split("?")[0]
$ticketA = $urlA.Split("=")[-1]

"2. Made-up ticket"
$c = Start-Client "fake" @("-ExecCmds=open $server`?edgegap_mm_ticket_id=not-a-real-ticket")
$fake = Wait-Log "fake" "Unauthorized|PendingConnectionFailure|Welcomed by server" 60
"   " + $fake

"3. Real ticket already in use"
$d = Start-Client "reuse" @("-ExecCmds=open $server`?edgegap_mm_ticket_id=$ticketA")
$reuse = Wait-Log "reuse" "Unauthorized|PendingConnectionFailure|Welcomed by server" 60
"   " + $reuse

foreach ($p in @($a, $b, $c, $d)) { if ($p -and -not $p.HasExited) { $p.Kill() } }

$ok = $joinA -and $joinB -and $fake -and ($fake -notmatch "Welcomed") -and $reuse -and ($reuse -notmatch "Welcomed")
if ($ok) { "E2E TICKETS PASSED (server $server)" } else { "E2E TICKETS FAILED (logs: $logs)"; exit 1 }
