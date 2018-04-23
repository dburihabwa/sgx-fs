#!/usr/bin/env bash

run() {
    if [[ "${#}" -ne 1 ]]; then
        echo "Usage: run <parent_result_directory>" >&2
        return 0
    fi
    readonly parent_results_directory="${1}"
    if [[ ! -d  "${parent_results_directory}" ]]; then
        echo "${parent_results_directory} is not a directory"
        return 0
    fi
    readonly script_directory="$(dirname $(readlink -f $0))"
    readonly workload_folder="${script_directory}/../../filebench_workloads"
    local workloads=(
        "filemicro_create.f"
        "filemicro_createfiles.f"
        "filemicro_createrand.f"
        "filemicro_delete.f"
        "filemicro_rread.f"
        "filemicro_rwrite.f"
        "filemicro_seqread.f"
        "filemicro_seqwrite.f"
    )
    for workload in "${workloads[@]}"; do
        local workload_file="${workload_folder}/${workload}"
        if [[ -e "${workload_file}" ]] ; then
            echo "* ${workload_file}"
            filebench -f "${workload_file}"
        fi
    done
}


if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    if [[ "${#}" -ne 2 ]]; then
        echo "Usage: ${0} <mountpoint> <parent_result_directory>" >&2
        exit 0
    fi
    mountpoint="${1}"
    if [ "$(stat -fc%t:%T "${mountpoint}")" == "$(stat -fc%t:%T "${mountpoint}/..")" ]; then
        echo "${mountpoint} is not a valid mounpoint"
        exit 0
    fi
    results_directory="${2}"
    mkdir -p "${results_directory}"
    run "${results_directory}"
fi
