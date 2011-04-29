#include "littleuuid.h"

#include <Rpc.h>

bool
uuid_generate(std::string& sUuid) {
    UUID uuid;
    unsigned char* pch = NULL;
    if (UuidCreate(&uuid) != RPC_S_OK) {
        return false;
    }
    if (UuidToStringA(&uuid, &pch) != RPC_S_OK) {
        return false;
    }
    if (pch) {
        sUuid = reinterpret_cast<char*>(pch);
    }
    RpcStringFreeA(&pch);
    return true;
}
