# NETCONF schema-mount

## Devcontainer

If your editor does not support to run in a devcontainer, start it up manually with docker.

```bash
docker image build -t accelleran/nc-schema-mount-dev:latest -f .devcontainer/Containerfile .
docker container run -d --rm --name nc-schema-mount-dev --volume $(pwd):/workspace accelleran/nc-schema-mount-dev:latest
```

Now you can jump in (multiple times if in need of multiple shells):

```bash
docker container exec -it nc-schema-mount-dev /bin/bash
# cd /workspace
```

When done, clean up:

```bash
docker container stop nc-schema-mount-dev
```

## Build

```bash
mkdir -p build
cd build
cmake ..
make
```

## Run

Run the netopeer2-server in a separate shell before starting the main application.

```bash
./scripts/netopeer2.sh
```

```bash
./scripts/nc-schema-mount.sh
```
