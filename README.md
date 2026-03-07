# KUBU Core

KUBU is a Proof-of-Work cryptocurrency focused on human-readable nicknames.
Instead of sending only to long wallet addresses, users can send payments to
active nicknames and optionally include memo tags for exchange/service routing.

## Project Overview

KUBU Core is the reference node and wallet implementation for the KUBU network.
It includes:

- full node (`kubud`)
- CLI (`kubu-cli`)
- transaction tool (`kubu-tx`)
- GUI wallet (`kubu-qt`)
- nickname operations at protocol and wallet level

## Core Characteristics

- Name: `KUBU`
- Ticker: `KUBU`
- Algorithm: `Scrypt` PoW + `AuxPoW` (merge mining)
- Block time: `54 seconds`
- AuxPoW activation height (mainnet): `39,000`
- Address prefix (mainnet): starts with `P`
- Reward schedule:
  - Blocks 0-1,167,999: `48 KUBU`
  - Blocks 1,168,000-2,335,999: `24 KUBU`
  - Blocks 2,336,000-3,503,999: `12 KUBU`
  - Blocks 3,504,000-4,671,999: `6 KUBU`
  - Blocks 4,672,000+: `2 KUBU` tail reward

## Nickname System

KUBU nicknames are on-chain identities with a lifecycle and anti-squatting
bond model.

- Register nickname
- Update payout address
- Transfer ownership
- Renew validity period
- Release nickname
- Claim bond
- Send to nickname (`sendtonickname`)
- Optional memo tags (`KMEM1`) for exchange/service flows

Nickname economics combine:

- registration fee (network fee requirement)
- locked bond amount (claimable later according to status rules)

## Network Ports

Default ports configured in KUBU Core:

| Function | mainnet | testnet | regtest |
| :------- | ------: | ------: | ------: |
| P2P      |   45874 |   46874 |   47874 |
| RPC      |   45873 |   46873 |   47873 |

Do not expose RPC ports to the public internet.

## Getting Started

- Build/install basics: [INSTALL.md](INSTALL.md)
- First run: [docs/getting-started.md](docs/getting-started.md)
- Daily usage and nickname operations: [docs/how-to-use.md](docs/how-to-use.md)

## Contributing

Contributions are welcome. Please read [CONTRIBUTING.md](CONTRIBUTING.md)
before opening pull requests.

## Communities

- Website: [kubucoin.org](https://kubucoin.org)
- Discord: [https://kubucoin.org/discord](https://kubucoin.org/discord)

## License

See LICENSE file.

## Support

    GitHub Issues: https://github.com/kubucoindev/kubu/issues
    Documentation: See docs/ directory
