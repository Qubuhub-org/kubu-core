#ifndef BITCOIN_NICKNAMEOP_H
#define BITCOIN_NICKNAMEOP_H

#include "script/script.h"

#include <string>
#include <vector>

enum class NicknameOpType : uint8_t
{
    NONE = 0,
    REGISTER = 1,
    UPDATE = 2,
    TRANSFER = 3,
    RENEW = 4,
    RELEASE = 5,
    CLAIM_BOND = 6,
};

struct NicknameOperation
{
    NicknameOpType type;
    std::string nickname;
    std::vector<unsigned char> ownerPubKey;
    std::string payoutAddress;
    std::vector<unsigned char> newOwnerPubKey;

    NicknameOperation();
};

bool BuildNicknameOpScript(const NicknameOperation& operation, CScript& scriptOut, std::string* reason = nullptr);
bool ExtractNicknameOperation(const CScript& script, NicknameOperation& operationOut, std::string* reason = nullptr);
std::string NicknameOpTypeToString(NicknameOpType type);

#endif // BITCOIN_NICKNAMEOP_H
