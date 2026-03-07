// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "sendcoinsentry.h"
#include "ui_sendcoinsentry.h"

#include "addressbookpage.h"
#include "addresstablemodel.h"
#include "guiutil.h"
#include "nicknamedb.h"
#include "nicknames.h"
#include "optionsmodel.h"
#include "platformstyle.h"
#include "walletmodel.h"

#include "base58.h"
#include "sync.h"
#include "validation.h"

#include <QApplication>
#include <QClipboard>
#include <QValidator>

namespace {
class AddressOrNicknameEntryValidator : public QValidator
{
public:
    explicit AddressOrNicknameEntryValidator(QObject* parent) : QValidator(parent)
    {
    }

    QValidator::State validate(QString& input, int& pos) const override
    {
        Q_UNUSED(pos);

        if (input.isEmpty()) {
            return QValidator::Intermediate;
        }

        for (int idx = 0; idx < input.size();) {
            bool removeChar = false;
            const QChar ch = input.at(idx);
            switch (ch.unicode()) {
            case 0x200B: // ZERO WIDTH SPACE
            case 0xFEFF: // ZERO WIDTH NO-BREAK SPACE
                removeChar = true;
                break;
            default:
                break;
            }
            if (ch.isSpace()) {
                removeChar = true;
            }

            if (removeChar) {
                input.remove(idx, 1);
            } else {
                ++idx;
            }
        }

        QValidator::State state = QValidator::Acceptable;
        for (int idx = 0; idx < input.size(); ++idx) {
            const int ch = input.at(idx).unicode();
            if (ch == '@' && idx == 0) {
                continue;
            }
            const bool allowedAlnum =
                ((ch >= '0' && ch <= '9') ||
                 (ch >= 'a' && ch <= 'z') ||
                 (ch >= 'A' && ch <= 'Z'));
            if ((allowedAlnum && ch != 'l' && ch != 'I' && ch != '0' && ch != 'O') ||
                ch == '_') {
                continue;
            }
            state = QValidator::Invalid;
            break;
        }

        return state;
    }
};

class AddressOrNicknameCheckValidator : public QValidator
{
public:
    explicit AddressOrNicknameCheckValidator(QObject* parent) : QValidator(parent)
    {
    }

