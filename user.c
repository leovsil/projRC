  
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
#include <signal.h>
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
    if (strlen(uid) != UID_SIZE) { //user ID tem de ter exatamente 6 dígitos
        return 0;
    }
    for (int i = 0; i < UID_SIZE; i++) {                            // verfica se todos os caracteres são dígitos                             
        if (!isdigit(uid[i])) {
            return 0;
        }
    }
    return 1;
}

// validação da password
int valid_password (const char *password) {
    if (strlen(password) != PASSWORD_SIZE) { // password tem de ter exatamente 8 caractéres alfanuméricos
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
    while (filename[i] != '.' && filename[i] != '\0') {
        if (!isalnum(filename[i]) && filename[i] != '_' && filename[i] != '-') { // antes do ponto so pode haver letras/numeros - _
            return 0; 
        }
        i++;
    }
    if (filename[i] != '.' || i == 0) { // garante que tem extensão(ponto) e existe um caracter antes o ponto
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
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return -1;
    };                          // move o cursor ate ao fim do ficheiro
    long bytes = ftell(file);   // ftell diz a posicao atual do cursor que é o taamnho do ficheiro
    
    if (bytes<0) {
        fclose(file);
        return -1;
    }
    fclose(file);
    return bytes;
}

// TIMEOUT
void set_timeout(int fd, int secs) {
    struct timeval tv = { secs, 0 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

// PRINT_LIST: imprime uma lista dos ficheiros disponíveis (50 máx)
void print_list(char *response) {
    char copy[MAXSIZE];
    strcpy(copy, response); //copy = "RLS OK foto.jpg trabalho.pdf teste.txt\n" porque o strtok altera a string original

    char *token = strtok(copy, " \n");  //RLS
    token = strtok(NULL, " \n");        //OK e o NULL é para comecar na msm string onde estava
    token = strtok(NULL, " \n");        //primeiro filename

    int i = 1;
    while(token != NULL) {
        printf("%d %s\n", i, token);
        i++;
        token = strtok(NULL, " \n"); //separar ppor espaço ou newline, vai buscar o proximo filename
    }
    
}

// PRINT_VERSIONS: imprime que versões há disponíveis do ficheiro segundo as informações do DS
void print_versions(char *response){
    char *copy = malloc(strlen(response) + 1);      // aloca a memoria para as versões para não desperdiçar memória, +1 para o \0
    if (copy == NULL) {
        perror("malloc");
        return;
    }
    strcpy(copy, response);

    char *token = strtok(copy, " \n");  // RVS
    token = strtok(NULL, " \n");        // OK 

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
        i++; // passamos para o numero da proxima versão
    }
    free(copy);
}

// AUXILIARES ENTRE PEERS

// SELECT_PEER: escolhe o peer com a primeira versão disponível do ficheiro
int select_peer(char *response, char *peer_uid, User *user) {
    char *copy = malloc(strlen(response) + 1);
    if (copy == NULL) {
        perror("malloc");
        return 0;
    }

    strcpy(copy, response);
    char *token = strtok(copy, " \n");
    if (token == NULL || strcmp(token, "RVR") != 0) {           // RVR
        free(copy);
        return 0;
    }

    token = strtok(NULL, " \n");                                // OK
    if (token == NULL || strcmp(token, "OK") != 0) {
        free(copy);
        return 0;
    }

    char *uid;
    char *fsize;
    char *label;
    char *publication_time;
    char *availability;

    while ((uid = strtok(NULL, " \n")) != NULL) {               // percorrer todas as versões disponiveis do ficheiro

        fsize = strtok(NULL, " \n");
        label = strtok(NULL, " \n");
        publication_time = strtok(NULL, " \n");
        availability = strtok(NULL, " \n");

        if (fsize == NULL || label == NULL || publication_time == NULL || availability == NULL) {  // verificar se todos os campos da resposta estão completos
            free(copy);
            return 0;
        }

        if (strcmp(availability, "AVL") == 0 && valid_uid(uid) 
            && strcmp(uid, user->uid) != 0) {   // escolher a primeira versao com o user disponivel
            strcpy(peer_uid, uid);
            free(copy);
            return 1;
        }
    }

    free(copy);
    return 0;
}

// RECEIVE_FILE: recebe o conteúdo do ficheiro do peer e cria o localmente
int receive_file(int fd_get, char* filename) {
    char response[MAXSIZE];                 // resposta do peer (RGT NOK/RGT OK bytes conteudo\n)
    char command[MAXSIZE];                  // RGT
    char status [MAXSIZE];                  // OK, NOK
    int total = 0, spaces = 0;              // total: bytes da response, Fsize tamanho ficheiro da response
    int Fsize;
    ssize_t bytes;

    // leitura da resposta do peer do socket tcp garantindo que recebemos tudo
    while (total < sizeof(response) - 1) {
        bytes = read(fd_get, response + total, 1);
        if (bytes <= 0) {
            printf("Response error\n");
            return 0;
        }

       char c = response[total];
       total++;                                 // total de caracteres recebidos até agora
       if(c == ' ') spaces++;
       if(spaces == 3 || c == '\n') break;      // spaces = 3 o próximo byte é conteúdo e '\n' para casos de NOK (sem filename)
    }
    // para de ler do socket quando acaba a resposta ou o conteúdo começa no byte seguinte
    response[total] = '\0';                     // fica com o lido do socket
    int n = sscanf(response, "%3s %3s %d", command, status, &Fsize);

    if (n < 2 || strcmp(command, "RGT") != 0) {
        printf("Invalid peer response\n");  
        return 0;
    }
    if(strcmp(status, "NOK") == 0) {
        printf("File unavailable\n");  
        return 0;
    }
    if (strcmp(status, "OK") != 0 || n != 3 ||
        spaces != 3 || Fsize < FSIZE_MIN || Fsize > FSIZE_MAX) {
        printf("Invalid peer response\n");
        return 0;
    }
    
    // abrimos o ficheiro localmente (se não existir é criado, se existir o conteúdo anterior é apagado)
    FILE *file = fopen(filename, "wb");            

    if (file == NULL) {
        perror("fopen");
        return 0;
    }

    char content[MAXSIZE];                      // buffer que armazena o conteúdo do ficheiro recebido
    total = 0;                                  // nº bytes do ficheiro guardados

    // lê do socket o resto da mensagem que contém o conteúdo do ficheiro
    while (total < Fsize) {    
        int to_read = Fsize - total;

        if (to_read > MAXSIZE) {
            to_read = MAXSIZE;
        }

        n = read(fd_get, content, to_read);   

        // erro de leitura/ não escrevemos todos os bytes recebidos
        if (n <= 0 || fwrite(content, 1, n, file) != n) {   // fwrite escreve n elementos de 1 byte vindos de content no ficheiro
            printf("Error receiving or writing file\n");
            fclose(file);
            remove(filename);
            return 0;
        }

        total += n;
    }

    // lê o último byte que deve ser um '\n' (o total lido deve ser igual ao nºbytes dado na resposta)
    char end_char;
    n = read(fd_get, &end_char, 1);

    if (n != 1 || end_char != '\n') {            // confirma que é 1 byte de \n e nao é escrito
        printf("Invalid end of response\n");
        fclose(file);
        remove(filename);
        return 0;
    }

    // fecha o ficheiro
    if (fclose(file) != 0) {
        printf("Error saving file\n");
        remove(filename);
        return 0;
    }

    // resultado no terminal
    printf("File saved in current directory: %s (%d bytes)\n",
           filename, total);
    return 1;

}

// SEND_RESPONSE: auxiliar a peer_request que envia o conteúdo em 'message' resposta a um pedido GET através do socket TCP
int send_response(int fd_client, char *message, int size) {
    int total = 0;
    ssize_t n;

    while (total < size) {
        n = send(fd_client, message + total, size - total, MSG_NOSIGNAL);                  // envia os bytes através do socket TCP para o peer
        
        if (n <= 0) {
            perror("write");
            return 0;
        }
        total += n;
    }
    return 1;
}

// PEER_REQUEST: responde ao peer com a resposta ao seu pedido de ficheiro 'GET filename\n' 
void peer_request(int fd_client){
    char request[MAXSIZE];                 // guarda o pedido do peer 'GET filename\n'
    char command[MAXSIZE];
    char filename[MAXSIZE];
    int total = 0;
    ssize_t bytes;

    // lê o pedido enviado no socket entre peers
    while (total < MAXSIZE - 1) {
        bytes = read(fd_client, request + total, 1);

        if (bytes <= 0) {
            printf("Error receiving peer request\n");
            return;
        }

        total++;
        if (request[total - 1] == '\n') {
            break;
        }
    }

    request[total] = '\0';

    // Interpretar o pedido 'GET filename' 
    int n = sscanf(request, "%s %s", command, filename);

    if (request[total - 1] != '\n' || n != 2 ||
        strcmp(command, "GET") != 0 || !valid_filename(filename)) {

        char message[] = "RGT ERR\n";
        send_response(fd_client, message, strlen(message));     // envia ERR no caso de qualquer tipo de erro no pedido
        return;
    }

    // obter o tamanho do ficheiro pedido
    int Fsize = open_file(filename);

    // caso o ficheiro seja maior do que o permitido enviar 
    if (Fsize < FSIZE_MIN || Fsize > FSIZE_MAX) {
        char message[] = "RGT NOK\n";
        send_response(fd_client, message, strlen(message));     // envia a responsta do NOK (sem conteúdo)
        return;
    }

    FILE *file = fopen(filename, "rb");

    // se nao consegue abrir envia erro
    if (file == NULL) {
        char message[] = "RGT NOK\n";
        send_response(fd_client, message, strlen(message));
        return;
    }

    char message[MAXSIZE];                          // mensagem de resposta correta
    sprintf(message, "RGT OK %d ", Fsize);

    // envia RGT OK FileSize
    if (!send_response(fd_client, message, strlen(message))) {
        fclose(file);
        return;
    }

    // guarda o conteúdo lido do ficheiro
    char content[MAXSIZE];
    total = 0;

    // leitura do ficheiro que pretendemos enviar
    while (total < Fsize) {
        int to_read = Fsize - total;

        if (to_read > MAXSIZE) {
            to_read = MAXSIZE;
        }

        int bytes = fread(content, 1, to_read, file);

        if (bytes == 0) {
            printf("Error reading file\n");
            fclose(file);
            return;
        }

        // envia pelo socket o conteúdo do ficheiro
        if (!send_response(fd_client, content, bytes)) {
            fclose(file);
            return;
        }

        total += bytes;
    }

    fclose(file);

    // termina a resposta ao pedido enviando pelo socket o terminador
    if (!send_response(fd_client, "\n", 1)) {
        return;
    }

    printf("File sent: %s (%d bytes)\n", filename, total);
}

// DS, UDP E TCP

// CONVERT_DS: converte a mensagem de input no formato pretendido pelo DS
void convert_ds(char *command, User *user, char * ds_command, char *filename, int Fsize, char *label,char *peer_uid) {
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
    } else if (strcmp(command, "where") == 0) {
        sprintf(ds_command, "WHR %s %s\n", filename, peer_uid);
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
        //return 0; erro a ver
    }
    
    response[n] = '\0';                                       // adiciona o terminador de string no final da resposta
    return 1;

}

// DS_TCP_COMMUNICATION: comunicação TCP para o DS: cria socket, connect, envia message e recebe em response
int ds_tcp_communication( char *message, char *response, struct addrinfo *res_tcp) {
    int fd_tcp, total = 0;
    ssize_t n;
    
    fd_tcp = socket(AF_INET, SOCK_STREAM, 0);
    if(fd_tcp == -1) {
        perror("socket");
        return 0;
    }

    set_timeout(fd_tcp, 5);

    if (connect(fd_tcp, res_tcp->ai_addr, res_tcp->ai_addrlen) == -1) { //inicia a ligação TCP com o DS
        perror("connect");
        close(fd_tcp);
        return 0;
    }

    n = write(fd_tcp, message, strlen(message));        // envia mensagem para o DS, escreve bytes no socket
    
    if (n == -1) {
        perror("write");
        close(fd_tcp);
        return 0;
    }
    if (n != strlen(message)) {
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
        if (response[total - 1] == '\n') {
            break;
        }
    }

    response[total] = '\0';                                 // transforma os bytes recebidos numa string valida 
    close(fd_tcp);
    return 1;
}

// DS_REPLY: recebe a resposta do DS e imprime a mensagem apropriada para o utilizador
void ds_reply(char *response, User *user, char *peer_ip, int *peer_tcpport) {
    char command[MAXSIZE]; // RLI, RLO, RUR, RPB, RRM, RLS, RVR
    char status [MAXSIZE]; // OK, REG, NOK, NLG, UNR, WRP
    int n = sscanf(response, "%s %s\n", command, status);

    if (n ==1 && strcmp(command, "ERR") == 0) {
        printf("error\n");  
        return;
    }
    if(n != 2) { //o resto tem que ter exatamente 2 campos
        printf("Invalid DS response\n");
        return;
    }
    
    if (strcmp(command, "RLI") == 0) {                      // LOGIN
        
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
            printf("Incorrect password\n");         
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
            printf("User not registered\n");        //nao esta no enunciado mas faz sentido lol

        } else if (strcmp(status, "WRP") == 0) {
            printf("Incorrect password\n");

        } else if (strcmp(status, "NOK") == 0) {
            printf("File not published\n");
        }
    } else if (strcmp(command, "RLS") == 0) {
        if(strcmp(status, "OK") == 0) {
            print_list(response);
            
        } else if (strcmp(status, "NOK") == 0) {
            printf("No published resources\n");         //nao esta enunciado
        }
    } else if (strcmp(command, "RVR") == 0) {
        if(strcmp(status, "OK") == 0) {
            print_versions(response);
        } else if (strcmp(status, "NOK") == 0) {
            printf("No versions available\n");
        }
    } else if (strcmp(command, "RWH") == 0) { 
        //peer_ip[0] = '\0';
        //*peer_tcpport = 0;                 
        if(strcmp(status, "OK") == 0) {                     
            if(sscanf(response, "RWH OK %15s %d", peer_ip, peer_tcpport) == 2 
        && valid_peerport(*peer_tcpport)) {
                printf("Peer available found IP: %s, TCP port: %d\n", peer_ip, *peer_tcpport);
            } else {
                peer_ip[0] = '\0';
                *peer_tcpport = 0;
                printf("Invalid DS response\n");
            }

        } else if (strcmp(status, "NOK") == 0) {
            printf("Peer or publication no longer available\n");
        } else {
            printf("Invalid DS response\n");
        }
    } else {
        printf("Unknown command from DS: %s\n", command);
    }
}

