#! /usr/bin/env bash

FS_START_SCRIPTS=(
  "../../ramfs.bin"
  "../../sgx-ramfs.bin"
)
FILEBENCH_WORKLOADS=(
  "../../filebench_workloads/filemicro_rread.f"
  "../../filebench_workloads/filemicro_rwrite.f"
  "../../filebench_workloads/filemicro_seqwrite.f"
  "../../filebench_workloads/filemicro_seqread.f"
)
RUNS=5
BASE_RESULTS_DIR="fs_results"
MOUNT_POINT=/tmp/mnt/fuse

wait_for_mounpoint() {
  if [[ "${#}" -ne 1 ]]; then
    echo "Usage: wait_for_mounpoint <mountpoint>" >&2
    exit 0
  fi
  local dir="${1}"
  local count=0
  local max=60
  while [[ "${count}" -lt "${max}" ]]; do
    if [[ $(stat -fc%t:%T "${dir}") != $(stat -fc%t:%T "${dir}/..") ]]; then
      return 0
    fi
    echo "Waiting for ${dir} to be mounted..."
    count=$((count + 1))
    sleep 1
  done
  echo "${dir} was not mounted after ${max} seconds"
  exit 0
}


main() {
  # Unmount previous file system
  mkdir -p "${MOUNT_POINT}"
  sudo umount --force "${MOUNT_POINT}"

  for start_script in "${FS_START_SCRIPTS[@]}"; do
    if [[ ! -f "${start_script}" ]]; then
        echo "Could not find script: ${start_script}" >&2
        continue
    fi
    local fs_results_dir="${BASE_RESULTS_DIR}/$(basename "${start_script}")"
    mkdir -p "${fs_results_dir}"
    for run in $(seq "${RUNS}"); do
      for workload in "${FILEBENCH_WORKLOADS[@]}"; do
        if [[ ! -f "${workload}" ]]; then
            echo "Could not find workload: ${workload}" >&2
            continue
        fi
        # Unmount previous file system
        sudo umount --force "${MOUNT_POINT}"
        local run_results_dir="${fs_results_dir}/${run}"
        mkdir -p "${run_results_dir}"
        local workload_name="$(basename "${workload}")"
        local workload_results_file="${run_results_dir}/${workload_name}"
        local workload_fs_log="${run_results_dir}/${workload_name}-fs.log"
        ${start_script} ${MOUNT_POINT} -oallow_other -f 1>"${workload_fs_log}" 2>&1 &
        local pid="${!}"
        echo "Running ${workload} on top of ${start_script} (pid=${pid}) (${run}/${RUNS})"
        wait_for_mounpoint "${MOUNT_POINT}"
        sudo filebench -f "${workload}" 1>"${workload_results_file}" 2>&1
        continue
      done
    done
  done

  # Unmount previous file system
  sudo umount --force "${MOUNT_POINT}"
}

main "${@}"
