#include "module.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "logging.h"
#include "enclave_utils.h"

// please change this.
#define OUT_CONN_IDS_SIZE 32
#define OUT_PAYLOADS_SIZE 256
#define OUT_TAGS_SIZE 256

TEEC_Result call_entry(ModuleContext *ctx, Entrypoint entry_id) {
    TEEC_Session temp_sess;
    TEEC_Context temp_ctx;
    temp_ctx.fd = ctx->ctx.fd;
    temp_ctx.reg_mem = ctx->ctx.reg_mem;
    temp_ctx.memref_null = ctx->ctx.memref_null;

    temp_sess.session_id = ctx->sess.session_id;
    temp_sess.ctx = &temp_ctx;

    uint32_t err_origin;
    return TEEC_InvokeCommand(
        &temp_sess,
        entry_id,
        &ctx->op,
        &err_origin
    );
}

void send_outputs(
    unsigned int num_outputs,
    unsigned char *conn_ids,
    unsigned char *payloads,
    unsigned char *tags
) {
    // check if there are any outputs to send out, calling handle_output
    // for each of them

    int index = 0;
    for(int i = 0; i < num_outputs; i++) {
        uint16_t conn_id = 0;
        int j = 0;
        // TODO only one byte for the ciphertext length?!
        int cipher_len = payloads[index] & 0xFF;

        unsigned char *output_payload = malloc(cipher_len + SECURITY_BYTES);
        if(output_payload == NULL) {
            WARNING("Failed to allocate payload for handle_output");
            return;
        }

        // prepare parameters
        for(int m = (2 * i) + 1; m >= (2*i); --m) {
            // TODO what is this!? why is this an addition?
            conn_id = conn_id + (( conn_ids[m] & 0xFF ) << (8*j));
            ++j;
        }
        memcpy(output_payload, payloads + index + 1, cipher_len);
        memcpy(output_payload + cipher_len, tags + (SECURITY_BYTES * i), SECURITY_BYTES);

        // call handle_output
        reactive_handle_output(conn_id, output_payload, cipher_len + SECURITY_BYTES);

        index =  index + cipher_len + 1;
        free(output_payload);
    }
}

