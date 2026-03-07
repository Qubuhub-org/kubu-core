# How to Use KUBU Core

This page covers common wallet and nickname workflows.

## Send and Receive

Create a receive address:

```bash
kubu-cli getnewaddress
```

Send to an address:

```bash
kubu-cli sendtoaddress "<address>" 10
```

## Nickname Workflow

Check nickname availability and pricing:

```bash
kubu-cli checknickname "kubu_user"
```

Resolve active nickname:

```bash
kubu-cli resolvenickname "kubu_user"
```

List your wallet-controlled nicknames:

```bash
kubu-cli listwalletnicknames
```

## Register a Nickname

1. Get owner key (pubkey from an address you control).
2. Choose payout address.
3. Broadcast registration.

```bash
# Example values shown for format only:
kubu-cli registernickname "kubu_user" "<33-byte-compressed-owner-pubkey-hex>" "<payout-address>"
```

## Update / Transfer / Renew / Release / Claim Bond

Update payout address:

```bash
kubu-cli updatenickname "kubu_user" "<new-payout-address>"
```

Transfer ownership:

```bash
kubu-cli transfernickname "kubu_user" "<new-owner-compressed-pubkey-hex>"
```

Renew:

```bash
kubu-cli renewnickname "kubu_user"
```

Release:

```bash
kubu-cli releasenickname "kubu_user"
```

Claim bond:

```bash
kubu-cli claimnicknamebond "kubu_user"
```

## Send to Nickname

```bash
kubu-cli sendtonickname "kubu_user" 5
```

Send to nickname with memo tag:

```bash
kubu-cli sendtonickname "kubu_user" 5 "" "" false "123456" "numeric"
```

## Useful RPC Help

```bash
kubu-cli help checknickname
kubu-cli help registernickname
kubu-cli help sendtonickname
```
