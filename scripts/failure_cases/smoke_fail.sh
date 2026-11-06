#!/bin/sh
# smoke_fail.sh - deliberate failure workload for the capture path.
# Exits 3 on purpose; the harness should match this expected failure.
echo "smoke: deliberate failure workload ran"
echo "smoke: first relevant error: deliberate exit 3" >&2
exit 3
