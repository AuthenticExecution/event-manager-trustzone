#ifndef __TA_H__
#define __TA_H__

#include "tee_client_api.h"

typedef struct {
    TEEC_UUID uuid;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    uint16_t module_id;
} TA_CTX;

int ta_ctx_add(TA_CTX* ta_ctx);
TA_CTX *ta_ctx_get_from_uuid(TEEC_UUID uuid);
TA_CTX *ta_ctx_get_from_module_id(uint16_t id);
TA_CTX initialize_ctx(unsigned char* buf);

#endif