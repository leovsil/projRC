  
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#define MAXSIZE 256
#define UID_SIZE 6
#define PASSWORD_SIZE 8
#define PEERPORT 65535
#define PEERPORTSIZE 5

typedef struct {
    int logged_in;
    char uid[UID_SIZE + 1];
    char password[PASSWORD_SIZE + 1];
    int peerport;
} User;

int valid_uid(const char *uid) {
    if (strlen(uid) != UID_SIZE) {
        return 0;
    }
    for (int i = 0; i < UID_SIZE; i++) {
        if (!isdigit(uid[i])) {
            return 0;
        }
    }
    return 1;
}

int valid_password (const char *password) {
    if (strlen(password) != PASSWORD_SIZE) {
        return 0;
    }
    for (int i = 0; i < PASSWORD_SIZE; i++) {
        char c = password[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z'))) {
            return 0;
        }
    }
    return 1;
}

int valid_peerport(int peerport) {
    return (peerport > 0 && peerport <= PEERPORT);
}

int valid_input(char *uid, char *password) {
    
    if(valid_uid(uid) && valid_password(password)) {
        return 1;
    } else { 
        return 0; 
    }
}

void clear_user(User *user) {
    user->logged_in = 0;
    memset(user->uid, 0, sizeof(user->uid));
    memset(user->password, 0, sizeof(user->password));
}

void convert_ds(char *command, User *user) {
    char ds_command[MAXSIZE];
    if(strcmp(command, "login") == 0) {
        sprintf(ds_command, "LIN %s %s %d\n", user->uid, user->password, user->peerport);
        printf("%s\n", ds_command);
    } else if (strcmp(command, "logout") == 0) {
        sprintf(ds_command, "LOU %s %s %d\n", user->uid, user->password, user->peerport);
        printf("%s\n", ds_command);
    } else if (strcmp(command, "unregister") == 0) {
        sprintf(ds_command, "UNR %s %s %d\n", user->uid, user->password, user->peerport);
        printf("%s\n", ds_command);
    } 

}


int main(int argc, char *argv[]) {
    User user;
    clear_user(&user);
    user.peerport = 0;
    
    char buffer[MAXSIZE];
    char command[MAXSIZE];
    char uid_aux[MAXSIZE];
    char password_aux[MAXSIZE];
    char dsip[MAXSIZE];
    char dsport[MAXSIZE];
    int peerport = atoi(argv[2]);

    if(argc >= 5) {   
        if ((strcmp(argv[3],"-n")) == 0)  {                         
            strcpy(dsip, argv[4]);
        } else {
           strcpy(dsport, argv[4]); 
        }
        if (argc == 7) {
            strcpy(dsport, argv[6]);
        }
    }
    if (argc == 3 || argc == 7 || argc == 5) {
        printf("IP: %s\n PORT: %s\n", dsip, dsport);
        if (valid_peerport(peerport)) user.peerport = peerport;
        
        while(1) {
            fgets(buffer, MAXSIZE, stdin);
            sscanf(buffer, "%s", command);
            
            // LOGIN
            if (strcmp(command, "login") == 0) {
                sscanf(buffer, "%*s %s %s", uid_aux, password_aux);
                if(valid_input(uid_aux, password_aux)) {            // verifica o  input
                    user.logged_in = 1;
                    strcpy(user.uid, uid_aux);
                    strcpy(user.password, password_aux);
                    convert_ds(command, &user);
                    printf("successful login\n");
                } else {
                    printf("Incorrect arguments\n");
                }
                
            // LOGOUT
            } else if (strcmp(command, "logout") == 0) {
                convert_ds(command, &user);
                clear_user(&user);
                user.logged_in = 0;
                printf("successful logout\n");

            // UNREGISTER
            }  else if (strcmp(command, "unregister") == 0) {
                convert_ds(command, &user);
                clear_user(&user);
                user.logged_in = 0;
                printf("successful unregister\n"); 

            // EXIT
            } else if (strcmp(command, "exit") == 0) {
                if(user.logged_in) {
                    printf("please logout first\n");
                } else {
                    break;                                          //acaba o programa
                }

            // ERRO COMANDO
            } else {
                printf("unknown command\n");
            }
        }
    }
    return 0;
    
}
//login\1234567\0abc123456\n
// ./user -m peerport [-n DSIP] [-p DSport]
