Response *format_get_text(char *body);
Response *format_get_file(char *body, char *file_name);
Response *format_unsuccessful_get();
Response *method_not_allowed();
Response *internal_server_error();
int check_get(char *method);
