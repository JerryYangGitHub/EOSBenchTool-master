#include "packer.h"
#include "push/pushmanager.h"
#include "datamanager.h"

extern QString super_account;
extern QString token_name;
extern int trx_size;

Packer::Packer()
{
    needStop = false;
}

void Packer::setAccountName(const QString &name)
{
    accountName = name;
}

void Packer::setCallback(const packedtrx_callback& func)
{
    this->func = func;
}

void Packer::run()
{
    PushManager push(true);
    for (int i = 0; i < trx_size; ++i) {
        push.transferToken(super_account, accountName, QString("0.0001 %1").arg(token_name));
    }
    while(!needStop) {
      //   connect(&push, &PushManager::trxPacked, this, [&](const QByteArray& data){
            // emit newPackedTrx(data);
            // func(QString("PACKED: %1 to %2.").arg(accountName).arg(super_account));
      //   });
//        push.transferToken(super_account, accountName, QString("0.0001 %1").arg(token_name));
        push.make_push();
    }
}

void Packer::stop()
{
    needStop = true;
}
