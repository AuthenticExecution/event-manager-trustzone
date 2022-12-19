#ifndef __TA_H__
#define __TA_H__

#include "tee_client_api.h"

// data structure containing all the required fields
typedef struct {
    TEEC_UUID uuid;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    uint16_t module_id;
} ModuleContext;

// functions to manage this data structure (internal linked list used)
int add_module(ModuleContext *ta_ctx);
ModuleContext *get_module_from_uuid(TEEC_UUID uuid);
ModuleContext *get_module_from_id(uint16_t id);
ModuleContext initialize_context(unsigned char* buf);

// functions for interfacing with TAs


#endif