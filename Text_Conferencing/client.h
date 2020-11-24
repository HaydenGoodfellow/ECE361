int get_cmd(int* sockfd);
void login(const char* name, const char* pass, const char* server_ip, const char* server_port, int* sockfd);
void logout(int* sockfd);
void join_session();
void leave_session();
void create_session();
void list();
void quit();