package state

import (
	"errors"
	"strings"
	"sync"
)

type AliasRecord struct {
	Handle   string `json:"handle"`   // Normalized handle, e.g. "alice"
	Owner    string `json:"owner"`    // Hex public address: "0x..."
	BlockNum uint64 `json:"block_num"`// Registration block height
}

type AliasState struct {
	mu      sync.RWMutex
	records map[string]*AliasRecord // Key: normalized handle
}

func NewAliasState() *AliasState {
	return &AliasState{
		records: make(map[string]*AliasRecord),
	}
}

// RegisterAlias binds a username handle to a wallet address
func (s *AliasState) RegisterAlias(handle, ownerAddress string, currentBlock uint64) error {
	s.mu.Lock()
	defer s.mu.Unlock()

	normalized := strings.ToLower(strings.TrimPrefix(handle, "@"))
	if len(normalized) < 3 || len(normalized) > 20 {
		return errors.New("handle must be between 3 and 20 characters")
	}

	if _, exists := s.records[normalized]; exists {
		return errors.New("alias already registered")
	}

	s.records[normalized] = &AliasRecord{
		Handle:   normalized,
		Owner:    ownerAddress,
		BlockNum: currentBlock,
	}
	return nil
}

// ResolveAlias fetches the underlying wallet address for a handle
func (s *AliasState) ResolveAlias(handle string) (string, error) {
	s.mu.RLock()
	defer s.mu.RUnlock()

	normalized := strings.ToLower(strings.TrimPrefix(handle, "@"))
	record, exists := s.records[normalized]
	if !exists {
		return "", errors.New("alias not found")
	}
	return record.Owner, nil
}
