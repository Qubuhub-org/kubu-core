// Copyright (c) 2026 The KUBU developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_NICKNAMESPAGE_H
#define BITCOIN_QT_NICKNAMESPAGE_H

#include "amount.h"

#include <QWidget>
#include <string>
#include <set>

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTabWidget;
class QStringList;
class WalletModel;
class UniValue;
class QPoint;

class NicknamesPage : public QWidget
{
    Q_OBJECT

public:
    explicit NicknamesPage(QWidget* parent = nullptr);
    void setModel(WalletModel* model);

Q_SIGNALS:
    void sendCoins(const QString& destination);

private Q_SLOTS:
    void onNicknameTextChanged(const QString& text);
    void onLookupClicked();
    void onSendClicked();
    void onActionsClicked();
    void onRefreshWalletNicknamesClicked();
    void onWalletNicknameActivated(int row, int column);
    void onWalletNicknameDoubleClicked(int row, int column);
    void onWalletNicknamesContextMenu(const QPoint& point);
    void onRegisterClicked();
    void onUpdateClicked();
    void onTransferClicked();
    void onRenewClicked();
    void onReleaseClicked();
    void onClaimClicked();
    void onUseNewReceivePayoutClicked();

private:
    WalletModel* model;

    QLineEdit* nicknameEdit;
    QLabel* normalizedValue;
    QLabel* statusValue;
    QLabel* availabilityValue;
    QLabel* pricingValue;
    QPushButton* lookupButton;
    QPushButton* actionsButton;
    QLabel* actionStatusValue;
    QLabel* walletAlertsValue;
    QPushButton* refreshButton;
    QTabWidget* walletNicknamesTabs;
    QTableWidget* walletNicknamesTable;
    QTableWidget* renewNicknamesTable;
    QTableWidget* claimableNicknamesTable;
    QTableWidget* historyNicknamesTable;

    QString currentLookupNickname;
    QString currentLookupPayoutAddress;
    QString currentLookupOwnerAddress;
    QString currentLookupOwnerPubKey;
    QString currentLookupBondRef;
    QString currentRegistrationFee;
    QString currentBondAmount;
    QString currentRenewalFee;
    QString currentRenewalBondIncrease;
    CAmount currentLookupBondAmount;
    CAmount currentRenewalBondIncreaseAmount;
    bool canSendLookup;
    bool canRegisterLookup;
    bool currentLookupWalletOwned;
    bool currentLookupMutable;
    bool currentLookupClaimable;

    void clearLookupResult();
    void refreshWalletNicknames();
    QTableWidget* currentNicknamesTable() const;
    bool ensureLookupForNickname(const QString& nickname);
    void openActionMenuForNickname(const QString& nickname, const QPoint& globalPos);
    void showNicknameDetailsDialog(const QString& nickname);
    bool confirmAction(const QString& title, const QString& text) const;
    bool promptTextDialog(const QString& title,
                          const QString& description,
                          const QString& fieldLabel,
                          const QString& placeholder,
                          const QString& initialValue,
                          QString& valueOut);
    bool promptBuyDialog(const QString& nickname,
                         const QString& registrationFee,
                         const QString& bondAmount,
                         const QString& renewalFee,
                         const QString& suggestedPayout,
                         QString& payoutOut);
    bool promptUpdatePayoutDialog(const QString& nickname,
                                  const QString& oldPayout,
                                  QString& newPayoutOut);
    bool promptDelayedConfirmationDialog(const QString& title,
                                         const QString& description,
                                         int delaySeconds);
    bool promptForPayoutAddress(const QString& title, const QString& defaultValue, QString& payoutOut);
    bool promptForOwnerPubKey(const QString& title,
                              const QString& nickname,
                              const QString& currentOwnerAddress,
                              const QString& currentOwnerPubKey,
                              QString& pubKeyOut);
    bool executeRpc(const std::string& method, const UniValue& params, UniValue& result, QString& errorOut) const;
    bool getNormalizedNickname(std::string& normalizedOut, QString& errorOut) const;
    bool getNewWalletAddress(QString& addressOut, QString* pubKeyHexOut, QString& errorOut) const;
    bool getWalletReceiveAddresses(QStringList& addressesOut, QString& errorOut) const;
    bool getFundedOwnerAddressAndPubKey(const std::set<std::string>& excludedPubKeys,
                                        QString& addressOut,
                                        QString& pubKeyHexOut,
                                        QString& errorOut) const;
    bool hasConfirmedSpendableUtxoForAddress(const QString& address, QString& errorOut) const;
    bool ensureOwnerAddressFundedForUpdate(const QString& ownerAddress, QString& infoOut, QString& errorOut) const;
    bool resolveOwnerInputToPubKey(const QString& ownerInput,
                                   const QString& currentOwnerPubKey,
                                   QString& pubKeyHexOut,
                                   QString& errorOut) const;
    void addReceiveRequestEntry(const QString& address, const QString& label, const QString& message) const;
    QString maybeMineNicknameTxOnRegtest() const;
    void setActionStatus(const QString& message, bool isError);
    bool ensureWalletReady();
};

#endif // BITCOIN_QT_NICKNAMESPAGE_H