int initialize_context(ModuleContext *ctx, unsigned char* buf, unsigned int size) {
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

ResultMessage load_module(unsigned char* buf, unsigned int size) {

    ModuleContext ctx;
    TEEC_Result rc;
    uint32_t err_origin;

    if(!initialize_context(&ctx, buf, size)) {
        ERROR("initialize_context: buffer too small %u/18", size);
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

void handle_input(uint16_t sm, uint16_t conn_id, unsigned char* data, unsigned int len) {
    // check if buffer has enough size
    if(len < SECURITY_BYTES) {
        ERROR("Payload too small: %u", len);
        return;
    }

    unsigned int cipher_size = len - SECURITY_BYTES;
    ModuleContext *ctx = get_module_from_id(sm);

    if(ctx == NULL) {
        ERROR("Module %d not found", sm);
        return;
    }

    //TODO this is very bad. This assumes only 256 bytes of payload for future
    //      payloads, even if the input calls multiple outputs. Fix it.
    unsigned char *arg1_buf = malloc(OUT_CONN_IDS_SIZE);
    unsigned char *arg2_buf = malloc(OUT_PAYLOADS_SIZE);
    unsigned char *arg3_buf = malloc(OUT_TAGS_SIZE);

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
    ctx->op.params[1].tmpref.size = OUT_CONN_IDS_SIZE;
    ctx->op.params[2].tmpref.buffer = (void *) arg2_buf;
    ctx->op.params[2].tmpref.size = OUT_PAYLOADS_SIZE;
    ctx->op.params[3].tmpref.buffer = (void *) arg3_buf;
    ctx->op.params[3].tmpref.size = OUT_TAGS_SIZE;
    ctx->op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_VALUE_INOUT,
        TEEC_MEMREF_TEMP_OUTPUT,
        TEEC_MEMREF_TEMP_INOUT,
        TEEC_MEMREF_TEMP_INOUT
    );

    // call entry point
    TEEC_Result rc = call_entry(ctx, Entrypoint_HandleInput);

    if (rc == TEEC_SUCCESS) {
        send_outputs(ctx->op.params[0].value.b, arg1_buf, arg2_buf, arg3_buf);
    } else {
        ERROR("TEEC_InvokeCommand failed: %d", rc);
    }

    // free buffers
    free(arg1_buf);
    free(arg2_buf);
    free(arg3_buf);
}

ResultMessage set_key(
    uint16_t sm,
    unsigned char* ad,
    unsigned int ad_len,
    unsigned char* cipher,
    unsigned char* tag
) {
    ModuleContext *ctx = get_module_from_id(sm);

    if(ctx == NULL) {
        ERROR("Module %d not found", sm);
        return RESULT(ResultCode_BadRequest);
    }

    unsigned char *arg0_buf = malloc(ad_len);
    unsigned char *arg1_buf = malloc(SECURITY_BYTES);
    unsigned char *arg2_buf = malloc(SECURITY_BYTES);

    if(arg0_buf == NULL || arg1_buf == NULL || arg2_buf == NULL) {
        ERROR("Failed to allocate buffers");
        if(arg0_buf != NULL) free(arg0_buf);
        if(arg1_buf != NULL) free(arg1_buf);
        if(arg2_buf != NULL) free(arg2_buf);
        return RESULT(ResultCode_InternalError);
    }

    // initialize buffers
    memcpy(arg0_buf, ad, ad_len);
    memcpy(arg1_buf, cipher, SECURITY_BYTES);
    memcpy(arg2_buf, tag, SECURITY_BYTES);
    memset(&ctx->op, 0, sizeof(ctx->op));

    // prepare parameters
    ctx->op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,
        TEEC_MEMREF_TEMP_INPUT,
        TEEC_MEMREF_TEMP_INPUT,
        TEEC_NONE
    );
    ctx->op.params[0].tmpref.buffer = arg0_buf;
    ctx->op.params[0].tmpref.size = ad_len;
    ctx->op.params[1].tmpref.buffer = arg1_buf;
    ctx->op.params[1].tmpref.size = SECURITY_BYTES;
    ctx->op.params[2].tmpref.buffer = arg2_buf;
    ctx->op.params[2].tmpref.size = SECURITY_BYTES;

    // call entry point
    TEEC_Result rc = call_entry(ctx, Entrypoint_SetKey);

    // free buffers
    free(arg0_buf);
    free(arg1_buf);
    free(arg2_buf);

    if(rc != TEEC_SUCCESS) {
        ERROR("TEEC_InvokeCommand failed: %d", rc);
        return RESULT(ResultCode_InternalError);
    }

    return RESULT(ResultCode_Ok);
}

ResultMessage attest(
    uint16_t sm,
    unsigned char* challenge,
    unsigned int challenge_len
) {
    ModuleContext *ctx = get_module_from_id(sm);

    if(ctx == NULL) {
        ERROR("Module %d not found", sm);
        return RESULT(ResultCode_BadRequest);
    }

    unsigned char *arg0_buf = malloc(challenge_len);
    unsigned char *arg1_buf = malloc(SECURITY_BYTES);

    if(arg0_buf == NULL || arg1_buf == NULL) {
        ERROR("Failed to allocate buffers");
        if(arg0_buf != NULL) free(arg0_buf);
        if(arg1_buf != NULL) free(arg1_buf);
        return RESULT(ResultCode_InternalError);
    }

    // initialize buffers
    memcpy(arg0_buf, challenge, challenge_len);
    memset(&ctx->op, 0, sizeof(ctx->op));

    // prepare parameters
    ctx->op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,
        TEEC_MEMREF_TEMP_OUTPUT,
        TEEC_NONE,
        TEEC_NONE
    );
    ctx->op.params[0].tmpref.buffer = arg0_buf;
    ctx->op.params[0].tmpref.size = challenge_len;
    ctx->op.params[1].tmpref.buffer = arg1_buf;
    ctx->op.params[1].tmpref.size = SECURITY_BYTES;

    // call entry point
    TEEC_Result rc = call_entry(ctx, Entrypoint_Attest);

    // free buffers
    free(arg0_buf);

    // fetch result
    if(rc == TEEC_SUCCESS) {
        return RESULT_DATA(ResultCode_Ok, SECURITY_BYTES, arg1_buf);
    } else {
        ERROR("TEEC_InvokeCommand failed: %d", rc);
        free(arg1_buf);
        return RESULT(ResultCode_InternalError);
    }
}

