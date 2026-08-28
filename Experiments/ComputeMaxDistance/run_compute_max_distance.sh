#!/usr/bin/env bash
set -euo pipefail

if [ "$#" -lt 1 ]; then
    echo "Usage: $0 <resolution> [resolution ...]" >&2
    echo "Example: $0 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15" >&2
    exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"
RESULT_DIR="${SCRIPT_DIR}/results"

mkdir -p "${RESULT_DIR}"

BINARY=""
for candidate in \
    "${PROJECT_ROOT}/build/Experiments/ComputeMaxDistance/h3_max_cell_radius" \
    "${PROJECT_ROOT}/build/Experiments/ComputeMaxDistance/Release/h3_max_cell_radius"; do

    if [ -x "${candidate}" ]; then
        BINARY="${candidate}"
        break
    fi
done

if [ -z "${BINARY}" ]; then
    echo "Could not find h3_max_cell_radius. Build it with: cmake -S . -B build && cmake --build build --target h3_max_cell_radius" >&2
    exit 1
fi

for resolution in "$@"; do
    if [[ ! "${resolution}" =~ ^[0-9]+$ ]]; then
        echo "Invalid resolution: ${resolution}" >&2
        exit 1
    fi

    if [ "${resolution}" -ge 6 ]; then
        THREADS="${SLURM_CPUS_PER_TASK:-${OMP_NUM_THREADS:-$(nproc)}}"
    else
        THREADS=1
    fi

    OUTPUT_FILE="${RESULT_DIR}/h3_max_cell_radius_resolution${resolution}.csv"
    echo "Running H3 max-distance computation for resolution ${resolution} with ${THREADS} thread(s)."

    OMP_NUM_THREADS="${THREADS}" "${BINARY}" "${resolution}" > "${OUTPUT_FILE}"

    echo "Wrote ${OUTPUT_FILE}"
done
