#! /usr/bin/env python
"""
A script to parse results from filebench output and produce a formatted data
file that can be used by gnuplot.
"""

import argparse
import os
import os.path
import re
import sys

import numpy


RESULTS_PATH_ROOT = os.path.join(os.path.dirname(__file__), "results")
EASY_NAMES = {
    "sfuse_align_no_encryption": "SFuse w/ align",
    "sfuse_align_det_encryption": "SFuse w/ det.",
    "sfuse_align_rand_encryption": "SFuse w/ rand.",
    "sfuse_no_align_no_encryption": "SFuse"
}

# Color codes
RGB_GREEN = "0  255  0"
RGB_YELLOW = "255  255  0"
RGB_ORANGE = "255  165  0"
RGB_RED = "255  0  0"

NUMBER_BULLETS = {
    "File-server-50th.f"      : "\"{/ZapfDingbats \\300}\"",
    "Mail-server-16th.f"      : "\"{/ZapfDingbats \\301}\"",
    "Web-server-100th.f"      : "\"{/ZapfDingbats \\302}\"",
    "filemicro_rread_4K.f"    : "\"{/ZapfDingbats \\303}\"",
    "filemicro_rwrite_4K.f"   : "\"{/ZapfDingbats \\304}\"",
    "filemicro_seqread_4K.f"  : "\"{/ZapfDingbats \\305}\"",
    "filemicro_seqwrite_4K.f" : "\"{/ZapfDingbats \\306}\""
}

def get_clean_name(fs_name):
    """
    Returns a clean name for the filesystem/container name
    Args:
        fs_name(str): Original name of the filesystem
    Returns:
        str: A friendly name if listed in EASY_NAMES or the same name with the
             underscores properly escaped
    """
    return '"' + EASY_NAMES.get(fs_name, fs_name.replace("_", r"\\_")) + '"'

def filter_common_workloads(results):
    """
    Returns the list of common workloads across a list of filebench results
    Args:
        results (dict(str, dict(str, dict(str, float)))): A dictionnary containing
                                               filesystems, the workloads under
                                               which they were tested and
                                               statistics recorded during that
                                               benchmark
    Returns:
        list(str): The list of workloads tested across all filesystems
    """
    filesystems = results.keys()
    if not filesystems:
        return []
    common_workloads = set(results[filesystems[0]].keys())
    for filesystem in filesystems[1:]:
        fs_workloads = results[filesystem]
        common_workloads = common_workloads.intersection(set(fs_workloads))
    return sorted(list(common_workloads))

def group_all_workloads(results):
    """
    Returns the list of all workloads across a list of filebench results
    Args:
        results (dict(str, dict(str, dict(str, float)))): A dictionnary containing
                                               filesystems, the workloads under
                                               which they were tested and
                                               statistics recorded during that
                                               benchmark
    Returns:
        list(str): The list of workloads tested across the different filesystems
                   regardless of whether they ran on the same system
    """
    filesystems = list(results.keys())
    if not filesystems:
        return []
    workloads = set(results[filesystems[0]].keys())
    for filesystem in filesystems[1:]:
        fs_workloads = results[filesystem].keys()
        workloads = workloads.union(set(fs_workloads))
    return sorted(workloads)

def list_all_workloads(filesystem_directory):
    """
    List all the workloads in a filesystem directory
    Args:
        filesystem_directory(str): Path to the filesystem directory
    Returns:
        list(str): A sorted list of workloads containing files
    """
    workloads = set()
    for run_directory in [os.path.join(filesystem_directory, d) for d in os.listdir(filesystem_directory)]:
        for workload in [wk for wk in os.listdir(run_directory) if wk.endswith(".f")]:
            workloads.add(workload)
    return sorted(workloads)

class ParsingError(RuntimeError):
    """
    Signals a parsing error on a filebench result file
    """
    pass

