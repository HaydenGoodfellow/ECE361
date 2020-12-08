#ifndef MESSAGE_H
#define MESSAGE_H

#define MAX_NAME 64 // bytes/characters
#define MAX_DATA 2048 // bytes/characters

typedef enum messageTypes {
    MESSAGE = 0,
    MESSAGE_ACK,
    MESSAGE_NACK,
    LOGIN,
    LOGIN_ACK,
    LOGIN_NACK,
    LOGOUT,
    LOGOUT_ACK,
    LOGOUT_NACK,
    EXIT,
    NEW_SESS,
    NEW_SESS_ACK,
    NEW_SESS_NACK,
    JOIN_SESS,
    JOIN_SESS_ACK,
    JOIN_SESS_NACK,
    LEAVE_SESS,
    LEAVE_SESS_ACK,
    LEAVE_SESS_NACK,
    SWITCH_SESS,
    QUERY,
    QUERY_ACK
} messageTypes;

typedef struct message {
    messageTypes type;
    unsigned size;
    char session[MAX_NAME];
    char source[MAX_NAME];
    char data[MAX_DATA];
} message;

#endif // MESSAGE_H