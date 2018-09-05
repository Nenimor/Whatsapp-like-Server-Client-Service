
#include "whatsappio.h"
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <iostream>
#include <cerrno>
#include <map>
#include <set>


#define MAX_CONNECTIONS 10

#define ARG_NUM_FOR_SERVER 2
// Struct that holds relevant data for each client
struct ClientData{
    int client_fd;
    std::string name;
    std::set<std::string> groups;
};


// Global variables
std::map<std::string, struct ClientData> clients;
std::map<std::string, std::set<std::string>> groups;
std::set<std::string> clientToDelete;
fd_set clientFDs;


// Read data from connection
int server_read_data(int s, char* buf, int n)
{
    int bcount = 0;          /* counts bytes read */
    int br = 0;              /* bytes read this pass */
    while (bcount < n) {             /* loop until full buffer */
        if ((br = read(s, buf, n - bcount)) > 0) {
            bcount += br;                /* increment byte counter */
            buf += br;                   /* move buffer ptr for next read */
        }
        if (br < 1)
            return -EXIT_FAILURE;
    }
    return br;
}


// Writing data to connection
int server_write_data(int s, char* buf, int n)
{
    int bcount = 0;          /* counts bytes read */
    int br = 0;              /* bytes read this pass */
    while (bcount < n) {             /* loop until full buffer */
        if ((br = write(s, buf, n - bcount)) > 0) {
            bcount += br;                /* increment byte counter */
            buf += br;                   /* move buffer ptr for next read */
        }
        if (br < 0)
            return -EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
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


//validates input and creates group
int create_group(std::string& groupName, std::vector<std::string>& groupMembers, std::string& groupCreator){
    //groupCreator- the client which opens the group

    // groupName is also a client name, or groupName already used in groups
    if (clients.count(groupName) || groups.count(groupName)) {
        print_create_group(true, false, groupCreator, groupName);
        // Sending fail msg to client
        char ack[WA_MAX_MESSAGE + 1] = {0};
        char fail_msg[] = "FAIL";
        strncpy(ack, fail_msg, strlen(fail_msg));
        if (server_write_data(clients[groupCreator].client_fd, ack, sizeof(ack)) == -EXIT_FAILURE)
            print_error("write", errno);
        return EXIT_FAILURE;
    }
    else
    {
        for (auto member : groupMembers) //checks if all group members are registered clients
        {
            if (!clients.count(member)){
                // Sending fail msg to client
                char ack[WA_MAX_MESSAGE + 1] = {0};
                char fail_msg[] = "FAIL";
                strncpy(ack, fail_msg, strlen(fail_msg));
                if (server_write_data(clients[groupCreator].client_fd, ack, sizeof(ack)) == -EXIT_FAILURE)
                    print_error("write", errno);
                return EXIT_FAILURE;
            }
        }
        std::set<std::string> newGroup(groupMembers.begin(), groupMembers.end());
        newGroup.insert(groupCreator);
        groups[groupName] = newGroup;
        for (auto member : newGroup) //adds the new group to all members' struct
            clients[member].groups.insert(groupName);
        print_create_group(true, true, groupCreator, groupName);
        //send msg to client (success)
        char ack[WA_MAX_MESSAGE + 1] = {0};
        char success_msg[] = "SUCCESS";
        strncpy(ack, success_msg, strlen(success_msg));
        if (server_write_data(clients[groupCreator].client_fd, ack, sizeof(ack)) == -EXIT_FAILURE)
            print_error("write", errno);
        return EXIT_SUCCESS;
    }
}

int send_command(std::string& sendTo, std::string& message, std::string& clientName)
{
    std::string newMessage = clientName + ": " + message;
    if (clients.count(sendTo)) //msg is designated to a client
    {
        if (clientName == sendTo) //client send message to itself
        {
            print_send(true, false, clientName, sendTo, message);
            //send fail msg to sender
            char ack[WA_MAX_MESSAGE + 1] = {0};
            char fail_msg[] = "FAIL";
            strncpy(ack, fail_msg, strlen(fail_msg));
            if (server_write_data(clients[clientName].client_fd, ack, sizeof(ack)) == -EXIT_FAILURE)
                print_error("write", errno);
            return EXIT_FAILURE;
        }
        //send success msg to sender
        char ack[WA_MAX_MESSAGE + 1] = {0};
        char success_msg[] = "SUCCESS";
        strncpy(ack, success_msg, strlen(success_msg));
        if (server_write_data(clients[clientName].client_fd, ack, sizeof(ack)) == -EXIT_FAILURE)
            print_error("write", errno);
        //send newMessage to client sendTo
        char msg[WA_MAX_MESSAGE + 1] = {0};
        strncpy(msg, newMessage.c_str(), newMessage.length());
        if (server_write_data(clients[sendTo].client_fd, msg, sizeof(msg)) == -EXIT_FAILURE)
            print_error("write", errno);
        print_send(true, true, clientName, sendTo, message);
        return EXIT_SUCCESS;
    }
    else if (groups.count(sendTo)) //msg is designated to a group
    {
        if (!groups[sendTo].count(clientName)) //sender isn't a group member
        {
            print_send(true, false, clientName, sendTo, message);
            //send fail msg to sender
            char ack[WA_MAX_MESSAGE + 1] = {0};
            char fail_msg[] = "FAIL";
            strncpy(ack, fail_msg, strlen(fail_msg));
            if (server_write_data(clients[clientName].client_fd, ack, sizeof(ack)) == -EXIT_FAILURE)
                print_error("write", errno);
            return EXIT_FAILURE;
        }
        for (auto groupMember : groups[sendTo])
        {
            if (groupMember == clientName) //doesn't send the message to it's sender
            {
                //send success msg to sender
                char ack[WA_MAX_MESSAGE + 1] = {0};
                char success_msg[] = "SUCCESS";
                strncpy(ack, success_msg, strlen(success_msg));
                if (server_write_data(clients[clientName].client_fd, ack, sizeof(ack)) == -EXIT_FAILURE)
                    print_error("write", errno);
                continue;
            }
            //send newMessage to client groupMember
            char msg[WA_MAX_MESSAGE + 1] = {0};
            strncpy(msg, newMessage.c_str(), newMessage.length());
            if (server_write_data(clients[groupMember].client_fd, msg, sizeof(msg)) == -EXIT_FAILURE)
                print_error("write", errno);
        }
        print_send(true, true, clientName, sendTo, message);
        return EXIT_SUCCESS;
    }
    else //msg destination is unknown
    {
        print_send(true, false, clientName, sendTo, message);
        //send fail msg to sender
        char ack[WA_MAX_MESSAGE + 1] = {0};
        char fail_msg[] = "FAIL";
        strncpy(ack, fail_msg, strlen(fail_msg));
        if (server_write_data(clients[clientName].client_fd, ack, sizeof(ack)) == -EXIT_FAILURE)
            print_error("write", errno);
        return EXIT_FAILURE;
    }
}

void who_command(std::string& clientName)
{
    std::string msg = "";
    for (auto client : clients)
    {
        if(!clientToDelete.count(client.first))
            msg += client.first + ",";
    }
    msg.pop_back(); //removes last unnecessary ','
    print_who_server(clientName);
    // Sending success msg to client
    char whoList[WA_MAX_MESSAGE + 1] = {0};
    strncpy(whoList, msg.c_str(), msg.length());
    if (server_write_data(clients[clientName].client_fd, whoList, sizeof(whoList)) == -EXIT_FAILURE)
        print_error("write", errno);
}


void exit_command(std::string& clientName)
{
    int tempClientFD = clients[clientName].client_fd;
    for (auto groupName : clients[clientName].groups) //deletes client from all relevant groups
    {
        groups[groupName].erase(clientName);
        if (groups[groupName].size() < 2) //deletes group if now contains only 1 member
        {
            for (auto member : groups[groupName]) //deletes group from all clients' structs
                clients[member].groups.erase(groupName);
            groups.erase(groupName);
        }
    }

    clientToDelete.insert(clientName); //adds the exiting client to clientsToDelete set
    //send success msg to sender
    char ack[WA_MAX_MESSAGE + 1] = {0};
    char success_msg[] = "SUCCESS";
    strncpy(ack, success_msg, strlen(success_msg));
    if (server_write_data(tempClientFD, ack, WA_MAX_MESSAGE + 1) == -EXIT_FAILURE)
        print_error("write", errno);
    if (shutdown(tempClientFD, SHUT_RDWR) < 0)
        print_error("shutdown", errno);

    if (close(tempClientFD) < 0)
        print_error("close", errno);

    FD_CLR(tempClientFD, &clientFDs);
    print_exit(true, clientName);

}

void receive_command(const std::string& command, std::string& clientName, int clientFD)
{
    command_type commandT;
    std::string name;
    std::string message;
    std::vector<std::string> clients;

    // parses received command into: type, client/group name, message, vector of clients
    parse_command(command, commandT, name, message, clients);


    if (commandT == CREATE_GROUP)
    {
        create_group(name, clients, clientName);
    }
    else if (commandT == SEND)
    {
        send_command(name, message, clientName);
    }
    else if (commandT == WHO)
    {
        who_command(clientName);
    }
    else if (commandT == EXIT)
    {
        exit_command(clientName);
    }
}

// Accept a new client
void _accept_new_client(int server_fd){
    // Trying to accept connection
    int clientFD = 0;
    clientFD = accept(server_fd, nullptr, nullptr);  /* accept connection if there is one */
    if (clientFD < 0)
        print_error("accept", errno);

    // Reading client's name
    char clientName[WA_MAX_NAME + 1] = {0};
    if (server_read_data(clientFD, clientName, WA_MAX_NAME + 1) == -EXIT_FAILURE)
        print_error("read", errno);
    if (clients.count(clientName))
    {
        //send fail msg to sender
        char ack[WA_MAX_MESSAGE + 1] = {0};
        char fail_msg[] = "DUPLICATE";
        strncpy(ack, fail_msg, strlen(fail_msg));
        if (server_write_data(clientFD, ack, sizeof(ack)) == -EXIT_FAILURE)
            print_error("write", errno);
        if (shutdown(clientFD, SHUT_RDWR) < 0)
            print_error("shutdown", errno);

        if (close(clientFD) < 0)
            print_error("close", errno);

        FD_CLR(clientFD, &clientFDs);
    }
    else {
        // Printing upon successful connection
        print_connection_server(clientName);

        // Sending connection ack to client
        char ack[WA_MAX_MESSAGE + 1] = {0};
        char connected_msg[] = "CONNECTED";
        strncpy(ack, connected_msg, strlen(connected_msg));
        if (server_write_data(clientFD, ack, sizeof(ack)) == -EXIT_FAILURE)
            print_error("write", errno);
        // Adding the new client to our data structures
        ClientData clientData = {clientFD, clientName};
        clients[clientName] = clientData;
        FD_SET(clientFD, &clientFDs);
    }
}



// Handle server input
void _server_input(int server_fd){
    // handle exit request
    std::string cmd;
    getline(std::cin, cmd);
    if (cmd == "EXIT")
    {
        print_exit();
        for (auto client : clients) //closes all clients' sockets
        {

            //send SERVER_EXIT msg to client
            char ack[WA_MAX_MESSAGE + 1] = {0};
            char success_msg[] = "SERVER_EXIT";
            strncpy(ack, success_msg, strlen(success_msg));
            if (server_write_data(client.second.client_fd, ack, sizeof(ack)) == -EXIT_FAILURE)
                print_error("write", errno);

            if (shutdown(client.second.client_fd, SHUT_RDWR) < 0)
                print_error("shutdown", errno);

            if (close(client.second.client_fd) < 0)
                print_error("close", errno);

            FD_CLR(client.second.client_fd, &clientFDs);
        }
        clients.clear();
        groups.clear();
        clientToDelete.clear();
        if (shutdown(server_fd, SHUT_RDWR) < 0)
            print_error("shutdown", errno);

        if (close(server_fd) < 0)
            print_error("close", errno);

        FD_CLR(server_fd, &clientFDs);

        exit(0);
    } else
        print_invalid_input();
}

int main(int argc ,char *argv[])
{
    if (argc != ARG_NUM_FOR_SERVER)
        print_server_usage();
    
    char hostName[WA_MAX_NAME + 1];
    int server_fd;
    struct sockaddr_in sa;
    struct hostent* hp;
    int portNum = std::stoi(argv[1]);

    //hostnet initialization
    if (gethostname(hostName, WA_MAX_NAME) == -EXIT_FAILURE){
        print_error("gethostname", errno);
    }
    if ((hp = gethostbyname(hostName)) == nullptr) {
        print_error("gethostbyname", errno);
    }

    //sockaddr_in initialization
    memset(&sa, 0, sizeof(struct sockaddr_in));
    sa.sin_family = hp->h_addrtype;
    memcpy(&sa.sin_addr, hp->h_addr, hp->h_length);
    sa.sin_port = htons((unsigned short) portNum);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        print_error("socket", errno);

    if (bind(server_fd, (struct sockaddr*) &sa, sizeof(struct sockaddr_in)) < 0)
        print_error("bind", errno);

    if (listen(server_fd, MAX_CONNECTIONS) < 0)
        print_error("listen", errno);

    // Handling multiple clients and stdin input
    fd_set readFDs;
    FD_ZERO(&clientFDs);
    FD_SET(server_fd, &clientFDs);
    FD_SET(STDIN_FILENO, &clientFDs);

    while (true){
        FD_ZERO(&readFDs);
        readFDs = clientFDs;

        // waiting for activity from sockets
        if (select(MAX_CONNECTIONS + 1, &readFDs, nullptr, nullptr, nullptr) < 0){
            print_error("select", errno);
        }
        // if activity found in server's socket, its an incoming connection
        if (FD_ISSET(server_fd, &readFDs)){
            _accept_new_client(server_fd);
        }
        // if its not a new connection, it's an IO operation
        if (FD_ISSET(STDIN_FILENO, &readFDs)){
            _server_input(server_fd);
        }

        else{

            //read client's command
            for (auto client : clients)
            {

                if(FD_ISSET(client.second.client_fd, &readFDs))
                {
                    char clientCmd[WA_MAX_MESSAGE + 1] = {0};
                    server_read_data(client.second.client_fd, clientCmd, WA_MAX_MESSAGE + 1);
                    receive_command(clientCmd, client.second.name, client.second.client_fd);
                }
            }
            for (auto exitingClient : clientToDelete)
            {
                clients.erase(exitingClient);
            }
            clientToDelete.clear();
        }
    }

}