#include "pushmanager.h"
#include "account/accountmanager.h"

#include "codebase/chain/packedtransaction.h"
#include "codebase/chain/chainmanager.h"
#include "codebase/utility/httpclient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QByteArray>
#include <QEventLoop>
#include <QDebug>

extern QString contract_account;

PushManager::PushManager(bool flag) : result(false)
{
    sendTrx = flag;
    initHttpClients();
}

PushManager::~PushManager()
{
    httpcs.clear();
}

void PushManager::transferToken(const QString &fromAccount, const QString &toAccount, const QString &quantity)
{
    transactionId.clear();

    QJsonObject obj;
    obj.insert("from", QJsonValue(fromAccount));
    obj.insert("to", QJsonValue(toAccount));
    obj.insert("quantity", QJsonValue(quantity));
    obj.insert("memo", QJsonValue(QString::number(rand() % 10000)));
//    obj.insert("memo", QJsonValue(QString::number(666)));

    actor = fromAccount;
    action = "transfer";
    _input = QJsonDocument(obj).toJson();

    QString code = "evsio.token";
    QString action = "transfer";
    QJsonObject obj2;
    auto argObj = QJsonDocument::fromJson(_input).object();
    obj2.insert("args", QJsonValue(argObj));
    obj2.insert("action", QJsonValue(action));
    obj2.insert("code", QJsonValue(code));

    QEventLoop loop;
    connect(this, &PushManager::oneRoundFinished, &loop, &QEventLoop::quit);

    connect(httpcs[FunctionID::abi_json_to_bin].get(), &HttpClient::responseData, this, &PushManager::abi_json_to_bin_returned);
    httpcs[FunctionID::abi_json_to_bin]->request(FunctionID::abi_json_to_bin, QJsonDocument(obj2).toJson());

    loop.exec();
//    return make_push(contract_account, action, QJsonDocument(obj).toJson());
}

QString PushManager::getLastTransactionId()
{
    return transactionId;
}

void PushManager::initHttpClients()
{
    httpcs[FunctionID::get_info]            = std::make_shared<HttpClient>(nullptr);
    httpcs[FunctionID::abi_json_to_bin]     = std::make_shared<HttpClient>(nullptr);
    httpcs[FunctionID::get_required_keys]   = std::make_shared<HttpClient>(nullptr);
    httpcs[FunctionID::push_transaction]    = std::make_shared<HttpClient>(nullptr);
}

bool PushManager::make_push()
{
    result = false;

    QEventLoop loop;
    connect(this, &PushManager::oneRoundFinished, &loop, &QEventLoop::quit);

    connect(httpcs[FunctionID::get_info].get(), &HttpClient::responseData, this, &PushManager::get_info_returned);
    httpcs[FunctionID::get_info]->request(FunctionID::get_info);

    loop.exec();

    return result;
}

QByteArray PushManager::packGetRequiredKeysParam()
{
    auto binargs    = QJsonDocument::fromJson(abiJsonToBinData).object().value("binargs").toString();

    signedTrx = ChainManager::createTransaction(contract_account.toStdString(), action.toStdString(), binargs.toStdString(),
                                                ChainManager::getActivePermission(actor.toStdString()), getInfoData);

    QJsonArray avaibleKeys;
    auto keys = AccountManager::instance().listKeys(actor);
    avaibleKeys.append(QJsonValue(QString::fromStdString(eos_key::get_eos_public_key_by_wif(keys.second))));

    QJsonObject obj;
    obj.insert("available_keys", avaibleKeys);
    obj.insert("transaction", signedTrx.toJson().toObject());
    return QJsonDocument(obj).toJson();
}

QByteArray PushManager::packPushTransactionParam()
{
    auto array = QJsonDocument::fromJson(getRequiredKeysData).object().value("required_keys").toArray();
    if (!array.size()) {
        return QByteArray();
    }

    auto keys = AccountManager::instance().listKeys(actor);
    std::vector<unsigned char> pri;
    auto match = false;
    for (int i = 0; i < array.size(); ++i) {
        auto key = array.at(i).toString().toStdString();
        if (key.compare(eos_key::get_eos_public_key_by_wif(keys.first)) == 0) {
            pri = eos_key::get_private_key_by_wif(keys.first);
            match = true;
            break;
        }

        if (key.compare(eos_key::get_eos_public_key_by_wif(keys.second)) == 0) {
            pri = eos_key::get_private_key_by_wif(keys.second);
            match = true;
            break;
        }
    }

    if (!match) {
        return QByteArray();
    }

    auto info = QJsonDocument::fromJson(getInfoData).object();
    if (info.isEmpty()) {
        return QByteArray();
    }

    signedTrx.sign(pri, TypeChainId::fromHex(info.value("chain_id").toString().toStdString()));
    return QJsonDocument(PackedTransaction(signedTrx, "none").toJson().toObject()).toJson();
}

void PushManager::abi_json_to_bin_returned(const QByteArray &data)
{
    disconnect(httpcs[FunctionID::abi_json_to_bin].get(), &HttpClient::responseData, this, &PushManager::abi_json_to_bin_returned);

    if (data.isEmpty()) {
        emit oneRoundFinished();
        return;
    }

    abis.push_back(data);
    emit oneRoundFinished();

}

void PushManager::get_info_returned(const QByteArray &data)
{
    disconnect(httpcs[FunctionID::get_info].get(), &HttpClient::responseData, this, &PushManager::get_info_returned);

    if (data.isEmpty()) {
        emit oneRoundFinished();
        return;
    }

    getInfoData.clear();
    getInfoData = data;

    abiJsonToBinData.clear();
    abiJsonToBinData = abis[abisIndex];
    if (++abisIndex >= abis.size())
        abisIndex = 0;
    auto param = packGetRequiredKeysParam();
    if (param.isEmpty()) {
        emit oneRoundFinished();
        return;
    }

    connect(httpcs[FunctionID::get_required_keys].get(), &HttpClient::responseData, this, &PushManager::get_required_keys_returned);
    httpcs[FunctionID::get_required_keys]->request(FunctionID::get_required_keys, param);
}

void PushManager::get_required_keys_returned(const QByteArray &data)
{
    disconnect(httpcs[FunctionID::get_required_keys].get(), &HttpClient::responseData, this, &PushManager::get_required_keys_returned);

    if (data.isEmpty()) {
        emit oneRoundFinished();
        return;
    }

    getRequiredKeysData.clear();
    getRequiredKeysData = data;

    auto param = packPushTransactionParam();
//    emit trxPacked(param);

    if (param.isEmpty() || !sendTrx) {
        emit oneRoundFinished();
        return;
    }

    connect(httpcs[FunctionID::push_transaction].get(), &HttpClient::responseData, this, &PushManager::push_transaction_returned);
    httpcs[FunctionID::push_transaction]->request(FunctionID::push_transaction, param);
}

void PushManager::push_transaction_returned(const QByteArray &data)
{
    disconnect(httpcs[FunctionID::push_transaction].get(), &HttpClient::responseData, this, &PushManager::push_transaction_returned);

    auto obj = QJsonDocument::fromJson(data).object();
    if (!obj.isEmpty()) {
        if (!obj.contains("code") && !obj.contains("error")) {
            result = true;

            transactionId = obj.value("transaction_id").toString();
        }
    }

    emit oneRoundFinished();
}
