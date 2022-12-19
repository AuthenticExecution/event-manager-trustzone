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

typedef enum {
    ModuleCallResult_Ok,
    ModuleCallResult_GenericError
} ModuleCallResult;

/* Functions for managing internal linked list */

// Generic functions
int initialize_context(ModuleContext *ctx, unsigned char* buf, size_t size);
int add_module(ModuleContext *ta_ctx);
ModuleContext *get_module_from_id(uint16_t id);

// Platform-specific functions
//TODO check if this can be removed
ModuleContext *get_module_from_uuid(TEEC_UUID uuid);


/* API for interfacing with modules */

// Generic functions
ModuleCallResult load_module(unsigned char* buf, size_t size);

#endif