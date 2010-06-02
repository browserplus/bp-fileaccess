#include "littleuuid.h"

#ifdef WIN32
#include <Rpc.h>
#else 
#include <uuid/uuid.h>
#endif

bool uuid_generate( std::string & sUuid )
{
#ifdef WIN32
    UUID uuid;
    if (UuidCreate( &uuid ) != RPC_S_OK)
        return false;
    unsigned char* pch = NULL;
    if (UuidToStringA( &uuid, &pch ) != RPC_S_OK)
        return false;
    if (pch)
        sUuid = reinterpret_cast<char*>(pch);
    RpcStringFreeA( &pch );
    return true;
#else
    uuid_t uuid;
    uuid_generate( uuid );
    char szUuid[37];
    uuid_unparse( uuid, szUuid );
    sUuid = szUuid;
    return true;
#endif
}
