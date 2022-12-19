#ifndef __TA_H__
#define __TA_H__

#include "networking.h"

// platform-specific library and definitions
#include "tee_client_api.h"
#define SECURITY_BYTES 16
#define LOAD_HEADER_LEN 18

// data structure containing all the required fields
typedef struct {
    TEEC_UUID uuid;
    TEEC_Context ctx;
    TEEC_Session sess;
    TEEC_Operation op;
    uint16_t module_id;
} ModuleContext;

/* Functions for managing internal linked list */

// Generic functions
int add_module(ModuleContext *ta_ctx);
ModuleContext *get_module_from_id(uint16_t id);

// Platform-specific functions
//TODO check if this can be removed
ModuleContext *get_module_from_uuid(TEEC_UUID uuid);


/* API for interfacing with modules */

// Generic functions
int initialize_context(ModuleContext *ctx, unsigned char* buf, size_t size);
ResultMessage load_module(unsigned char* buf, size_t size);
void handle_input(uint16_t sm, uint16_t conn_id, void* data, size_t len);

#endif