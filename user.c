  
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#define MAXSIZE 2048 
#define UID_SIZE 6
#define PASSWORD_SIZE 8
#define PEERPORT 65535
#define FILENAMESIZE 24
#define FSIZE_MIN 0
#define FSIZE_MAX 10000000
#define LABELSIZE 20
#define LABEL_MIN 1

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

int valid_filename(char *filename) {
    if (strlen(filename) > 24) {
         return 0;
    } 
    int i = 0;
    while (filename[i] != '.' && filename[i] != '\0') {
        if (!isalnum(filename[i]) && filename[i] != '_' && filename[i] != '-') {
            return 0; 
        }
        i++;
    }
    if (filename[i] != '.' || i == 0) { // garante que tem ponto e existe um caracter antes o ponto
        return 0;
    }
    i++;  // passar o .
    for (int j = 0; j < 3; j++) {
        if (filename[i + j] == '\0' || !isalnum(filename[i + j])) {
            return 0;
        }
    }
    if (filename[i + 3] != '\0') {
        return 0;
    }
    return 1;
}

int valid_publish(char *filename, int Fsize, char *label) {
     if (Fsize < FSIZE_MIN || Fsize > FSIZE_MAX)
        return 0;
    if (strlen(label) < LABEL_MIN || strlen(label) > LABELSIZE)
        return 0;
    for (int i = 0; label[i] != '\0'; i++) {
        if (!isalnum(label[i]) && label[i] != '-' && label[i] != '_')
            return 0;
    }
    if (!valid_filename(filename)) {
        return 0;
    }
    return 1;
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

int open_file(char *filename) {
    FILE *file = fopen(filename, "rb"); //rb = read binary
    if (file == NULL) {
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    }; //move o cursor ate ao fim do ficheiro
    long bytes = ftell(file); //ftell diz a posicao atual do cursor que é o taamnho do ficheiro
    
    if (bytes<0) {
        fclose(file);
        return -1;
    }
    fclose(file);
    return bytes;
}


void convert_ds(char *command, User *user, char * ds_command, char *filename, int Fsize, char *label) {
    
    if(strcmp(command, "login") == 0) {
        sprintf(ds_command, "LIN %s %s %d\n", user->uid, user->password, user->peerport);
    } else if (strcmp(command, "logout") == 0) {
        sprintf(ds_command, "LOU %s %s\n", user->uid, user->password);
    } else if (strcmp(command, "unregister") == 0) {
        sprintf(ds_command, "UNR %s %s\n", user->uid, user->password);
        //phase 2
    } else if (strcmp(command, "publish") == 0) {
        sprintf(ds_command, "PUB %s %s %s %d %s\n", user->uid, user->password, filename, Fsize, label); 
    } else if (strcmp(command, "remove") == 0) {
        sprintf(ds_command, "REM %s %s %s\n", user->uid, user->password, filename);
    } else if (strcmp(command, "list") == 0) {
        sprintf(ds_command, "LST\n");
    } else if (strcmp(command, "versions") == 0) {
        sprintf(ds_command, "VRS %s\n", filename);
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
void print_list(char *response) {
    char copy[MAXSIZE];
    strcpy(copy, response); //copy = "RLS OK foto.jpg trabalho.pdf teste.txt\n"

    char *token = strtok(copy, " \n");  // RLS
    token = strtok(NULL, " \n");        // OK e o NULL é para comecar na msm string onde estava
    token = strtok(NULL, " \n");        // primeiro filename

    int i = 1;
    while(token != NULL) {
        printf("%d %s\n", i, token);
        i++;
        token = strtok(NULL, " \n"); //separar por newline ou espaço
    }
    
}

void print_versions(char *response){
    
}

int ds_tcp_communication( char *message, char *response, struct addrinfo *res_tcp) {
    int fd_tcp;
    ssize_t n;
    
    fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if(fd_tcp == -1) {
        perror("socket");
        return 0;
    }
    if (connect(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen) == -1) {
        perror("connect");
        close(fd_tcp);
        return 0;
    }

    n = write(fd_tcp, message, strlen(message)); 
    if (n == -1) {
        perror("write");
        close(fd_tcp);
        return 0;
    }
    
    n = read(fd_tcp, response, MAXSIZE - 1);
        if (n == -1) {
        perror("read");
        close(fd_tcp);
        return 0;
    }
    response[n] = '\0';
    close(fd_tcp);
    return 1;
}



void ds_reply(char *response, User *user) {
    char command[MAXSIZE];
    char status [MAXSIZE];
    int n = sscanf(response, "%s %s\n", command, status);

    if (n ==1 && strcmp(command, "ERR") == 0) {
        printf("error\n");  //nao sei qual o output, nao encontro no enunciado
        return;
    }
    if(n != 2) {
        printf("Invalid DS response\n");
        return;
    }
    
    if (strcmp(command, "RLI") == 0) {                      // LOG
        
        if(strcmp(status, "OK") == 0) {  
            user->logged_in = 1;
            printf("Successful login\n");

        } else if (strcmp(status, "REG") == 0) {
            printf("New user registered\n");
            user->logged_in = 1;
            
        } else if (strcmp(status, "NOK") == 0 || strcmp(status, "ERR") == 0) {
            printf("Incorrect login attempt\n");
            clear_user(user);
        }
    } else if (strcmp(command, "RLO") == 0) {               // LOGOUT
        if(strcmp(status, "OK") == 0) {
            clear_user(user);
            printf("Successful logout\n");

        } else if (strcmp(status, "NLG") == 0) {
            printf("User not logged in\n");
            
        } else if (strcmp(status, "UNR") == 0) {
            printf("Uknown user\n");

        } else if (strcmp(status, "WRP") == 0) {
            printf("Incorrect password\n");
        }
    } else if (strcmp(command, "RUR") == 0) {               // UNREGISTER
        if(strcmp(status, "OK") == 0) {
            clear_user(user);
            printf("Successful unregister\n");

        } else if (strcmp(status, "NOK") == 0) {
            printf("Incorrect unregister attempt\n");
            
        } else if (strcmp(status, "UNR") == 0) {
            printf("Uknown user\n");

        } else if (strcmp(status, "WRP") == 0) {
            printf("Incorrect password\n");         // nao sei se e suposto dizer isto 
        } 
    }  else if (strcmp(command, "RPB") == 0) {
        if(strcmp(status, "OK") == 0) {
            printf("Successful publication\n");

        } else if (strcmp(status, "NLG") == 0) {
            printf("User not logged in\n");
            
        } else if (strcmp(status, "UNR") == 0) {
            printf("Uknown user\n");

        } else if (strcmp(status, "WRP") == 0) {
            printf("Incorrect password\n");

        } else if (strcmp(status, "NOK") == 0) {
            printf("Incorrect publish attempt\n");
        }
    } else if (strcmp(command, "RRM") == 0) {
        
        if(strcmp(status, "OK") == 0) {
            printf("Successful removal\n");

        } else if (strcmp(status, "NLG") == 0) {
            printf("User not logged in\n");
            
        } else if (strcmp(status, "UNR") == 0) {
            printf("User not registered\n"); //nao esta no enunciado mas faz sentido lol

        } else if (strcmp(status, "WRP") == 0) {
            printf("Incorrect password\n");

        } else if (strcmp(status, "NOK") == 0) {
            printf("File not published\n");
        }
    } else if (strcmp(command, "RLS") == 0) {
        if(strcmp(status, "OK") == 0) {
            print_list(response);
            
        } else if (strcmp(status, "NOK") == 0) {
            printf("No published resources\n"); //nao esta enunciado
        }
    } else if (strcmp(command, "RVR") == 0) {
        if(strcmp(status, "OK") == 0) {
            print_versions(response);
        } else if (strcmp(status, "NOK") == 0) {
            printf("No versions available\n");
        }
    } else {
        printf("Unknown command from DS: %s\n", command);
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

    char filename[MAXSIZE];
    char label[MAXSIZE];

    struct addrinfo hints;
    struct addrinfo *res;
    struct addrinfo hints_tcp;
    struct addrinfo *res_tcp;
    int fd, errcode, Fsize = 0;
    
    if(argc != 3 && argc != 5 && argc != 7) {
        printf("Usage: %s -m peerport [-n DSIP] [-p DSport]\n", argv[0]);
        exit(1);
    }
    if (strcmp(argv[1], "-m") != 0) {
        printf("missing -m option\n");
        exit(1);
    }
    for (int i = 0; argv[2][i] != '\0'; i++) {      // verifica se peerport é número antes de utilizar atoi
        if(!isdigit(argv[2][i])) {
            printf("Invalid peerport\n");
            exit(1);
        }
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
        user.peerport = peerport;

        fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (fd == -1) exit(1);
        
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;

        errcode = getaddrinfo(dsip, dsport, &hints, &res);
        if (errcode != 0) exit(1);
               
        memset(&hints_tcp, 0, sizeof(hints_tcp));
        hints_tcp.ai_family = AF_INET;
        hints_tcp.ai_socktype = SOCK_STREAM;

        errcode = getaddrinfo(dsip, dsport, &hints_tcp, &res_tcp);
        if (errcode != 0) exit(1);

  
        while(1) {
            if(fgets(buffer, MAXSIZE, stdin) == NULL) {
                break;
            }
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
                    convert_ds(command, &user, ds_command, filename, Fsize, label);     // converte dados do user numa mensagem para DS
                    if (ds_communication(fd, ds_command, response, res)) {              // envia a mensagem para o DS e recebe resposta
                        ds_reply(response, &user);
                    }
                } else {
                    printf("Incorrect arguments\n");
                }
                
            // LOGOUT
            } else if (strcmp(command, "logout") == 0) {
                if (!user.logged_in) {
                    printf("Please login first\n");
                    continue;
                }

                convert_ds(command, &user, ds_command, filename, Fsize, label);                     // comunicação com o DS 
                if (ds_communication(fd, ds_command, response, res)) {
                    ds_reply(response, &user);
                } 
                

            // UNREGISTER
            }  else if (strcmp(command, "unregister") == 0) {
                if (!user.logged_in) {
                    printf("Please login first\n");
                    continue;
                }
                
                convert_ds(command, &user, ds_command, filename, Fsize, label);                     // comunicação com o DS
                if (ds_communication(fd, ds_command, response, res)) {
                    ds_reply(response, &user);
                }
                

            // EXIT
            } else if (strcmp(command, "exit") == 0) {
                if(user.logged_in) { printf("Please logout first\n");} 
                else { break; }                                           //acaba o programa

            // PUBLISH 
            } else if(strcmp(command, "publish") == 0) {
                if (!user.logged_in) {
                    printf("please login first\n");
                    continue;
                }

                int n = sscanf(buffer, "%*s %s %s", filename, label); 
                if (n != 2) {
                    printf("Incorrect arguments\n");
                    continue;
                }
                Fsize = open_file(filename);
                if (Fsize == -1) {
                    printf ("File does not exist\n");
                    continue;
                }

                if (!valid_publish(filename, Fsize, label)) {
                    printf("Invalid file\n");
                    continue;
                }
                convert_ds(command, &user, ds_command, filename, Fsize, label);                     // comunicação com o DS 
                if (ds_communication(fd, ds_command, response, res)) {
                    ds_reply(response, &user);
                } 

            // REMOVE 
            } else if (strcmp(command, "remove") == 0) {
                if (!user.logged_in) {
                    printf("Please login first\n");
                    continue;
                }
                int n = sscanf(buffer, "%*s %s", filename);
                if (n != 1) {
                    printf("Incorrect arguments\n");
                    continue;
}
                if (!valid_filename(filename)) {
                    printf("Incorrect arguments\n");
                    continue;
                }
                convert_ds(command, &user, ds_command, filename, Fsize, label);                     // comunicação com o DS 
                if (ds_communication(fd, ds_command, response, res)) {
                    ds_reply(response, &user);
                }
                
            } else if (strcmp(command, "list") == 0) {
                convert_ds(command, &user, ds_command, filename, Fsize, label);                     // comunicação com o DS 
                if (ds_communication(fd, ds_command, response, res)) {
                    ds_reply(response, &user);
                }
            } else if(strcmp(command, "versions") == 0) {

                int n = sscanf(buffer, "%*s %s", filename); 
                if (n != 1) {
                    printf("Incorrect arguments\n");
                    continue;
                }
                if (!valid_filename(filename)) {
                    printf("Invalid file\n");
                    continue;
                }
                convert_ds(command, &user, ds_command, filename, Fsize, label);                     // comunicação com o DS 
                if (ds_tcp_communication(ds_command, response, res_tcp)) {
                    ds_reply(response, &user);
                } 
            
            
            } else {
                printf("Unknown command\n");
            } 
        }
    }
    freeaddrinfo(res);
    freeaddrinfo(res_tcp);
    close(fd);
    return 0;
    
}
//login\1234567\0abc123456\n
// ./user -m peerport [-n DSIP] [-p DSport]

//login 675846 abcdef12
//login 676767 abcdef12


//melhorias ainda por ver:
//sscanf(buffer, "%*s %24s %20s", filename, label);   para o sscanf nao dar overflow