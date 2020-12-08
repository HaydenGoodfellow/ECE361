#ifndef LOGIN_H
#define LOGIN_H

typedef struct users {
    char* userName;
    char* userPassword;
} users;

// hardcode all the users
users AllUsers[] = {{"grace", "pass1"}, {"ben", "pass2"}, {"hayden", "pass3"}, {"bob", "pass4"},
{"joanne", "pass5"}, {"may", "pass6"}};

#endif /* LOGIN_H */

