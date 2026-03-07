// Copyright (c) 2026 The KUBU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "nicknamespage.h"

#include "nicknamedb.h"
#include "nicknames.h"
#include "rpc/server.h"
#include "recentrequeststablemodel.h"
#include "utilmoneystr.h"
#include "walletmodel.h"

#include "base58.h"
#include "hash.h"
#include "pubkey.h"
#include "sync.h"
#include "utilstrencodings.h"
#include "validation.h"

#include <univalue.h>

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPoint>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringList>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <limits>
#include <set>

namespace {
bool WalletOwnsNickname(WalletModel* model, const NicknameInfo& info)
{
    if (!model || info.ownerPubKey.size() != CPubKey::COMPRESSED_SIZE) {
        return false;
    }

    const CKeyID keyID(Hash160(info.ownerPubKey.begin(), info.ownerPubKey.end()));
    return model->havePrivKey(keyID);
}

bool IsMutableStatus(const Nicknames::Status status)
{
    return status == Nicknames::Status::ACTIVE || status == Nicknames::Status::EXPIRED_GRACE;
}

bool IsOwnerInputMissingError(const QString& error)
{
    return error.contains("No spendable wallet UTXO found", Qt::CaseInsensitive) &&
           error.contains("nickname owner pubkey", Qt::CaseInsensitive);
}

QString RpcValueToQString(const UniValue& value)
{
    if (value.isNull()) {
        return "-";
    }
    if (value.isStr()) {
        return QString::fromStdString(value.get_str());
    }
    return QString::fromStdString(value.write());
}

QString RpcResultToString(const UniValue& value)
{
    if (value.isStr()) {
        return QString::fromStdString(value.get_str());
    }
    return QString::fromStdString(value.write());
}

QString CompactDecimalAmount(const QString& text)
{
    const QRegularExpression pattern("^(-?\\d+)\\.(\\d+)$");
    const QRegularExpressionMatch match = pattern.match(text.trimmed());
    if (!match.hasMatch()) {
        return text;
    }

    const QString intPart = match.captured(1);
    QString fraction = match.captured(2);
    while (!fraction.isEmpty() && fraction.endsWith('0')) {
        fraction.chop(1);
    }

    if (fraction.isEmpty()) {
        return intPart;
    }

    return intPart + "." + fraction;
}

QString GroupDecimalIntegerPart(const QString& digits)
{
    QString grouped;
    grouped.reserve(digits.size() + digits.size() / 3);
    for (int i = 0; i < digits.size(); ++i) {
        const int left = digits.size() - i;
        grouped.append(digits.at(i));
        if (left > 1 && (left - 1) % 3 == 0) {
            grouped.append(' ');
        }
    }
    return grouped;
}

QString FormatKubuAmount(const QString& value)
{
    const QString compact = CompactDecimalAmount(value.trimmed());
    const QRegularExpression pattern("^(-?)(\\d+)(?:\\.(\\d+))?$");
    const QRegularExpressionMatch match = pattern.match(compact);
    if (!match.hasMatch()) {
        return compact;
    }

    const QString sign = match.captured(1);
    const QString intPart = GroupDecimalIntegerPart(match.captured(2));
    const QString fracPart = match.captured(3);
    if (fracPart.isEmpty()) {
        return sign + intPart + " KUBU";
    }

    return sign + intPart + "." + fracPart + " KUBU";
}

QString RpcAmountToQString(const UniValue& value)
{
    if (value.isNull()) {
        return "-";
    }
    if (value.isNum()) {
        return FormatKubuAmount(QString::fromStdString(value.getValStr()));
    }
    return FormatKubuAmount(RpcValueToQString(value));
}

CAmount RpcAmountToNative(const UniValue& value, const CAmount fallback = 0)
{
    if (!value.isNum()) {
        return fallback;
    }

    CAmount amount = 0;
    if (!ParseFixedPoint(value.getValStr(), 8, &amount)) {
        return fallback;
    }
    return amount;
}

QString FormatKubuAmountFromNative(const CAmount amount)
{
    return FormatKubuAmount(QString::fromStdString(FormatMoney(amount)));
}

QString TimeInfoForStatus(const NicknameInfo& info, const Nicknames::Status status, const int currentHeight)
{
    if (status == Nicknames::Status::ACTIVE) {
        const int activeLeft = info.activeUntilHeight >= currentHeight ? (info.activeUntilHeight - currentHeight) : 0;
        return QObject::tr("Active, %1 blocks remaining").arg(activeLeft);
    }
    if (status == Nicknames::Status::EXPIRED_GRACE) {
        const int graceLeft = info.graceUntilHeight >= currentHeight ? (info.graceUntilHeight - currentHeight) : 0;
        return QObject::tr("Grace period, %1 blocks to renew").arg(graceLeft);
    }
    if (status == Nicknames::Status::BOND_CLAIMABLE) {
        return QObject::tr("Ended: bond can be claimed now");
    }
    if (status == Nicknames::Status::RELEASED) {
        return QObject::tr("Ended: released by owner, bond claimed");
    }
    if (status == Nicknames::Status::EXPIRED_AVAILABLE) {
        return QObject::tr("Ended: expired, available for registration");
    }
    return "-";
}

QString StatusLabelForUi(const Nicknames::Status status, const bool compactForHistory = false)
{
    if (compactForHistory &&
        (status == Nicknames::Status::RELEASED || status == Nicknames::Status::EXPIRED_AVAILABLE)) {
        return QObject::tr("ENDED");
    }
    if (status == Nicknames::Status::EXPIRED_AVAILABLE) {
        return QObject::tr("AVAILABLE");
    }
    return QString::fromStdString(Nicknames::StatusToString(status));
}

QString BondInfoForStatus(const NicknameInfo& info, const Nicknames::Status status)
{
    if (info.bondAmount <= 0) {
        return "-";
    }
    if (info.bondClaimed) {
        return QObject::tr("claimed");
    }
    if (status == Nicknames::Status::BOND_CLAIMABLE) {
        return QObject::tr("claimable %1").arg(FormatKubuAmountFromNative(info.bondAmount));
    }
    return QObject::tr("locked %1").arg(FormatKubuAmountFromNative(info.bondAmount));
}
} // namespace

NicknamesPage::NicknamesPage(QWidget* parent)
    : QWidget(parent),
      model(nullptr),
      nicknameEdit(new QLineEdit(this)),
      normalizedValue(new QLabel("-", this)),
      statusValue(new QLabel("-", this)),
      availabilityValue(new QLabel("-", this)),
      pricingValue(new QLabel("-", this)),
      lookupButton(new QPushButton(tr("Check"), this)),
      actionsButton(new QPushButton(tr("Actions"), this)),
      actionStatusValue(new QLabel("-", this)),
      walletAlertsValue(new QLabel("-", this)),
      refreshButton(new QPushButton(tr("Refresh"), this)),
      walletNicknamesTabs(new QTabWidget(this)),
      walletNicknamesTable(new QTableWidget(this)),
      renewNicknamesTable(new QTableWidget(this)),
      claimableNicknamesTable(new QTableWidget(this)),
      historyNicknamesTable(new QTableWidget(this)),
      currentRegistrationFee("-"),
      currentBondAmount("-"),
      currentRenewalFee("-"),
      currentRenewalBondIncrease("-"),
      currentLookupBondAmount(0),
      currentRenewalBondIncreaseAmount(0),
      canSendLookup(false),
      canRegisterLookup(false),
      currentLookupWalletOwned(false),
      currentLookupMutable(false),
      currentLookupClaimable(false)
{
    setStyleSheet(
        "QFrame#topBar, QFrame#tableCard {"
        "  background: #f8fafd;"
        "  border: 1px solid #ccd7e7;"
        "  border-radius: 10px;"
        "}"
        "QLabel#hubTitle {"
        "  color: #1f2b3d;"
        "  font-size: 24px;"
        "  font-weight: 700;"
        "}"
        "QLabel#hubSubtitle {"
        "  color: #556780;"
        "  font-size: 14px;"
        "}"
        "QLabel#sectionTitle {"
        "  color: #1f2b3d;"
        "  font-weight: 700;"
        "  font-size: 18px;"
        "}"
        "QLabel#subtle {"
        "  color: #60728b;"
        "}"
        "QLabel#alerts {"
        "  color: #385272;"
        "  background: #eef5ff;"
        "  border: 1px solid #d4e2f5;"
        "  border-radius: 8px;"
        "  padding: 8px 10px;"
        "}"
        "QLabel#metaLabel {"
        "  color: #52637b;"
        "}"
        "QLabel#metaValue {"
        "  color: #1f2b3d;"
        "  font-weight: 600;"
        "}"
        "QLabel#actionStatusOk { color: #0c5e28; font-weight: 600; }"
        "QLabel#actionStatusErr { color: #a62222; font-weight: 600; }"
        "QPushButton { min-height: 30px; padding: 0 14px; }"
    );

    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    QLabel* titleLabel = new QLabel(tr("Nickname Hub"), this);
    titleLabel->setObjectName("hubTitle");
    titleLabel->setAlignment(Qt::AlignHCenter);
    mainLayout->addWidget(titleLabel);

    QLabel* subtitleLabel = new QLabel(tr("Check nickname, then use right-click Actions in the list."), this);
    subtitleLabel->setObjectName("hubSubtitle");
    subtitleLabel->setAlignment(Qt::AlignHCenter);
    subtitleLabel->setWordWrap(true);
    mainLayout->addWidget(subtitleLabel);

    QFrame* topBar = new QFrame(this);
    topBar->setObjectName("topBar");
    QVBoxLayout* topBarLayout = new QVBoxLayout(topBar);
    topBarLayout->setContentsMargins(12, 10, 12, 10);
    topBarLayout->setSpacing(8);

    QHBoxLayout* controlsLayout = new QHBoxLayout();
    QLabel* nicknameLabel = new QLabel(tr("Nickname"), topBar);
    nicknameLabel->setObjectName("metaLabel");
    controlsLayout->addWidget(nicknameLabel);
    controlsLayout->addWidget(nicknameEdit, 1);
    controlsLayout->addWidget(lookupButton);
    controlsLayout->addWidget(actionsButton);
    controlsLayout->addWidget(refreshButton);
    topBarLayout->addLayout(controlsLayout);

    QGridLayout* metaLayout = new QGridLayout();
    metaLayout->setHorizontalSpacing(12);
    metaLayout->setVerticalSpacing(4);

    QLabel* normalizedLabel = new QLabel(tr("Normalized"), topBar);
    normalizedLabel->setObjectName("metaLabel");
    normalizedValue->setObjectName("metaValue");
    normalizedValue->setWordWrap(true);
    metaLayout->addWidget(normalizedLabel, 0, 0);
    metaLayout->addWidget(normalizedValue, 0, 1);

    QLabel* statusLabel = new QLabel(tr("Status"), topBar);
    statusLabel->setObjectName("metaLabel");
    statusValue->setObjectName("metaValue");
    statusValue->setWordWrap(true);
    metaLayout->addWidget(statusLabel, 0, 2);
    metaLayout->addWidget(statusValue, 0, 3);

    QLabel* availabilityLabel = new QLabel(tr("Availability"), topBar);
    availabilityLabel->setObjectName("metaLabel");
    availabilityValue->setObjectName("metaValue");
    availabilityValue->setWordWrap(true);
    metaLayout->addWidget(availabilityLabel, 1, 0);
    metaLayout->addWidget(availabilityValue, 1, 1);

    QLabel* infoLabel = new QLabel(tr("Info"), topBar);
    infoLabel->setObjectName("metaLabel");
    pricingValue->setObjectName("metaValue");
    pricingValue->setWordWrap(true);
    metaLayout->addWidget(infoLabel, 1, 2);
    metaLayout->addWidget(pricingValue, 1, 3);
    metaLayout->setColumnStretch(1, 1);
    metaLayout->setColumnStretch(3, 1);
    topBarLayout->addLayout(metaLayout);

    actionStatusValue->setObjectName("actionStatusOk");
    actionStatusValue->setWordWrap(true);
    topBarLayout->addWidget(actionStatusValue);
    mainLayout->addWidget(topBar);

    QFrame* tableCard = new QFrame(this);
    tableCard->setObjectName("tableCard");
    QVBoxLayout* tableCardLayout = new QVBoxLayout(tableCard);
    tableCardLayout->setContentsMargins(12, 12, 12, 12);
    tableCardLayout->setSpacing(8);

    QHBoxLayout* tableHeadLayout = new QHBoxLayout();
    QLabel* walletHeader = new QLabel(tr("My Nicknames"), tableCard);
    walletHeader->setObjectName("sectionTitle");
    tableHeadLayout->addWidget(walletHeader);
    tableHeadLayout->addStretch(1);
    QLabel* walletHint = new QLabel(tr("Right-click any nickname for actions"), tableCard);
    walletHint->setObjectName("subtle");
    tableHeadLayout->addWidget(walletHint);
    tableCardLayout->addLayout(tableHeadLayout);

    walletAlertsValue->setObjectName("alerts");
    walletAlertsValue->setWordWrap(true);
    walletAlertsValue->setText(tr("No nickname alerts."));
    tableCardLayout->addWidget(walletAlertsValue);

    auto setupTable = [&](QTableWidget* table) {
        table->setColumnCount(5);
        table->setHorizontalHeaderLabels(
            QStringList() << tr("Nickname") << tr("Status") << tr("Time") << tr("Bond") << tr("Payout Address"));
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->verticalHeader()->setVisible(false);
        table->setSelectionBehavior(QAbstractItemView::SelectRows);
        table->setSelectionMode(QAbstractItemView::SingleSelection);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setAlternatingRowColors(true);
        table->setContextMenuPolicy(Qt::CustomContextMenu);
    };

    setupTable(walletNicknamesTable);
    setupTable(renewNicknamesTable);
    setupTable(claimableNicknamesTable);
    setupTable(historyNicknamesTable);

    walletNicknamesTabs->addTab(walletNicknamesTable, tr("My Nicknames"));
    walletNicknamesTabs->addTab(renewNicknamesTable, tr("Grace Period"));
    walletNicknamesTabs->addTab(claimableNicknamesTable, tr("Claimable"));
    walletNicknamesTabs->addTab(historyNicknamesTable, tr("History"));
    tableCardLayout->addWidget(walletNicknamesTabs, 1);
    mainLayout->addWidget(tableCard, 1);

    nicknameEdit->setPlaceholderText(tr("e.g. kubu_dev or @kubu_dev"));

    actionsButton->setEnabled(false);

    connect(lookupButton, SIGNAL(clicked()), this, SLOT(onLookupClicked()));
    connect(actionsButton, SIGNAL(clicked()), this, SLOT(onActionsClicked()));
    connect(refreshButton, SIGNAL(clicked()), this, SLOT(onRefreshWalletNicknamesClicked()));
    connect(nicknameEdit, SIGNAL(textChanged(QString)), this, SLOT(onNicknameTextChanged(QString)));
    connect(nicknameEdit, SIGNAL(returnPressed()), this, SLOT(onLookupClicked()));
    connect(walletNicknamesTable, SIGNAL(cellClicked(int,int)), this, SLOT(onWalletNicknameActivated(int,int)));
    connect(walletNicknamesTable, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(onWalletNicknameDoubleClicked(int,int)));
    connect(walletNicknamesTable, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onWalletNicknamesContextMenu(QPoint)));
    connect(renewNicknamesTable, SIGNAL(cellClicked(int,int)), this, SLOT(onWalletNicknameActivated(int,int)));
    connect(renewNicknamesTable, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(onWalletNicknameDoubleClicked(int,int)));
    connect(renewNicknamesTable, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onWalletNicknamesContextMenu(QPoint)));
    connect(claimableNicknamesTable, SIGNAL(cellClicked(int,int)), this, SLOT(onWalletNicknameActivated(int,int)));
    connect(claimableNicknamesTable, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(onWalletNicknameDoubleClicked(int,int)));
    connect(claimableNicknamesTable, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onWalletNicknamesContextMenu(QPoint)));
    connect(historyNicknamesTable, SIGNAL(cellClicked(int,int)), this, SLOT(onWalletNicknameActivated(int,int)));
    connect(historyNicknamesTable, SIGNAL(cellDoubleClicked(int,int)), this, SLOT(onWalletNicknameDoubleClicked(int,int)));
    connect(historyNicknamesTable, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(onWalletNicknamesContextMenu(QPoint)));

    clearLookupResult();
    setActionStatus(QString(), false);
}