class FilebenchRecord(object):
    """
    An object representation of the output of filebench
    """
    def __init__(self, text=None):
        """
        Default constructor
        Args:
            text(str, optional): Text output of a filebench run (defaults to None)
        """
        self.text = text
        self.operations = self.get_operations()
        self.operations_per_sec = self.get_operations_per_sec()
        self.throughput = self.get_throughput()
        self.threads = self.get_threads()
        self.runtime = self.get_runtime()


    def get_runtime(self):
        """
        Reads the total time spent on the workload from the original text
        Returns:
            float: The total time spent running the workload in seconds
        """
        lines = [line for line in self.text.split("\n") if len(line.strip()) > 0]
        if not lines:
            raise ParsingError("File is empty")
        last_line = lines[len(lines) - 1]
        match = re.match(r"(\d+(\.\d+)?):", last_line, flags=0)
        if match is None:
            raise ParsingError("Could not read total time from record")
        return float(match.group(1))

    def get_threads(self):
        """
        Reads informations about the threads from the original text
        Returns:
            list(dict): A list of threads and their related information
        """
        start_index = -1
        lines = [line.strip() for line in self.text.split("\n") if len(line.strip()) > 0]
        for i, text_line in enumerate(lines):
            if "Per-Operation Breakdown" in text_line:
                start_index = i + 1
                break
        if start_index == -1:
            raise ParsingError("The parser could not find any thread to list")
        thread_lines = []
        for line in lines[start_index:]:
            if "IO Summary" in line:
                break
            tokens = line.split()
            thread = {
                "name": tokens[0],
                "operations": tokens[1],
                "operations_per_sec": tokens[2],
                "throughput": tokens[3],
                "time_per_operation": tokens[4]
            }
            thread_lines.append(thread)
        return thread_lines

    def get_operations(self):
        """
        Reads the total number of operations from the original text
        Returns:
            int: The total number of operations during the workload run
        """
        try:
            lines = [line for line in self.text.split("\n") if len(line.strip()) > 0]
            summary_line = None
            for line in lines:
                if "IO Summary" in line:
                    summary_line = line
                    break
            if summary_line is None:
                raise ParsingError("Could not find summary line (IO Summary) in the text")
            return int(summary_line.split()[3])
        except ValueError:
            raise ParsingError("Could not read operations from IO Summary line")

    def get_throughput(self):
        """
        Read the total throughput from the original text
        Returns:
            float: The throughput during the workload run
        """
        summary_line = [line for line in self.text.split("\n") if "IO Summary:" in line][0]
        raw_throughput = summary_line.split()[9]
        return float(re.search(r"(\d+(\.\d+)?)", raw_throughput, flags=0).group(1))

    def get_operations_per_sec(self):
        """
        Reads the total number of operations per seconds from the original text
        Returns:
            float: The total number of operations during the workload run
        """
        summary_line = [line for line in self.text.split("\n") if "IO Summary:" in line][0]
        return float(re.search(r"(\d+(\.\d+)?)\s+ops/s", summary_line, flags=0).group(1))


