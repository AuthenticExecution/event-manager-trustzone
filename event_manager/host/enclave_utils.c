#include "enclave_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <arpa/inet.h>

#include "logging.h"
#include "command_handlers.h"
#include "connection.h"

#define SECURITY_BYTES 16

/* store the conn_idx ids associated to a connection ID. conn_idx is an index
 * used internally by each SM, which is returned by set_key.It has to be passed
 * as input to the handle_input entry point of the SM
 *
 * Note: adjust MAX_CONNECTIONS accordingly. This value should ALWAYS be >= than
 *       the total number of connections in the Authentic Execution deployment
 *
 * TODO: this array should be dynamic in a real scenario.
 */
#define MAX_CONNECTIONS 128
uint16_t connection_idxs[MAX_CONNECTIONS] = { -1 };

ResultMessage load_enclave(CommandMessage m) {
  //TODO
  // command is destroyed by the caller
  return RESULT(ResultCode_InternalError);
}


static int is_local_connection(Connection* connection) {
  return (int) connection->local;
}

static void handle_local_connection(Connection* connection,
                      void* data, size_t len) {
    reactive_handle_input(connection->to_sm, connection->conn_id, data, len);
    free(data);
}

static void handle_remote_connection(Connection* connection,
                                     void* data, size_t len) {
    unsigned char *payload = malloc(len + 4);
    if(payload == NULL) {
      WARNING("handle_remote_connection: OOM");
      return;
    }

    uint16_t sm_id = htons(connection->to_sm);
    uint16_t conn_id = htons(connection->conn_id);

    memcpy(payload, &sm_id, 2);
    memcpy(payload + 2, &conn_id, 2);
    memcpy(payload + 4, data, len);

    CommandMessage m = create_command_message(
            CommandCode_RemoteOutput,
            create_message(len + 4, payload)
    );

    //TODO send this to address
    //write_command_message(m, (unsigned char *) connection->to_address.u8, connection->to_port);

    destroy_command_message(m);
    free(data);
}


void reactive_handle_output(uint16_t conn_id, void* data, size_t len) {
  #if USE_MINTIMER && TIMING_TESTS
    start_time = mintimer_now_usec();
  #endif

  Connection* connection = connections_get(conn_id);

  if (connection == NULL) {
      DEBUG("no connection for id %u", conn_id);
      return;
  }

  DEBUG("accepted output %u to be delivered at %s:%d %d",
      connection->conn_id,
      inet_ntoa(connection->to_address),
      connection->to_port,
      connection->to_sm);

  if (is_local_connection(connection))
      handle_local_connection(connection, data, len);
  else
      handle_remote_connection(connection, data, len);
}


void reactive_handle_input(uint16_t sm, uint16_t conn_id, void* data, size_t len) {
    DEBUG("Calling handle_input of sm %d", sm);
    //TODO
}


ResultMessage handle_set_key(uint16_t id, ParseState *state) {
  uint8_t* ad;
  const size_t AD_LEN = sizeof(uint16_t) + sizeof(uint16_t) + 2;
  if (!parse_raw_data(state, AD_LEN, &ad))
      return RESULT(ResultCode_IllegalPayload);

  // check if we can store the conn_idx of this connection ID on connection_idxs
  uint16_t conn_id = (ad[0] << 8) | ad[1];
  if(conn_id >= MAX_CONNECTIONS) {
    ERROR("connection_idxs too small");
    return RESULT(ResultCode_InternalError);
  }

  uint8_t* cipher;
  if (!parse_raw_data(state, SECURITY_BYTES, &cipher))
      return RESULT(ResultCode_IllegalPayload);

  uint8_t* tag;
  if (!parse_raw_data(state, SECURITY_BYTES, &tag))
      return RESULT(ResultCode_IllegalPayload);

  //TODO call

  return RESULT(ResultCode_Ok);
}


ResultMessage handle_attest(uint16_t id, ParseState *state) {
  uint16_t challenge_len;
  if (!parse_int(state, &challenge_len))
      return RESULT(ResultCode_IllegalPayload);

  uint8_t* challenge;
  if (!parse_raw_data(state, challenge_len, &challenge))
      return RESULT(ResultCode_IllegalPayload);

  // The result format is [tag] where the tag is the challenge response
  const size_t RESULT_PAYLOAD_SIZE = SECURITY_BYTES;
  void* result_payload = malloc(RESULT_PAYLOAD_SIZE);

  if (result_payload == NULL)
      return RESULT(ResultCode_InternalError);

  //TODO call

  return RESULT_DATA(ResultCode_Ok, RESULT_PAYLOAD_SIZE, result_payload);
}

ResultMessage handle_disable(uint16_t id, ParseState *state) {
  uint8_t* ad;
  const size_t AD_LEN = 2;
  if (!parse_raw_data(state, AD_LEN, &ad))
      return RESULT(ResultCode_IllegalPayload);

  uint8_t* cipher;
  if (!parse_raw_data(state, 2, &cipher))
      return RESULT(ResultCode_IllegalPayload);

  uint8_t* tag;
  if (!parse_raw_data(state, SECURITY_BYTES, &tag))
      return RESULT(ResultCode_IllegalPayload);

  //TODO call

  return RESULT(ResultCode_Ok);
}

ResultMessage handle_user_entrypoint(uint16_t id, uint16_t index, ParseState *state) {
  uint8_t* payload;
  size_t payload_len;
  if(!parse_all_raw_data(state, &payload, &payload_len)) {
    return RESULT(ResultCode_IllegalPayload);
  }

  //TODO call

  return RESULT(ResultCode_Ok);
}
