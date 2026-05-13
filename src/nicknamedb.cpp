#include "nicknamedb.h"

#include "hash.h"
#include "pubkey.h"
#include "util.h"

#include <algorithm>
#include <limits>
#include <map>

namespace {

static const char DB_NICKNAME = 'n';
static const char DB_NICKNAME_UNDO = 'u';
static const char DB_NICKNAME_BOND_OUTPOINT = 'b';
static const char DB_NICKNAME_OWNER_KEYID = 'o';
static const char DB_NICKNAME_META = 'm';
static const char DB_NICKNAME_PRICING_UNDO = 'p';
static const char DB_NICKNAME_BOND_INDEX_UNDO = 'q';
static const char DB_NICKNAME_REG_COUNT = 'r';
static const std::string OWNER_INDEX_READY_KEY = "owner_index_ready_v1";
static const std::string PRICING_STATE_KEY = "pricing_state_v1";
static NicknameStateDB* g_nicknameStateDB = nullptr;

struct NicknamePricingState
{
    int lastProcessedHeight;
    int64_t rollingRegistrations;
    int64_t multiplierPermille;

    NicknamePricingState() :
        lastProcessedHeight(-1),
        rollingRegistrations(0),
        multiplierPermille(Nicknames::DEFAULT_PRICING_MULTIPLIER_PERMILLE)
    {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(lastProcessedHeight);
        READWRITE(rollingRegistrations);
        READWRITE(multiplierPermille);
    }
};

struct NicknamePricingUndoEntry
{
    bool existedBefore;
    NicknamePricingState previousState;

