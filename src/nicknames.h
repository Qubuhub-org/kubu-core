#ifndef BITCOIN_NICKNAMES_H
#define BITCOIN_NICKNAMES_H

#include "amount.h"

#include <cstdint>
#include <string>

namespace Nicknames {

static const size_t MIN_LENGTH = 4;
static const size_t MAX_LENGTH = 16;
// ~90 days and ~9 days at 54s block time.
static const int ACTIVE_BLOCKS = 144000;
static const int GRACE_BLOCKS = 14400;
static const int RENEWAL_PERCENT = 25;
static const int PRICING_WINDOW_BLOCKS = 11200; // ~7 days at 54s block time
static const int PRICING_EPOCH_BLOCKS = 1600;   // ~1 day at 54s block time
static const int PRICING_TARGET_REGISTRATIONS_7D = 280;
static const int PRICING_ALPHA_PERCENT = 20;
static const int PRICING_STEP_LIMIT_PERCENT = 10;
static const int64_t DEFAULT_PRICING_MULTIPLIER_PERMILLE = 1000;
static const int64_t MIN_PRICING_MULTIPLIER_PERMILLE = 500;
static const int64_t MAX_PRICING_MULTIPLIER_PERMILLE = 3000;

enum class Status {
    ACTIVE,
    EXPIRED_GRACE,
    EXPIRED_AVAILABLE,
    RELEASED,
    BOND_CLAIMABLE,
};

struct Pricing
{
    CAmount registrationFee;
    CAmount bondAmount;
};

bool NormalizeNickname(const std::string& input, std::string& normalized);
bool IsValidNormalizedNickname(const std::string& name, std::string* reason = nullptr);
bool IsValidNickname(const std::string& input, std::string* reason = nullptr);
Pricing GetPricing(size_t normalizedLength, int64_t multiplierPermille = DEFAULT_PRICING_MULTIPLIER_PERMILLE);
CAmount GetRenewalFee(size_t normalizedLength, int64_t multiplierPermille = DEFAULT_PRICING_MULTIPLIER_PERMILLE);
CAmount GetRenewalBondIncrease(size_t normalizedLength, int64_t multiplierPermille = DEFAULT_PRICING_MULTIPLIER_PERMILLE);
int64_t ClampPricingMultiplierPermille(int64_t multiplierPermille);
int64_t ComputeNextPricingMultiplierPermille(int64_t previousMultiplierPermille, int64_t recentRegistrations);
int ActiveBlocks();
int GraceBlocks();
Status GetStatus(int currentHeight, int activeUntilHeight, int graceUntilHeight, bool released, bool bondClaimed);
std::string StatusToString(Status status);

} // namespace Nicknames

#endif // BITCOIN_NICKNAMES_H