void NicknamesPage::setModel(WalletModel* modelIn)
{
    model = modelIn;
    clearLookupResult();
    refreshWalletNicknames();
}

void NicknamesPage::clearLookupResult()
{
    normalizedValue->setText(tr("Enter nickname and click Check"));
    statusValue->setText(tr("Not checked"));
    availabilityValue->setText(tr("Unknown"));
    pricingValue->setText(tr("Price is shown only in Buy dialog."));

    currentLookupNickname.clear();
    currentLookupPayoutAddress.clear();
    currentLookupOwnerAddress.clear();
    currentLookupOwnerPubKey.clear();
    currentLookupBondRef.clear();
    currentRegistrationFee = "-";
    currentBondAmount = "-";
    currentRenewalFee = "-";
    currentRenewalBondIncrease = "-";
    currentLookupBondAmount = 0;
    currentRenewalBondIncreaseAmount = 0;
    canSendLookup = false;
    canRegisterLookup = false;
    currentLookupWalletOwned = false;
    currentLookupMutable = false;
    currentLookupClaimable = false;

    actionsButton->setEnabled(false);
}

void NicknamesPage::onNicknameTextChanged(const QString& text)
{
    clearLookupResult();
    setActionStatus(QString(), false);
    actionsButton->setEnabled(!text.trimmed().isEmpty());
}

void NicknamesPage::onLookupClicked()
{
    setActionStatus(QString(), false);
    clearLookupResult();

    NicknameStateDB* nicknameDB = GetNicknameStateDB();
    if (!nicknameDB) {
        statusValue->setText(tr("Nickname index is unavailable"));
        return;
    }

    QString error;
    std::string normalized;
    if (!getNormalizedNickname(normalized, error)) {
        statusValue->setText(error);
        return;
    }

    normalizedValue->setText(QString::fromStdString(normalized));

    UniValue checkParams(UniValue::VARR);
    checkParams.push_back(normalized);
    UniValue checkResult;
    if (executeRpc("checknickname", checkParams, checkResult, error) && checkResult.isObject()) {
        const UniValue& regFee = find_value(checkResult, "registration_fee");
        const UniValue& bondFee = find_value(checkResult, "bond_amount");
        const UniValue& renewFee = find_value(checkResult, "renewal_fee");
        const UniValue& renewBondIncrease = find_value(checkResult, "renewal_bond_increase");
        const UniValue* renewBondIncreaseValue = &renewBondIncrease;
        if (renewBondIncrease.isNull()) {
            renewBondIncreaseValue = &renewFee;
        }
        currentRegistrationFee = RpcAmountToQString(regFee);
        currentBondAmount = RpcAmountToQString(bondFee);
        currentRenewalFee = RpcAmountToQString(renewFee);
        currentRenewalBondIncrease = RpcAmountToQString(*renewBondIncreaseValue);
        currentLookupBondAmount = RpcAmountToNative(bondFee);
        currentRenewalBondIncreaseAmount = RpcAmountToNative(*renewBondIncreaseValue);
    }

    NicknameInfo info;
    if (!nicknameDB->ReadNickname(normalized, info)) {
        statusValue->setText(tr("NOT_REGISTERED"));
        availabilityValue->setText(tr("Available for registration"));
        pricingValue->setText(tr("Click Buy to view current registration price and bond."));

        canRegisterLookup = true;
        currentLookupNickname = QString::fromStdString(normalized);
        actionsButton->setEnabled(true);
        return;
    }

    int currentHeight = 0;
    {
        LOCK(cs_main);
        currentHeight = chainActive.Height();
    }

    const Nicknames::Status status = info.GetStatus(currentHeight);
    const bool walletOwned = WalletOwnsNickname(model, info);

    statusValue->setText(StatusLabelForUi(status));
    currentLookupBondAmount = info.bondAmount;
    currentBondAmount = FormatKubuAmountFromNative(info.bondAmount);
    pricingValue->setText(tr("Price is shown only in Buy dialog."));

    currentLookupNickname = QString::fromStdString(normalized);
    currentLookupPayoutAddress = QString::fromStdString(info.payoutAddress);
    currentLookupOwnerAddress.clear();
    currentLookupOwnerPubKey.clear();
    currentLookupBondRef.clear();
    if (info.ownerPubKey.size() == CPubKey::COMPRESSED_SIZE) {
        currentLookupOwnerPubKey = QString::fromStdString(HexStr(info.ownerPubKey.begin(), info.ownerPubKey.end()));
    }
    if (info.ownerPubKey.size() == CPubKey::COMPRESSED_SIZE) {
        const CPubKey ownerPubKey(info.ownerPubKey);
        if (ownerPubKey.IsFullyValid() && ownerPubKey.IsCompressed()) {
            currentLookupOwnerAddress = QString::fromStdString(CBitcoinAddress(ownerPubKey.GetID()).ToString());
        }
    }
    if (info.HasBondOutpoint()) {
        currentLookupBondRef = QString::fromStdString(info.bondTxid.GetHex()) + ":" + QString::number(info.bondVout);
    }

    canSendLookup = (status == Nicknames::Status::ACTIVE && !currentLookupPayoutAddress.isEmpty());
    canRegisterLookup =
        (status == Nicknames::Status::EXPIRED_AVAILABLE ||
         status == Nicknames::Status::RELEASED ||
         status == Nicknames::Status::BOND_CLAIMABLE);
    currentLookupWalletOwned = walletOwned;
    currentLookupMutable = walletOwned && IsMutableStatus(status);
    currentLookupClaimable = walletOwned &&
                             status == Nicknames::Status::BOND_CLAIMABLE &&
                             info.HasBondOutpoint();

    if (canRegisterLookup) {
        availabilityValue->setText(tr("Available for registration"));
        pricingValue->setText(tr("Click Buy to view current registration price and bond."));
    } else {
        availabilityValue->setText(walletOwned ? tr("Taken (owned by this wallet)") : tr("Taken"));
    }

    actionsButton->setEnabled(true);
}

void NicknamesPage::onSendClicked()
{
    if (!canSendLookup || currentLookupNickname.isEmpty()) {
        return;
    }

    Q_EMIT sendCoins(currentLookupNickname);
}

void NicknamesPage::onActionsClicked()
{
    const QString nickname = nicknameEdit->text().trimmed();
    if (nickname.isEmpty()) {
        setActionStatus(tr("Enter nickname first"), true);
        return;
    }

    if (!ensureLookupForNickname(nickname)) {
        return;
    }

    openActionMenuForNickname(currentLookupNickname, QCursor::pos());
}

QTableWidget* NicknamesPage::currentNicknamesTable() const
{
    if (!walletNicknamesTabs) {
        return walletNicknamesTable;
    }

    QWidget* currentWidget = walletNicknamesTabs->currentWidget();
    QTableWidget* currentTable = qobject_cast<QTableWidget*>(currentWidget);
    return currentTable ? currentTable : walletNicknamesTable;
}

