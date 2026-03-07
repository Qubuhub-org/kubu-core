// Copyright (c) 2015-2021 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "nicknames.h"
#include "kubu.h"

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(kubu_tests)

BOOST_AUTO_TEST_CASE(subsidy_test)
{
    const CChainParams& mainParams = Params(CBaseChainParams::MAIN);
    const uint256 prevHash = uint256S("0");

    BOOST_CHECK_EQUAL(mainParams.GetConsensus(0).nSubsidyHalvingInterval, 1168000);
    BOOST_CHECK_EQUAL(GetKubuBlockSubsidy(0, mainParams.GetConsensus(0), prevHash), 48 * COIN);
    BOOST_CHECK_EQUAL(GetKubuBlockSubsidy(1168000, mainParams.GetConsensus(1168000), prevHash), 24 * COIN);
    BOOST_CHECK_EQUAL(GetKubuBlockSubsidy(2336000, mainParams.GetConsensus(2336000), prevHash), 12 * COIN);
    BOOST_CHECK_EQUAL(GetKubuBlockSubsidy(3504000, mainParams.GetConsensus(3504000), prevHash), 6 * COIN);
    BOOST_CHECK_EQUAL(GetKubuBlockSubsidy(4672000, mainParams.GetConsensus(4672000), prevHash), 2 * COIN);
    BOOST_CHECK_EQUAL(GetKubuBlockSubsidy(999999999, mainParams.GetConsensus(999999999), prevHash), 2 * COIN);
}

BOOST_AUTO_TEST_CASE(consensus_parameters_match_kubu_mvp)
{
    const Consensus::Params& params = Params(CBaseChainParams::MAIN).GetConsensus(0);
    const Consensus::Params& preAuxpowParams = Params(CBaseChainParams::MAIN).GetConsensus(38999);
    const Consensus::Params& auxpowParams = Params(CBaseChainParams::MAIN).GetConsensus(39000);

    BOOST_CHECK_EQUAL(params.nPowTargetTimespan, 54);
    BOOST_CHECK_EQUAL(params.nPowTargetSpacing, 54);
    BOOST_CHECK_EQUAL(params.nSubsidyHalvingInterval, 1168000);
    BOOST_CHECK_EQUAL(params.fAllowLegacyBlocks, true);
    BOOST_CHECK_EQUAL(params.fDigishieldDifficultyCalculation, true);
    BOOST_CHECK_EQUAL(preAuxpowParams.fAllowLegacyBlocks, true);
    BOOST_CHECK_EQUAL(auxpowParams.fAllowLegacyBlocks, false);
    BOOST_CHECK_EQUAL(auxpowParams.nHeightEffective, 39000U);
}

BOOST_AUTO_TEST_CASE(nickname_constants_match_mvp)
{
    BOOST_CHECK_EQUAL(Nicknames::MIN_LENGTH, 4U);
    BOOST_CHECK_EQUAL(Nicknames::MAX_LENGTH, 16U);
    BOOST_CHECK_EQUAL(Nicknames::ActiveBlocks(), 144000);
    BOOST_CHECK_EQUAL(Nicknames::GraceBlocks(), 14400);
    BOOST_CHECK_EQUAL(Nicknames::RENEWAL_PERCENT, 25);
}

BOOST_AUTO_TEST_SUITE_END()
