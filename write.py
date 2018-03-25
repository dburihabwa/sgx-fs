#! /usr/bin/env python3
import argparse
import json
import os
import time

DEFAULT_BLOCKSIZE = 4096

def main(path, size):
    """
    Args:
        path(str): Path to the mount point
        size(int): Size of the file to write (in bytes)
    """
    if not path or not isinstance(path, str):
        raise ValueError("path argument should be a non-empty string")
    if not isinstance(size, int) or size <= 0:
        raise ValueError("size argument should be a integer greater than 0")
    if not os.path.isdir(path):
        error_msg = "{:s} mount point is not a valid directory".format(path)
        raise ValueError(error_msg)
    file_to_write = os.path.join(path, "dummy.txt")
    stats = {
        "size": size,
        "blocksize": DEFAULT_BLOCKSIZE,
        "response_times": {}
    }
    with open(file_to_write, "wb") as handle:
        for offset in range(0, size, DEFAULT_BLOCKSIZE):
            data = os.urandom(DEFAULT_BLOCKSIZE)
            start = time.time()
            handle.write(data)
            end = time.time()
            elapsed = end - start
            stats["response_times"][offset] = elapsed
    return stats

if __name__ == "__main__":
    PARSER = argparse.ArgumentParser(__file__)
    PARSER.add_argument("path", help="Path to the mount point", type=str)
    PARSER.add_argument("size", help="Size of the file to write (in bytes)", type=int)
    ARGS = PARSER.parse_args()
    STATS = main(ARGS.path, ARGS.size)
    print(json.dumps(STATS, indent=4, sort_keys=True))
    