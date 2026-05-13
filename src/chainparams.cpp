// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2022 The Dogecoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "chainparams.h"
#include "consensus/merkle.h"

#include "tinyformat.h"
#include "util.h"
#include "utilstrencodings.h"

#include <assert.h>

#include <boost/assign/list_of.hpp>

#include "chainparamsseeds.h"

static CBlock CreateGenesisBlock(const char* pszTimestamp, const CScript& genesisOutputScript, uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig = CScript() << 486604799 << CScriptNum(4) << std::vector<unsigned char>((const unsigned char*)pszTimestamp, (const unsigned char*)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime    = nTime;
    genesis.nBits    = nBits;
    genesis.nNonce   = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation
 * transaction cannot be spent since it did not originally exist in the
 * database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000, hashMerkleRoot=4a5e1e, nTime=1386325540, nBits=0x1e0ffff0, nNonce=99943, vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
static CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits, int32_t nVersion, const CAmount& genesisReward)
{
    const char* pszTimestamp = "WSJ 1/22/24 - Fed Review Clears Central Bank Officials of Violating Rules";
    const CScript genesisOutputScript = CScript() << ParseHex("0436d04f40a76a1094ea10b14a513b62bfd0b47472dda1c25aa9cf8266e53f3c4353680146177f8a3b328ed2c6e02f2b8e051d9d5ffc61a4e6ccabd03409109a5a") << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce, nBits, nVersion, genesisReward);
}

/**
 * Main network
 */
/**
 * What makes a good checkpoint block?
 * + Is surrounded by blocks with reasonable timestamps
 *   (no blocks before with a timestamp after, none after with
 *    timestamp before)
 * + Contains no strange transactions
 */

