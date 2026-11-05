int int_from_string(char *string);
int string_from_int(char *buf, int num);
int read_file(char *file);
int file_exists_and_content(char *file_path);
char *request_file_to_path(char *requested_file);
int matching_front(char *wildcard, char *requested_resource);