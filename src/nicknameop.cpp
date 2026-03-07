#include "nicknameop.h"

#include "base58.h"
#include "nicknames.h"
#include "pubkey.h"
#include "script/standard.h"
#include "streams.h"

#include <boost/variant/get.hpp>

namespace {

static const char KUBU_NICKNAME_MAGIC[] = {'K', 'N', 'A', '1'};
static const unsigned char DEST_PKH = 1;
static const unsigned char DEST_SH = 2;

bool EncodeDestinationBytes(const std::string& address, std::vector<unsigned char>& encoded, std::string* reason)
{
    const CBitcoinAddress parsed(address);
    if (!parsed.IsValid()) {
        if (reason) {
            *reason = "invalid payout address";
        }
        return false;
    }

    const CTxDestination dest = parsed.Get();
    if (const CKeyID* keyID = boost::get<CKeyID>(&dest)) {
        encoded.assign(1, DEST_PKH);
        encoded.insert(encoded.end(), keyID->begin(), keyID->end());
        return true;
    }

    if (const CScriptID* scriptID = boost::get<CScriptID>(&dest)) {
        encoded.assign(1, DEST_SH);
        encoded.insert(encoded.end(), scriptID->begin(), scriptID->end());
        return true;
    }

    if (reason) {
        *reason = "unsupported payout destination type";
    }
    return false;
}

bool DecodeDestinationBytes(const std::vector<unsigned char>& encoded, std::string& addressOut, std::string* reason)
{
    if (encoded.size() != 21) {
        if (reason) {
            *reason = "invalid compact destination payload";
        }
        return false;
    }

    std::vector<unsigned char> hash(encoded.begin() + 1, encoded.end());
    if (encoded[0] == DEST_PKH) {
        addressOut = CBitcoinAddress(CKeyID(uint160(hash))).ToString();
        return true;
    }

    if (encoded[0] == DEST_SH) {
        addressOut = CBitcoinAddress(CScriptID(uint160(hash))).ToString();
        return true;
    }

    if (reason) {
        *reason = "unknown compact destination type";
    }
    return false;
}

bool ReadBytes(CDataStream& ss, std::vector<unsigned char>& data, size_t maxSize, std::string* reason)
{
    std::vector<unsigned char> tmp;
    try {
        ss >> tmp;
    } catch (const std::exception&) {
        if (reason) {
            *reason = "failed to deserialize nickname operation field";
        }
        return false;
    }

    if (tmp.size() > maxSize) {
        if (reason) {
            *reason = "nickname operation field exceeds limit";
        }
        return false;
    }

    data = tmp;
    return true;
}

} // namespace

NicknameOperation::NicknameOperation() : type(NicknameOpType::NONE)
{
}

bool BuildNicknameOpScript(const NicknameOperation& operation, CScript& scriptOut, std::string* reason)
{
    std::string normalized;
    Nicknames::NormalizeNickname(operation.nickname, normalized);
    if (!Nicknames::IsValidNormalizedNickname(normalized, reason)) {
        return false;
    }

    std::vector<unsigned char> payoutBytes;
    if ((operation.type == NicknameOpType::REGISTER || operation.type == NicknameOpType::UPDATE) &&
        !EncodeDestinationBytes(operation.payoutAddress, payoutBytes, reason)) {
        return false;
    }

    if ((operation.type == NicknameOpType::REGISTER && operation.ownerPubKey.size() != CPubKey::COMPRESSED_SIZE) ||
        (operation.type == NicknameOpType::TRANSFER && operation.newOwnerPubKey.size() != CPubKey::COMPRESSED_SIZE)) {
        if (reason) {
            *reason = "nickname operations currently require compressed pubkeys";
        }
        return false;
    }

    CDataStream payload(SER_NETWORK, PROTOCOL_VERSION);
    payload.write(KUBU_NICKNAME_MAGIC, sizeof(KUBU_NICKNAME_MAGIC));
    payload << static_cast<uint8_t>(operation.type);
    payload << normalized;

    switch (operation.type) {
    case NicknameOpType::REGISTER:
        payload << operation.ownerPubKey;
        payload << payoutBytes;
        break;
    case NicknameOpType::UPDATE:
        payload << payoutBytes;
        break;
    case NicknameOpType::TRANSFER:
        payload << operation.newOwnerPubKey;
        break;
    case NicknameOpType::RENEW:
    case NicknameOpType::RELEASE:
    case NicknameOpType::CLAIM_BOND:
        break;
    default:
        if (reason) {
            *reason = "unknown nickname operation type";
        }
        return false;
    }

    if (payload.size() > MAX_OP_RETURN_RELAY) {
        if (reason) {
            *reason = "nickname operation exceeds current OP_RETURN relay limit";
        }
        return false;
    }

    std::vector<unsigned char> raw(payload.begin(), payload.end());
    scriptOut = CScript() << OP_RETURN << raw;
    return true;
}