// CONNECT_PEER: cria socket TCP e estabelece ligação com o peer e devolve o fd do peer que queremos comunicar
int connect_peer(char *peer_ip, int peer_tcpport) {
    int fd_get;
    struct addrinfo hints_peer, *res_get;
    char peerport_str[6];

    fd_get = socket(AF_INET, SOCK_STREAM, 0);               // cria o socket TCO
    if (fd_get == -1) {
        perror("socket");
        //é preciso dar freeaddrinfo(res_get) aqui? 
        return -1;
    }

    // prepara argumentos tcp
    sprintf(peerport_str, "%d", peer_tcpport);              // converter int para string para passar para getaddrinfo 
    memset(&hints_peer, 0, sizeof(hints_peer));
    hints_peer.ai_family = AF_INET;                         // endereços IPv4
    hints_peer.ai_socktype = SOCK_STREAM;                   // TCP 

    int errcode = getaddrinfo(peer_ip, peerport_str, &hints_peer, &res_get);
    if (errcode != 0) {
        fprintf(stderr, "getaddrinfo failed\n");
        close(fd_get);
        return -1;
    }

    set_timeout(fd_get, 5);                                 // timeout para o socket

    if (connect(fd_get, res_get->ai_addr, res_get->ai_addrlen) == -1) {    // estabelecer conexção TCP com o peer
        perror("connect");
        freeaddrinfo(res_get);
        close(fd_get);
        return -1;
    }

    freeaddrinfo(res_get);
    return fd_get;                                          // devolve o fd do sokcet se funcionar
}

