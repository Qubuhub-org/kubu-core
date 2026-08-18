package rpc

import (
	"encoding/json"
	"net/http"
	"kubu-core/state"
)

type RPCServer struct {
	AliasState *state.AliasState
}

// HandleResolveAlias exposes RPC endpoint: /v1/alias/resolve?name=alice
func (s *RPCServer) HandleResolveAlias(w http.ResponseWriter, r *http.Request) {
	handle := r.URL.Query().Get("name")
	if handle == "" {
		http.Error(w, "missing handle parameter", http.StatusBadRequest)
		return
	}

	address, err := s.AliasState.ResolveAlias(handle)
	if err != nil {
		http.Error(w, err.Error(), http.StatusNotFound)
		return
	}

	json.NewEncoder(w).Encode(map[string]string{
		"handle":  handle,
		"address": address,
	})
}