bool ExtractNicknameOperation(const CScript& script, NicknameOperation& operationOut, std::string* reason)
{
    CScript::const_iterator pc = script.begin();
    opcodetype opcode;
    std::vector<unsigned char> pushed;

    if (!script.GetOp(pc, opcode) || opcode != OP_RETURN) {
        if (reason) {
            *reason = "script is not an OP_RETURN nickname operation";
        }
        return false;
    }

    if (!script.GetOp(pc, opcode, pushed) || pc != script.end()) {
        if (reason) {
            *reason = "nickname operation must contain a single pushed payload";
        }
        return false;
    }

    CDataStream payload(pushed, SER_NETWORK, PROTOCOL_VERSION);
    char magic[sizeof(KUBU_NICKNAME_MAGIC)];
    try {
        payload.read(magic, sizeof(magic));
    } catch (const std::exception&) {
        if (reason) {
            *reason = "nickname payload is truncated";
        }
        return false;
    }

    if (!std::equal(magic, magic + sizeof(magic), KUBU_NICKNAME_MAGIC)) {
        if (reason) {
            *reason = "nickname payload magic mismatch";
        }
        return false;
    }

    uint8_t opType = 0;
    try {
        payload >> opType;
    } catch (const std::exception&) {
        if (reason) {
            *reason = "nickname payload is malformed";
        }
        return false;
    }

    operationOut = NicknameOperation();
    operationOut.type = static_cast<NicknameOpType>(opType);
    try {
        payload >> operationOut.nickname;
    } catch (const std::exception&) {
        if (reason) {
            *reason = "nickname payload is malformed";
        }
        return false;
    }
    std::string validationReason;
    if (!Nicknames::IsValidNormalizedNickname(operationOut.nickname, &validationReason)) {
        if (reason) {
            *reason = validationReason;
        }
        return false;
    }

    switch (operationOut.type) {
    case NicknameOpType::REGISTER: {
        if (!ReadBytes(payload, operationOut.ownerPubKey, CPubKey::COMPRESSED_SIZE, reason) ||
            operationOut.ownerPubKey.size() != CPubKey::COMPRESSED_SIZE) {
            if (reason && reason->empty()) {
                *reason = "register operation requires compressed owner pubkey";
            }
            return false;
        }
        std::vector<unsigned char> payoutBytes;
        if (!ReadBytes(payload, payoutBytes, 21, reason) || !DecodeDestinationBytes(payoutBytes, operationOut.payoutAddress, reason)) {
            return false;
        }
        break;
    }
    case NicknameOpType::UPDATE: {
        std::vector<unsigned char> payoutBytes;
        if (!ReadBytes(payload, payoutBytes, 21, reason) || !DecodeDestinationBytes(payoutBytes, operationOut.payoutAddress, reason)) {
            return false;
        }
        break;
    }
    case NicknameOpType::TRANSFER:
        if (!ReadBytes(payload, operationOut.newOwnerPubKey, CPubKey::COMPRESSED_SIZE, reason) ||
            operationOut.newOwnerPubKey.size() != CPubKey::COMPRESSED_SIZE) {
            if (reason && reason->empty()) {
                *reason = "transfer operation requires compressed new owner pubkey";
            }
            return false;
        }
        break;
    case NicknameOpType::RENEW:
    case NicknameOpType::RELEASE:
    case NicknameOpType::CLAIM_BOND:
        break;
    default:
        if (reason) {
            *reason = "unknown nickname operation type";
        }
        return false;
    }

    return true;
}

std::string NicknameOpTypeToString(NicknameOpType type)
{
    switch (type) {
    case NicknameOpType::REGISTER:
        return "NAME_REGISTER";
    case NicknameOpType::UPDATE:
        return "NAME_UPDATE";
    case NicknameOpType::TRANSFER:
        return "NAME_TRANSFER";
    case NicknameOpType::RENEW:
        return "NAME_RENEW";
    case NicknameOpType::RELEASE:
        return "NAME_RELEASE";
    case NicknameOpType::CLAIM_BOND:
        return "NAME_CLAIM_BOND";
    case NicknameOpType::NONE:
        break;
    }

    return "NONE";
}
