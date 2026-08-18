import { ethers } from "ethers";

const CONTRACT_ADDRESS = "0xYourDeployedRegistryContractAddress";
const CONTRACT_ABI = [
  "function resolveName(string calldata _name) external view returns (address)",
  "function registerName(string calldata _name) external"
];

// Initialize provider and signer (e.g., via MetaMask or wallet connector)
const provider = new ethers.BrowserProvider(window.ethereum);
const signer = await provider.getSigner();
const registry = new ethers.Contract(CONTRACT_ADDRESS, CONTRACT_ABI, signer);

/**
 * Resolve handle to address and trigger coin transfer
 */
async function sendCoinByName(usernameInput, amountInCoins) {
  try {
    // 1. Sanitize handle input (remove @ or spaces)
    const cleanHandle = usernameInput.trim().replace(/^@/, "").toLowerCase();

    // 2. Query smart contract for destination address
    const recipientAddress = await registry.resolveName(cleanHandle);
    console.log(`Resolved @${cleanHandle} to address: ${recipientAddress}`);

    // 3. Send coins to the resolved address
    const tx = await signer.sendTransaction({
      to: recipientAddress,
      value: ethers.parseEther(amountInCoins.toString())
    });

    console.log(`Transaction sent! Hash: ${tx.hash}`);
    await tx.wait();
    console.log("Transaction confirmed!");
  } catch (err) {
    console.error("Payment failed:", err.message);
  }
}

// Example usage:
// sendCoinByName("alice", "1.5");
