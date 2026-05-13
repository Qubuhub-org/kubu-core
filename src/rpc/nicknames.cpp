#include "nicknamedb.h"
#include "nicknameop.h"
#include "net.h"
#include "nicknames.h"
#include "pubkey.h"
#include "rpc/server.h"
#include "script/standard.h"
#include "utilstrencodings.h"
#include "validation.h"

#ifdef ENABLE_WALLET
#include "wallet/coincontrol.h"
#include "wallet/wallet.h"
#endif

#include <univalue.h>
#include <algorithm>
#include <set>

namespace {

#ifdef ENABLE_WALLET
bool EnsureNicknameWalletIsAvailable(bool avoidException)
{
    if (!pwalletMain) {
        if (!avoidException) {
            throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (wallet support is disabled)");
        }
        return false;
    }
    return true;
}

void EnsureNicknameWalletIsUnlocked()
{
    if (pwalletMain->IsLocked()) {
        throw JSONRPCError(RPC_WALLET_UNLOCK_NEEDED, "Error: Please enter the wallet passphrase with walletpassphrase first.");
    }
}

CPubKey DecodeCompressedPubKeyHex(const std::string& pubKeyHex)
{
    if (!IsHex(pubKeyHex)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Owner pubkey must be hex");
    }

    const std::vector<unsigned char> pubKeyBytes = ParseHex(pubKeyHex);
    if (pubKeyBytes.size() != CPubKey::COMPRESSED_SIZE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Owner pubkey must be 33-byte compressed pubkey");
    }

    const CPubKey pubKey(pubKeyBytes);
    if (!pubKey.IsFullyValid() || !pubKey.IsCompressed()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid compressed pubkey");
    }

    return pubKey;
}

CPubKey DecodeOwnedPubKeyHex(const std::string& pubKeyHex)
{
    const CPubKey pubKey = DecodeCompressedPubKeyHex(pubKeyHex);
    if (!pwalletMain->HaveKey(pubKey.GetID())) {
        throw JSONRPCError(RPC_WALLET_ERROR, "Wallet does not have private key for owner pubkey");
    }
    return pubKey;
}

bool SelectOwnerInput(const CPubKey& ownerPubKey, CCoinControl& coinControl, const std::set<COutPoint>& forbiddenInputs)
{
    const CScript ownerP2PK = GetScriptForRawPubKey(ownerPubKey);
    const CScript ownerP2PKH = GetScriptForDestination(ownerPubKey.GetID());
    const CScript ownerP2WPKH = GetScriptForWitness(ownerP2PKH);
    const CScript ownerP2SHP2WPKH = GetScriptForDestination(CScriptID(ownerP2WPKH));

    std::vector<COutput> availableCoins;
    pwalletMain->AvailableCoins(availableCoins, true);
    for (const COutput& output : availableCoins) {
        if (!output.fSpendable) {
            continue;
        }

        const COutPoint outpoint(output.tx->GetHash(), output.i);
        if (forbiddenInputs.count(outpoint) > 0) {
            continue;
        }

        const CScript& scriptPubKey = output.tx->tx->vout[output.i].scriptPubKey;
        if (scriptPubKey != ownerP2PK &&
            scriptPubKey != ownerP2PKH &&
            scriptPubKey != ownerP2WPKH &&
            scriptPubKey != ownerP2SHP2WPKH) {
            continue;
        }

        coinControl.Select(outpoint);
        return true;
    }

    return false;
}

std::set<COutPoint> CollectSpendableTrackedBondOutpoints()
{
    std::set<COutPoint> result;
    NicknameStateDB* nicknameDB = GetNicknameStateDB();
    if (!nicknameDB) {
        return result;
    }

    std::vector<COutput> availableCoins;
    pwalletMain->AvailableCoins(availableCoins, true);
    for (const COutput& output : availableCoins) {
        if (!output.fSpendable) {
            continue;
        }

        const COutPoint outpoint(output.tx->GetHash(), output.i);
        std::string nickname;
        if (nicknameDB->ReadNicknameByBondOutpoint(outpoint, nickname)) {
            result.insert(outpoint);
        }
    }

    return result;
}

void CollectWalletTrackedBondOutpoints(std::vector<COutPoint>& outpointsOut)
{
    outpointsOut.clear();

    NicknameStateDB* nicknameDB = GetNicknameStateDB();
    if (!nicknameDB) {
        return;
    }

    std::set<COutPoint> dedup;

    std::vector<COutput> availableCoins;
    pwalletMain->AvailableCoins(availableCoins, true);
    for (const COutput& output : availableCoins) {
        if (!output.fSpendable) {
            continue;
        }

        const COutPoint outpoint(output.tx->GetHash(), output.i);
        std::string nickname;
        if (!nicknameDB->ReadNicknameByBondOutpoint(outpoint, nickname)) {
            continue;
        }
        dedup.insert(outpoint);
    }

    std::vector<COutPoint> lockedOutpoints;
    pwalletMain->ListLockedCoins(lockedOutpoints);
    for (const COutPoint& outpoint : lockedOutpoints) {
        if (!pwalletMain->mapWallet.count(outpoint.hash)) {
            continue;
        }
        if (outpoint.n >= pwalletMain->mapWallet[outpoint.hash].tx->vout.size()) {
            continue;
        }
        if (pwalletMain->IsSpent(outpoint.hash, outpoint.n)) {
            continue;
        }

        std::string nickname;
        if (!nicknameDB->ReadNicknameByBondOutpoint(outpoint, nickname)) {
            continue;
        }
        dedup.insert(outpoint);
    }

    outpointsOut.assign(dedup.begin(), dedup.end());
}

CAmount GetWalletOutpointAmount(const COutPoint& outpoint)
{
    if (!pwalletMain->mapWallet.count(outpoint.hash)) {
        return 0;
    }
    const CWalletTx& walletTx = pwalletMain->mapWallet[outpoint.hash];
    if (outpoint.n >= walletTx.tx->vout.size()) {
        return 0;
    }
    if (pwalletMain->IsSpent(outpoint.hash, outpoint.n)) {
        return 0;
    }
    return walletTx.tx->vout[outpoint.n].nValue;
}

bool TryAbandonInactiveWalletSpenders(const COutPoint& outpoint)
{
    bool abandonedAny = false;
    const std::set<uint256> spenders = pwalletMain->GetSpendsForOutpoint(outpoint);
    for (const uint256& txid : spenders) {
        const CWalletTx* spender = pwalletMain->GetWalletTx(txid);
        if (!spender) {
            continue;
        }
        if (spender->GetDepthInMainChain() != 0 || spender->InMempool() || spender->isAbandoned()) {
            continue;
        }
        abandonedAny = pwalletMain->AbandonTransaction(txid) || abandonedAny;
    }
    return abandonedAny;
}

class ScopedCoinLocker
{
public:
    explicit ScopedCoinLocker(CWallet* walletIn) : wallet(walletIn) {}

