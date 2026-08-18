document.addEventListener('DOMContentLoaded', () => {
  const sendBtn = document.querySelector('.btn-send-kubu');
  const handleInput = document.querySelector('.handle-input');
  const amountInput = document.querySelector('.amount-input');
  const statusDiv = document.querySelector('.resolution-status');

  if (!sendBtn) return;

  sendBtn.addEventListener('click', async () => {
    const handle = handleInput.value.trim();
    const amount = amountInput.value.trim();

    if (!handle || !amount) {
      statusDiv.innerHTML = `<p style="color: red;">Please enter both a @handle and amount.</p>`;
      return;
    }

    statusDiv.innerHTML = `<p style="color: #666;">Resolving ${handle} on Kubu Network...</p>`;

    try {
      // 1. Query proxy API for alias resolution
      const response = await fetch(`/api/rpc`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          jsonrpc: '2.0',
          id: 1,
          method: 'resolvealias',
          params: [handle.replace(/^@/, '')]
        })
      });

      const data = await response.json();

      if (data.error) {
        statusDiv.innerHTML = `<p style="color: red;">Resolution Error: ${data.error.message}</p>`;
        return;
      }

      const resolvedAddress = data.result.address;
      statusDiv.innerHTML = `
        <div style="background: #eef9f2; padding: 12px; border-radius: 6px; border: 1px solid #b7ebd0;">
          <p style="color: #0d6332; margin: 0 0 4px 0;"><strong>Handle Resolved!</strong></p>
          <p style="margin: 0 0 8px 0; font-family: monospace;">${handle} ➔ ${resolvedAddress}</p>
          <button id="confirm-send-btn" style="background: #0d6332; color: #fff; border: none; padding: 8px 16px; border-radius: 4px; cursor: pointer;">
            Confirm & Send ${amount} KUBU
          </button>
        </div>
      `;

      document.getElementById('confirm-send-btn').addEventListener('click', async () => {
        statusDiv.innerHTML = `<p style="color: #666;">Broadcasting transaction to Kubu Network...</p>`;
        
        // 2. Broadcast Transaction
        const txResp = await fetch(`/api/rpc`, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({
            jsonrpc: '2.0',
            id: 2,
            method: 'sendtoaddress',
            params: [resolvedAddress, parseFloat(amount)]
          })
        });

        const txData = await txResp.json();
        if (txData.error) {
          statusDiv.innerHTML = `<p style="color: red;">Transaction Failed: ${txData.error.message}</p>`;
        } else {
          statusDiv.innerHTML = `
            <p style="color: green; font-weight: bold;">Sent ${amount} KUBU to ${handle}!</p>
            <p style="font-family: monospace; font-size: 12px;">TxID: ${txData.result}</p>
          `;
        }
      });

    } catch (err) {
      statusDiv.innerHTML = `<p style="color: red;">RPC Proxy Error: ${err.message}</p>`;
    }
  });
});
