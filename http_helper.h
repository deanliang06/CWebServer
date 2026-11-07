#ifndef CWEB_HTTP_HELPER_H
#define CWEB_HTTP_HELPER_H

#include "dyna_buffer.h"
#include "types.h"

Response *format_get_text(char *body);
Response *format_get_file(Buffer *body, char *file_name);
Response *format_unsuccessful_get();
Response *method_not_allowed();
Response *internal_server_error();
int check_get(char *method);

#endif