ResultMessage disable(
    uint16_t sm,
    unsigned char* ad,
    unsigned int ad_len,
    unsigned char* cipher,
    unsigned int cipher_len,
    unsigned char* tag
) {
    ModuleContext *ctx = get_module_from_id(sm);

    if(ctx == NULL) {
        ERROR("Module %d not found", sm);
        return RESULT(ResultCode_BadRequest);
    }

    unsigned char *arg0_buf = malloc(ad_len);
    unsigned char *arg1_buf = malloc(cipher_len);
    unsigned char *arg2_buf = malloc(SECURITY_BYTES);

    if(arg0_buf == NULL || arg1_buf == NULL || arg2_buf == NULL) {
        ERROR("Failed to allocate buffers");
        if(arg0_buf != NULL) free(arg0_buf);
        if(arg1_buf != NULL) free(arg1_buf);
        if(arg2_buf != NULL) free(arg2_buf);
        return RESULT(ResultCode_InternalError);
    }

    // initialize buffers
    memcpy(arg0_buf, ad, ad_len);
    memcpy(arg1_buf, cipher, cipher_len);
    memcpy(arg2_buf, tag, SECURITY_BYTES);
    memset(&ctx->op, 0, sizeof(ctx->op));

    // prepare parameters
	ctx->op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_MEMREF_TEMP_INPUT,
		TEEC_MEMREF_TEMP_INPUT,
		TEEC_MEMREF_TEMP_INPUT,
        TEEC_NONE
    );
	ctx->op.params[0].tmpref.buffer = arg0_buf;
	ctx->op.params[0].tmpref.size = ad_len;
	ctx->op.params[1].tmpref.buffer = arg1_buf;
	ctx->op.params[1].tmpref.size = cipher_len;
	ctx->op.params[2].tmpref.buffer = arg2_buf;
	ctx->op.params[2].tmpref.size = SECURITY_BYTES;

    // call entry point
    TEEC_Result rc = call_entry(ctx, Entrypoint_Disable);

    // free buffers
    free(arg0_buf);
    free(arg1_buf);
    free(arg2_buf);

    if(rc != TEEC_SUCCESS) {
        ERROR("TEEC_InvokeCommand failed: %d", rc);
        return RESULT(ResultCode_InternalError);
    }

    return RESULT(ResultCode_Ok);
}

ResultMessage call(
    uint16_t sm,
    uint16_t entry_id,
    unsigned char* payload,
    unsigned int len
) {
    ModuleContext *ctx = get_module_from_id(sm);

    if(ctx == NULL) {
        ERROR("Module %d not found", sm);
        return RESULT(ResultCode_BadRequest);
    }

    unsigned char *arg1_buf = malloc(OUT_CONN_IDS_SIZE);
    unsigned char *arg2_buf = malloc(OUT_PAYLOADS_SIZE);
    unsigned char *arg3_buf = malloc(OUT_TAGS_SIZE);

    if(arg1_buf == NULL || arg2_buf == NULL || arg3_buf == NULL) {
        ERROR("Failed to allocate buffers");
        if(arg1_buf != NULL) free(arg1_buf);
        if(arg2_buf != NULL) free(arg2_buf);
        if(arg3_buf != NULL) free(arg3_buf);
        return RESULT(ResultCode_InternalError);
    }

    // initialize buffers
    memcpy(arg2_buf, payload, len);
    memset(&ctx->op, 0, sizeof(ctx->op));

    // prepare parameters
    ctx->op.params[0].value.b = entry_id;
    ctx->op.params[0].value.a = len;
    ctx->op.params[1].tmpref.buffer = (void *) arg1_buf;
    ctx->op.params[1].tmpref.size = OUT_CONN_IDS_SIZE;
    ctx->op.params[2].tmpref.buffer = (void *) arg2_buf;
    ctx->op.params[2].tmpref.size = OUT_PAYLOADS_SIZE;
    ctx->op.params[3].tmpref.buffer = (void *) arg3_buf;
    ctx->op.params[3].tmpref.size = OUT_TAGS_SIZE;
    ctx->op.paramTypes = TEEC_PARAM_TYPES(
        TEEC_VALUE_INOUT,
        TEEC_MEMREF_TEMP_OUTPUT,
        TEEC_MEMREF_TEMP_INOUT,
        TEEC_MEMREF_TEMP_OUTPUT
    );

    // call entry point
    TEEC_Result rc = call_entry(ctx, Entrypoint_User);

    ResultMessage res;
    if (rc == TEEC_SUCCESS) {
        send_outputs(ctx->op.params[0].value.b, arg1_buf, arg2_buf, arg3_buf);
        res = RESULT(ResultCode_Ok); //TODO where is the response!?!?!
    } else {
        ERROR("TEEC_InvokeCommand failed: %d", rc);
        res = RESULT(ResultCode_InternalError);
    }

    // free buffers
    free(arg1_buf);
    free(arg2_buf);
    free(arg3_buf);

    return res;
}