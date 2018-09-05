
#include "whatsappio.h"
#include <sys/socket.h>
#include <unistd.h> //for read()
#include <arpa/inet.h>
#include <iostream>
#include <set>
#include <map>
#include <cerrno>


#define ARGS_NUM_FOR_CLIENT 4
// Global variables
char clientName[WA_MAX_NAME + 1] = {0};
int client_fd = 0;
fd_set baseFDs;


// Read data from connection
void read_data(int s, char* buf, int n)
{
    int bcount = 0;          /* counts bytes read */
    int br = 0;              /* bytes read this pass */
    while (bcount < n) {             /* loop until full buffer */
        if ((br = read(s, buf, n - bcount)) > 0) {
            bcount += br;                /* increment byte counter */
            buf += br;                   /* move buffer ptr for next read */
        }
        if (br < 1){
            print_error("read", errno);
            exit(EXIT_FAILURE);
        }
    }
}


// Writing data to connection
void write_data(int s, char* buf, int n)
{
    int bcount = 0;          /* counts bytes read */
    int br = 0;              /* bytes read this pass */
    while (bcount < n) {             /* loop until full buffer */
        if ((br = write(s, buf, n - bcount)) > 0) {
            bcount += br;                /* increment byte counter */
            buf += br;                   /* move buffer ptr for next read */
        }
        if (br < 0){
            print_error("write", errno);
            exit(EXIT_FAILURE);
        }
    }
}


//checks if given name contains only digits and letters
bool is_name_valid(const std::string& name)
{
    for (unsigned int i = 0; i <name.length() ; ++i) {
        if (!isalnum(name[i]))
            return false;
    }
    return true;
}


//validates input and sends request to the server
int create_group_req(std::string& groupName, std::vector<std::string>& members, const std::string& command){

    bool isInputValid = true;
    int groupSize = members.size() + 1; // +1 for group creator
    std::string groupCreator(clientName);
    char ack[WA_MAX_MESSAGE + 1] = {0};

    for (auto groupMember : members) { //validates all group members' names
        if (!is_name_valid(groupMember))
        {
            isInputValid = false;
            break;
        }
        if (groupMember == groupCreator) // checks if group creator exists also in members list
            groupSize--;
    }

    if (groupSize < 2 || !isInputValid || !is_name_valid(groupName)) {
        print_create_group(false, false, groupCreator, groupName);
        return EXIT_FAILURE;
    }
    else
    {
        //send command to server
        char cmd[WA_MAX_MESSAGE + 1] = {0};
        strncpy(cmd, command.c_str(), command.length());
        write_data(client_fd, cmd, sizeof(cmd));
        // reading if command processed successfully
        read_data(client_fd, ack, WA_MAX_MESSAGE + 1);
        char success_msg[WA_MAX_MESSAGE + 1] = {0};
        strncpy(success_msg, "SUCCESS", strlen("SUCCESS"));
        if (strcmp(ack, success_msg) == 0) {
            print_create_group(false, true, groupCreator, groupName);
            return EXIT_SUCCESS;
        }
        print_create_group(false, false, groupCreator, groupName);
        return EXIT_FAILURE;
    }

}

int send_req(const std::string& command, std::string& sendTo)
{
    char recipient[WA_MAX_NAME + 1] = {0};
    strncpy(recipient, sendTo.c_str(), sendTo.length());
    if (strcmp(clientName, recipient) == 0) //client tries to send a message to himself
    {
        std::cout << "ERROR: failed to send.\n";
        return EXIT_FAILURE;
    }

    //send command to server
    char cmd[WA_MAX_MESSAGE + 1] = {0};
    strncpy(cmd, command.c_str(), command.length());
    write_data(client_fd, cmd, sizeof(cmd));
    // reading if command processed successfully
    char ack[WA_MAX_MESSAGE + 1] = {0};
    read_data(client_fd, ack, WA_MAX_MESSAGE + 1);
    char success_msg[WA_MAX_MESSAGE + 1] = {0};
    strncpy(success_msg, "SUCCESS", strlen("SUCCESS"));
    if (strcmp(ack, success_msg) == 0){
        std::cout << "Sent successfully.\n";
        return EXIT_SUCCESS;
    }
    std::cout << "ERROR: failed to send.\n";
    return EXIT_FAILURE;
}

void who_req(const std::string& command)
{

    //send command to server
    char cmd[WA_MAX_MESSAGE + 1] = {0};
    strncpy(cmd, command.c_str(), command.length());
    write_data(client_fd, cmd, WA_MAX_MESSAGE + 1);
    // reading if command processed successfully
    char whoList[WA_MAX_MESSAGE + 1] = {0};
    read_data(client_fd, whoList, WA_MAX_MESSAGE + 1);
    std::cout << whoList << std::endl;

}