void NicknamesPage::refreshWalletNicknames()
{
    walletNicknamesTable->clearContents();
    walletNicknamesTable->setRowCount(0);
    renewNicknamesTable->clearContents();
    renewNicknamesTable->setRowCount(0);
    claimableNicknamesTable->clearContents();
    claimableNicknamesTable->setRowCount(0);
    historyNicknamesTable->clearContents();
    historyNicknamesTable->setRowCount(0);
    walletAlertsValue->setText(tr("No nickname alerts."));

    NicknameStateDB* nicknameDB = GetNicknameStateDB();
    if (!nicknameDB) {
        if (walletNicknamesTabs) {
            walletNicknamesTabs->setTabText(0, tr("My Nicknames (0)"));
            walletNicknamesTabs->setTabText(1, tr("Grace Period (0)"));
            walletNicknamesTabs->setTabText(2, tr("Claimable (0)"));
            walletNicknamesTabs->setTabText(3, tr("History (0)"));
        }
        return;
    }

    int currentHeight = 0;
    {
        LOCK(cs_main);
        currentHeight = chainActive.Height();
    }

    const int expiringSoonThreshold = Nicknames::ActiveBlocks() > 20 ? (Nicknames::ActiveBlocks() / 20) : 1;
    int activeCount = 0;
    int renewCount = 0;
    int historyCount = 0;
    int expiringSoonCount = 0;
    int graceCount = 0;
    int claimableCount = 0;
    int releasedCount = 0;
    QStringList expiringSoonSample;
    QStringList graceSample;
    QStringList claimableSample;
    QStringList releasedSample;

    auto addSample = [](QStringList& list, const std::string& nickname) {
        if (list.size() < 3) {
            list << QString::fromStdString(nickname);
        }
    };

    auto appendRow = [&](QTableWidget* table, const NicknameInfo& info, const Nicknames::Status status) {
        const int row = table->rowCount();
        table->insertRow(row);
        const bool compactHistoryLabel = (table == historyNicknamesTable);
        table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(info.nickname)));
        table->setItem(row, 1, new QTableWidgetItem(StatusLabelForUi(status, compactHistoryLabel)));
        table->setItem(row, 2, new QTableWidgetItem(TimeInfoForStatus(info, status, currentHeight)));
        table->setItem(row, 3, new QTableWidgetItem(BondInfoForStatus(info, status)));
        table->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(info.payoutAddress)));
    };

    std::set<CKeyID> walletKeyIDs;
    if (model) {
        model->getWalletKeyIDs(walletKeyIDs);
    }

    const std::vector<NicknameInfo> entries =
        nicknameDB->ListNicknamesForOwnerKeyIDs(walletKeyIDs, "", std::numeric_limits<size_t>::max());
    for (const NicknameInfo& info : entries) {
        const Nicknames::Status status = info.GetStatus(currentHeight);

        if (status == Nicknames::Status::ACTIVE) {
            ++activeCount;
            const int activeLeft = info.activeUntilHeight >= currentHeight ? (info.activeUntilHeight - currentHeight) : 0;
            if (activeLeft <= expiringSoonThreshold) {
                ++expiringSoonCount;
                addSample(expiringSoonSample, info.nickname);
            }
            appendRow(walletNicknamesTable, info, status);
        } else if (status == Nicknames::Status::EXPIRED_GRACE) {
            ++graceCount;
            ++renewCount;
            addSample(graceSample, info.nickname);
            appendRow(renewNicknamesTable, info, status);
        } else if (status == Nicknames::Status::BOND_CLAIMABLE && info.HasBondOutpoint()) {
            ++claimableCount;
            addSample(claimableSample, info.nickname);
            appendRow(claimableNicknamesTable, info, status);
        } else if (status == Nicknames::Status::RELEASED) {
            ++releasedCount;
            addSample(releasedSample, info.nickname);
            ++historyCount;
            appendRow(historyNicknamesTable, info, status);
        } else {
            ++historyCount;
            appendRow(historyNicknamesTable, info, status);
        }
    }

    auto sampleText = [](const QStringList& sample, int total) -> QString {
        if (sample.isEmpty()) {
            return "-";
        }
        QString text = sample.join(", ");
        const int extra = total - sample.size();
        if (extra > 0) {
            text += QObject::tr(" +%1 more").arg(extra);
        }
        return text;
    };

    QStringList alertLines;
    if (expiringSoonCount > 0) {
        alertLines << tr("Expiring soon (%1): %2")
            .arg(expiringSoonCount)
            .arg(sampleText(expiringSoonSample, expiringSoonCount));
    }
    if (graceCount > 0) {
        alertLines << tr("Entered grace / renewal needed (%1): %2")
            .arg(graceCount)
            .arg(sampleText(graceSample, graceCount));
    }
    if (claimableCount > 0) {
        alertLines << tr("Bond claimable (%1): %2")
            .arg(claimableCount)
            .arg(sampleText(claimableSample, claimableCount));
    }
    if (releasedCount > 0) {
        alertLines << tr("Released (bond claimed) (%1): %2")
            .arg(releasedCount)
            .arg(sampleText(releasedSample, releasedCount));
    }

    if (alertLines.isEmpty()) {
        walletAlertsValue->setText(tr("No nickname alerts."));
    } else {
        walletAlertsValue->setText(alertLines.join("\n"));
    }

    auto setEmptyState = [](QTableWidget* table, const QString& message) {
        if (table->rowCount() != 0) {
            return;
        }
        table->setRowCount(1);
        QTableWidgetItem* empty = new QTableWidgetItem(message);
        empty->setFlags(Qt::ItemIsEnabled);
        empty->setTextAlignment(Qt::AlignCenter);
        table->setItem(0, 0, empty);
        table->setSpan(0, 0, 1, 5);
    };

    setEmptyState(walletNicknamesTable, tr("No active nicknames."));
    setEmptyState(renewNicknamesTable, tr("No nicknames in grace period."));
    setEmptyState(claimableNicknamesTable, tr("No claimable nicknames."));
    setEmptyState(historyNicknamesTable, tr("No nickname history yet."));

    if (walletNicknamesTabs) {
        walletNicknamesTabs->setTabText(0, tr("My Nicknames (%1)").arg(activeCount));
        walletNicknamesTabs->setTabText(1, tr("Grace Period (%1)").arg(renewCount));
        walletNicknamesTabs->setTabText(2, tr("Claimable (%1)").arg(claimableCount));
        walletNicknamesTabs->setTabText(3, tr("History (%1)").arg(historyCount));
    }
}

void NicknamesPage::onRefreshWalletNicknamesClicked()
{
    refreshWalletNicknames();
}

void NicknamesPage::onWalletNicknameActivated(int row, int column)
{
    if (column != 0) {
        return;
    }
    QTableWidget* table = qobject_cast<QTableWidget*>(sender());
    if (!table) {
        table = currentNicknamesTable();
    }
    if (!table || !table->item(row, 1)) {
        return;
    }

    QTableWidgetItem* nicknameItem = table->item(row, 0);
    if (!nicknameItem) {
        return;
    }

    nicknameEdit->setText(nicknameItem->text());
    ensureLookupForNickname(nicknameItem->text());
}

void NicknamesPage::onWalletNicknameDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    QTableWidget* table = qobject_cast<QTableWidget*>(sender());
    if (!table) {
        table = currentNicknamesTable();
    }
    if (!table || !table->item(row, 1)) {
        return;
    }

    QTableWidgetItem* nicknameItem = table->item(row, 0);
    if (!nicknameItem) {
        return;
    }

    nicknameEdit->setText(nicknameItem->text());
    if (!ensureLookupForNickname(nicknameItem->text())) {
        return;
    }

    showNicknameDetailsDialog(currentLookupNickname);
}

void NicknamesPage::showNicknameDetailsDialog(const QString& nickname)
{
    NicknameStateDB* nicknameDB = GetNicknameStateDB();
    if (!nicknameDB) {
        setActionStatus(tr("Nickname index is unavailable"), true);
        return;
    }

    std::string normalized;
    Nicknames::NormalizeNickname(nickname.trimmed().toStdString(), normalized);

    NicknameInfo info;
    if (!nicknameDB->ReadNickname(normalized, info)) {
        QMessageBox::information(this, tr("Nickname Details"), tr("Nickname %1 was not found in index.").arg(QString::fromStdString(normalized)));
        return;
    }

    int currentHeight = 0;
    {
        LOCK(cs_main);
        currentHeight = chainActive.Height();
    }

    const Nicknames::Status status = info.GetStatus(currentHeight);
    QString ownerAddress = tr("unknown");
    const QString ownerPubKey = QString::fromStdString(HexStr(info.ownerPubKey.begin(), info.ownerPubKey.end()));
    if (info.ownerPubKey.size() == CPubKey::COMPRESSED_SIZE) {
        const CPubKey pubKey(info.ownerPubKey);
        if (pubKey.IsFullyValid() && pubKey.IsCompressed()) {
            ownerAddress = QString::fromStdString(CBitcoinAddress(pubKey.GetID()).ToString());
        }
    }

    const QString bondRef = info.HasBondOutpoint()
        ? (QString::fromStdString(info.bondTxid.GetHex()) + ":" + QString::number(info.bondVout))
        : tr("none");

    const QString nicknameDisplay = QString("@%1").arg(QString::fromStdString(info.nickname));
    const QString statusText = StatusLabelForUi(status);
    const QString timeText = TimeInfoForStatus(info, status, currentHeight);
    const QString bondText = BondInfoForStatus(info, status);
    const QString payoutAddress = QString::fromStdString(info.payoutAddress);
    const QString ownerPubKeyDisplay = ownerPubKey.isEmpty() ? tr("none") : ownerPubKey;
    const QString lastTxid = QString::fromStdString(info.lastUpdateTxid.GetHex());

    const QString detailsText = tr(
        "Name: %1\n"
        "Status: %2\n"
        "Time: %3\n"
        "Bond: %4\n"
        "Payout address: %5\n"
        "Owner address: %6\n"
        "Owner pubkey: %7\n"
        "Registration height: %8\n"
        "Active until: %9\n"
        "Grace until: %10\n"
        "Bond outpoint: %11\n"
        "Released: %12\n"
        "Bond claimed: %13\n"
        "Last txid: %14")
        .arg(nicknameDisplay)
        .arg(statusText)
        .arg(timeText)
        .arg(bondText)
        .arg(payoutAddress)
        .arg(ownerAddress)
        .arg(ownerPubKeyDisplay)
        .arg(info.registrationHeight)
        .arg(info.activeUntilHeight)
        .arg(info.graceUntilHeight)
        .arg(bondRef)
        .arg(info.released ? tr("yes") : tr("no"))
        .arg(info.bondClaimed ? tr("yes") : tr("no"))
        .arg(lastTxid);

    QString html;
    html += "<html><font face='verdana, arial, helvetica, sans-serif'>";
    html += "<b>" + tr("Payment information") + "</b><br>";
    html += "<b>" + tr("Name") + "</b>: " + nicknameDisplay.toHtmlEscaped() + "<br>";
    html += "<b>" + tr("Status") + "</b>: " + statusText.toHtmlEscaped() + "<br>";
    html += "<b>" + tr("Time") + "</b>: " + timeText.toHtmlEscaped() + "<br>";
    html += "<b>" + tr("Bond") + "</b>: " + bondText.toHtmlEscaped() + "<br>";
    html += "<b>" + tr("Payout address") + "</b>: <span style='font-family:monospace;'>" + payoutAddress.toHtmlEscaped() + "</span><br>";
    html += "<b>" + tr("Owner address") + "</b>: <span style='font-family:monospace;'>" + ownerAddress.toHtmlEscaped() + "</span><br>";
    html += "<b>" + tr("Owner pubkey") + "</b>: <span style='font-family:monospace;'>" + ownerPubKeyDisplay.toHtmlEscaped() + "</span><br>";
    html += "<b>" + tr("Registration height") + "</b>: " + QString::number(info.registrationHeight) + "<br>";
    html += "<b>" + tr("Active until") + "</b>: " + QString::number(info.activeUntilHeight) + "<br>";
    html += "<b>" + tr("Grace until") + "</b>: " + QString::number(info.graceUntilHeight) + "<br>";
    html += "<b>" + tr("Bond outpoint") + "</b>: <span style='font-family:monospace;'>" + bondRef.toHtmlEscaped() + "</span><br>";
    html += "<b>" + tr("Released") + "</b>: " + QString(info.released ? tr("yes") : tr("no")).toHtmlEscaped() + "<br>";
    html += "<b>" + tr("Bond claimed") + "</b>: " + QString(info.bondClaimed ? tr("yes") : tr("no")).toHtmlEscaped() + "<br>";
    html += "<b>" + tr("Last txid") + "</b>: <span style='font-family:monospace;'>" + lastTxid.toHtmlEscaped() + "</span><br>";

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Nickname Details"));
    dialog.setModal(true);
    dialog.setStyleSheet(
        "QDialog { background: #f7faff; }"
        "QTextEdit { background: #ffffff; border: 1px solid #c8d6ea; border-radius: 8px; color: #1f2b3d; }"
        "QPushButton { min-width: 92px; min-height: 30px; }");

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(10);

    QTextEdit* detailsView = new QTextEdit(&dialog);
    detailsView->setReadOnly(true);
    detailsView->setUndoRedoEnabled(false);
    detailsView->setMinimumSize(620, 340);
    detailsView->setHtml(html);
    layout->addWidget(detailsView);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    QPushButton* copyButton = buttons->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    buttons->button(QDialogButtonBox::Close)->setText(tr("Close"));
    connect(buttons->button(QDialogButtonBox::Close), SIGNAL(clicked()), &dialog, SLOT(accept()));
    connect(copyButton, &QPushButton::clicked, [detailsText]() {
        QApplication::clipboard()->setText(detailsText);
    });
    layout->addWidget(buttons);

    dialog.exec();
}