    ~ScopedCoinLocker()
    {
        for (const COutPoint& outpoint : lockedByScope) {
            wallet->UnlockCoin(outpoint);
        }
    }

    void LockIfUnlocked(const COutPoint& outpoint)
    {
        if (wallet->IsLockedCoin(outpoint.hash, outpoint.n)) {
            return;
        }
        wallet->LockCoin(outpoint);
        lockedByScope.push_back(outpoint);
    }

private:
    CWallet* wallet;
    std::vector<COutPoint> lockedByScope;
};

class ScopedCoinRelocker
{
public:
    explicit ScopedCoinRelocker(CWallet* walletIn) : wallet(walletIn), active(true) {}

    ~ScopedCoinRelocker()
    {
        if (!active) {
            return;
        }
        for (const COutPoint& outpoint : unlockedByScope) {
            if (!wallet->IsLockedCoin(outpoint.hash, outpoint.n)) {
                wallet->LockCoin(outpoint);
            }
        }
    }

    void RememberUnlocked(const COutPoint& outpoint)
    {
        unlockedByScope.push_back(outpoint);
    }

    void Release()
    {
        active = false;
    }

private:
    CWallet* wallet;
    std::vector<COutPoint> unlockedByScope;
    bool active;
};

void LockSpendableTrackedBondOutpoints()
{
    const std::set<COutPoint> spendableBondOutpoints = CollectSpendableTrackedBondOutpoints();
    for (const COutPoint& outpoint : spendableBondOutpoints) {
        if (!pwalletMain->IsLockedCoin(outpoint.hash, outpoint.n)) {
            pwalletMain->LockCoin(outpoint);
        }
    }
}

CAmount GetRequiredNicknameFeeForRPC(const NicknameOperation& operation)
{
    int64_t pricingMultiplierPermille = Nicknames::DEFAULT_PRICING_MULTIPLIER_PERMILLE;
    NicknameStateDB* nicknameDB = GetNicknameStateDB();
    if (nicknameDB) {
        nicknameDB->ReadPricingMultiplierPermille(pricingMultiplierPermille);
    }

    switch (operation.type) {
    case NicknameOpType::REGISTER:
        return Nicknames::GetPricing(operation.nickname.size(), pricingMultiplierPermille).registrationFee;
    case NicknameOpType::RENEW:
        // Renewal cost is moved into additional locked bond.
        return 0;
    case NicknameOpType::UPDATE:
    case NicknameOpType::TRANSFER:
    case NicknameOpType::RELEASE:
    case NicknameOpType::CLAIM_BOND:
    case NicknameOpType::NONE:
        return 0;
    }

    return 0;
}

const char* NicknameOperationTypeToLabel(const NicknameOpType type)
{
    switch (type) {
    case NicknameOpType::REGISTER:
        return "register";
    case NicknameOpType::UPDATE:
        return "update";
    case NicknameOpType::TRANSFER:
        return "transfer";
    case NicknameOpType::RENEW:
        return "renew";
    case NicknameOpType::RELEASE:
        return "release";
    case NicknameOpType::CLAIM_BOND:
        return "claim_bond";
    case NicknameOpType::NONE:
        return "";
    }

    return "";
}

bool IsMutableNicknameStatus(const Nicknames::Status status)
{
    return status == Nicknames::Status::ACTIVE || status == Nicknames::Status::EXPIRED_GRACE;
}

bool ParseBondReference(const std::string& input, COutPoint& outpointOut)
{
    const size_t separator = input.find(':');
    if (separator == std::string::npos || separator == 0 || separator + 1 >= input.size()) {
        return false;
    }

    const std::string txidHex = input.substr(0, separator);
    const std::string voutStr = input.substr(separator + 1);
    if (txidHex.size() != 64 || !IsHex(txidHex)) {
        return false;
    }

    uint32_t vout = 0;
    if (!ParseUInt32(voutStr, &vout)) {
        return false;
    }

    outpointOut = COutPoint(uint256S(txidHex), vout);
    return true;
}

std::string SendNicknameOperationTx(const NicknameOperation& operation,
                                    const CPubKey& ownerPubKey,
                                    const NicknameInfo* currentInfo = nullptr,
                                    const COutPoint* explicitClaimOutpoint = nullptr)
{
    if (pwalletMain->GetBroadcastTransactions() && !g_connman) {
        throw JSONRPCError(RPC_CLIENT_P2P_DISABLED, "Error: Peer-to-peer functionality missing or disabled");
    }

    CScript opScript;
    std::string encodeReason;
    if (!BuildNicknameOpScript(operation, opScript, &encodeReason)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, strprintf("Failed to encode nickname operation: %s", encodeReason));
    }

