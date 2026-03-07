#ifndef BITCOIN_NICKNAMEDB_H
#define BITCOIN_NICKNAMEDB_H

#include "amount.h"
#include "dbwrapper.h"
#include "nicknames.h"
#include "primitives/transaction.h"
#include "pubkey.h"
#include "uint256.h"

#include <exception>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <vector>

struct NicknameInfo
{
    std::string nickname;
    std::vector<unsigned char> ownerPubKey;
    std::string payoutAddress;
    int registrationHeight;
    int activeUntilHeight;
    int graceUntilHeight;
    CAmount bondAmount;
    uint256 bondTxid;
    uint32_t bondVout;
    uint256 lastUpdateTxid;
    bool released;
    bool bondClaimed;

    NicknameInfo();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nickname);
        READWRITE(ownerPubKey);
        READWRITE(payoutAddress);
        READWRITE(VARINT(registrationHeight));
        READWRITE(VARINT(activeUntilHeight));
        READWRITE(VARINT(graceUntilHeight));
        READWRITE(bondAmount);
        if (ser_action.ForRead()) {
            bondTxid.SetNull();
            bondVout = std::numeric_limits<uint32_t>::max();
            try {
                READWRITE(bondTxid);
                READWRITE(VARINT(bondVout));
            } catch (const std::exception&) {
                bondTxid.SetNull();
                bondVout = std::numeric_limits<uint32_t>::max();
            }
        } else {
            READWRITE(bondTxid);
            READWRITE(VARINT(bondVout));
        }
        if (ser_action.ForRead()) {
            lastUpdateTxid.SetNull();
            released = false;
            bondClaimed = false;
            try {
                READWRITE(lastUpdateTxid);
                READWRITE(released);
                READWRITE(bondClaimed);
            } catch (const std::exception&) {
                lastUpdateTxid.SetNull();
                released = false;
                bondClaimed = false;
            }
        } else {
            READWRITE(lastUpdateTxid);
            READWRITE(released);
            READWRITE(bondClaimed);
        }
    }

    Nicknames::Status GetStatus(int currentHeight) const;
    bool HasBondOutpoint() const;
    COutPoint GetBondOutpoint() const;
};

struct NicknameUndoEntry
{
    std::string nickname;
    bool existedBefore;
    NicknameInfo previousInfo;

    NicknameUndoEntry();

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(nickname);
        READWRITE(existedBefore);
        READWRITE(previousInfo);
    }
};

class NicknameStateDB : public CDBWrapper
{
public:
    NicknameStateDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);

    bool ReadNickname(const std::string& nickname, NicknameInfo& info) const;
    bool WriteNickname(const NicknameInfo& info, bool fSync = false);
    bool EraseNickname(const std::string& nickname, bool fSync = false);
    std::vector<NicknameInfo> ListNicknames(const std::string& startNickname, size_t limit) const;
    std::vector<NicknameInfo> ListNicknamesByOwnerKeyID(const CKeyID& ownerKeyID,
                                                        const std::string& startNickname,
                                                        size_t limit) const;
    std::vector<NicknameInfo> ListNicknamesForOwnerKeyIDs(const std::set<CKeyID>& ownerKeyIDs,
                                                          const std::string& startNickname,
                                                          size_t limit) const;
    bool ReadNicknameUndo(const uint256& blockHash, std::vector<NicknameUndoEntry>& undoEntries) const;
    bool ReadNicknameByBondOutpoint(const COutPoint& outpoint, std::string& nicknameOut) const;
    bool ReadPricingMultiplierPermille(int64_t& multiplierPermilleOut) const;
    bool WriteNicknameBatch(const std::vector<NicknameInfo>& upserts,
                            const std::vector<std::string>& erases,
                            const uint256& blockHash,
                            int blockHeight,
                            uint32_t registeredCount,
                            const std::vector<NicknameUndoEntry>& undoEntries,
                            bool fSync = false);
    bool ApplyNicknameUndo(const uint256& blockHash, int blockHeight, const std::vector<NicknameUndoEntry>& undoEntries, bool fSync = false);
};

bool InitNicknameStateDB(size_t nCacheSize, bool fMemory = false, bool fWipe = false);
void DestroyNicknameStateDB();
NicknameStateDB* GetNicknameStateDB();

#endif // BITCOIN_NICKNAMEDB_H
