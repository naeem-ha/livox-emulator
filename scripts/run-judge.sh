#!/usr/bin/env bash
# Build and run the stock livox_ros_driver2 judge container against the
# emulator. Requires native Docker Engine inside this WSL distro: Docker
# Desktop's --network=host attaches to the docker-desktop VM's namespace,
# which cannot see our bridge/veth, so it is refused here.
set -euo pipefail
cd "$(dirname "$0")/.."

if ! docker info >/dev/null 2>&1; then
  echo "ERROR: docker daemon not reachable. Install/start native Docker Engine" >&2
  echo "inside this WSL distro (apt install docker.io && systemctl start docker)." >&2
  exit 1
fi
if docker info 2>/dev/null | grep -qi "docker desktop"; then
  echo "ERROR: this is Docker Desktop's engine. Use native Docker Engine in this" >&2
  echo "distro; Desktop's host network lives in a different VM namespace." >&2
  exit 1
fi

docker build -t livox-judge -f docker/judge.Dockerfile docker
exec docker run --rm -i --network=host -v "$PWD/config:/config:ro" livox-judge "$@"