void NicknamesPage::onWalletNicknamesContextMenu(const QPoint& point)
{
    QTableWidget* table = qobject_cast<QTableWidget*>(sender());
    if (!table) {
        table = currentNicknamesTable();
    }
    if (!table) {
        return;
    }

    QTableWidgetItem* item = table->itemAt(point);
    if (!item) {
        return;
    }

    const int row = item->row();
    if (!table->item(row, 1)) {
        return;
    }

    QTableWidgetItem* nameCell = table->item(row, 0);
    if (!nameCell) {
        return;
    }

    const QString nickname = nameCell->text().trimmed();
    if (nickname.isEmpty() || !ensureLookupForNickname(nickname)) {
        return;
    }

    openActionMenuForNickname(nickname, table->viewport()->mapToGlobal(point));
}

bool NicknamesPage::ensureLookupForNickname(const QString& nickname)
{
    if (nickname.trimmed().isEmpty()) {
        setActionStatus(tr("Nickname is required"), true);
        return false;
    }

    nicknameEdit->setText(nickname.trimmed());
    onLookupClicked();

    if (currentLookupNickname.isEmpty()) {
        const QString lookupReason = statusValue->text().trimmed();
        if (!lookupReason.isEmpty() && lookupReason != "-") {
            setActionStatus(lookupReason, true);
        } else {
            setActionStatus(tr("Failed to lookup nickname"), true);
        }
        return false;
    }

    return true;
}

void NicknamesPage::openActionMenuForNickname(const QString& nickname, const QPoint& globalPos)
{
    if (!ensureLookupForNickname(nickname)) {
        return;
    }

    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu {"
        "  background: #ffffff;"
        "  border: 1px solid #c8d6ea;"
        "  border-radius: 10px;"
        "  padding: 6px;"
        "}"
        "QMenu::item {"
        "  padding: 8px 16px;"
        "  border-radius: 7px;"
        "  color: #1f2b3d;"
        "}"
        "QMenu::item:selected {"
        "  background: #e7f0ff;"
        "}"
        "QMenu::item:disabled {"
        "  color: #8d9eb6;"
        "}"
        "QMenu::separator {"
        "  height: 1px;"
        "  background: #d9e2f0;"
        "  margin: 6px 4px;"
        "}"
    );

    QAction* buyAct = menu.addAction(tr("Buy..."));
    buyAct->setEnabled(canRegisterLookup);
    if (!canRegisterLookup) {
        buyAct->setText(tr("Buy (unavailable)"));
    }

    QAction* sendAct = menu.addAction(tr("Send to nickname"));
    sendAct->setEnabled(canSendLookup);
    if (!canSendLookup) {
        sendAct->setText(tr("Send to nickname (inactive)"));
    }

    menu.addSeparator();

    QAction* quickPayoutAct = menu.addAction(tr("Use new receive as payout"));
    QAction* updateAct = menu.addAction(tr("Update payout..."));
    QAction* transferAct = menu.addAction(tr("Change owner..."));
    QAction* renewAct = menu.addAction(tr("Renew..."));
    QAction* releaseAct = menu.addAction(tr("Free nickname..."));

    const bool hasManage = currentLookupMutable;
    quickPayoutAct->setEnabled(hasManage);
    updateAct->setEnabled(hasManage);
    transferAct->setEnabled(hasManage);
    renewAct->setEnabled(hasManage);
    releaseAct->setEnabled(hasManage);

    if (!hasManage) {
        quickPayoutAct->setText(tr("Use new receive as payout (wallet owner required)"));
        updateAct->setText(tr("Update payout (wallet owner required)"));
        transferAct->setText(tr("Change owner (wallet owner required)"));
        renewAct->setText(tr("Renew (wallet owner required)"));
        releaseAct->setText(tr("Free nickname (wallet owner required)"));
    }

    menu.addSeparator();

    QAction* claimAct = menu.addAction(tr("Claim bond..."));
    claimAct->setEnabled(currentLookupClaimable);
    if (!currentLookupClaimable) {
        claimAct->setText(tr("Claim bond (not available)"));
    }

    QAction* selected = menu.exec(globalPos);
    if (!selected) {
        return;
    }

    if (selected == buyAct && buyAct->isEnabled()) {
        onRegisterClicked();
    } else if (selected == sendAct && sendAct->isEnabled()) {
        onSendClicked();
    } else if (selected == quickPayoutAct && quickPayoutAct->isEnabled()) {
        onUseNewReceivePayoutClicked();
    } else if (selected == updateAct && updateAct->isEnabled()) {
        onUpdateClicked();
    } else if (selected == transferAct && transferAct->isEnabled()) {
        onTransferClicked();
    } else if (selected == renewAct && renewAct->isEnabled()) {
        onRenewClicked();
    } else if (selected == releaseAct && releaseAct->isEnabled()) {
        onReleaseClicked();
    } else if (selected == claimAct && claimAct->isEnabled()) {
        onClaimClicked();
    }
}

bool NicknamesPage::confirmAction(const QString& title, const QString& text) const
{
    QMessageBox box(const_cast<NicknamesPage*>(this));
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(title);
    box.setText(text);
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    box.setStyleSheet(
        "QMessageBox { background: #f7faff; }"
        "QLabel { color: #1f2b3d; min-width: 360px; }"
        "QPushButton { min-width: 92px; min-height: 30px; }");

    const QMessageBox::StandardButton answer = static_cast<QMessageBox::StandardButton>(box.exec());
    return answer == QMessageBox::Yes;
}

bool NicknamesPage::promptTextDialog(const QString& title,
                                     const QString& description,
                                     const QString& fieldLabel,
                                     const QString& placeholder,
                                     const QString& initialValue,
                                     QString& valueOut)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.setStyleSheet(
        "QDialog { background: #f7faff; }"
        "QLabel { color: #1f2b3d; }"
        "QLabel#desc { color: #5b6d86; }"
        "QLineEdit { min-width: 520px; }"
        "QPushButton { min-width: 100px; min-height: 30px; }");

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    QLabel* descriptionLabel = new QLabel(description, &dialog);
    descriptionLabel->setObjectName("desc");
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel);

    QLabel* field = new QLabel(fieldLabel, &dialog);
    layout->addWidget(field);

    QLineEdit* input = new QLineEdit(&dialog);
    input->setPlaceholderText(placeholder);
    input->setText(initialValue.trimmed());
    layout->addWidget(input);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Continue"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
    layout->addWidget(buttons);

    input->setFocus();
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString value = input->text().trimmed();
    if (value.isEmpty()) {
        setActionStatus(tr("%1 is required").arg(fieldLabel), true);
        return false;
    }

    valueOut = value;
    return true;
}