    NicknamePricingUndoEntry() :
        existedBefore(false)
    {
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream& s, Operation ser_action)
    {
        READWRITE(existedBefore);
        READWRITE(previousState);
    }
};

std::string NormalizeKeyName(const std::string& nickname)
{
    std::string normalized;
    Nicknames::NormalizeNickname(nickname, normalized);
    return normalized;
}

using OwnerIndexKey = std::pair<char, std::pair<CKeyID, std::string>>;

bool TryGetOwnerKeyID(const NicknameInfo& info, CKeyID& ownerKeyIDOut)
{
    if (info.ownerPubKey.size() != CPubKey::COMPRESSED_SIZE) {
        return false;
    }

    ownerKeyIDOut = CKeyID(Hash160(info.ownerPubKey.begin(), info.ownerPubKey.end()));
    return true;
}

void EraseNicknameOwnerIndexEntry(CDBBatch& batch, const NicknameInfo& info, const std::string& normalizedNickname)
{
    CKeyID ownerKeyID;
    if (TryGetOwnerKeyID(info, ownerKeyID)) {
        batch.Erase(std::make_pair(DB_NICKNAME_OWNER_KEYID, std::make_pair(ownerKeyID, normalizedNickname)));
    }
}

void EraseNicknameBondIndexEntry(CDBBatch& batch, const NicknameInfo& info)
{
    if (info.HasBondOutpoint()) {
        batch.Erase(std::make_pair(DB_NICKNAME_BOND_OUTPOINT, info.GetBondOutpoint()));
    }
}

void WriteNicknameIndexEntries(CDBBatch& batch, const NicknameInfo& info, const std::string& normalizedNickname)
{
    if (info.HasBondOutpoint()) {
        batch.Write(std::make_pair(DB_NICKNAME_BOND_OUTPOINT, info.GetBondOutpoint()), normalizedNickname);
    }

    CKeyID ownerKeyID;
    if (TryGetOwnerKeyID(info, ownerKeyID)) {
        batch.Write(std::make_pair(DB_NICKNAME_OWNER_KEYID, std::make_pair(ownerKeyID, normalizedNickname)), true);
    }
}

bool EnsureOwnerIndexReady(NicknameStateDB& db, bool fSync = false)
{
    bool ready = false;
    if (db.Read(std::make_pair(DB_NICKNAME_META, OWNER_INDEX_READY_KEY), ready) && ready) {
        return true;
    }

    CDBBatch batch(db);

    std::unique_ptr<CDBIterator> ownerIt(db.NewIterator());
    ownerIt->Seek(std::make_pair(DB_NICKNAME_OWNER_KEYID, std::make_pair(CKeyID(), std::string())));
    while (ownerIt->Valid()) {
        OwnerIndexKey ownerKey;
        if (!ownerIt->GetKey(ownerKey) || ownerKey.first != DB_NICKNAME_OWNER_KEYID) {
            break;
        }
        batch.Erase(ownerKey);
        ownerIt->Next();
    }

    // Rebuild owner index from current nickname states.
    std::unique_ptr<CDBIterator> it(db.NewIterator());
    it->Seek(std::make_pair(DB_NICKNAME, std::string()));
    while (it->Valid()) {
        std::pair<char, std::string> key;
        if (!it->GetKey(key) || key.first != DB_NICKNAME) {
            break;
        }

        NicknameInfo info;
        if (it->GetValue(info)) {
            CKeyID ownerKeyID;
            if (TryGetOwnerKeyID(info, ownerKeyID)) {
                batch.Write(std::make_pair(DB_NICKNAME_OWNER_KEYID, std::make_pair(ownerKeyID, key.second)), true);
            }
        }
        it->Next();
    }

    batch.Write(std::make_pair(DB_NICKNAME_META, OWNER_INDEX_READY_KEY), true);
    return db.WriteBatch(batch, fSync);
}

bool ReadNicknamePricingState(const NicknameStateDB& db, NicknamePricingState& stateOut)
{
    if (!db.Read(std::make_pair(DB_NICKNAME_META, PRICING_STATE_KEY), stateOut)) {
        stateOut = NicknamePricingState();
        return false;
    }

    stateOut.multiplierPermille = Nicknames::ClampPricingMultiplierPermille(stateOut.multiplierPermille);
    if (stateOut.rollingRegistrations < 0) {
        stateOut.rollingRegistrations = 0;
    }
    return true;
}

uint32_t ReadRegistrationCountAtHeight(const NicknameStateDB& db, const int height)
{
    if (height < 0) {
        return 0;
    }

    uint32_t count = 0;
    if (!db.Read(std::make_pair(DB_NICKNAME_REG_COUNT, height), count)) {
        return 0;
    }
    return count;
}

int64_t RebuildRollingRegistrations(const NicknameStateDB& db, const int tipHeight)
{
    if (tipHeight < 0) {
        return 0;
    }

    const int start = std::max(0, tipHeight - Nicknames::PRICING_WINDOW_BLOCKS + 1);
    int64_t sum = 0;
    for (int height = start; height <= tipHeight; ++height) {
        sum += ReadRegistrationCountAtHeight(db, height);
    }
    return sum;
}

NicknamePricingState ComputeNextPricingState(const NicknameStateDB& db,
                                             const NicknamePricingState& previousState,
                                             const int blockHeight,
                                             const uint32_t registeredCount)
{
    NicknamePricingState nextState = previousState;
    if (nextState.lastProcessedHeight + 1 != blockHeight) {
        nextState.lastProcessedHeight = blockHeight - 1;
        nextState.rollingRegistrations = RebuildRollingRegistrations(db, blockHeight - 1);
    }

    const int droppedHeight = blockHeight - Nicknames::PRICING_WINDOW_BLOCKS;
    const int64_t droppedCount = ReadRegistrationCountAtHeight(db, droppedHeight);

    nextState.rollingRegistrations = std::max<int64_t>(
        0, nextState.rollingRegistrations - droppedCount + static_cast<int64_t>(registeredCount));
    if (blockHeight > 0 && blockHeight % Nicknames::PRICING_EPOCH_BLOCKS == 0) {
        nextState.multiplierPermille = Nicknames::ComputeNextPricingMultiplierPermille(
            nextState.multiplierPermille, nextState.rollingRegistrations);
    }

    nextState.multiplierPermille = Nicknames::ClampPricingMultiplierPermille(nextState.multiplierPermille);
    nextState.lastProcessedHeight = blockHeight;
    return nextState;
}

} // namespace

NicknameInfo::NicknameInfo() :
    registrationHeight(0),
    activeUntilHeight(0),
    graceUntilHeight(0),
    bondAmount(0),
    bondVout(std::numeric_limits<uint32_t>::max()),
    released(false),
    bondClaimed(false)
{
}

NicknameUndoEntry::NicknameUndoEntry() :
    existedBefore(false)
{
}

NicknameBondIndexUndoEntry::NicknameBondIndexUndoEntry() :
    existedBefore(false)
{
}

Nicknames::Status NicknameInfo::GetStatus(int currentHeight) const
{
    return Nicknames::GetStatus(currentHeight, activeUntilHeight, graceUntilHeight, released, bondClaimed);
}

bool NicknameInfo::HasBondOutpoint() const
{
    return !bondClaimed && !bondTxid.IsNull() && bondVout != std::numeric_limits<uint32_t>::max();
}

COutPoint NicknameInfo::GetBondOutpoint() const
{
    if (!HasBondOutpoint()) {
        return COutPoint();
    }
    return COutPoint(bondTxid, bondVout);
}

