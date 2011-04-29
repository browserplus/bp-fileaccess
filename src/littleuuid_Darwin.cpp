#include "littleuuid.h"

#include <uuid/uuid.h>

bool
uuid_generate(std::string& sUuid) {
    uuid_t uuid;
    char szUuid[37];
    uuid_generate(uuid);
    uuid_unparse(uuid, szUuid);
    sUuid = szUuid;
    return true;
}
