// SPDX-License-Identifier: MIT
pragma solidity ^0.8.20;

contract KubuNameRegistry {
    mapping(string => address) private nameToAddress;
    mapping(address => string) private addressToName;

    event NameRegistered(string indexed name, address indexed owner);

    // Register a username to the sender's wallet
    function registerName(string calldata _name) external {
        require(bytes(_name).length > 0, "Name cannot be empty");
        require(nameToAddress[_name] == address(0), "Name already taken");
        require(bytes(addressToName[msg.sender]).length == 0, "Address already registered a name");

        nameToAddress[_name] = _nameToLower(_name);
        addressToName[msg.sender] = _nameToLower(_name);

        emit NameRegistered(_name, msg.sender);
    }

    // Resolve a handle to a wallet address
    function resolveName(string calldata _name) external view returns (address) {
        address resolved = nameToAddress[_name];
        require(resolved != address(0), "Name not found");
        return resolved;
    }

    // Resolve a wallet address back to a username
    function resolveAddress(address _addr) external view returns (string memory) {
        return addressToName[_addr];
    }

    // Convert input string to lowercase for consistency
    function _nameToLower(string memory _str) internal pure returns (string memory) {
        bytes memory bStr = bytes(_str);
        bytes memory bLower = new bytes(bStr.length);
        for (uint i = 0; i < bStr.length; i++) {
            if ((uint8(bStr[i]) >= 65) && (uint8(bStr[i]) <= 90)) {
                bLower[i] = bytes1(uint8(bStr[i]) + 32);
            } else {
                bLower[i] = bStr[i];
            }
        }
        return string(bLower);
    }
}
