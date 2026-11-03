  
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#define MAXSIZE 2048        // a definir
#define UID_SIZE 6
#define PASSWORD_SIZE 8
#define PEERPORT 65535
#define FILENAMESIZE 24
#define FSIZE_MIN 0
#define FSIZE_MAX 10000000
#define LABELSIZE 20
#define LABEL_MIN 1

//struct para guardar o estado e os dados do utilizador atual
typedef struct { 
    int logged_in;
    char uid[UID_SIZE + 1];
    char password[PASSWORD_SIZE + 1];
    int peerport;
} User;


// VALIDAÇÕES

// validação do UID
int valid_uid(const char *uid) {
    if (strlen(uid) != UID_SIZE) {              // user ID tem de ter exatamente 6 dígitos
        return 0;
    }
    for (int i = 0; i < UID_SIZE; i++) {       // verfica se todos os caracteres são dígitos                             
        if (!isdigit(uid[i])) {
            return 0;
        }
    }
    return 1;
}

// validação da password
int valid_password (const char *password) {
    if (strlen(password) != PASSWORD_SIZE) {    // password tem de ter exatamente 8 caractéres alfanuméricos
        return 0;
    }
    for (int i = 0; i < PASSWORD_SIZE; i++) {
        char c = password[i];
        if (!((c >= '0' && c <= '9') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= 'a' && c <= 'z'))) {        //verificamos se o caracter nao pertence a nenhum destes grupos: numeros, letras maiusculas e minusculas
            return 0;
        }
    }
    return 1;
}

// validação do peerport
int valid_peerport(int peerport) {
    return (peerport > 0 && peerport <= PEERPORT);
}

// validação do filename
int valid_filename(char *filename) {
    if (strlen(filename) > 24) {                        //comprimento máximo de 24 caracteres
         return 0;
    } 
    int i = 0;
    while (filename[i] != '.' && filename[i] != '\0') { // antes do ponto so pode haver letras/numeros - _
        if (!isalnum(filename[i]) && filename[i] != '_' && filename[i] != '-') { 
            return 0; 
        }
        i++;
    }
    if (filename[i] != '.' || i == 0) {                 // garante que tem extensão(ponto) e existe um caracter antes o ponto
        return 0;
    }
    i++;  // saltar o ponto
    for (int j = 0; j < 3; j++) {
        // se chegarmos ao fim da string antes dos 3 caracteres ou se tiver algum caracter nao alfanumerico, é invalido
        if (filename[i + j] == '\0' || !isalnum(filename[i + j])) {
            return 0;
        }
    }
    if (filename[i + 3] != '\0') {
        return 0;
    }
    return 1;
}

// validação do filename, label e o tamanho do ficheiro
int valid_publish(char *filename, int Fsize, char *label) {
     if (Fsize < FSIZE_MIN || Fsize > FSIZE_MAX)                        // verifica o tamanho do ficheiro
        return 0;
    if (strlen(label) < LABEL_MIN || strlen(label) > LABELSIZE)         // verifica que a label tem entre 1 e 20 caracteres
        return 0;
    for (int i = 0; label[i] != '\0'; i++) {
        if (!isalnum(label[i]) && label[i] != '-' && label[i] != '_')   // verifica que a label só tem letras,numeros, - ou _
            return 0;
    }
    if (!valid_filename(filename)) {
        return 0;
    }
    return 1;
}

// validação do input do login (uid + password)
int valid_input(char *uid, char *password) {
    if(valid_uid(uid) && valid_password(password)) {
        return 1;
    } else { 
        return 0; 
    }
}

// AUXILIARES

// CLEAR_USER: limpa a informação do user
void clear_user(User *user) {                       // ponteiro para a estrutura original
    user->logged_in = 0;
    memset(user->uid, 0, sizeof(user->uid));
    memset(user->password, 0, sizeof(user->password));
}

