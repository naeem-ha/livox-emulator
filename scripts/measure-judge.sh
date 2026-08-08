#!/usr/bin/env bash
# Measure what the judge is actually publishing. Run while run-judge.sh is up.
# Usage: measure-judge.sh [seconds]   (default 15)
set -uo pipefail
SECS="${1:-15}"

CID=$(docker ps -q -f ancestor=livox-judge | head -1)
if [ -z "$CID" ]; then
  echo "judge container is not running — start it with scripts/run-judge.sh" >&2
  exit 1
fi
SRC='source /opt/ros/humble/setup.bash && source /ws_livox/install/setup.bash'

echo "=== /livox/lidar hz (${SECS}s; expect ~10 Hz) ==="
docker exec "$CID" bash -c "$SRC && timeout $SECS ros2 topic hz /livox/lidar 2>&1 | tail -3"

echo "=== /livox/imu hz (10s; expect ~200 Hz) ==="
docker exec "$CID" bash -c "$SRC && timeout 12 ros2 topic hz /livox/imu 2>&1 | tail -3"

echo "=== CustomMsg point_num (expect ~20000, non-zero) ==="
docker exec "$CID" bash -c "$SRC && timeout 10 ros2 topic echo --once /livox/lidar --field point_num"

# `ros2 topic list` reports only what DDS discovery has seen so far, so a cold
# call often misses /livox/* — retry briefly instead of printing a false empty.
echo "=== topics ==="
for _ in 1 2 3 4 5; do
  TOPICS=$(docker exec "$CID" bash -c "$SRC && ros2 topic list" 2>/dev/null)
  case "$TOPICS" in */livox/*) break ;; esac
  sleep 2
done
echo "$TOPICS"
case "$TOPICS" in
  */livox/*) ;;
  *) echo "(warning: /livox topics not seen by discovery)" ;;
esac
