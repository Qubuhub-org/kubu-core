#include "nicknameop.h"
#include "nicknamedb.h"
#include "nicknames.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(nickname_tests)

BOOST_AUTO_TEST_CASE(validation_accepts_expected_examples)
{
    BOOST_CHECK(Nicknames::IsValidNickname("kubu"));
    BOOST_CHECK(Nicknames::IsValidNickname("nonkyc"));
    BOOST_CHECK(Nicknames::IsValidNickname("shop123"));
    BOOST_CHECK(Nicknames::IsValidNickname("kubu_dev"));
    BOOST_CHECK(Nicknames::IsValidNickname("mr_kubu"));
    BOOST_CHECK(Nicknames::IsValidNickname("bot_01"));
}

BOOST_AUTO_TEST_CASE(validation_rejects_expected_examples)
{
    BOOST_CHECK(Nicknames::IsValidNickname("KuBu"));
    BOOST_CHECK(!Nicknames::IsValidNickname("my-shop"));
    BOOST_CHECK(!Nicknames::IsValidNickname("lol.coin"));
    BOOST_CHECK(!Nicknames::IsValidNickname("_kubu"));
    BOOST_CHECK(!Nicknames::IsValidNickname("kubu_"));
    BOOST_CHECK(!Nicknames::IsValidNickname("kubu__dev"));
    BOOST_CHECK(!Nicknames::IsValidNickname("1234"));
    BOOST_CHECK(!Nicknames::IsValidNickname("abc"));
}

BOOST_AUTO_TEST_CASE(normalization_trims_strips_at_and_lowercases)
{
    std::string normalized;
    Nicknames::NormalizeNickname("  @KuBu_01 \r\n", normalized);
    BOOST_CHECK_EQUAL(normalized, "kubu_01");
    BOOST_CHECK(Nicknames::IsValidNickname(normalized));
}

BOOST_AUTO_TEST_CASE(pricing_matches_spec)
{
    const Nicknames::Pricing len4 = Nicknames::GetPricing(4);
    BOOST_CHECK_EQUAL(len4.registrationFee, 24 * COIN);
    BOOST_CHECK_EQUAL(len4.bondAmount, 48 * COIN);
    BOOST_CHECK_EQUAL(Nicknames::GetRenewalFee(4), 6 * COIN);

    const Nicknames::Pricing len5 = Nicknames::GetPricing(5);
    BOOST_CHECK_EQUAL(len5.registrationFee, 12 * COIN);
    BOOST_CHECK_EQUAL(len5.bondAmount, 24 * COIN);
    BOOST_CHECK_EQUAL(Nicknames::GetRenewalFee(5), 3 * COIN);

    const Nicknames::Pricing len8 = Nicknames::GetPricing(8);
    BOOST_CHECK_EQUAL(len8.registrationFee, 1 * COIN);
    BOOST_CHECK_EQUAL(len8.bondAmount, 3 * COIN);
    BOOST_CHECK_EQUAL(Nicknames::GetRenewalFee(8), COIN / 4);
}

BOOST_AUTO_TEST_CASE(pricing_multiplier_limits_are_applied)
{
    const Nicknames::Pricing low = Nicknames::GetPricing(8, 100);
    BOOST_CHECK_EQUAL(low.registrationFee, COIN / 2);
    BOOST_CHECK_EQUAL(low.bondAmount, (3 * COIN) / 2);

    const Nicknames::Pricing high = Nicknames::GetPricing(4, 10000);
    BOOST_CHECK_EQUAL(high.registrationFee, 72 * COIN);
    BOOST_CHECK_EQUAL(high.bondAmount, 144 * COIN);
}

BOOST_AUTO_TEST_CASE(dynamic_multiplier_moves_gradually)
{
    // target=280; recent=560 => raw=2000, but daily change is limited to +10%.
    BOOST_CHECK_EQUAL(Nicknames::ComputeNextPricingMultiplierPermille(1000, 560), 1100);

    // recent=0 => raw=0, but daily change is limited to -10%.
    BOOST_CHECK_EQUAL(Nicknames::ComputeNextPricingMultiplierPermille(1000, 0), 900);

    // Floor and ceiling remain enforced.
    BOOST_CHECK_EQUAL(Nicknames::ComputeNextPricingMultiplierPermille(500, 0), 500);
    BOOST_CHECK_EQUAL(Nicknames::ComputeNextPricingMultiplierPermille(3000, 1000000), 3000);
}