class FilebenchParser(object):
    """
    A grouping of functions that read filebench results
    """
    def __init__(self, path=RESULTS_PATH_ROOT, throughput=False, filesystems=None, workloads=None, ratio=False):
        """
        Constructor that takes an optional path argument (that defaults to
        RESULTS_PATH_ROOT)
        Args:
            path(str, optional): The path of the root of the results folder
            throughput(bool, optional): Whether the parser should read the number
                                        of operations per second (default) or
                                        throughput data
            filesystems(list(str), optional): The list of file systems for which
                                              result data must be parsed. If none
                                              are give, it is assumed that all
                                              result files must be parsed
            workloads(list(str), optional): The list of workloads to parse. If
                                            none are given, it is assummed that
                                            all result files must be parsed
            ratio(Boolean, optional): . Defaults to False
        """
        self.path = path
        self.throughput = throughput
        if filesystems is None:
            self.filesystems = []
        else:
            self.filesystems = sorted(filesystems)
        if workloads is None:
            self.workload_filter = []
        else:
            self.workload_filter = workloads
        if ratio and    filesystems:
            self.filesystems = sorted(set(self.filesystems + ["native"]))

    def list_filesystems(self):
        """
        Returns a list of the filesystems tested
        Returns:
            list(str): A list of the filesystems listed in the results directory
                       sorted by name
        """
        filesystems = sorted([directory for directory in os.listdir(self.path)
                              if os.path.isdir(os.path.join(self.path, directory))])
        if self.filesystems:
            filesystems = [fs for fs in filesystems if fs in self.filesystems]
        return filesystems

    def read_workload_result(self, filesystem, workload):
        """
        Returns the number of operations per second for a specific workload.
        Args:
            filesystem(str): Name of the filesystem
            workload(str): Name of the workload
        Returns:
            float: The number of operations per second for a given workload
        """
        file_path = os.path.join(self.path, filesystem, workload)
        record = None
        with open(file_path, "r") as workload_file:
            record = FilebenchRecord(text=workload_file.read())
        if self.throughput:
            return record.throughput
        return record.operations_per_sec

    def read_multiple_workload_results(self, filesystem, workload):
        """
        Reads multiple result files  for the same workload and computes
        statistics on multiple runs.
        Args:
            filesystem(str): Name of the filesystem tested
            workload(str): Name of the workload whose results must be gathered
        Returns:
            dict(str, float): Statistics on multiple workload runs
        """
        filesystem_path = os.path.join(self.path, filesystem)
        runs = os.listdir(filesystem_path)
        results = []
        for run in runs:
            workload_file = os.path.join(filesystem_path, run, workload)
            if not os.path.isfile(workload_file):
                sys.stderr.write(workload_file + " : Cannot be found\n")
                continue
            workload_path = os.path.join(run, workload)
            try:
                result = self.read_workload_result(filesystem, workload_path)
                results.append(result)
            except ParsingError as error:
                sys.stderr.write(workload_file + " : " + str(error) + "\n")
        if not results:
            return {
                "mean": -1,
                "median": -1,
                "min": -1,
                "max": -1,
                "std": -1,
                "files_read": 0
            }
        return {
            "mean": numpy.mean(results),
            "median": numpy.median(results),
            "min": numpy.min(results),
            "max": numpy.max(results),
            "std": numpy.std(results),
            "files_read": len(results)
        }

    def read_filesystems_results(self, filesystem):
        """
        Returns the results of various benchmarks on a specific filesystem.
        Args:
            filesystem(str): Name of the filesystem whose results are queried
        Returns:
            dict(str, dict(str, float)): A dictionnary with the workload as the key and
                         related statistics as the value
        """
        workloads = list_all_workloads(os.path.join(self.path, filesystem))
        if self.workload_filter:
            workloads = [w for w in workloads if w in self.workload_filter]
        return {workload: self.read_multiple_workload_results(filesystem, workload) for workload in workloads}

    def read_results(self):
        """
        Reads the workload results from the result files.
        Returns:
            results (dict(str, dict(str, dict(str, float)))): A dictionnary containing
                                                   filesystems, the workloads under
                                                   which they were tested and
                                                   statistics recorded during that
                                                   benchmark

        """
        filesystems = self.list_filesystems()
        results = {}
        for filesystem in filesystems:
            results[filesystem] = self.read_filesystems_results(filesystem)
        return results

    def read_ratio_results(self):
        """
        Returns a table with the results of each filesystem as a ratio of the
        native performance
            dict(str, dict(str, float)): A dictionnary containing the results as ratios
        """
        results = self.read_results()
        print("fs: " + ", ".join(results.keys()))
        workloads = group_all_workloads(results)
        filesystems = sorted([fs for fs in results if fs != "native"])
        baseline = results["native"]
        ratio_results = {}
        for file_system in filesystems:
            fs_results = results[file_system]
            ratio_results[file_system] = {}
            for workload in workloads:
                ratio = -1
                if workload in fs_results and fs_results[workload]["mean"] != -1 and baseline[workload]["mean"] != -1:
                    ratio = fs_results[workload]["mean"] / baseline[workload]["mean"]
                ratio_results[file_system][workload] = ratio
        return ratio_results

def format_results(results):
    """
    Returns the data formatted in table that can be used by gnuplot.
    Args:
        results (dict(str, dict(str, dict(str, float)))): A dictionnary containing
                                               filesystems, the workloads under
                                               which they were tested and
                                               statistics recorded during that
                                               benchmark
    Returns:
        str: The filebench data formatted in a table
    """
    workloads = group_all_workloads(results)
    filesystems = sorted(results.keys())
    filesystems_header = [get_clean_name(fs) for fs in filesystems]
    lines = ["benchmark " + " ".join(filesystems_header)]
    for workload in workloads:
        line = [get_clean_name(workload)]
        for filesystem in filesystems:
            if workload in results[filesystem]:
                line.append(str(results[filesystem][workload]["mean"]))
            else:
                line.append("-1")
        lines.append(" ".join(line))
    return "\n".join(lines)

