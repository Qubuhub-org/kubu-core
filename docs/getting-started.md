# Getting Started with KUBU Core

This guide helps you launch a node, connect your wallet, and verify the setup.

## 1) Install or Build

- See [../INSTALL.md](../INSTALL.md) for dependency and build steps.
- Expected binaries:
  - `kubud`
  - `kubu-cli`
  - `kubu-tx`
  - `kubu-qt`

## 2) Start a Node

Mainnet:

```bash
kubud -daemon
```

Testnet:

```bash
kubud -testnet -daemon
```

Regtest (for local development):

```bash
kubud -regtest -daemon
```

## 3) Verify Node Status

```bash
kubu-cli getblockchaininfo
kubu-cli getnetworkinfo
```

For testnet/regtest, add the network flag:

```bash
kubu-cli -testnet getblockchaininfo
kubu-cli -regtest getblockchaininfo
```

## 4) Wallet Basics

Create a receiving address:

```bash
kubu-cli getnewaddress
```

Check balance:

```bash
kubu-cli getbalance
```

## 5) Regtest Quick Mining (for local tests)

Generate blocks to a local address:

```bash
ADDR=$(kubu-cli -regtest getnewaddress)
kubu-cli -regtest generatetoaddress 101 "$ADDR"
kubu-cli -regtest getbalance
```

## 6) Next Steps

- Nickname usage examples: [how-to-use.md](how-to-use.md)
- RPC command list:

```bash
kubu-cli help
kubu-cli help sendtonickname
```
