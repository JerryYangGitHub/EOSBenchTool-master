#ifndef PUSHMANAGER_H
#define PUSHMANAGER_H

#include <QObject>
#include <QString>
#include <QMap>
#include <vector>
#include <memory>

#include "codebase/chain/signedtransaction.h"
#include "codebase/utility/httpclient.h"

class PushManager : public QObject
{
    Q_OBJECT

public:
    PushManager(bool flag = true);
    ~PushManager();

    void transferToken(const QString& fromAccount, const QString& toAccount, const QString& quantity);
    QString getLastTransactionId();
    bool make_push();

private:
    void initHttpClients();

    QByteArray packGetRequiredKeysParam();
    QByteArray packPushTransactionParam();

private slots:
    void abi_json_to_bin_returned(const QByteArray& data);
    void get_info_returned(const QByteArray& data);
    void get_required_keys_returned(const QByteArray& data);
    void push_transaction_returned(const QByteArray& data);

signals:
    void oneRoundFinished();
    void trxPacked(const QByteArray& data);

private:
    QMap<FunctionID, std::shared_ptr<HttpClient>> httpcs;

    bool result;
    bool sendTrx;

    std::vector<QByteArray> abis;
    uint32_t abisIndex = 0;
    QByteArray abiJsonToBinData;
    QByteArray getInfoData;
    QByteArray getRequiredKeysData;

    SignedTransaction signedTrx;

    QString actor;
    QString action;
    QString transactionId;

    QByteArray _input;
};

#endif // PUSHMANAGER_H
