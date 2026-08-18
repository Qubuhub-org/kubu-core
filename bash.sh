ADDR=$(kubu-cli -regtest getnewaddress)
kubu-cli -regtest generatetoaddress 101 "$ADDR"
kubu-cli -regtest getbalance
