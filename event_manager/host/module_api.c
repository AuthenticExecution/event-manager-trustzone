#include "module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "command_handlers.h"
#include "enclave_utils.h"

int initialize_context(ModuleContext *ctx, unsigned char* buf, size_t size) {
  TEEC_UUID uuid;

  if(size < LOAD_HEADER_LEN) {
    return 0;
  }
  
  int j = 0;
  uint16_t id = 0;
  for(int m = 1; m >= 0; --m){
    id = id + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }
  ctx->module_id = id;

  j = 0;
  int timelow = 0;
  for(int m = 5; m >= 2; --m){
    timelow = timelow + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }	
  uuid.timeLow = timelow;

  j = 0;
  int mid = 0;
  for(int m = 7; m >= 6; --m){
    mid = mid + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }	
  uuid.timeMid = mid;

  j = 0;
  int high = 0;
  for(int m = 9; m >= 8; --m){
    high = high + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }	
  uuid.timeHiAndVersion = high;

  for(int m = 10; m < 18; m++){
    uuid.clockSeqAndNode[m-10] = buf[m];
  }

  ctx->uuid =  uuid;
  return 1;
}

ResultMessage load_module(unsigned char* buf, size_t size) {

    ModuleContext ctx;
    TEEC_Result rc;
    uint32_t err_origin;

    if(!initialize_context(&ctx, buf, size)) {
        ERROR("initialize_context: buffer too small %lu/18", size);
        return RESULT(ResultCode_IllegalPayload);
    }

    char fname[100] = { 0 };
    FILE *file = NULL;
    char path[] = "/lib/optee_armtz";

    snprintf(
        fname,
        PATH_MAX,
        "%s/%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x.ta",
        path,
        ctx.uuid.timeLow,
        ctx.uuid.timeMid,
        ctx.uuid.timeHiAndVersion,
        ctx.uuid.clockSeqAndNode[0],
        ctx.uuid.clockSeqAndNode[1],
        ctx.uuid.clockSeqAndNode[2],
        ctx.uuid.clockSeqAndNode[3],
        ctx.uuid.clockSeqAndNode[4],
        ctx.uuid.clockSeqAndNode[5],
        ctx.uuid.clockSeqAndNode[6],
        ctx.uuid.clockSeqAndNode[7]
    );

    file = fopen(fname, "w"); 

    if(file == NULL) {
        ERROR("Cannot open file: %s", fname);
        return RESULT(ResultCode_InternalError);
    }

    fwrite(buf + LOAD_HEADER_LEN, 1, size - LOAD_HEADER_LEN, file);
    fclose(file); 

    /* Initialize a context connecting us to the TEE */
    rc = TEEC_InitializeContext(NULL, &ctx.ctx);
    if(rc != TEEC_SUCCESS) {
        ERROR("TEEC_InitializeContext failed: %d", rc);
        return RESULT(ResultCode_InternalError);
    }

    // open a session to the TA
    rc = TEEC_OpenSession(
        &ctx.ctx,
        &ctx.sess,
        &ctx.uuid,
        TEEC_LOGIN_PUBLIC,
        NULL,
        NULL,
        &err_origin
    );

    if(rc != TEEC_SUCCESS) {
        ERROR("TEEC_OpenSession failed: %d", rc);
        return RESULT(ResultCode_InternalError);
    }

    if(!add_module(&ctx)) {
        //TODO close session with TA?!
        ERROR("Could not add module to internal list");
        return RESULT(ResultCode_InternalError);
    }

    return RESULT(ResultCode_Ok);
}

void handle_input(uint16_t sm, uint16_t conn_id, void* data, size_t len) {
    // check if buffer has enough size
    if(len < SECURITY_BYTES) {
        ERROR("Payload too small: %lu", len);
        return;
    }

    size_t cipher_size = len - SECURITY_BYTES;
    ModuleContext *ctx = get_module_from_id(sm);
    

    // Sepideh's stuff, don't want to touch it so much. But I tried to improve
    //      and sanitize it
    //TODO this is very bad. This assumes only 256 bytes of payload for future
    //      payloads, even if the input calls multiple outputs. Fix it.

    unsigned char *arg1_buf = malloc(32);
    unsigned char *arg2_buf = malloc(256);
    unsigned char *arg3_buf = malloc(256);

    if(arg1_buf == NULL || arg2_buf == NULL || arg3_buf == NULL) {
        ERROR("Failed to allocate buffers");
        if(arg1_buf != NULL) free(arg1_buf);
        if(arg2_buf != NULL) free(arg2_buf);
        if(arg3_buf != NULL) free(arg3_buf);
        return;
    }

    // initialize buffers
    memcpy(arg2_buf, data, cipher_size);
    memcpy(arg3_buf, data + cipher_size, SECURITY_BYTES);
    memset(&ctx->op, 0, sizeof(ctx->op));

    // prepare parameters
    ctx->op.params[0].value.a = cipher_size;
    ctx->op.params[0].value.b = conn_id;
    ctx->op.params[1].tmpref.buffer = (void *) arg1_buf;
    ctx->op.params[1].tmpref.size = 32;
    ctx->op.params[2].tmpref.buffer = (void *) arg2_buf;
    ctx->op.params[2].tmpref.size = 256;
    ctx->op.params[3].tmpref.buffer = (void *) arg3_buf;
    ctx->op.params[3].tmpref.size = 256;
    ctx->op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_VALUE_INOUT,
        TEEC_MEMREF_TEMP_OUTPUT,
        TEEC_MEMREF_TEMP_INOUT,
        TEEC_MEMREF_TEMP_INOUT
    );

    TEEC_Session temp_sess;
    TEEC_Context temp_ctx;
    temp_ctx.fd = ctx->ctx.fd;
    temp_ctx.reg_mem = ctx->ctx.reg_mem;
    temp_ctx.memref_null = ctx->ctx.memref_null;

    temp_sess.session_id = ctx->sess.session_id;
    temp_sess.ctx = &temp_ctx;

    uint32_t err_origin;
    TEEC_Result rc = TEEC_InvokeCommand(
        &temp_sess,
        Entrypoint_HandleInput,
        &ctx->op,
        &err_origin
    );

    if (rc == TEEC_SUCCESS) {
        // check if there are any outputs to send out, calling handle_output
        // for each of them

        int index = 0;
        for(int i = 0; i < ctx->op.params[0].value.b; i++) {
            uint16_t conn_id = 0;
            int j = 0;
            // TODO only one byte for the ciphertext length?!
            int cipher_len = arg2_buf[index] & 0xFF;

            unsigned char *output_payload = malloc(cipher_len + SECURITY_BYTES);
            if(output_payload == NULL) {
                WARNING("Failed to allocate payload for handle_output");
                return;
            }

            // prepare parameters
            for(int m = (2 * i) + 1; m >= (2*i); --m) {
                // TODO what is this!? why is this an addition?
                conn_id = conn_id + (( arg1_buf[m] & 0xFF ) << (8*j));
                ++j;
            }
            memcpy(output_payload, arg2_buf + index + 1, cipher_len);
            memcpy(output_payload + cipher_len, arg3_buf + (SECURITY_BYTES * i), SECURITY_BYTES);

            // call handle_output
            reactive_handle_output(conn_id, output_payload, cipher_len + SECURITY_BYTES);

            index =  index + cipher_len + 1;
            free(output_payload);
        }
    }

    // free buffers
    free(arg1_buf);
    free(arg2_buf);
    free(arg3_buf);
}