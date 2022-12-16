#include "ta.h"

#include <stdlib.h>
#include <string.h>

typedef struct CTX_Node {
    TA_CTX ta_ctx;
    struct CTX_Node* next;
} CTX_Node;

static CTX_Node* ta_ctx_head = NULL;

int ta_ctx_add(TA_CTX* ta_ctx) {
    CTX_Node* node = malloc(sizeof(CTX_Node));

    if (node == NULL)
        return 0;

    node->ta_ctx = *ta_ctx;
    node->next = ta_ctx_head;
    ta_ctx_head = node;
    return 1;
}

TA_CTX *ta_ctx_get_from_uuid(TEEC_UUID uuid) {
    CTX_Node* current = ta_ctx_head;

    while (current != NULL) {
        TA_CTX* ctx = &current->ta_ctx;

        if ((ctx->uuid.timeLow == uuid.timeLow) &&
            (ctx->uuid.timeMid == uuid.timeMid) &&
            (ctx->uuid.timeHiAndVersion == uuid.timeHiAndVersion) &&
            !memcmp(ctx->uuid.clockSeqAndNode, uuid.clockSeqAndNode, 8)) {

            return ctx;
        }

        current = current->next;
    }

    return NULL;
}

TA_CTX *ta_ctx_get_from_module_id(uint16_t id) {
    CTX_Node* current = ta_ctx_head;

    while (current != NULL) {
        TA_CTX* ctx = &current->ta_ctx;

        if (ctx->module_id == id) {
            return ctx;
        }

        current = current->next;
    }

    return NULL;
}

TA_CTX initialize_ctx(unsigned char* buf) {
  TA_CTX ctx;
  TEEC_UUID uuid;

  int j = 0;
  uint16_t id = 0;
  for(int m = 1; m >= 0; --m){
    id = id + (( buf[m] & 0xFF ) << (8*j));
    ++j;
  }
  ctx.module_id = id;

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

  ctx.uuid =  uuid;
  return ctx;
}