    std::vector<CRecipient> vecSend;
    vecSend.push_back({opScript, 0, false});

    int64_t pricingMultiplierPermille = Nicknames::DEFAULT_PRICING_MULTIPLIER_PERMILLE;
    NicknameStateDB* nicknameDB = GetNicknameStateDB();
    if (nicknameDB) {
        nicknameDB->ReadPricingMultiplierPermille(pricingMultiplierPermille);
    }

    std::set<COutPoint> requiredInputs;
    bool lockCreatedBondOutput = false;
    CScript createdBondScript;
    CAmount createdBondAmount = 0;
    switch (operation.type) {
    case NicknameOpType::REGISTER: {
        const Nicknames::Pricing pricing = Nicknames::GetPricing(operation.nickname.size(), pricingMultiplierPermille);
        createdBondScript = GetScriptForRawPubKey(ownerPubKey);
        createdBondAmount = pricing.bondAmount;
        lockCreatedBondOutput = true;
        vecSend.push_back({createdBondScript, createdBondAmount, false});
        break;
    }
    case NicknameOpType::TRANSFER: {
        if (!currentInfo || !currentInfo->HasBondOutpoint()) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Indexed nickname has no active bond UTXO");
        }
        if (currentInfo->bondAmount <= 0) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Indexed nickname bond amount is invalid");
        }
        if (operation.newOwnerPubKey.size() != CPubKey::COMPRESSED_SIZE) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "New owner pubkey must be 33-byte compressed pubkey");
        }

        const CPubKey newOwnerPubKey(operation.newOwnerPubKey);
        if (!newOwnerPubKey.IsFullyValid() || !newOwnerPubKey.IsCompressed()) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "Invalid new owner compressed pubkey");
        }

        requiredInputs.insert(currentInfo->GetBondOutpoint());
        createdBondScript = GetScriptForRawPubKey(newOwnerPubKey);
        createdBondAmount = currentInfo->bondAmount;
        lockCreatedBondOutput = true;
        vecSend.push_back({createdBondScript, createdBondAmount, false});
        break;
    }
    case NicknameOpType::RENEW: {
        if (!currentInfo || !currentInfo->HasBondOutpoint()) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Indexed nickname has no active bond UTXO");
        }
        if (currentInfo->bondAmount <= 0) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Indexed nickname bond amount is invalid");
        }
        const CAmount renewalBondIncrease = Nicknames::GetRenewalBondIncrease(operation.nickname.size(), pricingMultiplierPermille);
        if (renewalBondIncrease < 0 || currentInfo->bondAmount > MAX_MONEY - renewalBondIncrease) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Renewed bond amount overflow");
        }

        requiredInputs.insert(currentInfo->GetBondOutpoint());
        createdBondScript = GetScriptForRawPubKey(ownerPubKey);
        createdBondAmount = currentInfo->bondAmount + renewalBondIncrease;
        lockCreatedBondOutput = true;
        vecSend.push_back({createdBondScript, createdBondAmount, false});
        break;
    }
    case NicknameOpType::CLAIM_BOND:
        if (explicitClaimOutpoint) {
            requiredInputs.insert(*explicitClaimOutpoint);
            break;
        }
        if (!currentInfo || !currentInfo->HasBondOutpoint()) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Indexed nickname has no claimable bond UTXO");
        }
        requiredInputs.insert(currentInfo->GetBondOutpoint());
        break;
    case NicknameOpType::UPDATE:
    case NicknameOpType::RELEASE:
    case NicknameOpType::NONE:
        break;
    }

    ScopedCoinRelocker relockRequiredBonds(pwalletMain);
    for (const COutPoint& required : requiredInputs) {
        if (pwalletMain->IsLockedCoin(required.hash, required.n)) {
            pwalletMain->UnlockCoin(required);
            relockRequiredBonds.RememberUnlocked(required);
        }
    }

    std::set<COutPoint> spendableBondOutpoints = CollectSpendableTrackedBondOutpoints();
    std::vector<COutPoint> missingRequiredBondOutpoints;
    for (const COutPoint& required : requiredInputs) {
        if (spendableBondOutpoints.count(required) == 0) {
            missingRequiredBondOutpoints.push_back(required);
        }
    }

    if (!missingRequiredBondOutpoints.empty()) {
        bool abandonedAny = false;
        for (const COutPoint& missingOutpoint : missingRequiredBondOutpoints) {
            abandonedAny = TryAbandonInactiveWalletSpenders(missingOutpoint) || abandonedAny;
        }
        if (abandonedAny) {
            spendableBondOutpoints = CollectSpendableTrackedBondOutpoints();
        }
    }

    for (const COutPoint& required : requiredInputs) {
        if (spendableBondOutpoints.count(required) == 0) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Required nickname bond UTXO is not spendable by this wallet");
        }
    }

    std::set<COutPoint> forbiddenBondOutpoints = spendableBondOutpoints;
    for (const COutPoint& required : requiredInputs) {
        forbiddenBondOutpoints.erase(required);
    }

    CCoinControl coinControl;
    coinControl.fAllowOtherInputs = true;
    coinControl.nMinimumTotalFee = GetRequiredNicknameFeeForRPC(operation);
    for (const COutPoint& required : requiredInputs) {
        coinControl.Select(required);
    }

    if (operation.type != NicknameOpType::TRANSFER &&
        operation.type != NicknameOpType::RENEW &&
        operation.type != NicknameOpType::CLAIM_BOND) {
        if (!SelectOwnerInput(ownerPubKey, coinControl, forbiddenBondOutpoints)) {
            throw JSONRPCError(RPC_WALLET_ERROR, "No spendable wallet UTXO found for nickname owner pubkey");
        }
    }

    ScopedCoinLocker lockForbiddenBonds(pwalletMain);
    for (const COutPoint& forbidden : forbiddenBondOutpoints) {
        lockForbiddenBonds.LockIfUnlocked(forbidden);
    }

    CWalletTx wtxNew;
    CReserveKey reservekey(pwalletMain);
    CAmount nFeeRequired = 0;
    int nChangePosRet = -1;
    std::string createError;
    if (!pwalletMain->CreateTransaction(vecSend, wtxNew, reservekey, nFeeRequired, nChangePosRet, createError, &coinControl)) {
        throw JSONRPCError(RPC_WALLET_ERROR, createError);
    }

    for (const CTxIn& txin : wtxNew.tx->vin) {
        if (forbiddenBondOutpoints.count(txin.prevout) > 0) {
            throw JSONRPCError(RPC_WALLET_ERROR, "Wallet selected a protected nickname bond input; retry with different UTXOs");
        }
    }

    CValidationState state;
    const char* operationLabel = NicknameOperationTypeToLabel(operation.type);
    if (operationLabel[0] != '\0') {
        wtxNew.mapValue["nickname_op"] = operationLabel;
    }
    if (!operation.nickname.empty()) {
        wtxNew.mapValue["nickname"] = operation.nickname;
    }
    if (!pwalletMain->CommitTransaction(wtxNew, reservekey, g_connman.get(), state)) {
        throw JSONRPCError(RPC_WALLET_ERROR, strprintf("Transaction rejected: %s", state.GetRejectReason()));
    }
    if (!state.IsValid()) {
        const uint256 txid = wtxNew.GetHash();
        const CWalletTx* committedTx = pwalletMain->GetWalletTx(txid);
        if (committedTx &&
            committedTx->GetDepthInMainChain() == 0 &&
            !committedTx->InMempool() &&
            !committedTx->isAbandoned()) {
            pwalletMain->AbandonTransaction(txid);
        }
        const std::string rejectReason = state.GetRejectReason().empty() ? "unknown reason" : state.GetRejectReason();
        throw JSONRPCError(RPC_WALLET_ERROR, strprintf("Transaction rejected: %s", rejectReason));
    }

    if (lockCreatedBondOutput) {
        for (size_t vout = 0; vout < wtxNew.tx->vout.size(); ++vout) {
            const CTxOut& out = wtxNew.tx->vout[vout];
            if (out.nValue != createdBondAmount || out.scriptPubKey != createdBondScript) {
                continue;
            }
            const COutPoint bondOutpoint(wtxNew.GetHash(), static_cast<uint32_t>(vout));
            if (!pwalletMain->IsLockedCoin(bondOutpoint.hash, bondOutpoint.n)) {
                pwalletMain->LockCoin(bondOutpoint);
            }
            break;
        }
    }

    // Keep tracked nickname bond UTXOs out of generic wallet coin selection.
    LockSpendableTrackedBondOutpoints();
    relockRequiredBonds.Release();

    return wtxNew.GetHash().GetHex();
}
#endif