bool NicknamesPage::promptBuyDialog(const QString& nickname,
                                    const QString& registrationFee,
                                    const QString& bondAmount,
                                    const QString& renewalFee,
                                    const QString& suggestedPayout,
                                    QString& payoutOut)
{
    QString payoutLoadError;
    QStringList payoutOptions;
    if (!getWalletReceiveAddresses(payoutOptions, payoutLoadError)) {
        setActionStatus(payoutLoadError, true);
        return false;
    }

    const QString suggested = suggestedPayout.trimmed();
    if (!suggested.isEmpty() && !payoutOptions.contains(suggested)) {
        payoutOptions.prepend(suggested);
    }

    QDialog dialog(this);
    dialog.setWindowTitle(tr("Buy Nickname"));
    dialog.setModal(true);
    dialog.setStyleSheet(
        "QDialog { background: #f3f8ff; }"
        "QLabel { color: #1d2a3b; }"
        "QLabel#meta { color: #4f6280; }"
        "QFrame#headerBox { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #eff5ff, stop:1 #deebff); border: 1px solid #c7d9f2; border-radius: 10px; }"
        "QLabel#headerTitle { color: #10233f; font-size: 14px; font-weight: 700; }"
        "QLabel#headerSub { color: #4e6180; }"
        "QFrame#feeBox { background: #ffffff; border: 1px solid #d3dfef; border-radius: 10px; }"
        "QLabel#feeLabel { color: #5b6c85; }"
        "QLabel#feeValue { color: #1f2b3d; font-weight: 700; }"
        "QLabel#countdownLabel { color: #3b4f6a; font-weight: 600; }"
        "QProgressBar#countdownProgress { border: 1px solid #c9d7ea; border-radius: 4px; background: #eef3fb; }"
        "QProgressBar#countdownProgress::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2fbf71, stop:1 #23a05d); border-radius: 3px; }"
        "QComboBox { min-width: 520px; }"
        "QPushButton { min-width: 110px; min-height: 32px; }");

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    QFrame* headerBox = new QFrame(&dialog);
    headerBox->setObjectName("headerBox");
    QVBoxLayout* headerLayout = new QVBoxLayout(headerBox);
    headerLayout->setContentsMargins(12, 10, 12, 10);
    headerLayout->setSpacing(3);
    QLabel* headerTitle = new QLabel(tr("Final purchase confirmation"), headerBox);
    headerTitle->setObjectName("headerTitle");
    QLabel* headerSub = new QLabel(tr("Review nickname details before broadcasting the transaction."), headerBox);
    headerSub->setObjectName("headerSub");
    headerSub->setWordWrap(true);
    headerLayout->addWidget(headerTitle);
    headerLayout->addWidget(headerSub);
    layout->addWidget(headerBox);

    QLabel* nicknameInfo = new QLabel(tr("Nickname: %1").arg(nickname), &dialog);
    nicknameInfo->setWordWrap(true);
    layout->addWidget(nicknameInfo);

    QFrame* feeBox = new QFrame(&dialog);
    feeBox->setObjectName("feeBox");
    QGridLayout* feeLayout = new QGridLayout(feeBox);
    feeLayout->setContentsMargins(10, 8, 10, 8);
    feeLayout->setHorizontalSpacing(10);
    feeLayout->setVerticalSpacing(6);

    QLabel* regLabel = new QLabel(tr("Registration fee"), feeBox);
    regLabel->setObjectName("feeLabel");
    QLabel* regValue = new QLabel(registrationFee, feeBox);
    regValue->setObjectName("feeValue");
    feeLayout->addWidget(regLabel, 0, 0);
    feeLayout->addWidget(regValue, 0, 1);

    QLabel* bondLabel = new QLabel(tr("Bond amount"), feeBox);
    bondLabel->setObjectName("feeLabel");
    QLabel* bondValue = new QLabel(bondAmount, feeBox);
    bondValue->setObjectName("feeValue");
    feeLayout->addWidget(bondLabel, 1, 0);
    feeLayout->addWidget(bondValue, 1, 1);

    QLabel* renewLabel = new QLabel(tr("Renewal fee"), feeBox);
    renewLabel->setObjectName("feeLabel");
    QLabel* renewValue = new QLabel(renewalFee, feeBox);
    renewValue->setObjectName("feeValue");
    feeLayout->addWidget(renewLabel, 2, 0);
    feeLayout->addWidget(renewValue, 2, 1);

    feeLayout->setColumnStretch(1, 1);
    layout->addWidget(feeBox);

    QLabel* payoutLabel = new QLabel(tr("Payout address (your wallet)"), &dialog);
    layout->addWidget(payoutLabel);

    QHBoxLayout* payoutLayout = new QHBoxLayout();
    payoutLayout->setContentsMargins(0, 0, 0, 0);
    payoutLayout->setSpacing(8);

    QComboBox* payoutCombo = new QComboBox(&dialog);
    payoutCombo->setEditable(false);
    payoutCombo->addItems(payoutOptions);
    payoutCombo->setMaxVisibleItems(10);
    if (payoutCombo->view()) {
        payoutCombo->view()->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        payoutCombo->view()->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    }
    if (!suggested.isEmpty()) {
        const int suggestedIndex = payoutCombo->findText(suggested);
        if (suggestedIndex >= 0) {
            payoutCombo->setCurrentIndex(suggestedIndex);
        }
    }
    payoutLayout->addWidget(payoutCombo, 1);

    QPushButton* generateAddressButton = new QPushButton(tr("Generate new"), &dialog);
    payoutLayout->addWidget(generateAddressButton);
    layout->addLayout(payoutLayout);

    QObject::connect(generateAddressButton, &QPushButton::clicked, [&]() {
        QString newAddress;
        QString generateError;
        if (!getNewWalletAddress(newAddress, nullptr, generateError)) {
            QMessageBox::warning(&dialog, tr("Generate payout address failed"), generateError);
            return;
        }

        int index = payoutCombo->findText(newAddress);
        if (index < 0) {
            payoutCombo->insertItem(0, newAddress);
            index = 0;
        }
        payoutCombo->setCurrentIndex(index);
    });

    QLabel* countdownLabel = new QLabel(tr("Security check in progress... confirm in 2 s"), &dialog);
    countdownLabel->setObjectName("countdownLabel");
    layout->addWidget(countdownLabel);

    QProgressBar* countdownProgress = new QProgressBar(&dialog);
    countdownProgress->setObjectName("countdownProgress");
    countdownProgress->setRange(0, 2);
    countdownProgress->setValue(0);
    countdownProgress->setTextVisible(false);
    countdownProgress->setFixedHeight(8);
    layout->addWidget(countdownProgress);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QPushButton* buyButton = buttons->button(QDialogButtonBox::Ok);
    buyButton->setText(tr("Confirm purchase"));
    buyButton->setEnabled(false);
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
    layout->addWidget(buttons);

    int secondsLeft = 2;
    QTimer countdownTimer(&dialog);
    countdownTimer.setInterval(1000);
    QObject::connect(&countdownTimer, &QTimer::timeout, [&]() {
        --secondsLeft;
        countdownProgress->setValue(2 - secondsLeft);
        if (secondsLeft > 0) {
            countdownLabel->setText(tr("Security check in progress... confirm in %1 s").arg(secondsLeft));
            return;
        }

        countdownTimer.stop();
        countdownLabel->setText(tr("Ready to confirm purchase."));
        buyButton->setEnabled(true);
    });
    countdownTimer.start();

    payoutCombo->setFocus();
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString value = payoutCombo->currentText().trimmed();
    if (value.isEmpty()) {
        setActionStatus(tr("Choose payout address from your wallet"), true);
        return false;
    }

    UniValue validateParams(UniValue::VARR);
    validateParams.push_back(value.toStdString());
    UniValue validateResult;
    QString validateError;
    if (!executeRpc("validateaddress", validateParams, validateResult, validateError) || !validateResult.isObject()) {
        setActionStatus(tr("Failed to validate selected payout address: %1").arg(validateError), true);
        return false;
    }

    const UniValue& isValidValue = find_value(validateResult, "isvalid");
    const UniValue& isMineValue = find_value(validateResult, "ismine");
    const UniValue& isWatchOnlyValue = find_value(validateResult, "iswatchonly");
    if (!isValidValue.isBool() || !isValidValue.get_bool() ||
        !isMineValue.isBool() || !isMineValue.get_bool() ||
        (isWatchOnlyValue.isBool() && isWatchOnlyValue.get_bool())) {
        setActionStatus(tr("Payout address must be your own receive address"), true);
        return false;
    }

    payoutOut = value;
    return true;
}

bool NicknamesPage::promptUpdatePayoutDialog(const QString& nickname,
                                             const QString& oldPayout,
                                             QString& newPayoutOut)
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("Update Payout"));
    dialog.setModal(true);
    dialog.setStyleSheet(
        "QDialog { background: #f3f8ff; }"
        "QLabel { color: #1d2a3b; }"
        "QLabel#meta { color: #4f6280; }"
        "QFrame#headerBox { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #eff5ff, stop:1 #deebff); border: 1px solid #c7d9f2; border-radius: 10px; }"
        "QLabel#headerTitle { color: #10233f; font-size: 14px; font-weight: 700; }"
        "QLineEdit { min-width: 520px; }"
        "QLineEdit[readOnly=\"true\"] { background: #edf2fb; color: #51617a; border: 1px solid #cad8ed; }"
        "QPushButton { min-width: 110px; min-height: 32px; }");

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    QFrame* headerBox = new QFrame(&dialog);
    headerBox->setObjectName("headerBox");
    QVBoxLayout* headerLayout = new QVBoxLayout(headerBox);
    headerLayout->setContentsMargins(12, 10, 12, 10);
    headerLayout->setSpacing(3);
    QLabel* headerTitle = new QLabel(tr("Payout update"), headerBox);
    headerTitle->setObjectName("headerTitle");
    QLabel* headerSub = new QLabel(tr("Nickname: %1").arg(nickname), headerBox);
    headerSub->setObjectName("meta");
    headerLayout->addWidget(headerTitle);
    headerLayout->addWidget(headerSub);
    layout->addWidget(headerBox);

    QLabel* oldLabel = new QLabel(tr("Current payout address"), &dialog);
    layout->addWidget(oldLabel);

    QLineEdit* oldEdit = new QLineEdit(&dialog);
    oldEdit->setReadOnly(true);
    oldEdit->setText(oldPayout.trimmed());
    layout->addWidget(oldEdit);

    QLabel* newLabel = new QLabel(tr("New payout address"), &dialog);
    layout->addWidget(newLabel);

    QLineEdit* newEdit = new QLineEdit(&dialog);
    newEdit->setPlaceholderText(tr("Enter new payout address"));
    layout->addWidget(newEdit);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Continue"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
    layout->addWidget(buttons);

    newEdit->setFocus();
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString value = newEdit->text().trimmed();
    if (value.isEmpty()) {
        setActionStatus(tr("New payout address is required"), true);
        return false;
    }
    if (value == oldPayout.trimmed()) {
        setActionStatus(tr("New payout address must differ from current payout address"), true);
        return false;
    }

    newPayoutOut = value;
    return true;
}

bool NicknamesPage::promptDelayedConfirmationDialog(const QString& title,
                                                    const QString& description,
                                                    const int delaySeconds)
{
    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.setStyleSheet(
        "QDialog { background: #f3f8ff; }"
        "QLabel { color: #1d2a3b; }"
        "QLabel#meta { color: #4f6280; }"
        "QLabel#countdownLabel { color: #3b4f6a; font-weight: 600; }"
        "QProgressBar#countdownProgress { border: 1px solid #c9d7ea; border-radius: 4px; background: #eef3fb; }"
        "QProgressBar#countdownProgress::chunk { background: qlineargradient(x1:0, y1:0, x2:1, y2:0, stop:0 #2fbf71, stop:1 #23a05d); border-radius: 3px; }"
        "QPushButton { min-width: 110px; min-height: 32px; }");

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    QLabel* info = new QLabel(description, &dialog);
    info->setObjectName("meta");
    info->setWordWrap(true);
    layout->addWidget(info);

    QLabel* countdownLabel = new QLabel(tr("Security check in progress... confirm in %1 s").arg(delaySeconds), &dialog);
    countdownLabel->setObjectName("countdownLabel");
    layout->addWidget(countdownLabel);

    QProgressBar* countdownProgress = new QProgressBar(&dialog);
    countdownProgress->setObjectName("countdownProgress");
    countdownProgress->setRange(0, delaySeconds);
    countdownProgress->setValue(0);
    countdownProgress->setTextVisible(false);
    countdownProgress->setFixedHeight(8);
    layout->addWidget(countdownProgress);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    QPushButton* confirmButton = buttons->button(QDialogButtonBox::Ok);
    confirmButton->setText(tr("Confirm"));
    confirmButton->setEnabled(false);
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
    layout->addWidget(buttons);

    int secondsLeft = delaySeconds;
    QTimer countdownTimer(&dialog);
    countdownTimer.setInterval(1000);
    QObject::connect(&countdownTimer, &QTimer::timeout, [&]() {
        --secondsLeft;
        countdownProgress->setValue(delaySeconds - secondsLeft);
        if (secondsLeft > 0) {
            countdownLabel->setText(tr("Security check in progress... confirm in %1 s").arg(secondsLeft));
            return;
        }

        countdownTimer.stop();
        countdownLabel->setText(tr("Ready to confirm."));
        confirmButton->setEnabled(true);
    });
    countdownTimer.start();

    return dialog.exec() == QDialog::Accepted;
}

bool NicknamesPage::promptForPayoutAddress(const QString& title, const QString& defaultValue, QString& payoutOut)
{
    QString initialValue = defaultValue.trimmed();
    if (initialValue.isEmpty()) {
        QString error;
        QString suggested;
        if (getNewWalletAddress(suggested, nullptr, error)) {
            initialValue = suggested;
        }
    }

    return promptTextDialog(
        title,
        tr("Set the payout destination for this nickname."),
        tr("Payout address"),
        tr("KUBU address"),
        initialValue,
        payoutOut);
}