NicknameStateDB::NicknameStateDB(size_t nCacheSize, bool fMemory, bool fWipe) :
    CDBWrapper(GetDataDir() / "nicknames", nCacheSize, fMemory, fWipe, true)
{
}

bool NicknameStateDB::ReadNickname(const std::string& nickname, NicknameInfo& info) const
{
    return Read(std::make_pair(DB_NICKNAME, NormalizeKeyName(nickname)), info);
}

bool NicknameStateDB::WriteNickname(const NicknameInfo& info, bool fSync)
{
    NicknameInfo normalizedInfo = info;
    normalizedInfo.nickname = NormalizeKeyName(info.nickname);

    CDBBatch batch(*this);

    NicknameInfo currentInfo;
    if (Read(std::make_pair(DB_NICKNAME, normalizedInfo.nickname), currentInfo)) {
        EraseNicknameBondIndexEntry(batch, currentInfo);
        EraseNicknameOwnerIndexEntry(batch, currentInfo, normalizedInfo.nickname);
    }

    batch.Write(std::make_pair(DB_NICKNAME, normalizedInfo.nickname), normalizedInfo);
    WriteNicknameIndexEntries(batch, normalizedInfo, normalizedInfo.nickname);

    return WriteBatch(batch, fSync);
}

bool NicknameStateDB::EraseNickname(const std::string& nickname, bool fSync)
{
    const std::string normalized = NormalizeKeyName(nickname);
    CDBBatch batch(*this);

    NicknameInfo currentInfo;
    if (Read(std::make_pair(DB_NICKNAME, normalized), currentInfo)) {
        EraseNicknameBondIndexEntry(batch, currentInfo);
        EraseNicknameOwnerIndexEntry(batch, currentInfo, normalized);
    }

    batch.Erase(std::make_pair(DB_NICKNAME, normalized));
    return WriteBatch(batch, fSync);
}

std::vector<NicknameInfo> NicknameStateDB::ListNicknames(const std::string& startNickname, size_t limit) const
{
    std::vector<NicknameInfo> result;
    std::unique_ptr<CDBIterator> it(const_cast<NicknameStateDB*>(this)->NewIterator());
    it->Seek(std::make_pair(DB_NICKNAME, NormalizeKeyName(startNickname)));

    while (it->Valid() && result.size() < limit) {
        std::pair<char, std::string> key;
        if (!it->GetKey(key) || key.first != DB_NICKNAME) {
            break;
        }

        NicknameInfo info;
        if (it->GetValue(info)) {
            result.push_back(info);
        }
        it->Next();
    }

    return result;
}

std::vector<NicknameInfo> NicknameStateDB::ListNicknamesByOwnerKeyID(const CKeyID& ownerKeyID,
                                                                     const std::string& startNickname,
                                                                     size_t limit) const
{
    std::vector<NicknameInfo> result;
    if (limit == 0) {
        return result;
    }

    EnsureOwnerIndexReady(*const_cast<NicknameStateDB*>(this));

    const std::string normalizedStart = NormalizeKeyName(startNickname);
    std::unique_ptr<CDBIterator> it(const_cast<NicknameStateDB*>(this)->NewIterator());
    it->Seek(std::make_pair(DB_NICKNAME_OWNER_KEYID, std::make_pair(ownerKeyID, normalizedStart)));

    while (it->Valid() && result.size() < limit) {
        OwnerIndexKey key;
        if (!it->GetKey(key) || key.first != DB_NICKNAME_OWNER_KEYID || key.second.first != ownerKeyID) {
            break;
        }

        NicknameInfo info;
        if (Read(std::make_pair(DB_NICKNAME, key.second.second), info)) {
            result.push_back(info);
        }
        it->Next();
    }

    return result;
}

std::vector<NicknameInfo> NicknameStateDB::ListNicknamesForOwnerKeyIDs(const std::set<CKeyID>& ownerKeyIDs,
                                                                       const std::string& startNickname,
                                                                       size_t limit) const
{
    std::vector<NicknameInfo> result;
    if (limit == 0 || ownerKeyIDs.empty()) {
        return result;
    }

    EnsureOwnerIndexReady(*const_cast<NicknameStateDB*>(this));

    const std::string normalizedStart = NormalizeKeyName(startNickname);
    std::map<std::string, NicknameInfo> dedup;
    for (const CKeyID& ownerKeyID : ownerKeyIDs) {
        std::unique_ptr<CDBIterator> it(const_cast<NicknameStateDB*>(this)->NewIterator());
        it->Seek(std::make_pair(DB_NICKNAME_OWNER_KEYID, std::make_pair(ownerKeyID, normalizedStart)));

        while (it->Valid()) {
            OwnerIndexKey key;
            if (!it->GetKey(key) || key.first != DB_NICKNAME_OWNER_KEYID || key.second.first != ownerKeyID) {
                break;
            }

            NicknameInfo info;
            if (Read(std::make_pair(DB_NICKNAME, key.second.second), info)) {
                dedup[NormalizeKeyName(info.nickname)] = info;
            }
            it->Next();
        }
    }

    for (const auto& item : dedup) {
        if (result.size() >= limit) {
            break;
        }
        result.push_back(item.second);
    }

    return result;
}

