#include "nicknames.h"

#include "util.h"

#include <algorithm>
#include <cctype>

namespace Nicknames {

int ActiveBlocks()
{
    return GetArg("-nicknameactiveblocks", ACTIVE_BLOCKS);
}

int GraceBlocks()
{
    return GetArg("-nicknamegraceblocks", GRACE_BLOCKS);
}

namespace {

bool IsAsciiLowerLetter(char c)
{
    return c >= 'a' && c <= 'z';
}

bool IsAsciiDigit(char c)
{
    return c >= '0' && c <= '9';
}

bool IsAllowedChar(char c)
{
    return IsAsciiLowerLetter(c) || IsAsciiDigit(c) || c == '_';
}

Pricing GetBasePricing(size_t normalizedLength)
{
    // Affordable pricing policy:
    // - long names should be cheap for everyday users
    // - short names stay more expensive to reduce squatting
    switch (normalizedLength) {
    case 4:
        return {24 * COIN, 48 * COIN};
    case 5:
        return {12 * COIN, 24 * COIN};
    case 6:
        return {6 * COIN, 12 * COIN};
    case 7:
        return {3 * COIN, 6 * COIN};
    default:
        return {1 * COIN, 3 * COIN};
    }
}

CAmount ScaleByMultiplierPermille(const CAmount baseAmount, const int64_t multiplierPermille)
{
    if (baseAmount <= 0 || multiplierPermille <= 0) {
        return 0;
    }

    // Integer fixed-point scaling with nearest rounding.
    return (baseAmount * multiplierPermille + 500) / 1000;
}

} // namespace

bool NormalizeNickname(const std::string& input, std::string& normalized)
{
    normalized = input;
    std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return true;
}

bool IsValidNormalizedNickname(const std::string& name, std::string* reason)
{
    if (name.size() < MIN_LENGTH || name.size() > MAX_LENGTH) {
        if (reason) {
            *reason = "nickname length must be between 4 and 16 characters";
        }
        return false;
    }

    if (name.front() == '_' || name.back() == '_') {
        if (reason) {
            *reason = "nickname cannot start or end with underscore";
        }
        return false;
    }

    bool hasLetter = false;
    bool previousUnderscore = false;
    for (const char c : name) {
        if (!IsAllowedChar(c)) {
            if (reason) {
                *reason = "nickname may only contain lowercase ascii letters, digits, and underscores";
            }
            return false;
        }

        if (c == '_') {
            if (previousUnderscore) {
                if (reason) {
                    *reason = "nickname cannot contain consecutive underscores";
                }
                return false;
            }
            previousUnderscore = true;
            continue;
        }

        previousUnderscore = false;
        hasLetter = hasLetter || IsAsciiLowerLetter(c);
    }

    if (!hasLetter) {
        if (reason) {
            *reason = "nickname must contain at least one letter";
        }
        return false;
    }

    return true;
}

bool IsValidNickname(const std::string& input, std::string* reason)
{
    std::string normalized;
    NormalizeNickname(input, normalized);
    return IsValidNormalizedNickname(normalized, reason);
}

int64_t ClampPricingMultiplierPermille(const int64_t multiplierPermille)
{
    if (multiplierPermille < MIN_PRICING_MULTIPLIER_PERMILLE) {
        return MIN_PRICING_MULTIPLIER_PERMILLE;
    }
    if (multiplierPermille > MAX_PRICING_MULTIPLIER_PERMILLE) {
        return MAX_PRICING_MULTIPLIER_PERMILLE;
    }
    return multiplierPermille;
}

int64_t ComputeNextPricingMultiplierPermille(const int64_t previousMultiplierPermille, const int64_t recentRegistrations)
{
    const int64_t prev = ClampPricingMultiplierPermille(previousMultiplierPermille);
    if (PRICING_TARGET_REGISTRATIONS_7D <= 0) {
        return prev;
    }

    int64_t raw = 0;
    if (recentRegistrations > 0) {
        raw = (recentRegistrations * 1000) / PRICING_TARGET_REGISTRATIONS_7D;
    }

    const int alpha = PRICING_ALPHA_PERCENT;
    int64_t smoothed = ((100 - alpha) * prev + alpha * raw + 50) / 100;
    smoothed = ClampPricingMultiplierPermille(smoothed);

    const int step = PRICING_STEP_LIMIT_PERCENT;
    const int64_t lowerBound = (prev * (100 - step) + 99) / 100;
    const int64_t upperBound = (prev * (100 + step)) / 100;
    if (smoothed < lowerBound) {
        smoothed = lowerBound;
    }
    if (smoothed > upperBound) {
        smoothed = upperBound;
    }

    return ClampPricingMultiplierPermille(smoothed);
}

Pricing GetPricing(const size_t normalizedLength, const int64_t multiplierPermille)
{
    const Pricing base = GetBasePricing(normalizedLength);
    const int64_t clampedMultiplier = ClampPricingMultiplierPermille(multiplierPermille);
    return {ScaleByMultiplierPermille(base.registrationFee, clampedMultiplier),
            ScaleByMultiplierPermille(base.bondAmount, clampedMultiplier)};
}

CAmount GetRenewalFee(const size_t normalizedLength, const int64_t multiplierPermille)
{
    const Pricing pricing = GetPricing(normalizedLength, multiplierPermille);
    return pricing.registrationFee * RENEWAL_PERCENT / 100;
}

CAmount GetRenewalBondIncrease(const size_t normalizedLength, const int64_t multiplierPermille)
{
    return GetRenewalFee(normalizedLength, multiplierPermille);
}

Status GetStatus(int currentHeight, int activeUntilHeight, int graceUntilHeight, bool released, bool bondClaimed)
{
    if (released) {
        return bondClaimed ? Status::RELEASED : Status::BOND_CLAIMABLE;
    }

    if (currentHeight <= activeUntilHeight) {
        return Status::ACTIVE;
    }

    if (currentHeight <= graceUntilHeight) {
        return Status::EXPIRED_GRACE;
    }

    return bondClaimed ? Status::EXPIRED_AVAILABLE : Status::BOND_CLAIMABLE;
}

std::string StatusToString(Status status)
{
    switch (status) {
    case Status::ACTIVE:
        return "ACTIVE";
    case Status::EXPIRED_GRACE:
        return "EXPIRED_GRACE";
    case Status::EXPIRED_AVAILABLE:
        return "EXPIRED_AVAILABLE";
    case Status::RELEASED:
        return "RELEASED";
    case Status::BOND_CLAIMABLE:
        return "BOND_CLAIMABLE";
    }

    return "UNKNOWN";
}

} // namespace Nicknames