// OPEN_fILE: vê se é possivel abrir o ficheiro, se sim devolve o tamanho em bytes
int open_file(char *filename) {
    FILE *file = fopen(filename, "rb");             // rb = read binary
    if (file == NULL) {                             // caso nao consiga abrir o ficheiro
        return -1;
    }
    if (fseek(file, 0, SEEK_END) != 0) {            // move o cursor até ao fim do ficheiro
        fclose(file);
        return -1;
    };                          
    long bytes = ftell(file);                       // ftell diz a posicao atual do cursor que é o tamanho do ficheiro
    
    if (bytes < 0) {
        fclose(file);
        return -1;
    }
    fclose(file);
    return bytes;
}

// PRINT_LIST: imprime uma lista dos ficheiros disponíveis (50 máx)
void print_list(char *response) {
    char copy[MAXSIZE];                             // strtok altera a string original
    strcpy(copy, response);                         //copy = "RLS OK foto.jpg trabalho.pdf teste.txt\n" 

    char *token = strtok(copy, " \n");              // RLS
    token = strtok(NULL, " \n");                    // OK e o NULL é para comecar na msm string onde estava
    token = strtok(NULL, " \n");                    // primeiro filename

    int i = 1;                                      // índice na lista
    while(token != NULL) {
        printf("%d %s\n", i, token);
        i++;
        token = strtok(NULL, " \n");                //separar por espaço ou newline,vai buscar o proximo filename
    }
    
}

// PRINT_VERSIONS: imprime que versões há disponíveis do ficheiro segundo as informações do DS
void print_versions(char *response){
    char *copy = malloc(strlen(response) + 1);      // aloca a memoria para as versões para não desperdiçar memória, +1 para o \0
    if (copy == NULL) {
        perror("malloc");
        return;
    }
    strcpy(copy, response);                         // strtok altera a string é preciso trabalhar numa cópia

    char *token = strtok(copy, " \n");              // RVS
    token = strtok(NULL, " \n");                    // OK 

    char *uid;
    char *fsize;
    char *label;
    char *publication_time;
    char *availability;

    int i = 1;
    while((uid = strtok(NULL, " \n")) != NULL) {
        fsize = strtok(NULL, " \n");
        label = strtok(NULL, " \n");
        publication_time = strtok(NULL, " \n");
        availability = strtok(NULL, " \n");

        if (fsize == NULL || label == NULL || publication_time == NULL || availability == NULL) { // se algum campo tiver em falta
            printf("Invalid DS response\n");
            free(copy);
            return;
        }

        printf("Version %d:\n", i);
        printf("UID: %s\n", uid);
        printf("Size: %s bytes\n", fsize);
        printf("Label: %s\n", label);
        printf("Publication time: %.4s-%.2s-%.2s %.2s:%.2s:%.2s\n", publication_time,
        publication_time + 4, publication_time + 6, publication_time + 9,
    publication_time + 11, publication_time + 13);
        if(strcmp(availability, "AVL") == 0) {
            printf("Availability: Available\n");
        } else {
            printf("Availability: Not Available\n");
        }
        printf("\n");
        i++;                                            // passamos para o número da proxima versão
    }
    free(copy);
}

// DS, UDP E TCP

// CONVERT_DS: converte a mensagem de input no formato pretendido pelo DS
void convert_ds(char *command, User *user, char * ds_command, char *filename, int Fsize, char *label) {
    // ds_command é o buffer onde vai ficar a mensagem para o server
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

// COMMUNICATIONS

// DS_COMMUNICATION: comunicação UDP para o DS: envia message e recebe a resposta em response
int ds_communication(int fd, char *message, char *response, struct addrinfo *res) {
    ssize_t n;                                                 // numero de bytes lidos/enviados
    // envia através do socket a mensagem
    n = sendto(fd, message, strlen(message), 0, res->ai_addr, res->ai_addrlen);
    if (n == -1) {
        perror("sendto");
        return 0;
    }
    // recebe a resposta do DS
    n = recvfrom(fd, response, MAXSIZE - 1, 0, NULL, NULL);         // os NULLs sao porque não nos interessa o endereço do remetente
    // se não recebeu e o erro deve se a timeout termina o programa
    if (n == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            printf("Timeout\n");
        } else {
            perror("recvfrom");
        }
        exit(1);
    }
    
    response[n] = '\0';                                       // adiciona o terminador de string no final da resposta
    return 1;

}

