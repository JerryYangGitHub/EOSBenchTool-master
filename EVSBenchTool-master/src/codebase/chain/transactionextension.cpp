#include "transactionextension.h"
#include "eosbytewriter.h"

TransactionExtension::TransactionExtension()
{

}

void TransactionExtension::serialize(EOSByteWriter *writer) const
{
    if (writer) {
        // It seems like that VKT deos not support any extensions yet.
        //writer->putShortLE(extensions.first);
        //SerializeCollection(extensions.second, writer);
    }
}
