# fusesgx

fusegx is a user-space in-enclave file system.

## Dependencies

Most of the dependencies can be installed by running the install.sh script on Ubuntu 16.04:
```bash
sudo ./install.sh
```

On other systems make sure that the following tools and libraries are installed.

* Intel SGX Driver
* Intel SGX SDK
* libfuse 2.9
* g++

## Build

```bash
make SGX_MODE=HW SGX_PRERELEASE=1
```

### Run

To run the file system as daemon, run:
```bash
./app path/to/mountpoint
```

To run it in foreground mode, try:
```bash
./app -f path/to/mountpoint
```

To run it in foreground mode with the fuse debug mode, run:
```bash
./app -d path/to/mountpoint
```