BOOST_AUTO_TEST_CASE(status_transitions_match_lifecycle)
{
    BOOST_CHECK_EQUAL(Nicknames::StatusToString(Nicknames::GetStatus(100, 200, 250, false, false)), "ACTIVE");
    BOOST_CHECK_EQUAL(Nicknames::StatusToString(Nicknames::GetStatus(220, 200, 250, false, false)), "EXPIRED_GRACE");
    BOOST_CHECK_EQUAL(Nicknames::StatusToString(Nicknames::GetStatus(300, 200, 250, false, false)), "BOND_CLAIMABLE");
    BOOST_CHECK_EQUAL(Nicknames::StatusToString(Nicknames::GetStatus(300, 200, 250, false, true)), "EXPIRED_AVAILABLE");
    BOOST_CHECK_EQUAL(Nicknames::StatusToString(Nicknames::GetStatus(120, 200, 250, true, false)), "BOND_CLAIMABLE");
    BOOST_CHECK_EQUAL(Nicknames::StatusToString(Nicknames::GetStatus(120, 200, 250, true, true)), "RELEASED");
}

BOOST_AUTO_TEST_CASE(info_status_obeys_height_fields)
{
    NicknameInfo info;
    info.registrationHeight = 10;
    info.activeUntilHeight = 100;
    info.graceUntilHeight = 120;
    info.released = false;
    info.bondClaimed = false;

    BOOST_CHECK_EQUAL((int)info.GetStatus( 90), (int)Nicknames::Status::ACTIVE);
    BOOST_CHECK_EQUAL((int)info.GetStatus(105), (int)Nicknames::Status::EXPIRED_GRACE);
    BOOST_CHECK_EQUAL((int)info.GetStatus(130), (int)Nicknames::Status::BOND_CLAIMABLE);

    info.released = true;
    BOOST_CHECK_EQUAL((int)info.GetStatus( 90), (int)Nicknames::Status::BOND_CLAIMABLE);
    BOOST_CHECK_EQUAL((int)info.GetStatus( 90), (int)Nicknames::Status::BOND_CLAIMABLE);
    info.bondClaimed = true;
    BOOST_CHECK_EQUAL((int)info.GetStatus( 90), (int)Nicknames::Status::RELEASED);
}

BOOST_AUTO_TEST_CASE(operation_roundtrip_transfer)
{
    NicknameOperation transferOp;
    transferOp.type = NicknameOpType::TRANSFER;
    transferOp.nickname = "kubu_dev";
    transferOp.newOwnerPubKey.assign(33, 0x02);
    transferOp.newOwnerPubKey[0] = 0x03;

    CScript script;
    std::string reason;
    BOOST_CHECK_MESSAGE(BuildNicknameOpScript(transferOp, script, &reason), reason);

    NicknameOperation decoded;
    BOOST_CHECK_MESSAGE(ExtractNicknameOperation(script, decoded, &reason), reason);
    BOOST_CHECK(decoded.type == NicknameOpType::TRANSFER);
    BOOST_CHECK_EQUAL(decoded.nickname, transferOp.nickname);
    BOOST_CHECK(decoded.newOwnerPubKey == transferOp.newOwnerPubKey);
}

BOOST_AUTO_TEST_CASE(operation_parser_rejects_foreign_opreturn_payload)
{
    std::vector<unsigned char> payload;
    payload.push_back('N');
    payload.push_back('O');
    payload.push_back('P');
    payload.push_back('E');

    CScript script = CScript() << OP_RETURN << payload;

    NicknameOperation decoded;
    std::string reason;
    BOOST_CHECK(!ExtractNicknameOperation(script, decoded, &reason));
}

BOOST_AUTO_TEST_SUITE_END()