void EnsureNicknameDB()
{
    if (!GetNicknameStateDB()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Nickname index is unavailable");
    }
}

UniValue NicknameInfoToJSON(const NicknameInfo& info)
{
    UniValue result(UniValue::VOBJ);
    const int currentHeight = chainActive.Height();
    const Nicknames::Status status = info.GetStatus(currentHeight);

    result.pushKV("nickname", info.nickname);
    result.pushKV("status", Nicknames::StatusToString(status));
    result.pushKV("payout_address", info.payoutAddress);
    result.pushKV("owner_pubkey", HexStr(info.ownerPubKey));
    result.pushKV("registration_height", info.registrationHeight);
    result.pushKV("active_until", info.activeUntilHeight);
    result.pushKV("grace_until", info.graceUntilHeight);
    result.pushKV("bond_amount", ValueFromAmount(info.bondAmount));
    if (info.HasBondOutpoint()) {
        result.pushKV("bond_txid", info.bondTxid.GetHex());
        result.pushKV("bond_vout", static_cast<int64_t>(info.bondVout));
    }
    result.pushKV("last_update_txid", info.lastUpdateTxid.GetHex());
    result.pushKV("released", info.released);
    result.pushKV("bond_claimed", info.bondClaimed);
    result.pushKV("claimable_bond", status == Nicknames::Status::BOND_CLAIMABLE);

    return result;
}

