#!/usr/bin/env bash
# Compile the Edgegap SDK (all four modules, through the host project's editor target) and run its
# automation tests inside Epic's Linux Unreal image for one engine version.
#   tools/compile-on-ue.sh 5.0.3 [host project dir, default C:/EdgegapMMHost]
# Needs Docker and ghcr.io access (GitHub account linked to Epic). Leaves the image in place:
# remove it afterwards with "docker rmi ghcr.io/epicgames/unreal-engine:dev-slim-<version>".
set -euo pipefail
VERSION=${1:?engine version, e.g. 5.0.3}
HOST=${2:-C:/EdgegapMMHost}
IMAGE=ghcr.io/epicgames/unreal-engine:dev-slim-$VERSION
VOLUME=edgegap-compat-${VERSION//./-}
LOG_DIR=$(dirname "$0")/../compat-logs
mkdir -p "$LOG_DIR"
LOG=$LOG_DIR/ue-$VERSION.log

docker pull "$IMAGE"

# Project copy in a volume (no Windows bind-mount writes during the build), without host build output.
# Target files lose IncludeOrderVersion and use the engine's own default build settings before 5.1.
MSYS_NO_PATHCONV=1 docker run --rm -u root -v "$HOST:/src:ro" -v "$VOLUME:/work" "$IMAGE" bash -c '
set -e
rm -rf /work/p && mkdir -p /work/p
tar -C /src --exclude=Saved --exclude=Intermediate --exclude=Binaries --exclude=DerivedDataCache -cf - . | tar -C /work/p -xf -
MINOR=$(echo '"$VERSION"' | cut -d. -f2)
if [ "$MINOR" -lt 1 ]; then
  sed -i "/IncludeOrderVersion/d; s/BuildSettingsVersion.Latest/BuildSettingsVersion.V2/" /work/p/Source/*.Target.cs
fi
chown -R ue4:ue4 /work
su ue4 -c "
set -e
cd /home/ue4/UnrealEngine
echo === BUILD MMHostEditor Linux Development on UE '"$VERSION"'
Engine/Build/BatchFiles/Linux/Build.sh MMHostEditor Linux Development -project=/work/p/MMHost.uproject
echo === TESTS
# Output goes to a file: the editor forks UnrealTraceServer, which would hold a pipe open forever.
Engine/Binaries/Linux/UnrealEditor-Cmd /work/p/MMHost.uproject -ExecCmds=\"Automation RunTests Edgegap; Quit\" -unattended -nullrhi -nosplash -nosound -stdout -FullStdOutLogOutput > /work/tests.log 2>&1 || true
grep -E \"Test Completed|TEST COMPLETE|Found [0-9]+ automation|LogEdgegap|Error:\" /work/tests.log | sort -u || true
"' 2>&1 | tee "$LOG"

grep -E "error|Result:|Test Completed|TEST COMPLETE" "$LOG" | tail -20