bool NicknameStateDB::ReadNicknameUndo(const uint256& blockHash, std::vector<NicknameUndoEntry>& undoEntries) const
{
    return Read(std::make_pair(DB_NICKNAME_UNDO, blockHash), undoEntries);
}

bool NicknameStateDB::ReadNicknameBondIndexUndo(const uint256& blockHash, std::vector<NicknameBondIndexUndoEntry>& undoEntries) const
{
    return Read(std::make_pair(DB_NICKNAME_BOND_INDEX_UNDO, blockHash), undoEntries);
}

bool NicknameStateDB::ReadNicknameByBondOutpoint(const COutPoint& outpoint, std::string& nicknameOut) const
{
    return Read(std::make_pair(DB_NICKNAME_BOND_OUTPOINT, outpoint), nicknameOut);
}

bool NicknameStateDB::ReadPricingMultiplierPermille(int64_t& multiplierPermilleOut) const
{
    NicknamePricingState state;
    const bool found = ReadNicknamePricingState(*this, state);
    multiplierPermilleOut = state.multiplierPermille;
    return found;
}

bool NicknameStateDB::WriteNicknameBatch(const std::vector<NicknameInfo>& upserts,
                                         const std::vector<std::string>& erases,
                                         const std::vector<std::pair<COutPoint, std::string> >& bondIndexUpserts,
                                         const std::vector<COutPoint>& bondIndexErases,
                                         const uint256& blockHash,
                                         int blockHeight,
                                         uint32_t registeredCount,
                                         const std::vector<NicknameUndoEntry>& undoEntries,
                                         bool fSync)
{
    CDBBatch batch(*this);

    for (const NicknameInfo& info : upserts) {
        NicknameInfo normalizedInfo = info;
        normalizedInfo.nickname = NormalizeKeyName(info.nickname);

        NicknameInfo currentInfo;
        if (Read(std::make_pair(DB_NICKNAME, normalizedInfo.nickname), currentInfo)) {
            EraseNicknameBondIndexEntry(batch, currentInfo);
            EraseNicknameOwnerIndexEntry(batch, currentInfo, normalizedInfo.nickname);
        }

        batch.Write(std::make_pair(DB_NICKNAME, normalizedInfo.nickname), normalizedInfo);
        WriteNicknameIndexEntries(batch, normalizedInfo, normalizedInfo.nickname);
    }

    for (const std::string& nickname : erases) {
        const std::string normalized = NormalizeKeyName(nickname);
        NicknameInfo currentInfo;
        if (Read(std::make_pair(DB_NICKNAME, normalized), currentInfo)) {
            EraseNicknameBondIndexEntry(batch, currentInfo);
            EraseNicknameOwnerIndexEntry(batch, currentInfo, normalized);
        }
        batch.Erase(std::make_pair(DB_NICKNAME, normalized));
    }

    batch.Write(std::make_pair(DB_NICKNAME_UNDO, blockHash), undoEntries);

    std::map<COutPoint, NicknameBondIndexUndoEntry> bondIndexUndoMap;
    auto rememberBondIndexUndo = [&](const COutPoint& outpoint) {
        if (bondIndexUndoMap.count(outpoint) > 0) {
            return;
        }
        NicknameBondIndexUndoEntry undo;
        undo.outpoint = outpoint;
        undo.existedBefore = Read(std::make_pair(DB_NICKNAME_BOND_OUTPOINT, outpoint), undo.previousNickname);
        bondIndexUndoMap.emplace(outpoint, undo);
    };

    for (const COutPoint& outpoint : bondIndexErases) {
        rememberBondIndexUndo(outpoint);
        batch.Erase(std::make_pair(DB_NICKNAME_BOND_OUTPOINT, outpoint));
    }

    for (const std::pair<COutPoint, std::string>& item : bondIndexUpserts) {
        rememberBondIndexUndo(item.first);
        batch.Write(std::make_pair(DB_NICKNAME_BOND_OUTPOINT, item.first), NormalizeKeyName(item.second));
    }

    std::vector<NicknameBondIndexUndoEntry> bondIndexUndoEntries;
    bondIndexUndoEntries.reserve(bondIndexUndoMap.size());
    for (const auto& item : bondIndexUndoMap) {
        bondIndexUndoEntries.push_back(item.second);
    }
    batch.Write(std::make_pair(DB_NICKNAME_BOND_INDEX_UNDO, blockHash), bondIndexUndoEntries);

    NicknamePricingState previousPricingState;
    const bool hadPreviousPricingState = ReadNicknamePricingState(*this, previousPricingState);

    NicknamePricingUndoEntry pricingUndoEntry;
    pricingUndoEntry.existedBefore = hadPreviousPricingState;
    if (hadPreviousPricingState) {
        pricingUndoEntry.previousState = previousPricingState;
    }
    batch.Write(std::make_pair(DB_NICKNAME_PRICING_UNDO, blockHash), pricingUndoEntry);
    batch.Write(std::make_pair(DB_NICKNAME_REG_COUNT, blockHeight), registeredCount);

    const NicknamePricingState nextPricingState = ComputeNextPricingState(
        *this, previousPricingState, blockHeight, registeredCount);
    batch.Write(std::make_pair(DB_NICKNAME_META, PRICING_STATE_KEY), nextPricingState);

    return WriteBatch(batch, fSync);
}