UniValue LegacyBondToJSON(const NicknameInfo& currentInfo, const COutPoint& bondOutpoint, const CAmount bondAmount)
{
    UniValue result(UniValue::VOBJ);
    result.pushKV("nickname", currentInfo.nickname);
    result.pushKV("status", Nicknames::StatusToString(Nicknames::Status::BOND_CLAIMABLE));
    result.pushKV("payout_address", currentInfo.payoutAddress);
    result.pushKV("owner_pubkey", HexStr(currentInfo.ownerPubKey));
    result.pushKV("registration_height", currentInfo.registrationHeight);
    result.pushKV("active_until", currentInfo.activeUntilHeight);
    result.pushKV("grace_until", currentInfo.graceUntilHeight);
    result.pushKV("bond_amount", ValueFromAmount(bondAmount));
    result.pushKV("bond_txid", bondOutpoint.hash.GetHex());
    result.pushKV("bond_vout", static_cast<int64_t>(bondOutpoint.n));
    result.pushKV("last_update_txid", currentInfo.lastUpdateTxid.GetHex());
    result.pushKV("released", currentInfo.released);
    result.pushKV("bond_claimed", false);
    result.pushKV("claimable_bond", true);
    result.pushKV("legacy_bond", true);
    return result;
}

UniValue checknickname(const JSONRPCRequest& request)
{
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "checknickname \"name\"\n"
            "\nValidate a nickname candidate and return the current KUBU pricing tier.\n"
            "\nArguments:\n"
            "1. \"name\"    (string, required) Nickname candidate to validate\n"
            "\nResult:\n"
            "{\n"
            "  \"input\": \"name\",\n"
            "  \"normalized\": \"name\",\n"
            "  \"valid\": true|false,\n"
            "  \"reason\": \"...\",\n"
            "  \"pricing_multiplier_permille\": n,\n"
            "  \"registration_fee\": n,\n"
            "  \"bond_amount\": n,\n"
            "  \"renewal_fee\": n,\n"
            "  \"renewal_bond_increase\": n,\n"
            "  \"active_blocks\": n,\n"
            "  \"grace_blocks\": n\n"
            "}\n"
            "\nExamples:\n"
            + HelpExampleCli("checknickname", "\"kubu_dev\"")
            + HelpExampleRpc("checknickname", "\"kubu_dev\"")
        );
    }

    const std::string input = request.params[0].get_str();
    std::string normalized;
    Nicknames::NormalizeNickname(input, normalized);

    std::string reason;
    const bool valid = Nicknames::IsValidNormalizedNickname(normalized, &reason);
    int64_t pricingMultiplierPermille = Nicknames::DEFAULT_PRICING_MULTIPLIER_PERMILLE;
    NicknameStateDB* nicknameDB = GetNicknameStateDB();
    if (nicknameDB) {
        nicknameDB->ReadPricingMultiplierPermille(pricingMultiplierPermille);
    }
    const Nicknames::Pricing pricing = Nicknames::GetPricing(normalized.size(), pricingMultiplierPermille);

    UniValue result(UniValue::VOBJ);
    result.pushKV("input", input);
    result.pushKV("normalized", normalized);
    result.pushKV("valid", valid);
    if (!valid) {
        result.pushKV("reason", reason);
    }
    result.pushKV("pricing_multiplier_permille", pricingMultiplierPermille);
    result.pushKV("registration_fee", ValueFromAmount(pricing.registrationFee));
    result.pushKV("bond_amount", ValueFromAmount(pricing.bondAmount));
    result.pushKV("renewal_fee", ValueFromAmount(Nicknames::GetRenewalFee(normalized.size(), pricingMultiplierPermille)));
    result.pushKV("renewal_bond_increase", ValueFromAmount(Nicknames::GetRenewalBondIncrease(normalized.size(), pricingMultiplierPermille)));
    result.pushKV("active_blocks", Nicknames::ActiveBlocks());
    result.pushKV("grace_blocks", Nicknames::GraceBlocks());
    return result;
}

UniValue getnicknameinfo(const JSONRPCRequest& request)
{
    EnsureNicknameDB();
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "getnicknameinfo \"name\"\n"
            "\nReturn the indexed state for a nickname.\n"
            "\nArguments:\n"
            "1. \"name\"    (string, required) Nickname to inspect\n"
            "\nExamples:\n"
            + HelpExampleCli("getnicknameinfo", "\"kubu_dev\"")
            + HelpExampleRpc("getnicknameinfo", "\"kubu_dev\"")
        );
    }

    NicknameInfo info;
    if (!GetNicknameStateDB()->ReadNickname(request.params[0].get_str(), info)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Nickname not found");
    }

    return NicknameInfoToJSON(info);
}

UniValue resolvenickname(const JSONRPCRequest& request)
{
    EnsureNicknameDB();
    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "resolvenickname \"name\"\n"
            "\nResolve a nickname to its current payout address.\n"
            "\nArguments:\n"
            "1. \"name\"    (string, required) Nickname to resolve\n"
            "\nExamples:\n"
            + HelpExampleCli("resolvenickname", "\"kubu_dev\"")
            + HelpExampleRpc("resolvenickname", "\"kubu_dev\"")
        );
    }

    NicknameInfo info;
    if (!GetNicknameStateDB()->ReadNickname(request.params[0].get_str(), info)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Nickname not found");
    }

    UniValue result = NicknameInfoToJSON(info);
    result.pushKV("resolves", info.GetStatus(chainActive.Height()) == Nicknames::Status::ACTIVE);
    return result;
}

