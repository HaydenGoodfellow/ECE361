#define MAX_NAME 64 // bytes/characters
#define MAX_DATA 2048 // bytes/characters

typedef enum messageTypes {
    MESSAGE = 0,
    LOGIN,
    LOGIN_ACK,
    LOGIN_NAK,
    LOGOUT,
    LOGOUT_ACK,
    LOGOUT_NACK,
    EXIT,
    NEW_SESS,
    NEW_SESS_ACK,
    JOIN_SESS,
    JOIN_SESS_ACK,
    JOIN_SESS_NACK,
    LEAVE_SESS,
    QUERY,
    QUERY_ACK
} messageTypes;

typedef struct message {
    messageTypes type;
    unsigned size;
    char source[MAX_NAME];
    char data[MAX_DATA];
} message;