class CMainParams : public CChainParams {
private:
    Consensus::Params digishieldConsensus;
    Consensus::Params auxpowConsensus;
public:
    CMainParams() {
        strNetworkID = "main";

        consensus.nSubsidyHalvingInterval = 1168000;
        consensus.nMajorityEnforceBlockUpgrade = 1500;
        consensus.nMajorityRejectBlockOutdated = 1900;
        consensus.nMajorityWindow = 2000;
        // BIP34 is never enforced in KUBU v2 blocks, so we enforce from v3
        consensus.BIP34Height = 1000;
        consensus.BIP34Hash = uint256S("0x1561d231b29d3b64b6b560f2e6eb462aa04fd7f295d02b5a40aaac801d8a0d01");
        consensus.BIP65Height = 1000;
        consensus.BIP66Height = 1000;
        consensus.powLimit = uint256S("0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20;
        consensus.nPowTargetTimespan = 54;
        consensus.nPowTargetSpacing = 54;
        consensus.fDigishieldDifficultyCalculation = true;
        consensus.nCoinbaseMaturity = 30;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowAllowDigishieldMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 10640; // 95% of 11,200
        consensus.nMinerConfirmationWindow = 11200; // 54 * 11,200 ~= 7 days
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        // XXX: BIP heights and hashes all need to be updated to KUBU values
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1462060800; // May 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800; // May 1st, 2017

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1479168000; // November 15th, 2016.
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 0; // Disabled

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x000000000000000000000000000000000000000000000000009dee9b9844cf89");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x6f7bb4ebc17469c8e622ace70de790a80e0f5948771827e0e08edcc7fcc605d7");

        // AuxPoW parameters
        consensus.nAuxpowChainId = 0x003f; // 63
        consensus.fStrictChainId = true;
        consensus.fAllowLegacyBlocks = true;
        consensus.nHeightEffective = 0;
        consensus.fSimplifiedRewards = true;

        // Blocks 1000 - 38,999 are Digishield without AuxPoW
        digishieldConsensus = consensus;
        digishieldConsensus.nHeightEffective = 1000;
        digishieldConsensus.fSimplifiedRewards = true;
        digishieldConsensus.fDigishieldDifficultyCalculation = true;
        digishieldConsensus.nPowTargetTimespan = 54;
        digishieldConsensus.nCoinbaseMaturity = 240;

        // Blocks 39,000+ are AuxPoW
        // Some tests from Dogecoin expect non-auxpow blocks. This allows those tests to pass.
        auxpowConsensus = digishieldConsensus;
        auxpowConsensus.nHeightEffective = 39000;
        auxpowConsensus.fAllowLegacyBlocks = false;

        // Assemble the binary search tree of consensus parameters
        pConsensusRoot = &digishieldConsensus;
        digishieldConsensus.pLeft = &consensus;
        digishieldConsensus.pRight = &auxpowConsensus;

        /**
         * The message start string is designed to be unlikely to occur in normal data.
         * The characters are rarely used upper ASCII, not valid as UTF-8, and produce
         * a large 32-bit integer with any alignment.
         */
        pchMessageStart[0] = 0xd3;
        pchMessageStart[1] = 0xc4;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xa6;
        nDefaultPort = 45874;
        nPruneAfterHeight = 100000;

        genesis = CreateGenesisBlock(1772474096, 136519, 0x1e0ffff0, 1, 88 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        digishieldConsensus.hashGenesisBlock = consensus.hashGenesisBlock;
        auxpowConsensus.hashGenesisBlock = consensus.hashGenesisBlock;
        assert(consensus.hashGenesisBlock == uint256S("0xb995d8d81cfb8c8cff5829b23cbff8ff4b34347f1c48c7e098ea7fb416a56951"));
        assert(genesis.hashMerkleRoot == uint256S("0xd22a1ba59a39cbd5904624933efb822c8baa121f97060c4cc9ea2f00a4bc6512"));

        // Note that of those with the service bits flag, most only support a subset of possible options
        vSeeds.clear();
        vSeeds.push_back(CDNSSeedData("kubu.supply", "seeds.kubu.supply"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,45); // Addresses start with K
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,22);
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,158);
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x02)(0xfa)(0xca)(0xfd).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x02)(0xfa)(0xc3)(0x98).convert_to_container<std::vector<unsigned char> >();

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_main, pnSeed6_main + ARRAYLEN(pnSeed6_main));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = true;
        fMineBlocksOnDemand = false;

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            (0, uint256S("0xb995d8d81cfb8c8cff5829b23cbff8ff4b34347f1c48c7e098ea7fb416a56951"))
            (1000, uint256S("0x1561d231b29d3b64b6b560f2e6eb462aa04fd7f295d02b5a40aaac801d8a0d01"))
            (5000, uint256S("0x09fab2e11c98c3653a21d0984f2183f0e8e388e71a3b80c31cb44adc4f307e2e"))
            (10000, uint256S("0xb2982cefe367b6c90bac8c4a9e29c22bbf2a404a482caf835e98c1cf8d661a3e"))
            (20000, uint256S("0x5762f555260c353042335f9d09f49f5403c7edf8a7b5e20ac132d5f12843fc51"))
            (30000, uint256S("0x846a49d7097c84d137c553a828b683c3c65bed00971cb617a6e3097413ab31a6"))
            (39000, uint256S("0x24386bf570dba80ff11fba1961f5f85e822b19ea6acd88ac64c838c8fdc5d0cc"))
            (40000, uint256S("0xb6557f7c7bc0a38cd3dd09440188af5534861cc41969b03d7b2cc1b0ed4b3396"))
            (50000, uint256S("0x376eeb8290918072e2778e348006438eba345e77e401bc28f2f4c0b310385811"))
            (60000, uint256S("0x1f534e0c5a54c8820c275ec881586db34ae74221989acf7e407bb64b1c6ba32d"))
            (70000, uint256S("0xb66ae689eef9a2daf88150ca63febb013e7d1bbb4a3ccf6c6b6650a88900d902"))
            (80000, uint256S("0x421e258c91c82fc8a9b18f91e0264334bf9c9146af267c542e5ef4950282fb59"))
            (84000, uint256S("0xcf601dd82c845599c4551be51f4112a48ae041e2d144a6bc98d2e57dfaa6b41b"))
            (84891, uint256S("0x6a27cb2f5a4c2e3b56796e417e0098e1571fd0cb4536721a5b742be9f1da4f1b"))
        };

        chainTxData = ChainTxData{
            1778108452,
            88721,
            0.015746253875331982,
        };
    }
};
static CMainParams mainParams;

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
private:
    Consensus::Params digishieldConsensus;
    Consensus::Params auxpowConsensus;
    Consensus::Params minDifficultyConsensus;