UniValue listnicknames(const JSONRPCRequest& request)
{
    EnsureNicknameDB();
    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            "listnicknames ( \"start\" count )\n"
            "\nList indexed nicknames in normalized-key order.\n"
            "\nArguments:\n"
            "1. \"start\"    (string, optional, default=\"\") Start listing from this nickname\n"
            "2. count        (numeric, optional, default=50) Maximum number of rows to return\n"
            "\nExamples:\n"
            + HelpExampleCli("listnicknames", "\"\" 20")
            + HelpExampleRpc("listnicknames", "\"\", 20")
        );
    }

    const std::string start = request.params.size() > 0 ? request.params[0].get_str() : "";
    int count = request.params.size() > 1 ? request.params[1].get_int() : 50;
    if (count <= 0 || count > 500) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Count must be between 1 and 500");
    }

    const std::vector<NicknameInfo> nicknames = GetNicknameStateDB()->ListNicknames(start, count);
    UniValue result(UniValue::VARR);
    for (const NicknameInfo& info : nicknames) {
        result.push_back(NicknameInfoToJSON(info));
    }
    return result;
}

UniValue listwalletnicknames(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    EnsureNicknameDB();
    if (!EnsureNicknameWalletIsAvailable(request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() > 2) {
        throw std::runtime_error(
            "listwalletnicknames ( \"start\" count )\n"
            "\nList indexed nicknames currently controlled by wallet keys.\n"
            "\nArguments:\n"
            "1. \"start\"    (string, optional, default=\"\") Start listing from this nickname\n"
            "2. count        (numeric, optional, default=50) Maximum number of rows to return\n"
            "\nExamples:\n"
            + HelpExampleCli("listwalletnicknames", "\"\" 20")
            + HelpExampleRpc("listwalletnicknames", "\"\", 20")
        );
    }

    const std::string start = request.params.size() > 0 ? request.params[0].get_str() : "";
    int count = request.params.size() > 1 ? request.params[1].get_int() : 50;
    if (count <= 0 || count > 500) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "Count must be between 1 and 500");
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);

    std::set<CKeyID> walletKeyIDs;
    pwalletMain->GetKeys(walletKeyIDs);

    struct WalletNicknameListEntry
    {
        std::string sortNickname;
        bool legacyBond;
        COutPoint bondOutpoint;
        UniValue json;
    };

    const std::vector<NicknameInfo> indexed =
        GetNicknameStateDB()->ListNicknamesForOwnerKeyIDs(walletKeyIDs, start, count);
    const std::string normalizedStart = start.empty() ? std::string() : [&]() {
        std::string normalized;
        Nicknames::NormalizeNickname(start, normalized);
        return normalized;
    }();

    std::vector<WalletNicknameListEntry> entries;
    entries.reserve(indexed.size());
    for (const NicknameInfo& info : indexed) {
        UniValue item = NicknameInfoToJSON(info);
        item.pushKV("legacy_bond", false);
        entries.push_back({info.nickname, false, info.GetBondOutpoint(), item});
    }

    std::vector<COutPoint> trackedBondOutpoints;
    CollectWalletTrackedBondOutpoints(trackedBondOutpoints);
    for (const COutPoint& outpoint : trackedBondOutpoints) {
        std::string nickname;
        if (!GetNicknameStateDB()->ReadNicknameByBondOutpoint(outpoint, nickname)) {
            continue;
        }

        NicknameInfo currentInfo;
        if (!GetNicknameStateDB()->ReadNickname(nickname, currentInfo)) {
            continue;
        }
        if (currentInfo.HasBondOutpoint() && currentInfo.GetBondOutpoint() == outpoint) {
            continue;
        }

        const CAmount bondAmount = GetWalletOutpointAmount(outpoint);
        if (bondAmount <= 0) {
            continue;
        }

        if (!normalizedStart.empty() && currentInfo.nickname < normalizedStart) {
            continue;
        }

        UniValue item = LegacyBondToJSON(currentInfo, outpoint, bondAmount);
        entries.push_back({currentInfo.nickname, true, outpoint, item});
    }

    std::sort(entries.begin(), entries.end(),
              [](const WalletNicknameListEntry& a, const WalletNicknameListEntry& b) {
                  if (a.sortNickname != b.sortNickname) {
                      return a.sortNickname < b.sortNickname;
                  }
                  if (a.legacyBond != b.legacyBond) {
                      return !a.legacyBond;
                  }
                  if (a.bondOutpoint.hash != b.bondOutpoint.hash) {
                      return a.bondOutpoint.hash < b.bondOutpoint.hash;
                  }
                  return a.bondOutpoint.n < b.bondOutpoint.n;
              });

    UniValue result(UniValue::VARR);
    for (const WalletNicknameListEntry& entry : entries) {
        result.push_back(entry.json);
        if (result.size() >= static_cast<size_t>(count)) {
            break;
        }
    }

    return result;
#else
    throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (wallet support is disabled)");
#endif
}

