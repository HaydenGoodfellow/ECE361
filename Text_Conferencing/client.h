int get_cmd(int* sockfd, char* session, bool* in_session);
void login(const char* name, const char* pass, const char* server_ip, const char* server_port, int* sockfd);
void logout(int* sockfd);
void join_session(const char* session, int* sockfd, bool* in_session);
void leave_session(const char* session, int* sockfd, bool* in_session);
void create_session();
void list();
void quit();