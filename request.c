

int end_of_request(char *buf, int buf_size){
    for (int i = 0; i < buf_size - 3; i++) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') return 1;
    }
    return 0;
}