UniValue registernickname(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    EnsureNicknameDB();
    if (!EnsureNicknameWalletIsAvailable(request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 3) {
        throw std::runtime_error(
            "registernickname \"name\" \"owner_pubkey\" \"payout_address\"\n"
            "\nCreate and broadcast NAME_REGISTER transaction.\n"
            "\nArguments:\n"
            "1. \"name\"            (string, required) Nickname to register\n"
            "2. \"owner_pubkey\"    (string, required) 33-byte compressed owner pubkey (hex)\n"
            "3. \"payout_address\"  (string, required) Payout address resolved by nickname\n"
            "\nExamples:\n"
            + HelpExampleCli("registernickname", "\"kubu_dev\" \"0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798\" \"Ppb2G1hMPrU6wgGUohLaeNzfP4pQyd5udg\"")
            + HelpExampleRpc("registernickname", "\"kubu_dev\", \"0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798\", \"Ppb2G1hMPrU6wgGUohLaeNzfP4pQyd5udg\"")
        );
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);
    EnsureNicknameWalletIsUnlocked();

    NicknameOperation operation;
    operation.type = NicknameOpType::REGISTER;
    operation.nickname = request.params[0].get_str();
    operation.payoutAddress = request.params[2].get_str();

    const CPubKey ownerPubKey = DecodeOwnedPubKeyHex(request.params[1].get_str());
    operation.ownerPubKey.assign(ownerPubKey.begin(), ownerPubKey.end());

    return SendNicknameOperationTx(operation, ownerPubKey);
#else
    throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (wallet support is disabled)");
#endif
}

UniValue updatenickname(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    EnsureNicknameDB();
    if (!EnsureNicknameWalletIsAvailable(request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            "updatenickname \"name\" \"payout_address\"\n"
            "\nCreate and broadcast NAME_UPDATE transaction.\n"
            "\nArguments:\n"
            "1. \"name\"            (string, required) Nickname to update\n"
            "2. \"payout_address\"  (string, required) New payout address\n"
            "\nExamples:\n"
            + HelpExampleCli("updatenickname", "\"kubu_dev\" \"Ppb2G1hMPrU6wgGUohLaeNzfP4pQyd5udg\"")
            + HelpExampleRpc("updatenickname", "\"kubu_dev\", \"Ppb2G1hMPrU6wgGUohLaeNzfP4pQyd5udg\"")
        );
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);
    EnsureNicknameWalletIsUnlocked();

    NicknameInfo info;
    if (!GetNicknameStateDB()->ReadNickname(request.params[0].get_str(), info)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Nickname not found");
    }
    if (!IsMutableNicknameStatus(info.GetStatus(chainActive.Height()))) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "nickname is not mutable in current status");
    }

    const CPubKey ownerPubKey(info.ownerPubKey);
    if (!ownerPubKey.IsFullyValid() || !ownerPubKey.IsCompressed()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Indexed nickname owner pubkey is invalid");
    }

    NicknameOperation operation;
    operation.type = NicknameOpType::UPDATE;
    operation.nickname = info.nickname;
    operation.payoutAddress = request.params[1].get_str();

    return SendNicknameOperationTx(operation, ownerPubKey);
#else
    throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (wallet support is disabled)");
#endif
}

UniValue transfernickname(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    EnsureNicknameDB();
    if (!EnsureNicknameWalletIsAvailable(request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 2) {
        throw std::runtime_error(
            "transfernickname \"name\" \"new_owner_pubkey\"\n"
            "\nCreate and broadcast NAME_TRANSFER transaction.\n"
            "\nArguments:\n"
            "1. \"name\"              (string, required) Nickname to transfer\n"
            "2. \"new_owner_pubkey\"  (string, required) 33-byte compressed pubkey (hex)\n"
            "\nExamples:\n"
            + HelpExampleCli("transfernickname", "\"kubu_dev\" \"0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798\"")
            + HelpExampleRpc("transfernickname", "\"kubu_dev\", \"0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798\"")
        );
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);
    EnsureNicknameWalletIsUnlocked();

    NicknameInfo info;
    if (!GetNicknameStateDB()->ReadNickname(request.params[0].get_str(), info)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Nickname not found");
    }
    if (!IsMutableNicknameStatus(info.GetStatus(chainActive.Height()))) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "nickname is not mutable in current status");
    }

    const CPubKey ownerPubKey(info.ownerPubKey);
    if (!ownerPubKey.IsFullyValid() || !ownerPubKey.IsCompressed()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Indexed nickname owner pubkey is invalid");
    }

    const CPubKey newOwnerPubKey = DecodeCompressedPubKeyHex(request.params[1].get_str());

    NicknameOperation operation;
    operation.type = NicknameOpType::TRANSFER;
    operation.nickname = info.nickname;
    operation.newOwnerPubKey.assign(newOwnerPubKey.begin(), newOwnerPubKey.end());

    return SendNicknameOperationTx(operation, ownerPubKey, &info);
#else
    throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (wallet support is disabled)");
#endif
}

UniValue renewnickname(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    EnsureNicknameDB();
    if (!EnsureNicknameWalletIsAvailable(request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "renewnickname \"name\"\n"
            "\nCreate and broadcast NAME_RENEW transaction.\n"
            "\nArguments:\n"
            "1. \"name\"            (string, required) Nickname to renew\n"
            "\nExamples:\n"
            + HelpExampleCli("renewnickname", "\"kubu_dev\"")
            + HelpExampleRpc("renewnickname", "\"kubu_dev\"")
        );
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);
    EnsureNicknameWalletIsUnlocked();

    NicknameInfo info;
    if (!GetNicknameStateDB()->ReadNickname(request.params[0].get_str(), info)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Nickname not found");
    }
    if (!IsMutableNicknameStatus(info.GetStatus(chainActive.Height()))) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "nickname is not renewable in current status");
    }

    const CPubKey ownerPubKey(info.ownerPubKey);
    if (!ownerPubKey.IsFullyValid() || !ownerPubKey.IsCompressed()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Indexed nickname owner pubkey is invalid");
    }

    NicknameOperation operation;
    operation.type = NicknameOpType::RENEW;
    operation.nickname = info.nickname;

    return SendNicknameOperationTx(operation, ownerPubKey, &info);
#else
    throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (wallet support is disabled)");
#endif
}

