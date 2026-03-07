#!/usr/bin/env python3
# Copyright (c) 2026 The KUBU developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import *


class NicknameRPCTest(BitcoinTestFramework):
    def __init__(self):
        super().__init__()
        self.setup_clean_chain = True
        self.num_nodes = 2

    def setup_network(self):
        extra_args = []
        for i in range(self.num_nodes):
            extra_args.append([
                f"-port={p2p_port(i)}",
                f"-rpcport={rpc_port(i)}",
                "-nicknameactiveblocks=10",
                "-nicknamegraceblocks=5",
            ])
        self.nodes = start_nodes(self.num_nodes, self.options.tmpdir, extra_args)
        connect_nodes_bi(self.nodes, 0, 1)
        self.is_network_split = False
        self.sync_all()

    def has_nickname(self, entries, nickname):
        return any(item["nickname"] == nickname for item in entries)

    def find_nulldata_script_hex(self, decoded_tx):
        for vout in decoded_tx["vout"]:
            script_pub_key = vout["scriptPubKey"]
            if script_pub_key.get("type") == "nulldata":
                return script_pub_key.get("hex", "")
        return ""

    def run_test(self):
        node0 = self.nodes[0]
        node1 = self.nodes[1]

        print("Preparing mature balance on node0...")
        node0.generate(200)
        self.sync_all()
        assert_greater_than(node0.getbalance(), Decimal("0"))

        print("Checking nickname normalization and pricing RPC...")
        check = node0.checknickname("KuBu_DeV")
        assert_equal(check["normalized"], "kubu_dev")
        assert_equal(check["valid"], True)
        assert_greater_than(check["registration_fee"], Decimal("0"))
        assert_greater_than(check["renewal_fee"], Decimal("0"))
        registration_funding = check["registration_fee"] + check["bond_amount"] + Decimal("10")
        renewal_funding = check["renewal_fee"] + Decimal("1")

        nickname = "kubu_dev"
        owner_addr0 = node0.getnewaddress()
        owner_pubkey0 = node0.validateaddress(owner_addr0)["pubkey"]
        payout_addr0 = node0.getnewaddress()

        foreign_addr = node1.getnewaddress()
        foreign_pubkey = node1.validateaddress(foreign_addr)["pubkey"]

        print("Verifying owner key checks before registration...")
        assert_raises_message(
            JSONRPCException,
            "Wallet does not have private key",
            node0.registernickname,
            nickname,
            foreign_pubkey,
            payout_addr0,
        )

        assert_raises_message(
            JSONRPCException,
            "No spendable wallet UTXO found",
            node0.registernickname,
            nickname,
            owner_pubkey0,
            payout_addr0,
        )

        print("Funding owner key and registering nickname...")
        node0.sendtoaddress(owner_addr0, registration_funding)
        node0.generate(1)
        self.sync_all()

        reg_txid = node0.registernickname(nickname, owner_pubkey0, payout_addr0)
        assert_is_hash_string(reg_txid)
        assert reg_txid in node0.getrawmempool()
        node0.generate(1)
        self.sync_all()

        info = node0.getnicknameinfo(nickname)
        assert_equal(info["nickname"], nickname)
        assert_equal(info["owner_pubkey"], owner_pubkey0.lower())
        assert_equal(info["payout_address"], payout_addr0)
        assert_equal(info["status"], "ACTIVE")
        print("Verifying bond UTXO cannot be spent without nickname operation...")
        raw_bond_spend = node0.createrawtransaction(
            [{"txid": info["bond_txid"], "vout": info["bond_vout"]}],
            {node0.getnewaddress(): info["bond_amount"] - Decimal("1")}
        )
        signed_bond_spend = node0.signrawtransaction(raw_bond_spend)
        assert_equal(signed_bond_spend["complete"], True)
        assert_raises_message(
            JSONRPCException,
            "bad-nickname-op",
            node0.sendrawtransaction,
            signed_bond_spend["hex"],
        )
        assert_equal(node0.resolvenickname(nickname)["resolves"], True)
        print("Checking KMEM1 memo encode/decode and sendtonickname attachment...")
        memo_encoded = node0.encodenicknamememo("ORDER123", "alnum")
        assert_equal(memo_encoded["memo"], "ORDER123")
        assert_equal(memo_encoded["memo_type"], "alnum")
        assert_equal(memo_encoded["payload_bytes"], 18)
        memo_decoded = node0.decodenicknamememo(memo_encoded["payload_hex"])
        assert_equal(memo_decoded["memo"], "ORDER123")
        assert_equal(memo_decoded["memo_type"], "alnum")
        assert_equal(memo_decoded["version"], 1)
        assert_raises_message(
            JSONRPCException,
            "Invalid memo_type",
            node0.encodenicknamememo,
            "ORDER123",
            "invalid",
        )
        assert_raises_message(
            JSONRPCException,
            "Numeric memo may only contain digits",
            node0.encodenicknamememo,
            "AB12",
            "numeric",
        )
        send_nick_memo_txid = node0.sendtonickname(nickname, Decimal("0.11"), "", "", False, "ORDER123", "alnum")
        assert_is_hash_string(send_nick_memo_txid)
        memo_wallet_tx = node0.gettransaction(send_nick_memo_txid)
        assert_equal(memo_wallet_tx["nickname_to"], nickname)
        assert_equal(memo_wallet_tx["nickname_memo"], "ORDER123")
        assert_equal(memo_wallet_tx["nickname_memo_type"], "alnum")
        memo_raw_tx = node0.getrawtransaction(send_nick_memo_txid, True)
        memo_script_hex = self.find_nulldata_script_hex(memo_raw_tx)
        assert memo_script_hex
        memo_decoded_script = node0.decodenicknamememo(memo_script_hex)
        assert_equal(memo_decoded_script["memo"], "ORDER123")
        assert_equal(memo_decoded_script["memo_type"], "alnum")
        assert_equal(memo_decoded_script["version"], 1)
        send_nick_txid = node0.sendtonickname(nickname, Decimal("0.25"))
        assert_is_hash_string(send_nick_txid)
        send_nick_prefixed_txid = node0.sendtonickname("@" + nickname, Decimal("0.10"))
        assert_is_hash_string(send_nick_prefixed_txid)
        send_many_txid = node0.sendmany("", {nickname: Decimal("0.05")})
        assert_is_hash_string(send_many_txid)
        send_many_prefixed_txid = node0.sendmany("", {"@" + nickname: Decimal("0.05")})
        assert_is_hash_string(send_many_prefixed_txid)
        node0.generate(1)
        self.sync_all()
        assert_greater_than(node0.getreceivedbyaddress(payout_addr0, 0), Decimal("0"))

        assert_equal(self.has_nickname(node0.listwalletnicknames("", 50), nickname), True)
        assert_equal(self.has_nickname(node1.listwalletnicknames("", 50), nickname), False)

        print("Updating payout address...")
        payout_addr1 = node0.getnewaddress()
        node0.sendtoaddress(owner_addr0, Decimal("1"))
        node0.sendtoaddress(owner_addr0, Decimal("1"))
        node0.generate(1)
        self.sync_all()
        up_txid = node0.updatenickname(nickname, payout_addr1)
        assert_is_hash_string(up_txid)

        assert_raises_message(
            JSONRPCException,
            "bad-nickname-op",
            node0.updatenickname,
            nickname,
            node0.getnewaddress(),
        )

        node0.generate(1)
        self.sync_all()

        info = node0.getnicknameinfo(nickname)
        assert_equal(info["payout_address"], payout_addr1)
        send_nick_txid = node0.sendtonickname(nickname, Decimal("0.15"))
        assert_is_hash_string(send_nick_txid)
        send_many_txid = node0.sendmany("", {nickname: Decimal("0.05")})
        assert_is_hash_string(send_many_txid)
        node0.generate(1)
        self.sync_all()
        assert_greater_than(node0.getreceivedbyaddress(payout_addr1, 0), Decimal("0"))

        print("Preparing node1 owner and transferring nickname...")
        owner_addr1 = node1.getnewaddress()
        owner_pubkey1 = node1.validateaddress(owner_addr1)["pubkey"]
        node0.sendtoaddress(owner_addr1, Decimal("1"))
        node0.generate(1)
        self.sync_all()

        node0.sendtoaddress(owner_addr0, Decimal("1"))
        node0.generate(1)
        self.sync_all()
        tr_txid = node0.transfernickname(nickname, owner_pubkey1)
        assert_is_hash_string(tr_txid)
        node0.generate(1)
        self.sync_all()

        info = node0.getnicknameinfo(nickname)
        assert_equal(info["owner_pubkey"], owner_pubkey1.lower())

        assert_raises_message(
            JSONRPCException,
            "No spendable wallet UTXO found",
            node0.updatenickname,
            nickname,
            node0.getnewaddress(),
        )

        assert_raises_message(
            JSONRPCException,
            "nickname bond is not claimable",
            node1.claimnicknamebond,
            nickname,
        )

        print("Renewing, releasing, and claiming bond from new owner...")
        node0.sendtoaddress(owner_addr1, renewal_funding)
        node0.generate(1)
        self.sync_all()
        rn_txid = node1.renewnickname(nickname)
        assert_is_hash_string(rn_txid)
        node1.generate(1)
        self.sync_all()

        node0.sendtoaddress(owner_addr1, Decimal("1"))
        node0.generate(1)
        self.sync_all()
        rl_txid = node1.releasenickname(nickname)
        assert_is_hash_string(rl_txid)
        node1.generate(1)
        self.sync_all()

        info = node1.getnicknameinfo(nickname)
        assert_equal(info["status"], "BOND_CLAIMABLE")
        assert_equal(info["claimable_bond"], True)
        assert_equal(node1.resolvenickname(nickname)["resolves"], False)

        assert_raises_message(
            JSONRPCException,
            "nickname is not releasable in current status",
            node0.releasenickname,
            nickname,
        )
        assert_raises_message(
            JSONRPCException,
            "nickname is not renewable in current status",
            node0.renewnickname,
            nickname,
        )

        node0.sendtoaddress(owner_addr1, Decimal("1"))
        node0.generate(1)
        self.sync_all()
        cb_txid = node1.claimnicknamebond(nickname)
        assert_is_hash_string(cb_txid)
        node1.generate(1)
        self.sync_all()

        info = node1.getnicknameinfo(nickname)
        assert_equal(info["status"], "RELEASED")
        assert_equal(info["bond_claimed"], True)
        assert_equal(info["claimable_bond"], False)

        assert_raises_message(
            JSONRPCException,
            "nickname is not mutable in current status",
            node1.updatenickname,
            nickname,
            node1.getnewaddress(),
        )
        assert_raises_message(
            JSONRPCException,
            "nickname is not renewable in current status",
            node1.renewnickname,
            nickname,
        )
        assert_raises_message(
            JSONRPCException,
            "nickname bond is not claimable",
            node1.claimnicknamebond,
            nickname,
        )

        assert_equal(self.has_nickname(node0.listwalletnicknames("", 50), nickname), False)
        assert_equal(self.has_nickname(node1.listwalletnicknames("", 50), nickname), True)

        expiring_nickname = "kubu_expire"
        exp_owner_addr = node0.getnewaddress()
        exp_owner_pubkey = node0.validateaddress(exp_owner_addr)["pubkey"]
        exp_payout_addr = node0.getnewaddress()
        exp_check = node0.checknickname(expiring_nickname)
        exp_registration_funding = exp_check["registration_fee"] + exp_check["bond_amount"] + Decimal("10")

        print("Registering an expiring nickname and walking through expiry states...")
        node0.sendtoaddress(exp_owner_addr, exp_registration_funding)
        node0.generate(1)
        self.sync_all()

        exp_txid = node0.registernickname(expiring_nickname, exp_owner_pubkey, exp_payout_addr)
        assert_is_hash_string(exp_txid)
        node0.generate(1)
        self.sync_all()

        exp_info = node0.getnicknameinfo(expiring_nickname)
        assert_equal(exp_info["status"], "ACTIVE")
        assert_equal(node0.resolvenickname(expiring_nickname)["resolves"], True)
        active_until = exp_info["active_until"]
        grace_until = exp_info["grace_until"]

        blocks_to_expired_grace = active_until + 1 - node0.getblockcount()
        if blocks_to_expired_grace > 0:
            node0.generate(blocks_to_expired_grace)
            self.sync_all()

        exp_info = node0.getnicknameinfo(expiring_nickname)
        assert_equal(exp_info["status"], "EXPIRED_GRACE")
        assert_equal(node0.resolvenickname(expiring_nickname)["resolves"], False)

        blocks_to_bond_claimable = grace_until + 1 - node0.getblockcount()
        if blocks_to_bond_claimable > 0:
            node0.generate(blocks_to_bond_claimable)
            self.sync_all()

        exp_info = node0.getnicknameinfo(expiring_nickname)
        assert_equal(exp_info["status"], "BOND_CLAIMABLE")
        old_bond_ref = "{}:{}".format(exp_info["bond_txid"], exp_info["bond_vout"])

        assert_equal(node0.resolvenickname(expiring_nickname)["resolves"], False)
        assert_raises_message(
            JSONRPCException,
            "nickname is not active",
            node0.sendtonickname,
            expiring_nickname,
            Decimal("0.1"),
        )
        assert_raises_message(
            JSONRPCException,
            "nickname is not active",
            node0.sendmany,
            "",
            {expiring_nickname: Decimal("0.1")},
        )

        reregis_owner_addr = node1.getnewaddress()
        reregis_owner_pubkey = node1.validateaddress(reregis_owner_addr)["pubkey"]
        reregis_payout_addr = node1.getnewaddress()
        reregis_check = node0.checknickname(expiring_nickname)
        reregis_registration_funding = reregis_check["registration_fee"] + reregis_check["bond_amount"] + Decimal("10")

        node0.sendtoaddress(reregis_owner_addr, reregis_registration_funding)
        node0.generate(1)
        self.sync_all()

        reregis_txid = node1.registernickname(expiring_nickname, reregis_owner_pubkey, reregis_payout_addr)
        assert_is_hash_string(reregis_txid)
        node1.generate(1)
        self.sync_all()

        exp_info = node0.getnicknameinfo(expiring_nickname)
        assert_equal(exp_info["status"], "ACTIVE")
        assert_equal(exp_info["owner_pubkey"], reregis_owner_pubkey.lower())
        assert_equal(exp_info["payout_address"], reregis_payout_addr)
        assert_equal(node0.resolvenickname(expiring_nickname)["resolves"], True)
        assert_raises_message(
            JSONRPCException,
            "nickname bond is not claimable",
            node0.claimnicknamebond,
            expiring_nickname,
        )

        legacy_claim_txid = node0.claimnicknamebond(old_bond_ref)
        assert_is_hash_string(legacy_claim_txid)
        node0.generate(1)
        self.sync_all()

        exp_info = node0.getnicknameinfo(expiring_nickname)
        assert_equal(exp_info["status"], "ACTIVE")
        assert_equal(exp_info["owner_pubkey"], reregis_owner_pubkey.lower())
        assert_equal(exp_info["payout_address"], reregis_payout_addr)
        assert_equal(node0.resolvenickname(expiring_nickname)["resolves"], True)

        send_exp_txid = node0.sendtonickname(expiring_nickname, Decimal("0.2"))
        assert_is_hash_string(send_exp_txid)
        send_exp_many_txid = node0.sendmany("", {expiring_nickname: Decimal("0.05")})
        assert_is_hash_string(send_exp_many_txid)
        node0.generate(1)
        self.sync_all()
        assert_greater_than(node1.getreceivedbyaddress(reregis_payout_addr, 0), Decimal("0"))
        assert_equal(self.has_nickname(node0.listwalletnicknames("", 50), expiring_nickname), False)
        assert_equal(self.has_nickname(node1.listwalletnicknames("", 50), expiring_nickname), True)


if __name__ == '__main__':
    NicknameRPCTest().main()
