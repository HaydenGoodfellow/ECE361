#include "server.h"

// Struct for storing username data in
typedef struct User {
    char* username;
    char* password;
} User;

// hardcode all the users
User UserList[] = {
    {"Hayden", "pass1"}, 
    {"Grace", "pass2"}, 
    {"Alice", "pass3"}, 
    {"Bob", "pass4"}, 
    {"Joanne", "pass5"}, 
    {"May", "pass6"},
    {"Jim", "longpasswordwithnumbers12345"}
};

// Ensure the user trying to log in is valid
bool checkValidLogin(char* clientID, char* password, char *error) {
    if (clientExists(clientID) != NULL) {
        strcpy(error, "Error: That user is already logged in!");
        return false;
    }
    int numUsers = sizeof(UserList) / sizeof(User);
    for (int i = 0; i < numUsers; ++i) {
        bool nameMatches = (strcmp(clientID, UserList[i].username) == 0);
        bool passwordMatches = (strcmp(password, UserList[i].password) == 0);
        if (nameMatches && passwordMatches) {
            return true;
        }
        else if (nameMatches && !passwordMatches) {
            strcpy(error, "Error: Incorrect Password!");
            return false;
        }
    }
    strcpy(error, "Error: User with that name not found!");
    return false;  
}