public:
    CTestNetParams() {
        strNetworkID = "test";

        consensus.nSubsidyHalvingInterval = 1168000;
        consensus.nMajorityEnforceBlockUpgrade = 501;
        consensus.nMajorityRejectBlockOutdated = 750;
        consensus.nMajorityWindow = 1000;
        // BIP34 is never enforced in KUBU v2 blocks, so we enforce from v3
        consensus.BIP34Height = 1000;
        consensus.BIP34Hash = uint256S("0x00");
        consensus.BIP65Height = 1000; // 
        consensus.BIP66Height = 1000; // - this is the last block that could be v2, 1900 blocks past the last v2 block
        consensus.powLimit = uint256S("0x00000fffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 20;
        consensus.nPowTargetTimespan = 54;
        consensus.nPowTargetSpacing = 54;
        consensus.fDigishieldDifficultyCalculation = true;
        consensus.nCoinbaseMaturity = 30;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowAllowDigishieldMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;
        consensus.nRuleChangeActivationThreshold = 3200;
        consensus.nMinerConfirmationWindow = 3360;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 1199145601; // January 1, 2008
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 1230767999; // December 31, 2008

        // Deployment of BIP68, BIP112, and BIP113.
        // XXX: BIP heights and hashes all need to be updated to KUBU values
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 1456790400; // March 1st, 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 1493596800; // May 1st, 2017

        // Deployment of SegWit (BIP141, BIP143, and BIP147)
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 1462060800; // May 1st 2016
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 0; // Disabled

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        // AuxPoW parameters
        consensus.nAuxpowChainId = 0x003f; // 63
        consensus.fStrictChainId = false;
        consensus.nHeightEffective = 0;
        consensus.fAllowLegacyBlocks = true;
        consensus.fSimplifiedRewards = true;

        // Blocks 1000 - 1249 are Digishield without minimum difficulty on all blocks
        digishieldConsensus = consensus;
        digishieldConsensus.nHeightEffective = 1000;
        digishieldConsensus.nPowTargetTimespan = 54;
        digishieldConsensus.fDigishieldDifficultyCalculation = true;
        digishieldConsensus.fSimplifiedRewards = true;
        digishieldConsensus.fPowAllowMinDifficultyBlocks = false;
        digishieldConsensus.nCoinbaseMaturity = 240;

        // Blocks 1250 - 38,999 are Digishield with minimum difficulty on all blocks
        minDifficultyConsensus = digishieldConsensus;
        minDifficultyConsensus.nHeightEffective = 1250;
        minDifficultyConsensus.fPowAllowDigishieldMinDifficultyBlocks = true;
        minDifficultyConsensus.fPowAllowMinDifficultyBlocks = true;

        // Enable AuxPoW at 39,000
        auxpowConsensus = minDifficultyConsensus;
        auxpowConsensus.nHeightEffective = 39000;
        auxpowConsensus.fPowAllowDigishieldMinDifficultyBlocks = true;
        auxpowConsensus.fAllowLegacyBlocks = false;

        // Assemble the binary search tree of parameters
        pConsensusRoot = &digishieldConsensus;
        digishieldConsensus.pLeft = &consensus;
        digishieldConsensus.pRight = &minDifficultyConsensus;
        minDifficultyConsensus.pRight = &auxpowConsensus;

        pchMessageStart[0] = 0xfe;
        pchMessageStart[1] = 0xc1;
        pchMessageStart[2] = 0xdb;
        pchMessageStart[3] = 0xcc;
        nDefaultPort = 46874;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1705975260, 3476, 0x1e0ffff0, 1, 88 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        digishieldConsensus.hashGenesisBlock = consensus.hashGenesisBlock;
        minDifficultyConsensus.hashGenesisBlock = consensus.hashGenesisBlock;
        auxpowConsensus.hashGenesisBlock = consensus.hashGenesisBlock;

        assert(consensus.hashGenesisBlock == uint256S("0xf9f4ea4ae7f6ea4c55040ede2019ba0a53e262f46ec9bce3dcda2cb11f96fc52"));
        assert(genesis.hashMerkleRoot == uint256S("0xd22a1ba59a39cbd5904624933efb822c8baa121f97060c4cc9ea2f00a4bc6512"));

        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.push_back(CDNSSeedData("kubucoin.org", "seeds-testnet.kubucoin.org"));
        vSeeds.push_back(CDNSSeedData("kubucoin.org", "seeds-testnet2.kubucoin.org"));

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,113); // 0x71
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196); // 0xc4
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,241); // 0xf1
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xcf).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();

        vFixedSeeds = std::vector<SeedSpec6>(pnSeed6_test, pnSeed6_test + ARRAYLEN(pnSeed6_test));

        fMiningRequiresPeers = true;
        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        fMineBlocksOnDemand = false;

        checkpointData = (CCheckpointData) {
            boost::assign::map_list_of
            (0, uint256S("0xf9f4ea4ae7f6ea4c55040ede2019ba0a53e262f46ec9bce3dcda2cb11f96fc52"))
        };

        chainTxData = ChainTxData{ };
    }
};
static CTestNetParams testNetParams;

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
private:
    Consensus::Params digishieldConsensus;
    Consensus::Params auxpowConsensus;