// SEND_GET: cria pedido de ficheiro através da ligação TCP
int send_get(int fd_get, char *filename){
    char message[32];                                       // máximo filename size é 24 + get + espaços+ \n +\0    ou FILENAMESIZE +6

    int size = snprintf(message, sizeof(message), "GET %s\n", filename);        // constrói a mensagem  GET foto.jpg\n
    if (size < 0 || size >= sizeof(message)) {                                  // verifica que cabe no buffer
        printf("invalid GET message\n");
        return 0;
    }

    // envio da mensagem por tcp ao peer
    int total = 0;
    while (total < size) {
        ssize_t n = write(fd_get, message + total, size - total);               // envia os bytes através do socket TCP para o peer
        if (n <= 0) {
            perror("write");
            return 0;
        }
        total += n;
    }
    return 1;
}


volatile sig_atomic_t ctrl_c = 0;

void handle_sigint(int sig) {
    ctrl_c = 1;
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
    
    char peer_ip[16];
    int peer_tcpport;
    char peer_uid[UID_SIZE + 1];

    signal(SIGPIPE, SIG_IGN); // casos como write num socket que fecha a meio, write falha e devolve - 1
    signal(SIGINT, handle_sigint);

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

        errcode = getaddrinfo(dsip, dsport, &hints, &res);
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
        
        int fd_peer;
        fd_peer = socket(AF_INET, SOCK_STREAM, 0);
        if (fd_peer == -1) {
            perror("socket");
            exit(1);
        }

        struct addrinfo hints_peer, *res_peer;
        char peerport_str[6];

        sprintf(peerport_str, "%d", peerport);
        memset(&hints_peer, 0, sizeof(hints_peer));

        hints_peer.ai_family = AF_INET;  //endereços IPv4
        hints_peer.ai_socktype = SOCK_STREAM; //TCP 
        hints_peer.ai_flags = AI_PASSIVE;

        errcode = getaddrinfo(NULL, peerport_str, &hints_peer, &res_peer);

        if (errcode != 0) {
            fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(errcode));
            close(fd_peer);
            exit(1);
        }

        if (bind(fd_peer, res_peer->ai_addr, res_peer->ai_addrlen) == -1) {
            perror("bind");
            freeaddrinfo(res_peer);
            close(fd_peer);
            exit(1);
        }

        freeaddrinfo(res_peer);

        if (listen(fd_peer, 5) == -1) {
            perror("listen");
            close(fd_peer);
            exit(1);
        }

        fd_set inputs, testfds;
        FD_ZERO(&inputs);
        FD_SET(STDIN_FILENO, &inputs);         //terminal para ler comandos escritos pelo user
        FD_SET(fd_peer, &inputs);             //novas conexões de peers

        int max_fd;                          //maior descriptor que o select vai monitorizar
        if (fd_peer > STDIN_FILENO) { 
            max_fd = fd_peer;
        } else {
            max_fd = STDIN_FILENO;
        }

        // comandos
        while(1) {

            testfds = inputs;               //copiar os descriptors para testfds que queremos monitorizar 
            int n = select(max_fd + 1, &testfds, NULL, NULL, NULL);
            if (n == -1) {
                if (errno == EINTR) {
                    if (ctrl_c) {
                        ctrl_c = 0;

                        if (user.logged_in) {
                            convert_ds("logout", &user, ds_command,
                                    filename, Fsize, label, peer_uid);

                            if (ds_communication(fd, ds_command, response, res)) {
                                ds_reply(response, &user, peer_ip, &peer_tcpport);
                            }
                        }
                    }
                    continue;
                }
                perror("select");
                break;
            }
            if (FD_ISSET(STDIN_FILENO, &testfds)) { //verifica se o terminal tem dados para ler
                if(fgets(buffer, MAXSIZE, stdin) == NULL) {           // lê a linha escrita no terminal
                    break;
                }
                if(sscanf(buffer, "%s", command)!= 1) continue;      // retiramos a primeira palavra(comando ex.login)
                
                // LOGIN
                if (strcmp(command, "login") == 0) {
                    int n = sscanf(buffer, "%*s %s %s", uid_aux, password_aux);
                    if (n != 2) {
                        printf("Incorrect arguments\n");
                        continue;
                    }
                    if(user.logged_in) {                            // se já existe alguem logged in, nao permitimos outro login
                        printf("Please logout first\n");
                        continue;
                    }
            
                    if(valid_input(uid_aux, password_aux)) {        // verifica o input
                        
                        strcpy(user.uid, uid_aux);
                        strcpy(user.password, password_aux);

                        convert_ds(command, &user, ds_command, filename, Fsize, label, peer_uid);   //converte dados do user numa mensagem para DS
                        if (ds_communication(fd, ds_command, response, res)) {                      //envia a mensagem para o DS e recebe resposta
                            ds_reply(response, &user, peer_ip, &peer_tcpport);                      //interpreta a resposta do DS
                        }
                    } else {
                        printf("Incorrect arguments\n");
                    }
                    
                // LOGOUT
                } else if (strcmp(command, "logout") == 0) {
                    if (!user.logged_in) {                          // so faz log out se estiver alguem logged in
                        printf("Please login first\n");
                        continue;
                    }
                    
                    // conversão, comunicação UDP, resposta
                    convert_ds(command, &user, ds_command, filename, Fsize, label, peer_uid);                      
                    if (ds_communication(fd, ds_command, response, res)) {
                        ds_reply(response, &user, peer_ip, &peer_tcpport);
                    } 
                    
                // UNREGISTER
                }  else if (strcmp(command, "unregister") == 0) {
                    if (!user.logged_in) {                          // só faz unregister se estiver alguem logged in
                        printf("Please login first\n");
                        continue;
                    }
                    
                    // conversão, comunicação UDP, resposta
                    convert_ds(command, &user, ds_command, filename, Fsize, label, peer_uid);                   
                    if (ds_communication(fd, ds_command, response, res)) {
                        ds_reply(response, &user, peer_ip, &peer_tcpport);
                    }
                    
                // EXIT
                } else if (strcmp(command, "exit") == 0) {
                    if(user.logged_in) { printf("Please logout first\n");} 
                    else { break; }                                  // acaba o programa         


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
                    Fsize = open_file(filename);                    // tenta abrir o ficheiro e obter o tamanho
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
                    convert_ds(command, &user, ds_command, filename, Fsize, label, peer_uid);                     // comunicação com o DS 
                    if (ds_communication(fd, ds_command, response, res)) {
                        ds_reply(response, &user, peer_ip, &peer_tcpport);
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
                    if (!valid_filename(filename)) {                 // valida o filename
                        printf("Incorrect arguments\n");
                        continue;
                    }

                    // conversão, comunicação UDP, resposta
                    convert_ds(command, &user, ds_command, filename, Fsize, label, peer_uid);                     // comunicação com o DS 
                    if (ds_communication(fd, ds_command, response, res)) {
                        ds_reply(response, &user, peer_ip, &peer_tcpport);
                    }
                
                // LIST
                } else if (strcmp(command, "list") == 0) {
                    // conversão, comunicação UDP, resposta
                    convert_ds(command, &user, ds_command, filename, Fsize, label, peer_uid);                     // comunicação com o DS 
                    if (ds_communication(fd, ds_command, response, res)) {
                        ds_reply(response, &user, peer_ip, &peer_tcpport);
                    }

                // VERSIONS - TCP
                } else if(strcmp(command, "versions") == 0) {

                    int n = sscanf(buffer, "%*s %s", filename); 
                    if (n != 1) {
                        printf("Incorrect arguments\n");
                        continue;
                    }
                    if (!valid_filename(filename)) {                    // valida filename
                        printf("Invalid file\n");
                        continue;
                    }
                    // conversão, comunicação TCP, resposta
                    convert_ds(command, &user, ds_command, filename, Fsize, label, peer_uid);                     // comunicação com o DS 
                    if (ds_tcp_communication(ds_command, response, res_tcp)) {
                        ds_reply(response, &user, peer_ip, &peer_tcpport);
                    } 

                // GET
                } else if(strcmp(command, "get") == 0) {
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
                        printf("Invalid file\n");
                        continue;
                    }
                    
                    // obtem o versions do ficheiro pedido internamente
                    convert_ds("versions", &user, ds_command, filename, Fsize, label, peer_uid);     // pedir versions ao DS
                    if (!ds_tcp_communication(ds_command, response, res_tcp)) {
                        printf("error communicating to ds\n");
                        continue;
                    }

                    // seleciona o peer mais recente
                    if (!select_peer(response, peer_uid, &user)) {                                   // escolher o peer com a primeira versão disponível do ficheiro
                        printf("No available peer found\n");
                        continue;
                    }
                    
                    convert_ds("where", &user, ds_command, filename, Fsize, label, peer_uid);       // construir a mensagem do where para o DS
                    if (!ds_communication(fd, ds_command, response, res)) {                         // enviar WHR ao DS por udp
                        printf("error communicating to ds\n");
                        continue;
                    }

                    //inicializar aqui em vez de ser no ds reply caso dê erro e a funcao nao entre ai, e depois o peer ta com valores antigos pq nao entrou no ds reply
                    peer_ip[0] = '\0'; 
                    peer_tcpport = 0;

                    ds_reply(response, &user, peer_ip, &peer_tcpport);                              // interpretar a resposta do DS e obter o IP e porta TCP do peer
                    if (peer_ip[0] == '\0' || peer_tcpport == 0) {
                        continue;
                    }

                    // TESTE LOCAL - retirar
                    #ifdef LOCALTEST
                    strcpy(peer_ip, "127.0.0.1");   // só para testes na mesma máquina
                    #endif  

                    int fd_get = connect_peer(peer_ip, peer_tcpport);                               // estabelecer ligação TCP com o peer
                    if (fd_get == -1) {
                        printf("error connecting to peer\n");
                        continue;
                    }

                    if (!send_get(fd_get, filename)) {                                              // enviar o pedido GET para o peer
                        printf("invalid GET request\n");
                        close(fd_get);
                        continue;
                    }

                    //falta receber a resposta do peer e guardar o ficheiro ig
                    if (!receive_file(fd_get, filename)) {
                        close(fd_get);
                        continue;
                    }   
                    close(fd_get); 

                // comando nao reconhecido
                } else {
                    printf("Unknown command\n");
                } 
            }
            if (FD_ISSET(fd_peer, &testfds)){
                int fd_client = accept(fd_peer, NULL, NULL);
                if (fd_client == -1) {
                    perror("accept");
                    continue;
                }
                set_timeout(fd_client, 5);
                //receber a mensagem GET filename do outro peer
                //verificar se o ficheiro existe e está disponível
                //enviar RGT OK + tamanho + conteúdo do ficheiro ou RGT NOK se não estiver disponível
                
                peer_request(fd_client);
                close(fd_client); // apenas aceitamos a conexão e fechamos, não há comunicação com o peer
            }
        
        }
    
    freeaddrinfo(res);
    freeaddrinfo(res_tcp);
    close(fd);
    close(fd_peer); 
    return 0;
    
}
//./user -m 58001 -n 193.136.138.142 -p 59000
// ./user -m peerport [-n DSIP] [-p DSport]

//login 675846 abcdef12
//login 676767 abcdef12


//melhorias ainda por ver:
//sscanf(buffer, "%*s %24s %20s", filename, label);   para o sscanf nao dar overflow
// ver se é preciso limitar "logout lixo" com "extra"