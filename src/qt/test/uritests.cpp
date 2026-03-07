// Copyright (c) 2009-2014 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "uritests.h"

#include "guiutil.h"
#include "walletmodel.h"

#include <QUrl>

void URITests::uriTests()
{
    SendCoinsRecipient rv;
    QUrl uri;
    uri.setUrl(QString("kubu:PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS?req-dontexist="));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("kubu:PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS?dontexist="));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("kubu:PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS?label=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS"));
    QVERIFY(rv.label == QString("Wikipedia Example Address"));
    QVERIFY(rv.amount == 0);

    uri.setUrl(QString("kubu:PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS?amount=0.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100000);

    uri.setUrl(QString("kubu:PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS?amount=1.001"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS"));
    QVERIFY(rv.label == QString());
    QVERIFY(rv.amount == 100100000);

    uri.setUrl(QString("kubu:PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS?amount=100&label=Wikipedia Example"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS"));
    QVERIFY(rv.amount == 10000000000LL);
    QVERIFY(rv.label == QString("Wikipedia Example"));

    uri.setUrl(QString("kubu:PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS?message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS"));
    QVERIFY(rv.label == QString());

    QVERIFY(GUIUtil::parseBitcoinURI("kubu://PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS?message=Wikipedia Example Address", &rv));
    QVERIFY(rv.address == QString("PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS"));
    QVERIFY(rv.label == QString());

    uri.setUrl(QString("kubu:PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS?req-message=Wikipedia Example Address"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("kubu:@mexc?amount=1.25&memo=123456&memo_type=numeric&req-memo=1"));
    QVERIFY(GUIUtil::parseBitcoinURI(uri, &rv));
    QVERIFY(rv.address == QString("@mexc"));
    QVERIFY(rv.amount == 125000000);
    QVERIFY(rv.nicknameMemo == QString("123456"));
    QVERIFY(rv.nicknameMemoType == QString("numeric"));
    QVERIFY(rv.nicknameMemoRequired);

    uri.setUrl(QString("kubu:@mexc?memo_type=badtype"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    SendCoinsRecipient uriRecipient;
    uriRecipient.address = "@mexc";
    uriRecipient.amount = 125000000;
    uriRecipient.nicknameMemo = "ABCD1234";
    uriRecipient.nicknameMemoType = "alnum";
    uriRecipient.nicknameMemoRequired = true;
    QString formattedUri = GUIUtil::formatBitcoinURI(uriRecipient);
    QVERIFY(formattedUri.contains("memo=ABCD1234"));
    QVERIFY(formattedUri.contains("memo_type=alnum"));
    QVERIFY(formattedUri.contains("req-memo=1"));

    uri.setUrl(QString("kubu:PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS?amount=1,000&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));

    uri.setUrl(QString("kubu:PvN5ZkKHqSbexjvKjuTpBTQJERL3qSHrhS?amount=1,000.0&label=Wikipedia Example"));
    QVERIFY(!GUIUtil::parseBitcoinURI(uri, &rv));
}
