# TrustZone Event Manager

## Build OP-TEE for QEMUv7

This has to be done on the host OS. The root of the OP-TEE installation will then mounted as a volume on the container.

- First, follow the first two steps [here](https://optee.readthedocs.io/en/latest/building/gits/build.html#get-and-build-the-solution)

```bash
# create a root folder
mkdir optee
cd optee

# get the repositories
repo init -u https://github.com/AuthenticExecution/event-manager-trustzone.git -m manifest.xml
repo sync -j4 --no-clone-bundle

# get the event manager and put under `optee_examples`
git clone https://github.com/AuthenticExecution/event-manager-trustzone.git
cp -r event-manager-trustzone/event_manager optee_examples/

# build OPTEE
cd build
make -j2 toolchains
make -j `nproc`
```

## Run a OPTEE instance using Docker

```bash
### <volume>: absolute path of the root folder created in the previous phase (default: /opt/optee)
make run PORT=<port> OPTEE_DIR=<volume>
```

The container automatically runs the Event Manager at startup.

## Troubleshooting

### OPTEE installation fails

- Be sure to install all
  [prerequisites](https://optee.readthedocs.io/en/latest/building/gits/build.html#get-and-build-the-solution)
- If you get an error like `ModuleNotFoundError: No module named 'Cryptodome'`,
  run `sudo apt install python3-pycryptodome` and try again

### Container starts but there is no output

This is probably due to OP-TEE not being correctly installed into the host.
Ensure OPTEE is correctly installed and mounted to the container
`OPTEE_DIR=<path_to_optee>`, then try again.