public:
    CRegTestParams() {
        strNetworkID = "regtest";
        consensus.nSubsidyHalvingInterval = 150;
        consensus.nMajorityEnforceBlockUpgrade = 750;
        consensus.nMajorityRejectBlockOutdated = 950;
        consensus.nMajorityWindow = 1000;
        consensus.BIP34Height = 100000000; // BIP34 has not activated on regtest (far in the future so block v1 are not rejected in tests)
        consensus.BIP34Hash = uint256();
        consensus.BIP65Height = 1351; // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251; // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.powLimit = uint256S("0x7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff"); // ~uint256(0) >> 1;
        consensus.nPowTargetTimespan = 4 * 60 * 60; // pre-digishield: 4 hours
        consensus.nPowTargetSpacing = 1; // regtest: 1 second blocks
        consensus.fDigishieldDifficultyCalculation = false;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;
        consensus.nRuleChangeActivationThreshold = 540; // 75% for testchains
        consensus.nMinerConfirmationWindow = 720; // Faster than normal for regtest (2,520 instead of 10,080)
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].bit = 28;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].bit = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_CSV].nTimeout = 999999999999ULL;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].bit = 1;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nStartTime = 0;
        consensus.vDeployments[Consensus::DEPLOYMENT_SEGWIT].nTimeout = 999999999999ULL;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are valid.
        consensus.defaultAssumeValid = uint256S("0x00");

        // AuxPow parameters
        consensus.nAuxpowChainId = 0x003f; // 63
        consensus.fStrictChainId = true;
        consensus.fAllowLegacyBlocks = true;

        // KUBU parameters
        consensus.fSimplifiedRewards = true;
        consensus.nCoinbaseMaturity = 60; // For easier testability in RPC tests

        digishieldConsensus = consensus;
        digishieldConsensus.nHeightEffective = 10;
        digishieldConsensus.nPowTargetTimespan = 1; // regtest: also retarget every second in digishield mode, for conformity
        digishieldConsensus.fDigishieldDifficultyCalculation = true;

        auxpowConsensus = digishieldConsensus;
        auxpowConsensus.fAllowLegacyBlocks = false;
        auxpowConsensus.nHeightEffective = 20;

        // Assemble the binary search tree of parameters
        digishieldConsensus.pLeft = &consensus;
        digishieldConsensus.pRight = &auxpowConsensus;
        pConsensusRoot = &digishieldConsensus;

        pchMessageStart[0] = 0xfa;
        pchMessageStart[1] = 0xbf;
        pchMessageStart[2] = 0xb5;
        pchMessageStart[3] = 0xda;
        nDefaultPort = 47874;
        nPruneAfterHeight = 1000;

        genesis = CreateGenesisBlock(1296688602, 0, 0x207fffff, 1, 88 * COIN);
        // MineGenesis(genesis, consensus.powLimit, true);
        consensus.hashGenesisBlock = genesis.GetHash();
        digishieldConsensus.hashGenesisBlock = consensus.hashGenesisBlock;
        auxpowConsensus.hashGenesisBlock = consensus.hashGenesisBlock;
        assert(consensus.hashGenesisBlock == uint256S("0x770975b0f98319520694563de107ff94fd501c0d1c16f3a405868faf36b51c28"));
        assert(genesis.hashMerkleRoot == uint256S("0xd22a1ba59a39cbd5904624933efb822c8baa121f97060c4cc9ea2f00a4bc6512"));

        vFixedSeeds.clear(); //!< Regtest mode doesn't have any fixed seeds.
        vSeeds.clear();      //!< Regtest mode doesn't have any DNS seeds.

        fMiningRequiresPeers = false;
        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        fMineBlocksOnDemand = true;

        checkpointData = (CCheckpointData){
            boost::assign::map_list_of
            ( 0, uint256S("0x770975b0f98319520694563de107ff94fd501c0d1c16f3a405868faf36b51c28"))
        };

        chainTxData = ChainTxData{
            0,
            0,
            0
        };

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<unsigned char>(1,111);  // 0x6f
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<unsigned char>(1,196);  // 0xc4
        base58Prefixes[SECRET_KEY] =     std::vector<unsigned char>(1,239);  // 0xef
        base58Prefixes[EXT_PUBLIC_KEY] = boost::assign::list_of(0x04)(0x35)(0x87)(0xCF).convert_to_container<std::vector<unsigned char> >();
        base58Prefixes[EXT_SECRET_KEY] = boost::assign::list_of(0x04)(0x35)(0x83)(0x94).convert_to_container<std::vector<unsigned char> >();
    }

    void UpdateBIP9Parameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
    {
        consensus.vDeployments[d].nStartTime = nStartTime;
        consensus.vDeployments[d].nTimeout = nTimeout;
    }
};
static CRegTestParams regTestParams;

