ADDR=$(kubu-cli -regtest getnewaddress)
kubu-cli -regtest generatetoaddress 101 "$ADDR"
kubu-cli -regtest getbalance
# 1. Create the man1 directory if it doesn't exist
sudo mkdir -p /usr/local/share/man/man1

# 2. Save the text into the man page file
sudo nano /usr/local/share/man/man1/kubu-cli.1
# (Paste the roff text into nano, then save with Ctrl+O -> Enter -> Ctrl+X)

# 3. Update the man database
sudo mandb

# 4. View the installed manual page
man kubu-cli
# List all available RPC commands
kubu-cli help

# Get help for a specific command
kubu-cli help getblockchaininfo

# Get general node status & sync status
kubu-cli getblockchaininfo
# Standard Positional Send
kubu-cli sendtoaddress "PAddressHere..." 10.5

# Named Arguments Send
kubu-cli -named sendtoaddress address="PAddressHere..." amount=10.5 comment="Web4 Tip"
# Register a handle for your active wallet address
kubu-cli -named registeralias handle="alice"

# Resolve a handle to its underlying address
kubu-cli -named resolvealias handle="alice"