    QValidator::State validate(QString& input, int& pos) const override
    {
        Q_UNUSED(pos);

        const std::string candidate = input.trimmed().toStdString();
        if (candidate.empty()) {
            return QValidator::Intermediate;
        }

        if (CBitcoinAddress(candidate).IsValid()) {
            return QValidator::Acceptable;
        }

        std::string nicknameCandidate = candidate;
        if (!nicknameCandidate.empty() && nicknameCandidate[0] == '@') {
            nicknameCandidate.erase(0, 1);
        }

        return Nicknames::IsValidNickname(nicknameCandidate) ? QValidator::Acceptable : QValidator::Invalid;
    }
};

bool ResolveNicknameToAddress(const QString& input, QString* normalizedOut, QString& payoutAddressOut, QString* reasonOut = nullptr)
{
    NicknameStateDB* nicknameDB = GetNicknameStateDB();
    if (!nicknameDB) {
        if (reasonOut) {
            *reasonOut = SendCoinsEntry::tr("Nickname index is unavailable");
        }
        return false;
    }

    std::string nicknameInput = input.trimmed().toStdString();
    if (!nicknameInput.empty() && nicknameInput[0] == '@') {
        nicknameInput.erase(0, 1);
    }

    std::string normalized;
    Nicknames::NormalizeNickname(nicknameInput, normalized);

    std::string validationReason;
    if (!Nicknames::IsValidNormalizedNickname(normalized, &validationReason)) {
        if (reasonOut) {
            *reasonOut = QString::fromStdString(validationReason);
        }
        return false;
    }

    NicknameInfo info;
    if (!nicknameDB->ReadNickname(normalized, info)) {
        if (reasonOut) {
            *reasonOut = SendCoinsEntry::tr("Nickname not found");
        }
        return false;
    }

    int currentHeight = 0;
    {
        LOCK(cs_main);
        currentHeight = chainActive.Height();
    }
    if (info.GetStatus(currentHeight) != Nicknames::Status::ACTIVE) {
        if (reasonOut) {
            *reasonOut = SendCoinsEntry::tr("Nickname is not active");
        }
        return false;
    }

    if (info.payoutAddress.empty()) {
        if (reasonOut) {
            *reasonOut = SendCoinsEntry::tr("Nickname has no payout address");
        }
        return false;
    }

    if (normalizedOut) {
        *normalizedOut = QString::fromStdString(normalized);
    }
    payoutAddressOut = QString::fromStdString(info.payoutAddress);
    return true;
}

bool IsNicknameLikeDestination(const QString& input)
{
    std::string candidate = input.trimmed().toStdString();
    if (candidate.empty()) {
        return false;
    }
    if (CBitcoinAddress(candidate).IsValid()) {
        return false;
    }
    if (!candidate.empty() && candidate[0] == '@') {
        candidate.erase(0, 1);
    }
    return Nicknames::IsValidNickname(candidate);
}

bool ValidateNicknameMemoInput(const QString& memo, const QString& memoTypeHint, QString* reasonOut = nullptr)
{
    const QByteArray memoUtf8 = memo.toUtf8();
    if (memoUtf8.size() > 48) {
        if (reasonOut) {
            *reasonOut = SendCoinsEntry::tr("Memo is too long (max 48 bytes)");
        }
        return false;
    }
    for (int i = 0; i < memoUtf8.size(); ++i) {
        if (memoUtf8.at(i) == '\0') {
            if (reasonOut) {
                *reasonOut = SendCoinsEntry::tr("Memo cannot contain NUL bytes");
            }
            return false;
        }
    }

    const QString memoType = memoTypeHint.trimmed().toLower().isEmpty()
        ? QString("utf8")
        : memoTypeHint.trimmed().toLower();

    if (memoType == "utf8") {
        return true;
    }

    if (memoType == "numeric") {
        for (int i = 0; i < memoUtf8.size(); ++i) {
            const unsigned char c = static_cast<unsigned char>(memoUtf8.at(i));
            if (c < '0' || c > '9') {
                if (reasonOut) {
                    *reasonOut = SendCoinsEntry::tr("Numeric memo may only contain digits 0-9");
                }
                return false;
            }
        }
        return true;
    }

    if (memoType == "alnum") {
        for (int i = 0; i < memoUtf8.size(); ++i) {
            const unsigned char c = static_cast<unsigned char>(memoUtf8.at(i));
            const bool asciiDigit = c >= '0' && c <= '9';
            const bool asciiLower = c >= 'a' && c <= 'z';
            const bool asciiUpper = c >= 'A' && c <= 'Z';
            if (!asciiDigit && !asciiLower && !asciiUpper) {
                if (reasonOut) {
                    *reasonOut = SendCoinsEntry::tr("Alnum memo may only contain ASCII letters and digits");
                }
                return false;
            }
        }
        return true;
    }

    if (reasonOut) {
        *reasonOut = SendCoinsEntry::tr("Unsupported memo type");
    }
    return false;
}

} // namespace

SendCoinsEntry::SendCoinsEntry(const PlatformStyle *_platformStyle, QWidget *parent) :
    QStackedWidget(parent),
    ui(new Ui::SendCoinsEntry),
    model(0),
    platformStyle(_platformStyle),
    applyingRecipientValue(false),
    nicknameMemoRequiredHint(false),
    nicknameMemoTypeHint("utf8")
{
    ui->setupUi(this);

    ui->addressBookButton->setIcon(platformStyle->SingleColorIcon(":/icons/address-book"));
    ui->pasteButton->setIcon(platformStyle->SingleColorIcon(":/icons/editpaste"));
    ui->deleteButton->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    ui->deleteButton_is->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));
    ui->deleteButton_s->setIcon(platformStyle->SingleColorIcon(":/icons/remove"));

    setCurrentWidget(ui->SendCoins);

    if (platformStyle->getUseExtraSpacing())
        ui->payToLayout->setSpacing(4);
#if QT_VERSION >= 0x040700
    ui->addAsLabel->setPlaceholderText(tr("Enter a label for this address to add it to your address book"));
#endif

    // normal bitcoin address field
    GUIUtil::setupAddressWidget(ui->payTo, this);
#if QT_VERSION >= 0x040700
    // Keep explicit send placeholder after setupAddressWidget(), which sets a default address-only text.
    ui->payTo->setPlaceholderText(tr("Enter a KUBU address or nickname"));