UniValue releasenickname(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    EnsureNicknameDB();
    if (!EnsureNicknameWalletIsAvailable(request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "releasenickname \"name\"\n"
            "\nCreate and broadcast NAME_RELEASE transaction.\n"
            "\nArguments:\n"
            "1. \"name\"            (string, required) Nickname to release\n"
            "\nExamples:\n"
            + HelpExampleCli("releasenickname", "\"kubu_dev\"")
            + HelpExampleRpc("releasenickname", "\"kubu_dev\"")
        );
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);
    EnsureNicknameWalletIsUnlocked();

    NicknameInfo info;
    if (!GetNicknameStateDB()->ReadNickname(request.params[0].get_str(), info)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Nickname not found");
    }
    if (!IsMutableNicknameStatus(info.GetStatus(chainActive.Height()))) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "nickname is not releasable in current status");
    }

    const CPubKey ownerPubKey(info.ownerPubKey);
    if (!ownerPubKey.IsFullyValid() || !ownerPubKey.IsCompressed()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Indexed nickname owner pubkey is invalid");
    }

    NicknameOperation operation;
    operation.type = NicknameOpType::RELEASE;
    operation.nickname = info.nickname;

    return SendNicknameOperationTx(operation, ownerPubKey);
#else
    throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (wallet support is disabled)");
#endif
}

UniValue claimnicknamebond(const JSONRPCRequest& request)
{
#ifdef ENABLE_WALLET
    EnsureNicknameDB();
    if (!EnsureNicknameWalletIsAvailable(request.fHelp)) {
        return NullUniValue;
    }

    if (request.fHelp || request.params.size() != 1) {
        throw std::runtime_error(
            "claimnicknamebond \"name_or_bondid\"\n"
            "\nCreate and broadcast NAME_CLAIM_BOND transaction.\n"
            "\nArguments:\n"
            "1. \"name_or_bondid\"  (string, required) Nickname or explicit bond reference in txid:vout format\n"
            "\nExamples:\n"
            + HelpExampleCli("claimnicknamebond", "\"kubu_dev\"")
            + HelpExampleCli("claimnicknamebond", "\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef:1\"")
            + HelpExampleRpc("claimnicknamebond", "\"kubu_dev\"")
        );
    }

    LOCK2(cs_main, pwalletMain->cs_wallet);
    EnsureNicknameWalletIsUnlocked();

    const std::string input = request.params[0].get_str();
    NicknameOperation operation;
    operation.type = NicknameOpType::CLAIM_BOND;

    COutPoint explicitClaimOutpoint;
    if (ParseBondReference(input, explicitClaimOutpoint)) {
        std::string indexedNickname;
        if (!GetNicknameStateDB()->ReadNicknameByBondOutpoint(explicitClaimOutpoint, indexedNickname)) {
            throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Bond reference not found");
        }
        operation.nickname = indexedNickname;

        NicknameInfo currentInfo;
        if (GetNicknameStateDB()->ReadNickname(indexedNickname, currentInfo) &&
            currentInfo.HasBondOutpoint() &&
            currentInfo.GetBondOutpoint() == explicitClaimOutpoint &&
            currentInfo.GetStatus(chainActive.Height()) != Nicknames::Status::BOND_CLAIMABLE) {
            throw JSONRPCError(RPC_INVALID_PARAMETER, "nickname bond is not claimable");
        }

        return SendNicknameOperationTx(operation, CPubKey(), nullptr, &explicitClaimOutpoint);
    }

    NicknameInfo info;
    if (!GetNicknameStateDB()->ReadNickname(input, info)) {
        throw JSONRPCError(RPC_INVALID_ADDRESS_OR_KEY, "Nickname not found");
    }
    if (info.GetStatus(chainActive.Height()) != Nicknames::Status::BOND_CLAIMABLE) {
        throw JSONRPCError(RPC_INVALID_PARAMETER, "nickname bond is not claimable");
    }

    const CPubKey ownerPubKey(info.ownerPubKey);
    if (!ownerPubKey.IsFullyValid() || !ownerPubKey.IsCompressed()) {
        throw JSONRPCError(RPC_MISC_ERROR, "Indexed nickname owner pubkey is invalid");
    }

    operation.nickname = info.nickname;
    return SendNicknameOperationTx(operation, ownerPubKey, &info);
#else
    throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found (wallet support is disabled)");
#endif
}

static const CRPCCommand commands[] =
{ //  category      name               actor              okSafe argNames
    { "nicknames",  "checknickname",   &checknickname,    true,  {"name"} },
    { "nicknames",  "getnicknameinfo", &getnicknameinfo,  true,  {"name"} },
    { "nicknames",  "resolvenickname", &resolvenickname,  true,  {"name"} },
    { "nicknames",  "listnicknames",   &listnicknames,    true,  {"start", "count"} },
    { "nicknames",  "listwalletnicknames", &listwalletnicknames, false, {"start", "count"} },
    { "nicknames",  "registernickname", &registernickname, false, {"name", "owner_pubkey", "payout_address"} },
    { "nicknames",  "updatenickname", &updatenickname, false, {"name", "payout_address"} },
    { "nicknames",  "transfernickname", &transfernickname, false, {"name", "new_owner_pubkey"} },
    { "nicknames",  "renewnickname", &renewnickname, false, {"name"} },
    { "nicknames",  "releasenickname", &releasenickname, false, {"name"} },
    { "nicknames",  "claimnicknamebond", &claimnicknamebond, false, {"name_or_bondid"} },
};

} // namespace

void RegisterNicknameRPCCommands(CRPCTable& t)
{
    for (unsigned int vcidx = 0; vcidx < ARRAYLEN(commands); vcidx++) {
        t.appendCommand(commands[vcidx].name, &commands[vcidx]);
    }
}
