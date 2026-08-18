package core

import (
	"errors"
	"fmt"
	"kubu-core/state"
)

const (
	TxTypeStandardTransfer = 0x01
	TxTypeAliasTransfer    = 0x02
	TxTypeRegisterAlias    = 0x03
)

type Transaction struct {
	Type        uint8  `json:"type"`
	Sender      string `json:"sender"`
	Recipient   string `json:"recipient"` // Can be an address (0x...) or handle (@alice)
	Amount      uint64 `json:"amount"`
	Fee         uint64 `json:"fee"`
	Signature   []byte `json:"signature"`
}

type Processor struct {
	AccountState *state.AccountState
	AliasState   *state.AliasState
}

// ProcessTransaction executes alias resolution and balance updates atomically
func (p *Processor) ProcessTransaction(tx *Transaction) error {
	switch tx.Type {
	case TxTypeStandardTransfer:
		return p.AccountState.Transfer(tx.Sender, tx.Recipient, tx.Amount + tx.Fee)

	case TxTypeRegisterAlias:
		// Register tx.Recipient as the desired handle for tx.Sender
		return p.AliasState.RegisterAlias(tx.Recipient, tx.Sender, p.AccountState.CurrentBlock())

	case TxTypeAliasTransfer:
		// 1. Resolve handle to raw address
		resolvedAddress, err := p.AliasState.ResolveAlias(tx.Recipient)
		if err != nil {
			return fmt.Errorf("alias resolution failed: %w", err)
		}

		// 2. Execute transfer to the resolved target
		if err := p.AccountState.Transfer(tx.Sender, resolvedAddress, tx.Amount+tx.Fee); err != nil {
			return fmt.Errorf("insufficient balance or state error: %w", err)
		}

		fmt.Printf("[Kubu-Core] Transferred %d KUBU from %s to @%s (%s)\n", 
			tx.Amount, tx.Sender, tx.Recipient, resolvedAddress)
		return nil

	default:
		return errors.New("unsupported transaction type")
	}
}