static CChainParams *pCurrentParams = 0;

const CChainParams &Params() {
    assert(pCurrentParams);
    return *pCurrentParams;
}

const Consensus::Params *Consensus::Params::GetConsensus(uint32_t nTargetHeight) const {
    if (nTargetHeight < this -> nHeightEffective && this -> pLeft != NULL) {
        return this -> pLeft -> GetConsensus(nTargetHeight);
    } else if (nTargetHeight > this -> nHeightEffective && this -> pRight != NULL) {
        const Consensus::Params *pCandidate = this -> pRight -> GetConsensus(nTargetHeight);
        if (pCandidate->nHeightEffective <= nTargetHeight) {
            return pCandidate;
        }
    }

    // No better match below the target height
    return this;
}

CChainParams& Params(const std::string& chain)
{
    if (chain == CBaseChainParams::MAIN)
            return mainParams;
    else if (chain == CBaseChainParams::TESTNET)
            return testNetParams;
    else if (chain == CBaseChainParams::REGTEST)
            return regTestParams;
    else
        throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
    SelectBaseParams(network);
    pCurrentParams = &Params(network);
}

void UpdateRegtestBIP9Parameters(Consensus::DeploymentPos d, int64_t nStartTime, int64_t nTimeout)
{
    regTestParams.UpdateBIP9Parameters(d, nStartTime, nTimeout);
}
