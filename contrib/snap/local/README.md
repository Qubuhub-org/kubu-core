# Kubu Snap Packaging

Commands for building and uploading a Kubu Core Snap to the Snap Store. Anyone on amd64 (x86_64), arm64 (aarch64), or i386 (i686) should be able to build it themselves with these instructions. This would pull the official Kubu binaries from the releases page, verify them, and install them on a user's machine.

## Building Locally
```
sudo apt install snapd
sudo snap install --classic snapcraft
sudo snapcraft
```

### Installing Locally
```
snap install \*.snap --devmode
```

### To Upload to the Snap Store
```
snapcraft login
snapcraft register kubu-core
snapcraft upload \*.snap
sudo snap install kubu-core
```

### Usage
```
kubu-unofficial.cli # for kubu-cli
kubu-unofficial.d # for kubud
kubu-unofficial.qt # for kubu-qt
kubu-unofficial.test # for test_kubu
kubu-unofficial.tx # for kubu-tx
```

### Uninstalling
```
sudo snap remove kubu-unofficial
```