void exit_req(const std::string& command)
{

    //send command to server
    char cmd[WA_MAX_MESSAGE + 1] = {0};
    strncpy(cmd, command.c_str(), command.length());
    write_data(client_fd, cmd, sizeof(cmd));

    // reading if command processed successfully
    char ack[WA_MAX_MESSAGE + 1] = {0};
    read_data(client_fd, ack, WA_MAX_MESSAGE + 1);

    char success_msg[WA_MAX_MESSAGE + 1] = {0};
    strncpy(success_msg, "SUCCESS", strlen("SUCCESS"));
    if (strcmp(ack, success_msg) == 0){
        print_exit(false, clientName);
        //terminate itself
        if (shutdown(client_fd, SHUT_RDWR) < 0)
        {
            print_error("shutdown", errno);
            exit(EXIT_FAILURE);
        }
        if (close(client_fd) < 0)
        {
            print_error("close", errno);
            exit(EXIT_FAILURE);
        }
        FD_CLR(client_fd, &baseFDs);
        exit(0);
    }
}

void receive_command(const std::string& command)
{
    command_type commandT;
    std::string name;
    std::string message;
    std::vector<std::string> clients;


    // parses received command into: type, client/group name, message, vector of clients
    parse_command(command, commandT, name, message, clients);


    if (commandT == CREATE_GROUP)
    {
        create_group_req(name, clients, command);
    }
    else if (commandT == SEND)
    {
        send_req(command, name);
    }
    else if (commandT == WHO)
    {
        who_req(command);
    }
    else if (commandT == EXIT)
    {
        exit_req(command);
    }
    else if (commandT == INVALID)
    {
        print_invalid_input();
    }
}



int main(int argc, char* argv[]){
    if (argc != ARGS_NUM_FOR_CLIENT)
        print_client_usage();
    struct sockaddr_in sa;
    strcpy(clientName, argv[1]);
    char* serverIP = argv[2];
    int portNum = std::stoi(argv[3]);
    char ack[WA_MAX_MESSAGE + 1] = {0};
    char emptyAck[WA_MAX_MESSAGE + 1] = {0};

    //initializing sockaddr_in
    memset(&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_family = AF_INET;
    if (inet_aton(serverIP, &(sa.sin_addr)) == 0)
        print_invalid_input();
    sa.sin_port = htons((unsigned short)portNum);

    if ((client_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        print_error("socket", errno);
        exit(EXIT_FAILURE);
    }

    if (connect(client_fd, (struct sockaddr*) &sa, sizeof(sa)) == -EXIT_FAILURE) {
        print_error("connect", errno);
        exit(EXIT_FAILURE);
    }

    // Writing client's name to the server
    write_data(client_fd, clientName, sizeof(clientName));
    // reading if connection was successful
    read_data(client_fd, ack, WA_MAX_MESSAGE + 1);
    char connectedMsg[WA_MAX_MESSAGE + 1] = {0};
    strncpy(connectedMsg, "CONNECTED", strlen("CONNECTED"));
    char duplicateMsg[WA_MAX_MESSAGE + 1] = {0};
    strncpy(duplicateMsg, "DUPLICATE", strlen("DUPLICATE"));
    if (strcmp(ack, connectedMsg) == 0)
        print_connection();
    else if (strcmp(ack, duplicateMsg) == 0){
        print_dup_connection();
        if (shutdown(client_fd, SHUT_RDWR) < 0)
        {
            print_error("shutdown", errno);
            exit(EXIT_FAILURE);
        }
        if (close(client_fd) < 0)
        {
            print_error("close", errno);
            exit(EXIT_FAILURE);
        }
        exit(EXIT_FAILURE);
    }
    else if (strcmp(ack, emptyAck) == 0){
        print_fail_connection();
        exit(EXIT_FAILURE);
    }

    fd_set readFDs;
    FD_ZERO(&baseFDs);
    FD_SET(client_fd, &baseFDs);
    FD_SET(STDIN_FILENO, &baseFDs);

    while (true){
        FD_ZERO(&readFDs);
        readFDs = baseFDs;

        // waiting for activity from sockets
        if (select(5, &readFDs, nullptr, nullptr, nullptr) < 0){
            print_error("select", errno);
        }
        // if it's an IO operation
        if (FD_ISSET(STDIN_FILENO, &readFDs)){
            std::string cmd;
            getline(std::cin, cmd);
            receive_command(cmd);
        }
        // if activity found in server's socket, its an incoming connection
        if (FD_ISSET(client_fd, &readFDs)){
            char ack2[WA_MAX_MESSAGE + 1] = {0};
            read_data(client_fd, ack2, WA_MAX_MESSAGE + 1);
            char serverExitMsg[WA_MAX_MESSAGE + 1] = {0};
            strncpy(serverExitMsg, "SERVER_EXIT", strlen("SERVER_EXIT"));
            if (strcmp(ack2, serverExitMsg) == 0){
                if (shutdown(client_fd, SHUT_RDWR) < 0)
                {
                    print_error("shutdown", errno);
                    exit(EXIT_FAILURE);
                }
                if (close(client_fd) < 0)
                {
                    print_error("close", errno);
                    exit(EXIT_FAILURE);
                }
                FD_CLR(client_fd, &baseFDs);
                exit(EXIT_FAILURE);
            }
            std::cout << ack2 << std::endl;
        }
    }
    return EXIT_SUCCESS;
}