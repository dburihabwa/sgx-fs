#! /usr/bin/env python3
import argparse
import json
import os
import time

DEFAULT_BLOCKSIZE = 4096


def write(path, size):
    if not path or not isinstance(path, str):
        raise ValueError("path argument should be a non-empty string")
    if not isinstance(size, int) or size <= 0:
        raise ValueError("size argument should be a integer greater than 0")
    base_directory = os.path.dirname(path)
    if not os.path.isdir(base_directory):
        error_msg = "{:s} mount point is not a valid directory".format(base_directory)
        raise ValueError(error_msg)
    stats = {
        "size": size,
        "blocksize": DEFAULT_BLOCKSIZE,
        "response_times": {}
    }
    with open(path, "wb") as handle:
        for offset in range(0, size, DEFAULT_BLOCKSIZE):
            data = os.urandom(min(size - offset , DEFAULT_BLOCKSIZE))
            start = time.time()
            handle.write(data)
            end = time.time()
            elapsed = end - start
            stats["response_times"][offset] = elapsed
        return stats

if __name__ == "__main__":
    PARSER = argparse.ArgumentParser(__file__, description="A script to write one or mulitple files to a fuse mount point for testing purposes")
    PARSER.add_argument("path", help="Path to the mount point", type=str)
    PARSER.add_argument("count", help="Number of files to write", type=int, default=1)
    PARSER.add_argument("size", help="Size of the file to write (in bytes)", type=int)
    ARGS = PARSER.parse_args()
    NAME_TEMPLATE = os.path.join(ARGS.path, "dummy-{:d}.txt")
    RESPONSE_TIMES = {}
    for index in range(ARGS.count):
        filepath = NAME_TEMPLATE.format(index + 1)
        RESPONSE_TIMES[filepath] = write(filepath, ARGS.size)
    print(json.dumps(RESPONSE_TIMES, indent=4, sort_keys=True))
            