bool NicknamesPage::promptForOwnerPubKey(const QString& title,
                                         const QString& nickname,
                                         const QString& currentOwnerAddress,
                                         const QString& currentOwnerPubKey,
                                         QString& pubKeyOut)
{
    Q_UNUSED(currentOwnerPubKey);

    QDialog dialog(this);
    dialog.setWindowTitle(title);
    dialog.setModal(true);
    dialog.setStyleSheet(
        "QDialog { background: #f3f8ff; }"
        "QLabel { color: #1d2a3b; }"
        "QLabel#meta { color: #4f6280; }"
        "QFrame#headerBox { background: qlineargradient(x1:0, y1:0, x2:1, y2:1, stop:0 #eff5ff, stop:1 #deebff); border: 1px solid #c7d9f2; border-radius: 10px; }"
        "QLabel#headerTitle { color: #10233f; font-size: 14px; font-weight: 700; }"
        "QLineEdit { min-width: 520px; }"
        "QLineEdit[readOnly=\"true\"] { background: #edf2fb; color: #51617a; border: 1px solid #cad8ed; }"
        "QPushButton { min-width: 110px; min-height: 32px; }");

    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    layout->setContentsMargins(16, 14, 16, 14);
    layout->setSpacing(10);

    QFrame* headerBox = new QFrame(&dialog);
    headerBox->setObjectName("headerBox");
    QVBoxLayout* headerLayout = new QVBoxLayout(headerBox);
    headerLayout->setContentsMargins(12, 10, 12, 10);
    headerLayout->setSpacing(3);
    QLabel* headerTitle = new QLabel(tr("Change nickname owner"), headerBox);
    headerTitle->setObjectName("headerTitle");
    QLabel* headerSub = new QLabel(tr("Nickname: %1").arg(nickname), headerBox);
    headerSub->setObjectName("meta");
    headerLayout->addWidget(headerTitle);
    headerLayout->addWidget(headerSub);
    layout->addWidget(headerBox);

    QLabel* ownerAddressLabel = new QLabel(tr("Current owner address"), &dialog);
    layout->addWidget(ownerAddressLabel);

    QLineEdit* ownerAddressEdit = new QLineEdit(&dialog);
    ownerAddressEdit->setReadOnly(true);
    ownerAddressEdit->setText(currentOwnerAddress.trimmed());
    layout->addWidget(ownerAddressEdit);

    QLabel* ownerPubKeyLabel = new QLabel(tr("Current owner pubkey"), &dialog);
    layout->addWidget(ownerPubKeyLabel);

    QLineEdit* ownerPubKeyEdit = new QLineEdit(&dialog);
    ownerPubKeyEdit->setReadOnly(true);
    ownerPubKeyEdit->setText(currentOwnerPubKey.trimmed());
    layout->addWidget(ownerPubKeyEdit);

    QLabel* newOwnerLabel = new QLabel(tr("New owner address or pubkey"), &dialog);
    layout->addWidget(newOwnerLabel);

    QLineEdit* input = new QLineEdit(&dialog);
    input->setPlaceholderText(tr("KUBU address or compressed pubkey hex"));
    layout->addWidget(input);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Continue"));
    buttons->button(QDialogButtonBox::Cancel)->setText(tr("Cancel"));
    connect(buttons, SIGNAL(accepted()), &dialog, SLOT(accept()));
    connect(buttons, SIGNAL(rejected()), &dialog, SLOT(reject()));
    layout->addWidget(buttons);

    input->setFocus();
    if (dialog.exec() != QDialog::Accepted) {
        return false;
    }

    const QString value = input->text().trimmed();
    if (value.isEmpty()) {
        setActionStatus(tr("New owner address or pubkey is required"), true);
        return false;
    }

    pubKeyOut = value;
    return true;
}

bool NicknamesPage::executeRpc(const std::string& method, const UniValue& params, UniValue& result, QString& errorOut) const
{
    try {
        JSONRPCRequest request;
        request.strMethod = method;
        request.params = params;
        result = tableRPC.execute(request);
        return true;
    } catch (UniValue& objError) {
        try {
            const int code = find_value(objError, "code").get_int();
            const std::string message = find_value(objError, "message").get_str();
            errorOut = QString::fromStdString(message) + tr(" (code %1)").arg(code);
        } catch (const std::runtime_error&) {
            errorOut = QString::fromStdString(objError.write());
        }
    } catch (const std::exception& e) {
        errorOut = QString::fromStdString(e.what());
    }

    if (errorOut.isEmpty()) {
        errorOut = tr("Unknown RPC error");
    }
    return false;
}

bool NicknamesPage::getNormalizedNickname(std::string& normalizedOut, QString& errorOut) const
{
    const QString nicknameInput = nicknameEdit->text().trimmed();
    if (nicknameInput.isEmpty()) {
        errorOut = tr("Nickname is required");
        return false;
    }

    std::string candidate = nicknameInput.toStdString();
    if (!candidate.empty() && candidate[0] == '@') {
        candidate.erase(0, 1);
    }

    std::string normalized;
    Nicknames::NormalizeNickname(candidate, normalized);

    std::string validationReason;
    if (!Nicknames::IsValidNormalizedNickname(normalized, &validationReason)) {
        errorOut = QString::fromStdString(validationReason);
        return false;
    }

    normalizedOut = normalized;
    return true;
}

bool NicknamesPage::getNewWalletAddress(QString& addressOut, QString* pubKeyHexOut, QString& errorOut) const
{
    UniValue getAddressParams(UniValue::VARR);
    // Keep nickname-generated addresses explicitly labeled so they are easy to spot in Receive.
    getAddressParams.push_back(std::string("nickname"));
    UniValue getAddressResult;
    if (!executeRpc("getnewaddress", getAddressParams, getAddressResult, errorOut)) {
        return false;
    }

    if (!getAddressResult.isStr()) {
        errorOut = tr("getnewaddress returned unexpected result");
        return false;
    }

    addressOut = QString::fromStdString(getAddressResult.get_str());

    if (!pubKeyHexOut) {
        return true;
    }

    UniValue validateParams(UniValue::VARR);
    validateParams.push_back(addressOut.toStdString());

    UniValue validateResult;
    if (!executeRpc("validateaddress", validateParams, validateResult, errorOut)) {
        return false;
    }

    if (!validateResult.isObject()) {
        errorOut = tr("validateaddress returned unexpected result");
        return false;
    }

    const UniValue& isValidValue = find_value(validateResult, "isvalid");
    if (!isValidValue.isBool() || !isValidValue.get_bool()) {
        errorOut = tr("Generated address failed validation");
        return false;
    }

    const UniValue& pubKeyValue = find_value(validateResult, "pubkey");
    if (!pubKeyValue.isStr() || pubKeyValue.get_str().empty()) {
        errorOut = tr("No public key found for generated address");
        return false;
    }

    *pubKeyHexOut = QString::fromStdString(pubKeyValue.get_str());
    return true;
}

bool NicknamesPage::getWalletReceiveAddresses(QStringList& addressesOut, QString& errorOut) const
{
    addressesOut.clear();

    UniValue listParams(UniValue::VARR);
    listParams.push_back(0);
    listParams.push_back(true);
    listParams.push_back(false);

    UniValue listResult;
    if (!executeRpc("listreceivedbyaddress", listParams, listResult, errorOut)) {
        return false;
    }

    if (!listResult.isArray()) {
        errorOut = tr("listreceivedbyaddress returned unexpected result");
        return false;
    }

    std::set<std::string> uniqueAddresses;
    for (size_t i = 0; i < listResult.size(); ++i) {
        const UniValue& item = listResult[i];
        if (!item.isObject()) {
            continue;
        }

        const UniValue& addressValue = find_value(item, "address");
        if (!addressValue.isStr() || addressValue.get_str().empty()) {
            continue;
        }

        const std::string address = addressValue.get_str();
        if (!CBitcoinAddress(address).IsValid()) {
            continue;
        }

        if (uniqueAddresses.insert(address).second) {
            addressesOut << QString::fromStdString(address);
        }
    }

    if (!addressesOut.isEmpty()) {
        return true;
    }

    QString generatedAddress;
    if (!getNewWalletAddress(generatedAddress, nullptr, errorOut)) {
        return false;
    }

    addressesOut << generatedAddress;
    return true;
}

bool NicknamesPage::getFundedOwnerAddressAndPubKey(const std::set<std::string>& excludedPubKeys,
                                                   QString& addressOut,
                                                   QString& pubKeyHexOut,
                                                   QString& errorOut) const
{
    UniValue listParams(UniValue::VARR);
    listParams.push_back(1);
    listParams.push_back(9999999);

    UniValue listResult;
    if (!executeRpc("listunspent", listParams, listResult, errorOut)) {
        return false;
    }
    if (!listResult.isArray()) {
        errorOut = tr("listunspent returned unexpected result");
        return false;
    }

    for (size_t i = 0; i < listResult.size(); ++i) {
        const UniValue& item = listResult[i];
        if (!item.isObject()) {
            continue;
        }

        const UniValue& spendableValue = find_value(item, "spendable");
        if (spendableValue.isBool() && !spendableValue.get_bool()) {
            continue;
        }

        const UniValue& addressValue = find_value(item, "address");
        if (!addressValue.isStr()) {
            continue;
        }
        const QString candidateAddress = QString::fromStdString(addressValue.get_str());
        if (candidateAddress.isEmpty()) {
            continue;
        }

        UniValue validateParams(UniValue::VARR);
        validateParams.push_back(candidateAddress.toStdString());

        QString validateError;
        UniValue validateResult;
        if (!executeRpc("validateaddress", validateParams, validateResult, validateError) || !validateResult.isObject()) {
            continue;
        }

        const UniValue& isValidValue = find_value(validateResult, "isvalid");
        if (!isValidValue.isBool() || !isValidValue.get_bool()) {
            continue;
        }

        const UniValue& pubKeyValue = find_value(validateResult, "pubkey");
        if (!pubKeyValue.isStr() || pubKeyValue.get_str().empty()) {
            continue;
        }
        if (excludedPubKeys.count(pubKeyValue.get_str()) > 0) {
            continue;
        }

        addressOut = candidateAddress;
        pubKeyHexOut = QString::fromStdString(pubKeyValue.get_str());
        return true;
    }

    errorOut = tr("No confirmed spendable UTXO with owner pubkey found. Receive some coins first and retry.");
    return false;
}

bool NicknamesPage::resolveOwnerInputToPubKey(const QString& ownerInput,
                                              const QString& currentOwnerPubKey,
                                              QString& pubKeyHexOut,
                                              QString& errorOut) const
{
    const QString value = ownerInput.trimmed();
    if (value.isEmpty()) {
        errorOut = tr("New owner address or pubkey is required");
        return false;
    }

    auto validateCompressedPubKeyHex = [&](const QString& candidate, QString& normalizedOut) -> bool {
        const std::string hex = candidate.trimmed().toStdString();
        if (!IsHex(hex)) {
            return false;
        }
        const std::vector<unsigned char> bytes = ParseHex(hex);
        if (bytes.size() != CPubKey::COMPRESSED_SIZE) {
            return false;
        }
        const CPubKey pubKey(bytes.begin(), bytes.end());
        if (!pubKey.IsFullyValid() || !pubKey.IsCompressed()) {
            return false;
        }
        normalizedOut = QString::fromStdString(HexStr(pubKey.begin(), pubKey.end()));
        return true;
    };

    QString normalizedPubKey;
    if (!validateCompressedPubKeyHex(value, normalizedPubKey)) {
        UniValue validateParams(UniValue::VARR);
        validateParams.push_back(value.toStdString());

        UniValue validateResult;
        if (!executeRpc("validateaddress", validateParams, validateResult, errorOut) || !validateResult.isObject()) {
            errorOut = tr("Invalid owner address or pubkey");
            return false;
        }

        const UniValue& isValidValue = find_value(validateResult, "isvalid");
        if (!isValidValue.isBool() || !isValidValue.get_bool()) {
            errorOut = tr("Invalid owner address");
            return false;
        }

        const UniValue& pubKeyValue = find_value(validateResult, "pubkey");
        if (!pubKeyValue.isStr() || pubKeyValue.get_str().empty()) {
            errorOut = tr("Cannot derive pubkey from this address. Use an address from this wallet or enter recipient compressed pubkey.");
            return false;
        }

        const QString addressPubKey = QString::fromStdString(pubKeyValue.get_str());
        if (!validateCompressedPubKeyHex(addressPubKey, normalizedPubKey)) {
            errorOut = tr("Address resolved to invalid owner pubkey");
            return false;
        }
    }

    if (!currentOwnerPubKey.trimmed().isEmpty() &&
        normalizedPubKey.compare(currentOwnerPubKey.trimmed(), Qt::CaseInsensitive) == 0) {
        errorOut = tr("New owner pubkey must differ from current owner pubkey");
        return false;
    }

    pubKeyHexOut = normalizedPubKey;
    return true;
}

