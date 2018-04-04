/*----------------------------------------------------------------------------
 *
 *  Name:  Alex Meyer
 *  Description:  File transfer server that listens for a connection and then
 *        opens a second connection to sends a list of diretory contents or a
 *        requested file to the client on a given port number.
 *
 *
 *----------------------------------------------------------------------------*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <ctype.h>
#include <fcntl.h>
#include <dirent.h>

#define BUFFER_SIZE 256

void error(const char *msg) { perror(msg); exit(1); } // Error function used for reporting issues

// ===========================================================================================
// Recieves the message sent from the client and breaks it apart into the 3 components
//      option1 = command directive (either -l or -g)
//      option2 = filename
//      option3 = port number
// Returns 1 if a valid request and 0 if an invalid request
// ===========================================================================================

int processRequest (char* msg, char* option1, char* option2, char* option3){
    char msgCopy[BUFFER_SIZE];
    memset(msgCopy, '\0', sizeof(msgCopy));
    strcpy(msgCopy, msg);
    char* token;
    int count = 0;
    int i;
    
    //message is delimeted by spaces between the parameters
    token = strtok(msgCopy, " ");
    while (token != NULL) {
        count++;
        if (count == 1)
            strcpy(option1, token);
        else if (count == 2)
            strcpy(option2, token);
        else
            strcpy(option3, token);
        
        token = strtok(NULL, " ");
    }
    
    //Option 3 must hold the port number so if only 2 parameters are given in the message from client,
    //then it is copied in
    if (count < 3)
        strcpy(option3, option2);
    
    // Validate that the message is in the correct form
    if (count > 1 && count < 4) {
        if (strcmp(option1, "-l") == 0 || strcmp(option1, "-g") == 0) {
            for (i = 0; i < strlen(option3); i++) {
                if (!isdigit(option3[i])){
                    return 0;
                }
            }
            
            //valid request
            return 1;
        }
    }
    
    //invalid request
    return 0;
}

// ===========================================================================================
// From "Beej's Guide to Network Programming"
//
// Receives the sockaddr from the TCP connection in order to extract the client host name
// ===========================================================================================
void getClientHostName ( struct sockaddr * addr, socklen_t sizeOfClientInfo, char* clientHostName) {
    char host[1024];
    char service[20];
    
    getnameinfo(addr, sizeOfClientInfo, host, sizeof host, service, sizeof service, 0);
    
    strcpy(clientHostName, host);
}


// ===========================================================================================
// Build a response to be sent on the data connection based on the instruction (l or g) and
// filename sent by the client.
//
// Receives pointers to the information from the client
// Returns a pointer to the response string.
// ===========================================================================================
char* constructResponse(char* choice, char* filename) {
    int fd_filename;  //file descriptor
    unsigned int responseSize = 0;
    int nChar;
    char* res;
    int charRead;
    int i;
    int fileNameLength;
    
    if (strcmp(choice, "-g") == 0) {            //client requested a file
        
        //open file
        fd_filename = open(filename, O_RDONLY);
        
        //File not found
        if (fd_filename < 0) {
            res = malloc(19 * sizeof(char));
            memset(res, '\0', 19);
            
            //put int the length of the "file not found" response
            res[0]=0;
            res[1]=0;
            res[2]=0;
            res[3]=14;
            
            strcpy(res+4, "FILE NOT FOUND");
            printf("File not found:  Sending error message to ");
            return res;
        }
        
        //find file size and return pointer to begining
        responseSize = (int)lseek(fd_filename, 0, SEEK_END);
        lseek(fd_filename, 0, SEEK_SET);
        
        //read the file
        res = malloc((responseSize+5)*sizeof(char));
        memset(res, '\0', responseSize+5);
        
        // convert responseSize to a 4 digit byte string
        // https://stackoverflow.com/questions/3784263/converting-an-int-into-a-4-byte-char-array-c
        for (i = 0; i < 4; i++) {
            res[i] = (responseSize >> ((3-i)*8)) & 0xFF;
        }

        //read in the file
        nChar = 4;
        charRead = read(fd_filename, res + nChar, responseSize);
        
        printf("Sending \"%s\" to ", filename);
        
    }
    else {          //client request directory list
        DIR* d;
        struct dirent* dir;
        
        //initialize the respons with 4 empty bytes reserved for the response length
        res = malloc(4);
        memset(res, '\0', 4);
        responseSize = 4;
        
        //open directory and read in each file to the response with a newline after each file name
        d = opendir(".");
        while ((dir = readdir(d)) != NULL) {
            fileNameLength = strlen(dir->d_name);
            res = realloc(res, responseSize+fileNameLength+1);      //allocate additional space for filename and \n
            memset(res+responseSize, '\0', fileNameLength+1);       //set new space to null
            strcpy(res+responseSize, dir->d_name);                  //copy in filename
            strcpy(res+responseSize+fileNameLength, "\n");          //add newline
            responseSize = strlen(res+4) + 4;
        }
        res[ 4 + strlen(res+4) - 1 ] = '\0';                        //replace last \n with null
        
        //write response length into the first 4 bytes
        for (i = 0; i < 4; i++) {
            res[i] = ((responseSize-4) >> ((3-i)*8)) & 0xFF;
        }
        
        printf("Sending directory contents to ");
    }
    
    return res;
}

// ====================================================================================================
// The server becomes the client and and connects to ftclient on the data port
// ====================================================================================================
void sendDataResponse(char* clientHostName, char* clientPort, char* msg) {
    
    int socketFD, portNumber, charsWritten;
    struct sockaddr_in serverAddress;
    struct hostent* serverHostInfo;
    unsigned int msgLength;

    
    //recover the message length.
    //strlen() does not work because the first byte (unless the file is very long) is \0
    // https://stackoverflow.com/questions/8173037/convert-4-bytes-char-to-int32-in-c
    msgLength =  ((msg[0] & 0xFF) << 24) | ((msg[1] & 0xFF) << 16) | ((msg[2] & 0xFF) << 8) | ((msg[3] & 0xFF) << 0);
    msgLength += 4;
    
    // Set up the server address struct
    memset((char*)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
    portNumber = atoi(clientPort); // Get the port number, convert to an integer from a string
    serverAddress.sin_family = AF_INET; // Create a network-capable socket
    serverAddress.sin_port = htons(portNumber); // Store the port number
    serverHostInfo = gethostbyname(clientHostName); // Convert the machine name into a special form of address
    if (serverHostInfo == NULL) { fprintf(stderr, "CLIENT: ERROR, no such host\n"); exit(0); }
    memcpy((char*)&serverAddress.sin_addr.s_addr, (char*)serverHostInfo->h_addr, serverHostInfo->h_length); // Copy in the address

    // Set up the socket
    socketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
    if (socketFD < 0) error("CLIENT: ERROR opening socket");
    
    // Connect to server
    if (connect(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to address
        error("CLIENT: ERROR connecting");
    
    
    // Message protocol:  32bit response length + response
    charsWritten = send(socketFD, msg, msgLength, 0); // Write to the server
    if (charsWritten < 0) error("CLIENT: ERROR writing to socket");
    if (charsWritten < msgLength) printf("CLIENT: WARNING: Not all data written to socket!\n");
    
    close(socketFD);
}


// ====================================================================================================
// Main function:  Server listening socket for the control connection is set up and gathers information
// ====================================================================================================
int main(int argc, const char * argv[]) {
    
    int listenSocketFD, establishedConnectionFD, portNumber, charsRead;
    socklen_t sizeOfClientInfo;
    char buffer[256];
    char clientHostName[256];
    struct sockaddr_in serverAddress, clientAddress;
    
    //variables for the incoming control message
    char option1[100], option2[100], option3[100];
    char* response;
    
    if (argc < 2) { fprintf(stderr,"USAGE: %s port\n", argv[0]); exit(1); } // Check usage & args
    
    // Set up the address struct for this process (the server)
    memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
    portNumber = atoi(argv[1]); // Get the port number, convert to an integer from a string
    serverAddress.sin_family = AF_INET; // Create a network-capable socket
    serverAddress.sin_port = htons(portNumber); // Store the port number
    serverAddress.sin_addr.s_addr = INADDR_ANY; // Any address is allowed for connection to this process
    
    // Set up the socket
    listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
    if (listenSocketFD < 0) error("ERROR opening socket");
    
    // Enable the socket to begin listening
    if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port
        error("ERROR on binding");
    listen(listenSocketFD, 5); // Flip the socket on - it can now receive up to 5 connections
    printf("Server open on %d\n", portNumber);
    
    while(1) {
        // Accept a connection, blocking if one is not available until one connects
        sizeOfClientInfo = sizeof(clientAddress); // Get the size of the address for the client that will connect
        establishedConnectionFD = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
        if (establishedConnectionFD < 0) error("ERROR on accept");
    
        //retrieve client host name from client that was accept()ed
        memset(clientHostName, '\0', 256);
        getClientHostName((struct sockaddr *)&clientAddress, sizeOfClientInfo, clientHostName);
        printf("Connection from %s\n", clientHostName);
        
        // Get the message from the client and display it
        memset(buffer, '\0', 256);
        charsRead = recv(establishedConnectionFD, buffer, 255, 0); // Read the client's message from the socket
        if (charsRead < 0) error("ERROR reading from socket");
        
        //clear parameters and fill them from the client message
        memset(option1, '\0', sizeof(option1));
        memset(option2, '\0', sizeof(option2));
        memset(option3, '\0', sizeof(option3));
        if (processRequest(buffer, option1, option2, option3)) {
            
            //Display request and build response
            if (strcmp(option1, "-l") == 0)
                printf("List directory requested on %s\n", option3);
            
            if (strcmp(option1, "-g") == 0)
                printf("File \"%s\" requested on port %s\n", option2, option3);
            
            response = constructResponse(option1, option2);
            printf("%s:%s\n", clientHostName, option3);
            
            sleep(1);
            sendDataResponse(clientHostName, option3, response);
            send(establishedConnectionFD, "All done", 9, 0);
        }
        else {
            charsRead = send(establishedConnectionFD, "404&Invalid Command", 19, 0); // Send success back
        }

        //let the client close the control connection
        if (charsRead < 0) error("ERROR writing to socket");
        
    }
    
    return 0;
}