#endif
    ui->payTo->setValidator(new AddressOrNicknameEntryValidator(this));
    ui->payTo->setCheckValidator(new AddressOrNicknameCheckValidator(this));
    ui->payTo->setToolTip(tr("Paste a KUBU address or active nickname to send the payment to"));
    ui->nicknameMemoEdit->setPlaceholderText(tr("Optional. Required by some services"));
    ui->nicknameMemoLabel->hide();
    ui->nicknameMemoEdit->hide();
    // just a label for displaying bitcoin address(es)
    ui->payTo_is->setFont(GUIUtil::fixedPitchFont());

    // Connect signals
    connect(ui->payAmount, SIGNAL(valueChanged()), this, SIGNAL(payAmountChanged()));
    connect(ui->checkboxSubtractFeeFromAmount, SIGNAL(toggled(bool)), this, SIGNAL(subtractFeeFromAmountChanged()));
    connect(ui->deleteButton, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    connect(ui->deleteButton_is, SIGNAL(clicked()), this, SLOT(deleteClicked()));
    connect(ui->deleteButton_s, SIGNAL(clicked()), this, SLOT(deleteClicked()));
}

SendCoinsEntry::~SendCoinsEntry()
{
    delete ui;
}

void SendCoinsEntry::on_pasteButton_clicked()
{
    // Paste text from clipboard into recipient field
    ui->payTo->setText(QApplication::clipboard()->text());
}

void SendCoinsEntry::on_addressBookButton_clicked()
{
    if(!model)
        return;
    AddressBookPage dlg(platformStyle, AddressBookPage::ForSelection, AddressBookPage::SendingTab, this);
    dlg.setModel(model->getAddressTableModel());
    if(dlg.exec())
    {
        ui->payTo->setText(dlg.getReturnValue());
        ui->payAmount->setFocus();
    }
}

void SendCoinsEntry::on_payTo_textChanged(const QString &address)
{
    if (!applyingRecipientValue) {
        nicknameMemoRequiredHint = false;
        nicknameMemoTypeHint = "utf8";
    }
    resolvedRecipientAddress.clear();
    resolvedRecipientNickname.clear();
    updateLabel(address);
    updateNicknameMemoUi(address);
}