// DS_TCP_COMMUNICATION: comunicação TCP para o DS: cria socket, connect, envia message e recebe em response
int ds_tcp_communication( char *message, char *response, struct addrinfo *res_tcp) {
    int fd_tcp, total = 0;
    ssize_t n;
    
    fd_tcp = socket(AF_INET, SOCK_STREAM, 0);           // cria o socket do tipo TCP
    if(fd_tcp == -1) {
        perror("socket");
        return 0;
    }
    if (connect(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen) == -1) { 
        perror("connect");                              // inicia a ligação TCP com o DS
        close(fd_tcp);
        return 0;
    }

    n = write(fd_tcp, message, strlen(message));        // envia mensagem para o DS, escreve bytes no socket
    
    if (n == -1) {
        perror("write");
        close(fd_tcp);
        return 0;
    }
    if (n != strlen(message)) {                         // confirma se os bytes enviados foram o tamanho da message
        perror("write");
        close(fd_tcp);
        return 0;
    }
    
    while (total < MAXSIZE - 1) {                       
        n = read(fd_tcp, response + total, MAXSIZE - 1 - total);
        if (n == -1) {
            perror("read");
            close(fd_tcp);
            return 0;
        }

        // o outro lado fechou a conexão
        if (n == 0) {
            break;
        }
        total += n;
        if (response[total - 1] == '\n') {              // read lê até newline
            break;
        }
    }

    response[total] = '\0';                                 // transforma os bytes recebidos numa string valida 
    close(fd_tcp);
    return 1;
}

// DS_REPLY: recebe a resposta do DS e imprime a mensagem apropriada para o utilizador
void ds_reply(char *response, User *user) {
    char command[MAXSIZE];                              // RLI, RLO, RUR, RPB, RRM, RLS, RVR
    char status [MAXSIZE];                              // OK, REG, NOK, NLG, UNR, WRP

    int n = sscanf(response, "%s %s\n", command, status);

    if (n ==1 && strcmp(command, "ERR") == 0) {
        printf("error\n");  
        return;
    }
    if(n != 2) {                                        // o resto tem que ter exatamente 2 campos
        printf("Invalid DS response\n");
        return;
    }
    
    if (strcmp(command, "RLI") == 0) {                  // LOGIN
        
        if(strcmp(status, "OK") == 0) {                     
            user->logged_in = 1;
            printf("Successful login\n");

        } else if (strcmp(status, "REG") == 0) {
            printf("New user registered\n");
            user->logged_in = 1;
            
        } else if (strcmp(status, "NOK") == 0) {
            printf("Incorrect login attempt\n");
            clear_user(user);
        }
    } else if (strcmp(command, "RLO") == 0) {           // LOGOUT
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
    } else if (strcmp(command, "RUR") == 0) {            // UNREGISTER
        if(strcmp(status, "OK") == 0) {
            clear_user(user);
            printf("Successful unregister\n");

        } else if (strcmp(status, "NOK") == 0) {
            printf("Incorrect unregister attempt\n");
            
        } else if (strcmp(status, "UNR") == 0) {
            printf("Uknown user\n");

        } else if (strcmp(status, "WRP") == 0) {
            printf("Incorrect password\n");         
        } 
    }  else if (strcmp(command, "RPB") == 0) {          // PUBLISH
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
    } else if (strcmp(command, "RRM") == 0) {           // REMOVE
        
        if(strcmp(status, "OK") == 0) {
            printf("Successful removal\n");

        } else if (strcmp(status, "NLG") == 0) {
            printf("User not logged in\n");
            
        } else if (strcmp(status, "UNR") == 0) {
            printf("User not registered\n");        //nao esta no enunciado mas faz sentido lol

        } else if (strcmp(status, "WRP") == 0) {
            printf("Incorrect password\n");

        } else if (strcmp(status, "NOK") == 0) {
            printf("File not published\n");
        }
    } else if (strcmp(command, "RLS") == 0) {           // LIST
        if(strcmp(status, "OK") == 0) {
            print_list(response);
            
        } else if (strcmp(status, "NOK") == 0) {
            printf("No published resources\n");         // nao esta enunciado
        }
    } else if (strcmp(command, "RVR") == 0) {           // VERSIONS
        if(strcmp(status, "OK") == 0) {
            print_versions(response);
        } else if (strcmp(status, "NOK") == 0) {
            printf("No versions available\n");
        }

    } else {
        printf("Unknown command from DS: %s\n", command);
    }
}