def format_debug_results(results):
    """
    Similar to format_results but adds and extra standard deviation column after
    each result.
    Args:
        results (dict(str, dict(str, dict(str, float)))): A dictionnary containing
                                               filesystems, the workloads under
                                               which they were tested and
                                               statistics recorded during that
                                               benchmark
    Returns:
        str: he filebench data formatted in a table
    """
    workloads = group_all_workloads(results)
    filesystems = sorted(results.keys())
    filesystems_header = [get_clean_name(fs) + " std" for fs in filesystems]
    lines = ["benchmark " + " ".join(filesystems_header)]
    for workload in workloads:
        line = [get_clean_name(workload)]
        for filesystem in filesystems:
            if workload in results[filesystem]:
                line.append(str(results[filesystem][workload]["mean"]))
                line.append(str(results[filesystem][workload]["std"]))
            else:
                line.append("-1")
                line.append("-1")
        lines.append(" ".join(line))
    return "\n".join(lines)

def format_ratio_results(ratio_results):
    """
    Returns the ratio data formatted in table that can be used by gnuplot.
    Args:
        results (dict(str, dict(str, dict(str, float)))): A dictionnary containing
                                               filesystems, the workloads under
                                               which they were tested and
                                               statistics recorded during that
                                               benchmark
    Returns:
        str: The filebench data formatted in a table
    """
    lines = ["# idx value color filesystem workload"]
    workloads = group_all_workloads(ratio_results)
    filesystems = sorted(ratio_results.keys())
    idx = 0
    lines.append(str(idx))
    idx += 1
    for file_system in filesystems:
        for workload in workloads:
            if workload in ratio_results[file_system]:
                ratio = ratio_results[file_system][workload]
            else:
                ratio = lines.append("-1.0")

            color = RGB_RED # default color is red
            if ratio >= 0.95:
                color = RGB_GREEN
            elif ratio >= 0.75:
                color = RGB_YELLOW
            elif ratio >= 0.25:
                color = RGB_ORANGE
            line = [
                str(idx),
                str(ratio),
                color,
                get_clean_name(file_system),
                get_clean_name(workload),
                NUMBER_BULLETS.get(workload, "")
            ]
            lines.append(" ".join(line))
            idx += 1
        lines.append(str(idx))
        idx += 1
    return "\n".join(lines)

if __name__ == "__main__":
    ARG_PARSER = argparse.ArgumentParser(prog="parse.py",
                                         description="Parse data from filebench results and output the number of operations per seconds for each workload on each file system")
    ARG_PARSER.add_argument("-d", "--directory", help="Specify the directory containing the results to parse", nargs=1, type=str)
    ARG_PARSER.add_argument("-r", "--ratio", help="Compute normalized results against \"native\" file system", action="store_true")
    ARG_PARSER.add_argument("-t", "--throughput", help="Output the throughput in MB/s rather than the number of operations per seconds", action="store_true")
    ARG_PARSER.add_argument("-f", "--filesystems", help="Only parse the results of given file systems", nargs="+")
    ARG_PARSER.add_argument("-w", "--workloads", help="Only parse the results of given workloads", nargs="+")
    ARG_PARSER.add_argument("-s", "--standard-deviation", help="Print standard deviation with the data", action="store_true")
    ARGS = ARG_PARSER.parse_args()
    RESULTS_PATH = RESULTS_PATH_ROOT
    if ARGS.directory:
        RESULTS_PATH = ARGS.directory[0]
    PARSER = FilebenchParser(path=RESULTS_PATH, throughput=ARGS.throughput, filesystems=ARGS.filesystems, workloads=ARGS.workloads, ratio=ARGS.ratio)
    if ARGS.ratio:
        RATIO_RESULTS = PARSER.read_ratio_results()
        print(format_ratio_results(RATIO_RESULTS))
    elif ARGS.standard_deviation:
        FILEBENCH_RESULTS = PARSER.read_results()
        print(format_debug_results(FILEBENCH_RESULTS))
    else:
        FILEBENCH_RESULTS = PARSER.read_results()
        print(format_results(FILEBENCH_RESULTS))