bool NicknameStateDB::ApplyNicknameUndo(const uint256& blockHash, int blockHeight, const std::vector<NicknameUndoEntry>& undoEntries, bool fSync)
{
    CDBBatch batch(*this);

    // Erase index entries for current states before restoring previous ones.
    for (const NicknameUndoEntry& undo : undoEntries) {
        const std::string normalized = NormalizeKeyName(undo.nickname);
        NicknameInfo currentInfo;
        if (Read(std::make_pair(DB_NICKNAME, normalized), currentInfo)) {
            EraseNicknameBondIndexEntry(batch, currentInfo);
            EraseNicknameOwnerIndexEntry(batch, currentInfo, normalized);
        }
    }

    for (auto it = undoEntries.rbegin(); it != undoEntries.rend(); ++it) {
        const std::string normalized = NormalizeKeyName(it->nickname);
        if (it->existedBefore) {
            NicknameInfo restored = it->previousInfo;
            restored.nickname = normalized;
            batch.Write(std::make_pair(DB_NICKNAME, normalized), restored);
            WriteNicknameIndexEntries(batch, restored, normalized);
        } else {
            batch.Erase(std::make_pair(DB_NICKNAME, normalized));
        }
    }

    batch.Erase(std::make_pair(DB_NICKNAME_UNDO, blockHash));
    batch.Erase(std::make_pair(DB_NICKNAME_REG_COUNT, blockHeight));

    std::vector<NicknameBondIndexUndoEntry> bondIndexUndoEntries;
    if (ReadNicknameBondIndexUndo(blockHash, bondIndexUndoEntries)) {
        for (const NicknameBondIndexUndoEntry& undo : bondIndexUndoEntries) {
            if (undo.existedBefore) {
                batch.Write(std::make_pair(DB_NICKNAME_BOND_OUTPOINT, undo.outpoint), undo.previousNickname);
            } else {
                batch.Erase(std::make_pair(DB_NICKNAME_BOND_OUTPOINT, undo.outpoint));
            }
        }
        batch.Erase(std::make_pair(DB_NICKNAME_BOND_INDEX_UNDO, blockHash));
    }

    NicknamePricingUndoEntry pricingUndoEntry;
    if (Read(std::make_pair(DB_NICKNAME_PRICING_UNDO, blockHash), pricingUndoEntry)) {
        if (pricingUndoEntry.existedBefore) {
            batch.Write(std::make_pair(DB_NICKNAME_META, PRICING_STATE_KEY), pricingUndoEntry.previousState);
        } else {
            batch.Erase(std::make_pair(DB_NICKNAME_META, PRICING_STATE_KEY));
        }
        batch.Erase(std::make_pair(DB_NICKNAME_PRICING_UNDO, blockHash));
    }

    return WriteBatch(batch, fSync);
}

bool InitNicknameStateDB(size_t nCacheSize, bool fMemory, bool fWipe)
{
    DestroyNicknameStateDB();
    g_nicknameStateDB = new NicknameStateDB(nCacheSize, fMemory, fWipe);
    return true;
}

void DestroyNicknameStateDB()
{
    delete g_nicknameStateDB;
    g_nicknameStateDB = nullptr;
}

NicknameStateDB* GetNicknameStateDB()
{
    return g_nicknameStateDB;
}