// MAIN

int main(int argc, char *argv[]) {
    User user;
    clear_user(&user);
    user.peerport = 0;
    
    char buffer[MAXSIZE];                               // guarda linha escrita no terminal (pelo user)
    char command[MAXSIZE];                              // tipo de pedido

    //auxiliar porque só depois de validar é que copiamos para user
    char uid_aux[MAXSIZE];
    char password_aux[MAXSIZE];
    
    char dsip[MAXSIZE] = "tejo.tecnico.ulisboa.pt";     // argumento default proque os argumentos sao opcionais
    char dsport[MAXSIZE] = "59000";
    char ds_command[MAXSIZE];                           // mensagem enviada para o DS
    char response[MAXSIZE];                             // resposta recebida do DS

    char filename[MAXSIZE];
    char label[MAXSIZE];

    struct addrinfo hints;                              // infromações sobre o tipo de socket que queremos criar
    struct addrinfo *res;                               // o que o getaddrinfo vai devolver, que contem a informação do DS
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
    for (int i = 0; argv[2][i] != '\0'; i++) {          // verifica se peerport é número antes de utilizar atoi
        if(!isdigit(argv[2][i])) {
            printf("Invalid peerport\n");
            exit(1);
        }
    }
    int peerport = atoi(argv[2]);                       //converter string para int
    if (!valid_peerport(peerport)) {
        printf("invalid peerport\n");
        exit(1);
    }


    if(argc >= 5) {                                     // existe pelo menos uma opção -n ou -p
        if ((strcmp(argv[3],"-n")) == 0)  {                         
            strcpy(dsip, argv[4]);
        } else if ((strcmp(argv[3], "-p")) == 0){
           strcpy(dsport, argv[4]); 
        } else {
            printf("Invalid input\n");
            exit(1);
        }
        if (argc == 7) {                                // existe as duas opcoes -n e -p
            if(strcmp(argv[3], "-n") == 0 && strcmp(argv[5], "-p") == 0) {
                strcpy(dsport, argv[6]);
            } else {
            printf("Invalid input\n");
            exit(1);
            }
        }
    }
    
        user.peerport = peerport; 

        fd = socket(AF_INET, SOCK_DGRAM, 0);            // cria socket UDP
        if (fd == -1) exit(1);
        
        // udp
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;                      // endereços IPv4
        hints.ai_socktype = SOCK_DGRAM;                 // UDP

        errcode = getaddrinfo(dsip, dsport, &hints, &res); // endereço e a porta do DS e info
        if (errcode != 0) exit(1);
           
        // tcp
        memset(&hints_tcp, 0, sizeof(hints_tcp));
        hints_tcp.ai_family = AF_INET;                  // endereços IPv4
        hints_tcp.ai_socktype = SOCK_STREAM;            // TCP

        errcode = getaddrinfo(dsip, dsport, &hints_tcp, &res_tcp);      
        if (errcode != 0) exit(1);

        // TIMEOUT 5s( servidor != DS)
        struct timeval timeout;
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        // aplicar esse timeout à receção no socket UDP
        if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == -1) {
            perror("setsockopt");
            close(fd);
            exit(1);
        }   

        // comados
        while(1) {
            if(fgets(buffer, MAXSIZE, stdin) == NULL) { // lê a linha escrita no terminal
                break;
            }
            sscanf(buffer, "%s", command);              // retiramos a primeira palavra (comando ex.login)
            
            // LOGIN
            if (strcmp(command, "login") == 0) {
                int n = sscanf(buffer, "%*s %s %s", uid_aux, password_aux);
                if (n != 2) {
                    printf("Incorrect arguments\n");
                    continue;
                }
                if(user.logged_in) {                    // se já existe alguém logged in, não permitimos outro login
                    printf("Please logout first\n");
                    continue;
                }
        
                if(valid_input(uid_aux, password_aux)) {  // verifica input( uid e password antes de guardar )                 
                    
                    strcpy(user.uid, uid_aux);
                    strcpy(user.password, password_aux);
                    
                    convert_ds(command, &user, ds_command, filename, Fsize, label);     //converte dados do user numa mensagem para DS
                    if (ds_communication(fd, ds_command, response, res)) {              //envia a mensagem para o DS e recebe resposta
                        ds_reply(response, &user);      //interpreta a resposta do DS
                    }
                } else {
                    printf("Incorrect arguments\n");
                }
                
            // LOGOUT
            } else if (strcmp(command, "logout") == 0) {
                if (!user.logged_in) {                  // só faz log out se estiver alguem logged in
                    printf("Please login first\n");
                    continue;
                }

                // conversão, comunicação UDP, resposta
                convert_ds(command, &user, ds_command, filename, Fsize, label);                     
                if (ds_communication(fd, ds_command, response, res)) {
                    ds_reply(response, &user);
                } 
                
            // UNREGISTER
            }  else if (strcmp(command, "unregister") == 0) {
                if (!user.logged_in) {                  // só faz unregister se estiver alguem logged in
                    printf("Please login first\n");
                    continue;
                }
                
                // conversão, comunicação UDP, resposta
                convert_ds(command, &user, ds_command, filename, Fsize, label);                    
                if (ds_communication(fd, ds_command, response, res)) {
                    ds_reply(response, &user);
                }
                
            // EXIT
            } else if (strcmp(command, "exit") == 0) {
                if(user.logged_in) { printf("Please logout first\n");} 
                else { break; }                         // acaba o programa

// PHASE 2
            // PUBLISH 
            } else if(strcmp(command, "publish") == 0) {
                if (!user.logged_in) {
                    printf("please login first\n");
                    continue;
                }

                int n = sscanf(buffer, "%*s %s %s", filename, label);  // ignoramos a primeira palavra e lemos o filename e label
                if (n != 2) {
                    printf("Incorrect arguments\n");
                    continue;
                }
                Fsize = open_file(filename);            // tenta abrir o ficheiro e obter o tamanho
                if (Fsize == -1) {
                    printf ("File does not exist\n");
                    continue;
                }

                // valida as informações do publish (filename, tamanho do ficheiro e label)
                if (!valid_publish(filename, Fsize, label)) {   
                    printf("Invalid file\n");
                    continue;
                }

                // conversão, comunicação UDP, resposta
                convert_ds(command, &user, ds_command, filename, Fsize, label);                    
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
                if (!valid_filename(filename)) {            // valida o filename
                    printf("Incorrect arguments\n");
                    continue;
                }
                // conversão, comunicação UDP, resposta
                convert_ds(command, &user, ds_command, filename, Fsize, label);                     
                if (ds_communication(fd, ds_command, response, res)) {
                    ds_reply(response, &user);
                }
               
            // LIST
            } else if (strcmp(command, "list") == 0) {
                // conversão, comunicação UDP, resposta
                convert_ds(command, &user, ds_command, filename, Fsize, label);                     
                if (ds_communication(fd, ds_command, response, res)) {
                    ds_reply(response, &user);
                }

            // VERSIONS - TCP
            } else if(strcmp(command, "versions") == 0) {

                int n = sscanf(buffer, "%*s %s", filename); 
                if (n != 1) {
                    printf("Incorrect arguments\n");
                    continue;
                }
                if (!valid_filename(filename)) {             // valida filename
                    printf("Invalid file\n");
                    continue;
                }
                // conversão, comunicação TCP, resposta
                convert_ds(command, &user, ds_command, filename, Fsize, label);                    
                if (ds_tcp_communication(ds_command, response, res_tcp)) {
                    ds_reply(response, &user);
                } 
            
            // comando nao reconhecido
            } else {
                printf("Unknown command\n");
            } 
        }
    
    freeaddrinfo(res);
    freeaddrinfo(res_tcp);
    close(fd);
    return 0;
    
}
//./user -m 58001 -n 193.136.138.142 -p 59000
// ./user -m peerport [-n DSIP] [-p DSport]

//login 675846 abcdef12
//login 676767 abcdef12


//melhorias ainda por ver:
//sscanf(buffer, "%*s %24s %20s", filename, label);   para o sscanf nao dar overflow
// ver se é preciso limitar "logout lixo" com "extra"