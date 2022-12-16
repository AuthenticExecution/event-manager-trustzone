#include "event_manager.h"

#include <stdio.h>

#include "logging.h"
#include "command_handlers.h"

ResultMessage process_message(CommandMessage m) {
  switch (m->code) {
    case CommandCode_AddConnection:
      return handler_add_connection(m);

    case CommandCode_CallEntrypoint:
      return handler_call_entrypoint(m);

    case CommandCode_RemoteOutput:
      return handler_remote_output(m);

    case CommandCode_LoadSM:
      return handler_load_sm(m);

    case CommandCode_Ping:
      return handler_ping(m);

    case CommandCode_RegisterEntrypoint:
      return handler_register_entrypoint(m);

    default: // CommandCode_Invalid
      WARNING("wrong cmd id");
      return NULL;
  }
}