void NicknamesPage::addReceiveRequestEntry(const QString& address, const QString& label, const QString& message) const
{
    if (!model || !model->getRecentRequestsTableModel()) {
        return;
    }

    const QString cleanAddress = address.trimmed();
    if (cleanAddress.isEmpty()) {
        return;
    }

    SendCoinsRecipient request(cleanAddress, label.trimmed(), 0, message.trimmed());
    model->getRecentRequestsTableModel()->addNewRequest(request);
}

bool NicknamesPage::hasConfirmedSpendableUtxoForAddress(const QString& address, QString& errorOut) const
{
    if (address.trimmed().isEmpty()) {
        errorOut = tr("Nickname owner address is unavailable");
        return false;
    }

    UniValue listParams(UniValue::VARR);
    listParams.push_back(1);
    listParams.push_back(9999999);

    UniValue listResult;
    if (!executeRpc("listunspent", listParams, listResult, errorOut)) {
        return false;
    }
    if (!listResult.isArray()) {
        errorOut = tr("listunspent returned unexpected result");
        return false;
    }

    const std::string ownerAddress = address.trimmed().toStdString();
    for (size_t i = 0; i < listResult.size(); ++i) {
        const UniValue& item = listResult[i];
        if (!item.isObject()) {
            continue;
        }

        const UniValue& spendableValue = find_value(item, "spendable");
        if (spendableValue.isBool() && !spendableValue.get_bool()) {
            continue;
        }

        const UniValue& addressValue = find_value(item, "address");
        if (addressValue.isStr() && addressValue.get_str() == ownerAddress) {
            return true;
        }
    }

    errorOut = tr("No confirmed spendable UTXO found for nickname owner address");
    return false;
}

bool NicknamesPage::ensureOwnerAddressFundedForUpdate(const QString& ownerAddress, QString& infoOut, QString& errorOut) const
{
    infoOut.clear();
    UniValue validateParams(UniValue::VARR);
    validateParams.push_back(ownerAddress.trimmed().toStdString());
    UniValue validateResult;
    if (executeRpc("validateaddress", validateParams, validateResult, errorOut) && validateResult.isObject()) {
        const UniValue& isMineValue = find_value(validateResult, "ismine");
        if (isMineValue.isBool() && !isMineValue.get_bool()) {
            errorOut = tr("Wallet does not control nickname owner address input");
            return false;
        }
    }

    QString checkError;
    if (hasConfirmedSpendableUtxoForAddress(ownerAddress, checkError)) {
        return true;
    }

    UniValue sendParams(UniValue::VARR);
    sendParams.push_back(ownerAddress.trimmed().toStdString());
    sendParams.push_back(std::string("0.01000000"));
    sendParams.push_back(std::string("nickname owner funding"));
    sendParams.push_back(std::string("nickname owner key input"));

    UniValue sendResult;
    if (!executeRpc("sendtoaddress", sendParams, sendResult, errorOut)) {
        return false;
    }

    const QString fundingTxid = RpcResultToString(sendResult);

    UniValue chainInfoParams(UniValue::VARR);
    UniValue chainInfoResult;
    if (executeRpc("getblockchaininfo", chainInfoParams, chainInfoResult, errorOut) && chainInfoResult.isObject()) {
        const UniValue& chainValue = find_value(chainInfoResult, "chain");
        if (chainValue.isStr() && chainValue.get_str() == "regtest") {
            UniValue getAddrParams(UniValue::VARR);
            UniValue getAddrResult;
            QString mineError;
            if (executeRpc("getnewaddress", getAddrParams, getAddrResult, mineError) && getAddrResult.isStr()) {
                UniValue mineParams(UniValue::VARR);
                mineParams.push_back(1);
                mineParams.push_back(getAddrResult.get_str());
                UniValue mineResult;
                executeRpc("generatetoaddress", mineParams, mineResult, mineError);
            }
        }
    }

    QString postFundingCheckError;
    if (!hasConfirmedSpendableUtxoForAddress(ownerAddress, postFundingCheckError)) {
        errorOut = tr("Owner key funding tx sent (%1), but it is not confirmed yet. Wait for 1 confirmation and retry update.")
                       .arg(fundingTxid);
        return false;
    }

    infoOut = tr("Owner key input auto-funded (%1)").arg(fundingTxid);
    return true;
}

QString NicknamesPage::maybeMineNicknameTxOnRegtest() const
{
    QString error;
    UniValue chainInfoParams(UniValue::VARR);
    UniValue chainInfoResult;
    if (!executeRpc("getblockchaininfo", chainInfoParams, chainInfoResult, error) || !chainInfoResult.isObject()) {
        return QString();
    }

    const UniValue& chainValue = find_value(chainInfoResult, "chain");
    if (!chainValue.isStr() || chainValue.get_str() != "regtest") {
        return QString();
    }

    UniValue getAddrParams(UniValue::VARR);
    UniValue getAddrResult;
    if (!executeRpc("getnewaddress", getAddrParams, getAddrResult, error) || !getAddrResult.isStr()) {
        return tr("Regtest auto-confirm skipped: failed to create mining address");
    }

    UniValue mineParams(UniValue::VARR);
    mineParams.push_back(1);
    mineParams.push_back(getAddrResult.get_str());

    UniValue mineResult;
    if (!executeRpc("generatetoaddress", mineParams, mineResult, error)) {
        return tr("Regtest auto-confirm skipped: %1").arg(error);
    }

    if (mineResult.isArray() && mineResult.size() > 0 && mineResult[0].isStr()) {
        return tr("Regtest auto-confirmed in 1 block (%1)").arg(QString::fromStdString(mineResult[0].get_str()));
    }

    return tr("Regtest auto-confirmed in 1 block");
}

void NicknamesPage::setActionStatus(const QString& message, bool isError)
{
    if (message.isEmpty()) {
        actionStatusValue->setText(tr("Ready."));
        actionStatusValue->setObjectName("actionStatusOk");
        style()->unpolish(actionStatusValue);
        style()->polish(actionStatusValue);
        return;
    }

    actionStatusValue->setText(message);
    actionStatusValue->setObjectName(isError ? "actionStatusErr" : "actionStatusOk");
    style()->unpolish(actionStatusValue);
    style()->polish(actionStatusValue);
}

bool NicknamesPage::ensureWalletReady()
{
    if (model) {
        return true;
    }

    setActionStatus(tr("Wallet model is unavailable"), true);
    return false;
}

void NicknamesPage::onRegisterClicked()
{
    if (!ensureWalletReady()) {
        return;
    }

    std::string normalized;
    QString error;
    if (!getNormalizedNickname(normalized, error)) {
        setActionStatus(error, true);
        return;
    }

    const QString normalizedQt = QString::fromStdString(normalized);
    if (!ensureLookupForNickname(normalizedQt)) {
        return;
    }

    if (!canRegisterLookup) {
        setActionStatus(tr("Nickname is not available for registration"), true);
        return;
    }

    QString suggestedPayoutAddress;
    QString suggestedPayoutError;
    if (!getNewWalletAddress(suggestedPayoutAddress, nullptr, suggestedPayoutError)) {
        suggestedPayoutAddress.clear();
    }

    QString payoutAddress;
    if (!promptBuyDialog(normalizedQt,
                         currentRegistrationFee,
                         currentBondAmount,
                         currentRenewalFee,
                         suggestedPayoutAddress,
                         payoutAddress)) {
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) {
        setActionStatus(tr("Wallet unlock was cancelled"), true);
        return;
    }

    QString ownerAddress;
    QString ownerPubKey;
    QString lastOwnerInputError;
    std::set<std::string> excludedOwnerPubKeys;
    UniValue result;
    while (true) {
        if (!getFundedOwnerAddressAndPubKey(excludedOwnerPubKeys, ownerAddress, ownerPubKey, error)) {
            if (!lastOwnerInputError.isEmpty()) {
                error = lastOwnerInputError + "\n" + error;
            }
            setActionStatus(error, true);
            return;
        }

        UniValue params(UniValue::VARR);
        params.push_back(normalized);
        params.push_back(ownerPubKey.toStdString());
        params.push_back(payoutAddress.toStdString());

        if (executeRpc("registernickname", params, result, error)) {
            break;
        }

        if (!IsOwnerInputMissingError(error)) {
            setActionStatus(error, true);
            return;
        }

        lastOwnerInputError = error;
        excludedOwnerPubKeys.insert(ownerPubKey.toStdString());
    }

    QString registerStatus = tr("Registration transaction sent: %1 (owner key address: %2)")
        .arg(RpcResultToString(result))
        .arg(ownerAddress);
    const QString registerMineInfo = maybeMineNicknameTxOnRegtest();
    if (!registerMineInfo.isEmpty()) {
        registerStatus += "\n" + registerMineInfo;
    }
    setActionStatus(registerStatus, false);
    addReceiveRequestEntry(
        payoutAddress,
        tr("nickname: %1").arg(normalizedQt),
        tr("Nickname payout address"));
    onLookupClicked();
    refreshWalletNicknames();
}

void NicknamesPage::onUpdateClicked()
{
    if (!ensureWalletReady()) {
        return;
    }

    std::string normalized;
    QString error;
    if (!getNormalizedNickname(normalized, error)) {
        setActionStatus(error, true);
        return;
    }

    const QString normalizedQt = QString::fromStdString(normalized);
    if (!ensureLookupForNickname(normalizedQt)) {
        return;
    }

    if (!currentLookupMutable) {
        setActionStatus(tr("Nickname is not mutable by this wallet"), true);
        return;
    }

    QString payoutAddress;
    if (!promptUpdatePayoutDialog(normalizedQt, currentLookupPayoutAddress, payoutAddress)) {
        return;
    }

    if (!promptDelayedConfirmationDialog(
            tr("Confirm payout update"),
            tr("Nickname: %1\nCurrent payout: %2\nNew payout: %3")
                .arg(normalizedQt)
                .arg(currentLookupPayoutAddress)
                .arg(payoutAddress),
            2)) {
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) {
        setActionStatus(tr("Wallet unlock was cancelled"), true);
        return;
    }

    UniValue params(UniValue::VARR);
    params.push_back(normalized);
    params.push_back(payoutAddress.toStdString());

    QString fundingInfo;
    if (!ensureOwnerAddressFundedForUpdate(currentLookupOwnerAddress, fundingInfo, error)) {
        setActionStatus(error, true);
        return;
    }
    if (!fundingInfo.isEmpty()) {
        setActionStatus(fundingInfo, false);
    }

    UniValue result;
    if (!executeRpc("updatenickname", params, result, error)) {
        if (IsOwnerInputMissingError(error)) {
            QString retryFundingInfo;
            QString retryFundingError;
            if (!ensureOwnerAddressFundedForUpdate(currentLookupOwnerAddress, retryFundingInfo, retryFundingError)) {
                setActionStatus(retryFundingError, true);
                return;
            }
            if (!executeRpc("updatenickname", params, result, error)) {
                setActionStatus(error, true);
                return;
            }
        } else {
            setActionStatus(error, true);
            return;
        }
    }

    QString updateStatus = tr("Update transaction sent: %1").arg(RpcResultToString(result));
    const QString updateMineInfo = maybeMineNicknameTxOnRegtest();
    if (!updateMineInfo.isEmpty()) {
        updateStatus += "\n" + updateMineInfo;
    }
    setActionStatus(updateStatus, false);
    onLookupClicked();
    refreshWalletNicknames();
}