void SendCoinsEntry::setModel(WalletModel *_model)
{
    this->model = _model;

    if (_model && _model->getOptionsModel())
        connect(_model->getOptionsModel(), SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

    clear();
}

void SendCoinsEntry::clear()
{
    applyingRecipientValue = false;
    nicknameMemoRequiredHint = false;
    nicknameMemoTypeHint = "utf8";
    resolvedRecipientAddress.clear();
    resolvedRecipientNickname.clear();

    // clear UI elements for normal payment
    ui->payTo->clear();
    ui->addAsLabel->clear();
    ui->payAmount->clear();
    ui->checkboxSubtractFeeFromAmount->setCheckState(Qt::Unchecked);
    ui->nicknameMemoEdit->clear();
    ui->nicknameMemoEdit->hide();
    ui->nicknameMemoLabel->hide();
    ui->messageTextLabel->clear();
    ui->messageTextLabel->hide();
    ui->messageLabel->hide();
    // clear UI elements for unauthenticated payment request
    ui->payTo_is->clear();
    ui->memoTextLabel_is->clear();
    ui->payAmount_is->clear();
    // clear UI elements for authenticated payment request
    ui->payTo_s->clear();
    ui->memoTextLabel_s->clear();
    ui->payAmount_s->clear();

    // update the display unit, to not use the default ("BTC")
    updateDisplayUnit();
}

void SendCoinsEntry::deleteClicked()
{
    Q_EMIT removeEntry(this);
}

bool SendCoinsEntry::validate()
{
    if (!model)
        return false;

    // Check input validity
    bool retval = true;

    // Skip checks for payment request
    if (recipient.paymentRequest.IsInitialized())
        return retval;

    const QString enteredDestination = ui->payTo->text().trimmed();
    QString destinationAddress = enteredDestination;
    resolvedRecipientAddress.clear();
    resolvedRecipientNickname.clear();

    if (!model->validateAddress(destinationAddress)) {
        QString normalizedNickname;
        QString reason;
        if (!ResolveNicknameToAddress(enteredDestination, &normalizedNickname, destinationAddress, &reason) ||
            !model->validateAddress(destinationAddress)) {
            ui->payTo->setValid(false);
            retval = false;
        } else {
            resolvedRecipientAddress = destinationAddress;
            resolvedRecipientNickname = QString("@%1").arg(normalizedNickname);
            ui->payTo->setValid(true);
        }
    }

    updateNicknameMemoUi(enteredDestination);
    if (ui->nicknameMemoEdit->isVisible()) {
        const QString trimmedMemo = ui->nicknameMemoEdit->text().trimmed();
        if (nicknameMemoRequiredHint && trimmedMemo.isEmpty()) {
            ui->nicknameMemoEdit->setStyleSheet("border: 1px solid #d9534f;");
            ui->nicknameMemoEdit->setToolTip(tr("This payment request requires a memo"));
            retval = false;
        } else if (!trimmedMemo.isEmpty()) {
            QString memoReason;
            if (!ValidateNicknameMemoInput(trimmedMemo, nicknameMemoTypeHint, &memoReason)) {
                ui->nicknameMemoEdit->setStyleSheet("border: 1px solid #d9534f;");
                ui->nicknameMemoEdit->setToolTip(memoReason);
                retval = false;
            } else {
                ui->nicknameMemoEdit->setStyleSheet(QString());
                ui->nicknameMemoEdit->setToolTip(tr("Optional memo/tag for nickname payments. Some services require it to route funds."));
            }
        } else {
            ui->nicknameMemoEdit->setStyleSheet(QString());
            ui->nicknameMemoEdit->setToolTip(tr("Optional memo/tag for nickname payments. Some services require it to route funds."));
        }
    } else {
        ui->nicknameMemoEdit->setStyleSheet(QString());
        ui->nicknameMemoEdit->setToolTip(tr("Optional memo/tag for nickname payments. Some services require it to route funds."));
    }

    if (ui->payAmount->isAll()) {
        const CAmount balance = model->getBalance();
        if (balance > 0) {
            ui->payAmount->setValue(balance);
            ui->checkboxSubtractFeeFromAmount->setChecked(true);
            ui->payAmount->setValid(true);
        } else {
            ui->payAmount->setValid(false);
            retval = false;
        }
    } else if (!ui->payAmount->validate()) {
        retval = false;
    }

    // Sending a zero amount is invalid
    if (ui->payAmount->value(0) <= 0)
    {
        ui->payAmount->setValid(false);
        retval = false;
    }

    // Reject dust outputs:
    if (retval && GUIUtil::isDust(destinationAddress, ui->payAmount->value())) {
        ui->payAmount->setValid(false);
        retval = false;
    }

    return retval;
}

SendCoinsRecipient SendCoinsEntry::getValue()
{
    // Payment request
    if (recipient.paymentRequest.IsInitialized())
        return recipient;

    // Normal payment
    recipient.address = resolvedRecipientAddress.isEmpty() ? ui->payTo->text() : resolvedRecipientAddress;
    recipient.label = ui->addAsLabel->text();
    recipient.amount = ui->payAmount->value();
    recipient.message = ui->messageTextLabel->text();
    recipient.fSubtractFeeFromAmount = (ui->checkboxSubtractFeeFromAmount->checkState() == Qt::Checked);
    recipient.originalDestination = ui->payTo->text().trimmed();
    recipient.nicknameDestination = resolvedRecipientNickname;
    recipient.isNicknameDestination = !resolvedRecipientNickname.isEmpty();
    recipient.nicknameMemo = ui->nicknameMemoEdit->isVisible() ? ui->nicknameMemoEdit->text().trimmed() : QString();
    recipient.nicknameMemoType = nicknameMemoTypeHint;
    recipient.nicknameMemoRequired = nicknameMemoRequiredHint;

    return recipient;
}

QWidget *SendCoinsEntry::setupTabChain(QWidget *prev)
{
    QWidget::setTabOrder(prev, ui->payTo);
    QWidget::setTabOrder(ui->payTo, ui->addAsLabel);
    QWidget::setTabOrder(ui->addAsLabel, ui->nicknameMemoEdit);
    QWidget *w = ui->payAmount->setupTabChain(ui->nicknameMemoEdit);
    QWidget::setTabOrder(w, ui->checkboxSubtractFeeFromAmount);
    QWidget::setTabOrder(ui->checkboxSubtractFeeFromAmount, ui->addressBookButton);
    QWidget::setTabOrder(ui->addressBookButton, ui->pasteButton);
    QWidget::setTabOrder(ui->pasteButton, ui->deleteButton);
    return ui->deleteButton;
}

void SendCoinsEntry::setValue(const SendCoinsRecipient &value)
{
    recipient = value;
    if (recipient.paymentRequest.IsInitialized()) {
        nicknameMemoRequiredHint = false;
        nicknameMemoTypeHint = "utf8";
    }

    if (recipient.paymentRequest.IsInitialized()) // payment request
    {
        if (recipient.authenticatedMerchant.isEmpty()) // unauthenticated
        {
            ui->payTo_is->setText(recipient.address);
            ui->memoTextLabel_is->setText(recipient.message);
            ui->payAmount_is->setValue(recipient.amount);
            ui->payAmount_is->setReadOnly(true);
            setCurrentWidget(ui->SendCoins_UnauthenticatedPaymentRequest);
        }
        else // authenticated
        {
            ui->payTo_s->setText(recipient.authenticatedMerchant);
            ui->memoTextLabel_s->setText(recipient.message);
            ui->payAmount_s->setValue(recipient.amount);
            ui->payAmount_s->setReadOnly(true);
            setCurrentWidget(ui->SendCoins_AuthenticatedPaymentRequest);
        }
    }
    else // normal payment
    {
        // message
        ui->messageTextLabel->setText(recipient.message);
        ui->messageTextLabel->setVisible(!recipient.message.isEmpty());
        ui->messageLabel->setVisible(!recipient.message.isEmpty());

        ui->addAsLabel->clear();
        nicknameMemoRequiredHint = recipient.nicknameMemoRequired;
        nicknameMemoTypeHint = recipient.nicknameMemoType.trimmed().isEmpty()
            ? QString("utf8")
            : recipient.nicknameMemoType.trimmed().toLower();
        applyingRecipientValue = true;
        ui->payTo->setText(recipient.address); // this may set a label from addressbook
        applyingRecipientValue = false;
        ui->nicknameMemoEdit->setText(recipient.nicknameMemo);
        updateNicknameMemoUi(recipient.address);
        if (!recipient.label.isEmpty()) // if a label had been set from the addressbook, don't overwrite with an empty label
            ui->addAsLabel->setText(recipient.label);
        ui->payAmount->setValue(recipient.amount);
    }
}

void SendCoinsEntry::setAddress(const QString &address)
{
    ui->payTo->setText(address);
    ui->payAmount->setFocus();
}

bool SendCoinsEntry::isClear()
{
    return ui->payTo->text().isEmpty() && ui->payTo_is->text().isEmpty() && ui->payTo_s->text().isEmpty();
}

void SendCoinsEntry::setFocus()
{
    ui->payTo->setFocus();
}

void SendCoinsEntry::updateDisplayUnit()
{
    if(model && model->getOptionsModel())
    {
        // Update payAmount with the current unit
        ui->payAmount->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_is->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
        ui->payAmount_s->setDisplayUnit(model->getOptionsModel()->getDisplayUnit());
    }
}

bool SendCoinsEntry::updateLabel(const QString &address)
{
    if(!model)
        return false;

    // Fill in label from address book, if address has an associated label
    QString associatedLabel = model->getAddressTableModel()->labelForAddress(address);
    if(!associatedLabel.isEmpty())
    {
        ui->addAsLabel->setText(associatedLabel);
        return true;
    }

    return false;
}

void SendCoinsEntry::updateNicknameMemoUi(const QString& destination)
{
    const bool showMemo = nicknameMemoRequiredHint || !resolvedRecipientNickname.isEmpty() || IsNicknameLikeDestination(destination);
    ui->nicknameMemoLabel->setText(tr("Memo:"));
    ui->nicknameMemoEdit->setPlaceholderText(
        nicknameMemoRequiredHint ? tr("Required by recipient/service") : tr("Optional. Required by some services"));
    ui->nicknameMemoLabel->setVisible(showMemo);
    ui->nicknameMemoEdit->setVisible(showMemo);
    if (!showMemo) {
        ui->nicknameMemoEdit->clear();
        ui->nicknameMemoEdit->setStyleSheet(QString());
    }
}
