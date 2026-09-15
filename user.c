  
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
    if (strlen(uid) != UID_SIZE) {                                 // user ID tem de ter exatamente 6 dígitos
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
    if (strlen(password) != PASSWORD_SIZE) {                        // password tem de ter exatamente 8 caractéres alfanuméricos
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

void convert_ds(char *command, User *user, char * ds_command) {
    
    if(strcmp(command, "login") == 0) {
        sprintf(ds_command, "LIN %s %s %d\n", user->uid, user->password, user->peerport);
    } else if (strcmp(command, "logout") == 0) {
        sprintf(ds_command, "LOU %s %s\n", user->uid, user->password);
    } else if (strcmp(command, "unregister") == 0) {
        sprintf(ds_command, "UNR %s %s\n", user->uid, user->password);
    } 
}

int ds_communication(int fd, char *message, char *response, struct addrinfo *res) {
    ssize_t n;
    
    n = sendto(fd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen);
    if (n == -1) {
        perror("sendto");
        return 0;
    }

    n = recvfrom(fd, response, MAXSIZE - 1, 0, NULL, NULL);
    if (n == -1) {
        perror("recvfrom");
        return 0;
    }
    
    response[n] = '\0'; // Null-terminate the received string
    return 1;

}

void ds_reply(char *response, User *user) {
    char command[MAXSIZE];
    char status [MAXSIZE];
    sscanf(response, "%s %s\n", command, status);
    if (strcmp(command, "RLI") == 0) {
        
        if(strcmp(status, "OK") == 0) {  
            user->logged_in = 1;
            printf("Successful login\n");

        } else if (strcmp(status, "REG") == 0) {
            printf("New user registered\n");
            user->logged_in = 1;
            
        } else if (strcmp(status, "NOK") == 0) {
            printf("Incorrect login attempt\n");
        } else if (strcmp(status, "ERR") == 0) {
            printf("Incorrect login attempt\n");
        }
    } else if (strcmp(command, "RLO") == 0) {
        if(strcmp(status, "OK") == 0) {
            clear_user(user);
            printf("Successful logout\n");

        } else if (strcmp(status, "NLG") == 0) {
            printf("User not logged in\n");
            user->logged_in = 0;
            
        } else if (strcmp(status, "UNR") == 0) {
            printf("Uknown user\n");

        } else if (strcmp(status, "WRP") == 0) {
            printf("Incorrect password\n");
        } else if (strcmp(status, "ERR") == 0) {
            printf("Incorrect logout attempt\n");
        }
    } else if (strcmp(command, "RUR") == 0) {
        if(strcmp(status, "OK") == 0) {
            clear_user(user);
            printf("Successful unregister\n");

        } else if (strcmp(status, "NOK") == 0) {
            printf("Incorrect unregister attempt\n");
            
        } else if (strcmp(status, "UNR") == 0) {
            printf("Uknown user\n");

        } else if (strcmp(status, "WRP") == 0) {
            printf("Incorrect unregister attempt\n");         // nao sei se e suposto dizer isto 

        } else if (strcmp(status, "ERR") == 0) {
            printf("Incorrect unregister attempt\n");
        }
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
    
    char dsip[MAXSIZE] = "tejo.tecnico.ulisboa.pt"; //argumento default proque os argumentos sao opcionais
    char dsport[MAXSIZE] = "59000";
    char ds_command[MAXSIZE];
    char response[MAXSIZE];

    struct addrinfo hints;
    struct addrinfo *res;
    int fd, errcode;
    
    if( argc != 3 && argc != 5 && argc != 7) {
        printf("Usage: %s -m peerport [-n DSIP] [-p DSport]\n", argv[0]);
        exit(1);
    }
    if (strcmp(argv[1], "-m") != 0) {
        printf("missing -m option\n");
        exit(1);
    }
    int peerport = atoi(argv[2]);
    if (!valid_peerport(peerport)) {
        printf("invalid peerport\n");
        exit(1);
    }

    user.peerport = peerport;

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
        if (valid_peerport(peerport)) user.peerport = peerport;

        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd == -1) exit(1);
        
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        errcode = getaddrinfo(dsip, dsport, &hints, &res);
        if (errcode != 0) exit(1);

  
        while(1) {
            fgets(buffer, MAXSIZE, stdin);
            sscanf(buffer, "%s", command);
            
            // LOGIN
            if (strcmp(command, "login") == 0) {
                int n = sscanf(buffer, "%*s %s %s", uid_aux, password_aux);
                if (n != 2) {
                    printf("Incorrect arguments\n");
                    continue;
                }
                if(user.logged_in) {
                    printf("Please logout first\n");
                    continue;
                }
        
                if(valid_input(uid_aux, password_aux)) {                   // verifica o  input
                    
                    strcpy(user.uid, uid_aux);
                    strcpy(user.password, password_aux);

                    convert_ds(command, &user, ds_command);                // converte dados do user numa mensagem para DS
                 
                    if (ds_communication(fd, ds_command, response, res)) {     // envia a mensagem para o DS e recebe resposta
                        ds_reply(response, &user);
                        //printf("depois do reply\n");
                    }
                } else {
                    printf("Incorrect arguments\n");
                }
                
            // LOGOUT
            } else if (strcmp(command, "logout") == 0) {
                if (!user.logged_in) {
                    printf("please login first\n");
                    continue;
                }

                convert_ds(command, &user, ds_command);                     // comunicação com o DS 
                if (ds_communication(fd, ds_command, response, res)) {
                    ds_reply(response, &user);
                } 
                

            // UNREGISTER
            }  else if (strcmp(command, "unregister") == 0) {
                if (!user.logged_in) {
                    printf("please login first\n");
                    continue;
                }
                
                convert_ds(command, &user, ds_command);                     // comunicação com o DS
                if (ds_communication(fd, ds_command, response, res)) {
                    ds_reply(response, &user);
                }
                

            // EXIT
            } else if (strcmp(command, "exit") == 0) {
                if(user.logged_in) { printf("please logout first\n");} 
                else { break; }                                           //acaba o programa

            // ERRO COMANDO
            } else {
                printf("unknown command\n");
            }  
        
        }
    }
    freeaddrinfo(res);
    close(fd);
    return 0;
    
}
//login\1234567\0abc123456\n
// ./user -m peerport [-n DSIP] [-p DSport]

//login 675846 abcdef12
//login 676767 abcdef12