void NicknamesPage::onUseNewReceivePayoutClicked()
{
    if (!ensureWalletReady()) {
        return;
    }

    std::string normalized;
    QString error;
    if (!getNormalizedNickname(normalized, error)) {
        setActionStatus(error, true);
        return;
    }

    const QString normalizedQt = QString::fromStdString(normalized);
    if (!ensureLookupForNickname(normalizedQt)) {
        return;
    }

    if (!currentLookupMutable) {
        setActionStatus(tr("Nickname is not mutable by this wallet"), true);
        return;
    }

    QString newPayoutAddress;
    if (!getNewWalletAddress(newPayoutAddress, nullptr, error)) {
        setActionStatus(error, true);
        return;
    }

    if (!promptDelayedConfirmationDialog(
            tr("Confirm payout update"),
            tr("Nickname: %1\nCurrent payout: %2\nNew payout: %3")
                .arg(normalizedQt)
                .arg(currentLookupPayoutAddress)
                .arg(newPayoutAddress),
            2)) {
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) {
        setActionStatus(tr("Wallet unlock was cancelled"), true);
        return;
    }

    UniValue params(UniValue::VARR);
    params.push_back(normalized);
    params.push_back(newPayoutAddress.toStdString());

    QString fundingInfo;
    if (!ensureOwnerAddressFundedForUpdate(currentLookupOwnerAddress, fundingInfo, error)) {
        setActionStatus(error, true);
        return;
    }
    if (!fundingInfo.isEmpty()) {
        setActionStatus(fundingInfo, false);
    }

    UniValue result;
    if (!executeRpc("updatenickname", params, result, error)) {
        if (IsOwnerInputMissingError(error)) {
            QString retryFundingInfo;
            QString retryFundingError;
            if (!ensureOwnerAddressFundedForUpdate(currentLookupOwnerAddress, retryFundingInfo, retryFundingError)) {
                setActionStatus(retryFundingError, true);
                return;
            }
            if (!executeRpc("updatenickname", params, result, error)) {
                setActionStatus(error, true);
                return;
            }
        } else {
            setActionStatus(error, true);
            return;
        }
    }

    QString quickPayoutStatus = tr("Payout switched to new receive address: %1 (tx: %2)")
                        .arg(newPayoutAddress)
                        .arg(RpcResultToString(result));
    const QString quickPayoutMineInfo = maybeMineNicknameTxOnRegtest();
    if (!quickPayoutMineInfo.isEmpty()) {
        quickPayoutStatus += "\n" + quickPayoutMineInfo;
    }
    setActionStatus(quickPayoutStatus, false);
    onLookupClicked();
    refreshWalletNicknames();
}

void NicknamesPage::onTransferClicked()
{
    if (!ensureWalletReady()) {
        return;
    }

    std::string normalized;
    QString error;
    if (!getNormalizedNickname(normalized, error)) {
        setActionStatus(error, true);
        return;
    }

    const QString normalizedQt = QString::fromStdString(normalized);
    if (!ensureLookupForNickname(normalizedQt)) {
        return;
    }

    if (!currentLookupMutable) {
        setActionStatus(tr("Nickname is not transferable by this wallet"), true);
        return;
    }

    QString newOwnerInput;
    if (!promptForOwnerPubKey(tr("Change owner"),
                              normalizedQt,
                              currentLookupOwnerAddress,
                              currentLookupOwnerPubKey,
                              newOwnerInput)) {
        return;
    }

    QString newOwnerPubKey;
    if (!resolveOwnerInputToPubKey(newOwnerInput, currentLookupOwnerPubKey, newOwnerPubKey, error)) {
        setActionStatus(error, true);
        return;
    }

    QString confirmationText = tr("Nickname: %1\nCurrent owner address: %2\nNew owner input: %3\nResolved owner pubkey: %4")
                                   .arg(normalizedQt)
                                   .arg(currentLookupOwnerAddress)
                                   .arg(newOwnerInput)
                                   .arg(newOwnerPubKey);
    if (!promptDelayedConfirmationDialog(
            tr("Confirm owner change"),
            confirmationText,
            2)) {
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) {
        setActionStatus(tr("Wallet unlock was cancelled"), true);
        return;
    }

    UniValue params(UniValue::VARR);
    params.push_back(normalized);
    params.push_back(newOwnerPubKey.toStdString());

    QString fundingInfo;
    if (!ensureOwnerAddressFundedForUpdate(currentLookupOwnerAddress, fundingInfo, error)) {
        setActionStatus(error, true);
        return;
    }
    if (!fundingInfo.isEmpty()) {
        setActionStatus(fundingInfo, false);
    }

    UniValue result;
    if (!executeRpc("transfernickname", params, result, error)) {
        if (IsOwnerInputMissingError(error)) {
            QString retryFundingInfo;
            QString retryFundingError;
            if (!ensureOwnerAddressFundedForUpdate(currentLookupOwnerAddress, retryFundingInfo, retryFundingError)) {
                setActionStatus(retryFundingError, true);
                return;
            }
            if (!executeRpc("transfernickname", params, result, error)) {
                setActionStatus(error, true);
                return;
            }
        } else {
            setActionStatus(error, true);
            return;
        }
    }

    QString transferStatus = tr("Transfer completed (transaction sent): %1").arg(RpcResultToString(result));
    const QString transferMineInfo = maybeMineNicknameTxOnRegtest();
    if (!transferMineInfo.isEmpty()) {
        transferStatus += "\n" + transferMineInfo;
    }
    setActionStatus(transferStatus, false);
    onLookupClicked();
    refreshWalletNicknames();
}

void NicknamesPage::onRenewClicked()
{
    if (!ensureWalletReady()) {
        return;
    }

    std::string normalized;
    QString error;
    if (!getNormalizedNickname(normalized, error)) {
        setActionStatus(error, true);
        return;
    }

    const QString normalizedQt = QString::fromStdString(normalized);
    if (!ensureLookupForNickname(normalizedQt)) {
        return;
    }

    if (!currentLookupMutable) {
        setActionStatus(tr("Nickname is not renewable by this wallet"), true);
        return;
    }

    const CAmount projectedBondAfterRenew =
        (currentLookupBondAmount <= MAX_MONEY - currentRenewalBondIncreaseAmount)
            ? (currentLookupBondAmount + currentRenewalBondIncreaseAmount)
            : currentLookupBondAmount;

    if (!promptDelayedConfirmationDialog(
            tr("Confirm renewal"),
            tr("Nickname: %1\nRenewal fee: %2\nAdditional locked bond: %3\nBond after renewal: %4")
                .arg(normalizedQt)
                .arg(currentRenewalFee)
                .arg(currentRenewalBondIncrease)
                .arg(FormatKubuAmountFromNative(projectedBondAfterRenew)),
            2)) {
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) {
        setActionStatus(tr("Wallet unlock was cancelled"), true);
        return;
    }

    UniValue params(UniValue::VARR);
    params.push_back(normalized);

    UniValue result;
    if (!executeRpc("renewnickname", params, result, error)) {
        setActionStatus(error, true);
        return;
    }

    QString renewStatus = tr("Renew transaction sent: %1").arg(RpcResultToString(result));
    const QString renewMineInfo = maybeMineNicknameTxOnRegtest();
    if (!renewMineInfo.isEmpty()) {
        renewStatus += "\n" + renewMineInfo;
    }
    setActionStatus(renewStatus, false);
    onLookupClicked();
    refreshWalletNicknames();
}

void NicknamesPage::onReleaseClicked()
{
    if (!ensureWalletReady()) {
        return;
    }

    std::string normalized;
    QString error;
    if (!getNormalizedNickname(normalized, error)) {
        setActionStatus(error, true);
        return;
    }

    const QString normalizedQt = QString::fromStdString(normalized);
    if (!ensureLookupForNickname(normalizedQt)) {
        return;
    }

    if (!currentLookupMutable) {
        setActionStatus(tr("Nickname is not releasable by this wallet"), true);
        return;
    }

    if (!promptDelayedConfirmationDialog(
            tr("Confirm free nickname"),
            tr("Nickname: %1\nAfter this action the nickname will stop resolving until someone registers it again.\nThe bond can be claimed separately.")
                .arg(normalizedQt),
            2)) {
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) {
        setActionStatus(tr("Wallet unlock was cancelled"), true);
        return;
    }

    UniValue params(UniValue::VARR);
    params.push_back(normalized);

    QString fundingInfo;
    if (!ensureOwnerAddressFundedForUpdate(currentLookupOwnerAddress, fundingInfo, error)) {
        setActionStatus(error, true);
        return;
    }
    if (!fundingInfo.isEmpty()) {
        setActionStatus(fundingInfo, false);
    }

    UniValue result;
    if (!executeRpc("releasenickname", params, result, error)) {
        if (IsOwnerInputMissingError(error)) {
            QString retryFundingInfo;
            QString retryFundingError;
            if (!ensureOwnerAddressFundedForUpdate(currentLookupOwnerAddress, retryFundingInfo, retryFundingError)) {
                setActionStatus(retryFundingError, true);
                return;
            }
            if (!executeRpc("releasenickname", params, result, error)) {
                setActionStatus(error, true);
                return;
            }
        } else {
            setActionStatus(error, true);
            return;
        }
    }

    QString releaseStatus = tr("Free nickname transaction sent: %1").arg(RpcResultToString(result));
    const QString releaseMineInfo = maybeMineNicknameTxOnRegtest();
    if (!releaseMineInfo.isEmpty()) {
        releaseStatus += "\n" + releaseMineInfo;
    }
    setActionStatus(releaseStatus, false);
    onLookupClicked();
    refreshWalletNicknames();
}

void NicknamesPage::onClaimClicked()
{
    if (!ensureWalletReady()) {
        return;
    }

    std::string normalized;
    QString error;
    if (!getNormalizedNickname(normalized, error)) {
        setActionStatus(error, true);
        return;
    }

    const QString normalizedQt = QString::fromStdString(normalized);
    if (!ensureLookupForNickname(normalizedQt)) {
        return;
    }

    if (!currentLookupClaimable) {
        setActionStatus(tr("Nickname bond is not claimable by this wallet"), true);
        return;
    }

    if (!promptDelayedConfirmationDialog(
            tr("Confirm bond claim"),
            tr("Nickname: %1\nBond amount: %2\nBond UTXO: %3")
                .arg(normalizedQt)
                .arg(currentBondAmount)
                .arg(currentLookupBondRef.isEmpty() ? tr("(lookup required)") : currentLookupBondRef),
            2)) {
        return;
    }

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) {
        setActionStatus(tr("Wallet unlock was cancelled"), true);
        return;
    }

    UniValue params(UniValue::VARR);
    if (!currentLookupBondRef.trimmed().isEmpty()) {
        params.push_back(currentLookupBondRef.trimmed().toStdString());
    } else {
        params.push_back(normalized);
    }

    UniValue result;
    if (!executeRpc("claimnicknamebond", params, result, error)) {
        setActionStatus(error, true);
        return;
    }

    QString claimStatus = tr("Bond claim transaction sent: %1").arg(RpcResultToString(result));
    const QString claimMineInfo = maybeMineNicknameTxOnRegtest();
    if (!claimMineInfo.isEmpty()) {
        claimStatus += "\n" + claimMineInfo;
    }
    setActionStatus(claimStatus, false);
    onLookupClicked();
    refreshWalletNicknames();
}
