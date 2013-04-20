#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <conio.h>
#include <winsock2.h>
#include "mtcr_sck.hpp"

#define MAX_CONNECTION 255

#define HANDLER_CLOSED    0
#define HANDLER_CONNECTED 1
#define HANDLER_JOINED    2
#define HANDLER_PASSWORD  3
#define HANDLER_EOF       4

using namespace std;

WSADATA wDta;
WORD sckVersion;
int sckHandlerInUse[MAX_CONNECTION], sizeNew;
SOCKET sckListener, sckHandler[MAX_CONNECTION], sckNew;
sockaddr_in sckInfo, sckNewInfo;
u_long sckMode;
int keyStatus;
char lstNickname[MAX_CONNECTION][256];

const char *verServer = "1.0.0.0";
const char *verClient = "1.0.0.0";
const char *pwreqNick = "mt";
const char *password = "lili40229";

char *ipToString(u_long sckIP)
{
    char *strIP = (char *)malloc(16 * sizeof(char));
    u_long tmpIP = sckIP;
    int a, b, c, d;

    a = tmpIP & 0xFF;

    tmpIP >>= 8;
    b = tmpIP & 0xFF;

    tmpIP >>= 8;
    c = tmpIP & 0xFF;

    tmpIP >>= 8;
    d = tmpIP & 0xFF;

    sprintf(strIP, "%d.%d.%d.%d", a, b, c, d);
    return strIP;
}

int loadConfig(int);
int initSocket(int);
void cleanupSocket();

void handleNewConnections();
void handleDataArrivals();
void handleSocketCleanup();
int handleKeyboardEvents();

int main()
{
    int listenPort = 83;
    int isRunning = 1;

    cout << "mt's Chatroom - Server" << endl;
    cout << "Copyright (c) 2010-2013 Ming Tsay. All rights reserved." << endl << endl;
    cout << "Keyboard events: q = quit the program." << endl << endl;
    keyStatus = 0; // for easter egg

    cout << "Loading configure file..." << endl;
    listenPort = loadConfig(listenPort);

    if(initSocket(listenPort) != 0)
    {
        cout << "Error while initializing socket, maybe port " << listenPort << " is in use!" << endl;
        cout << "Server stopping..." << endl;
        return 0;
    }

    cout << "Server listen on port " << listenPort << "." << endl;

    while(isRunning)
    {
        handleNewConnections();
        handleDataArrivals();
        handleSocketCleanup();
        isRunning = handleKeyboardEvents();
    }

    cout << "Server is stopping..." << endl;

    cleanupSocket();

    return 0;
}

int loadConfig(int defaultPort)
{
    FILE *fileConfig = fopen("mtcrd.conf", "r");
    if(fileConfig)
    {
        if(!feof(fileConfig)) fscanf(fileConfig, "%d", &defaultPort);
    }
    else
    {
        FILE *fileConfig = fopen("mtcrd.conf", "w");
        if(fileConfig)
        {
            fprintf(fileConfig, "%d", defaultPort);
        }
    }

    return defaultPort;
}

int initSocket(int p)
{
    int numError, i;
    sckVersion = MAKEWORD(2, 2);

    for(i = 0; i < MAX_CONNECTION; ++i) sckHandlerInUse[i] = HANDLER_CLOSED;

    numError = WSAStartup(sckVersion, &wDta);
    if(numError != 0) return -1;

    sckListener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sckListener == INVALID_SOCKET)
    {
        WSACleanup();
        return -1;
    }

    sckInfo.sin_family = AF_INET;
    sckInfo.sin_addr.s_addr = INADDR_ANY;
    sckInfo.sin_port = htons(p);

    if(bind(sckListener, (SOCKADDR *)(&sckInfo), sizeof(sckInfo)) == SOCKET_ERROR)
    {
        WSACleanup();
        return -1;
    }

    listen(sckListener, 1);

    sckMode = 1; // Enable non-blocking mode.
    ioctlsocket(sckListener, FIONBIO, &sckMode);

    return 0;
}

void cleanupSocket()
{
    closesocket(sckListener);
    WSACleanup();
}

int getFreeHandler()
{
    int i;

    for(i = 0; i < MAX_CONNECTION; ++i)
    {
        if(sckHandlerInUse[i] == HANDLER_CLOSED)
        {
            sckHandlerInUse[i] = HANDLER_CONNECTED;
            return i;
        }
    }

    return -1;
}

void freeHandler(int i)
{
    if(i >= 0 && i < MAX_CONNECTION) sckHandlerInUse[i] = HANDLER_CLOSED;
}

/** handling functions **/

void handleNewConnections()
{
    int handlerID;
    char *clientIP;

    sizeNew = sizeof(sockaddr_in);
    sckNew = accept(sckListener, (sockaddr *)&sckNewInfo, &sizeNew);

    if(sckNew != INVALID_SOCKET)
    {
        // A client connected.

        handlerID = getFreeHandler();
        if(handlerID == -1)
        {
            cout << "No free handler for new connections! Maximum: " << MAX_CONNECTION << endl;
            return;
        }

        sckHandler[handlerID] = sckNew;

        clientIP = ipToString(sckNewInfo.sin_addr.s_addr);
        cout << "A connection was accepted and assigned ID " << handlerID << ". IP: " << clientIP << endl;
    }
}

int isNicknameExists(const char *strNickname)
{
    int i;

    for(i = 0; i < MAX_CONNECTION; ++i)
    {
        if(sckHandlerInUse[i] == HANDLER_JOINED)
        {
            if(strcmp(strNickname, lstNickname[i]) == 0) return 1;
        }
    }
    return 0;
}

void sendSocket(byte sckType, const char *sckData, SOCKET sckClient)
{
    byte *sckSend = createMTCRSocket(sckType, sckData);
    printf("Send socket type %02X.\n", sckType);
    send(sckClient, (char *)sckSend, strlen((char *)sckSend), 0);
}

void handleDataArrivals()
{
    int i, j, k;
    int lenReceived, onlineCount, intColorID, intToID;
    byte strReceived[MTCR_MAX_SOCKET_LENGTH];
    int lenClient, lenServer, lenNickname, lenPassword, lenMessage, lenOnlineList;
    char *strSend, *strNickname, *strPassword, *strMessage, *strOnlineList;

    for(i = 0; i < MAX_CONNECTION; ++i)
    {
        if(sckHandlerInUse[i] != HANDLER_CLOSED)
        {
            lenReceived = recv(sckHandler[i], (char *)(&strReceived), MTCR_MAX_SOCKET_LENGTH, 0);
            if(lenReceived > 0)
            {
                cout << "Client sent " << lenReceived << " bytes to server." << endl;
                if(
                   strReceived[0] == 'M' &&
                   strReceived[1] == 'T' &&
                   strReceived[2] == 'C' &&
                   strReceived[3] == 'R' &&
                   strReceived[4] == MTCR_PROTOCOL_VERSION
                   )
                {
                    cout << "Client sent a MTCR socket." << endl;
                    switch(strReceived[5])
                    {
                    case MTCR_CLIENT_GET_VERSION:
                        cout << "Client send: Get Version" << endl;

                        lenClient = strlen(verClient);
                        lenServer = strlen(verServer);
                        strSend = (char *)malloc((lenClient + lenServer + 5) * sizeof(char));

                        strSend[0] = lenClient & 0xFF;
                        strSend[lenClient + 1] = lenServer & 0xFF;

                        memcpy(strSend + 1, verClient, lenClient);
                        memcpy(strSend + 2 + lenClient, verServer, lenServer);
                        strSend[lenClient + lenServer + 2] = '\0';

                        sendSocket(MTCR_SERVER_VERSION, strSend, sckHandler[i]);
                        continue;
                    case MTCR_CLIENT_JOIN:
                        cout << "Client send: Join" << endl;
                        if(sckHandlerInUse[i] == HANDLER_CONNECTED)
                        {
                            lenNickname = (int)strReceived[6];
                            strNickname = (char *)malloc((lenNickname + 1) * sizeof(char));
                            memcpy(strNickname, strReceived + 7, lenNickname);
                            strNickname[lenNickname] = '\0';

                            if(isNicknameExists(strNickname))
                            {
                                sendSocket(MTCR_SERVER_JOIN_FAILED, "", sckHandler[i]);
                            }
                            else if(strcmp(strNickname, pwreqNick) == 0)
                            {
                                sendSocket(MTCR_SERVER_PASSWORD_REQUIRED, "", sckHandler[i]);
                            }
                            else
                            {
                                sckHandlerInUse[i] = HANDLER_JOINED;
                                strcpy(lstNickname[i], strNickname);
                                sendSocket(MTCR_SERVER_JOIN_SUCCESSED, "", sckHandler[i]);
                            }

                            continue;
                        }
                        break;
                    case MTCR_CLIENT_PASSWORD:
                        cout << "Client send: Password" << endl;
                        if(sckHandlerInUse[i] == HANDLER_PASSWORD)
                        {
                            lenPassword = (int)strReceived[6];
                            strPassword = (char *)malloc((lenPassword + 1) * sizeof(char));
                            memcpy(strPassword, strReceived + 7, lenPassword);
                            strPassword[lenPassword] = '\0';

                            if(strcmp(strPassword, password) == 0)
                            {
                                sckHandlerInUse[i] = HANDLER_JOINED;
                                strcpy(lstNickname[i], pwreqNick);
                                sendSocket(MTCR_SERVER_JOIN_SUCCESSED, "", sckHandler[i]);
                            }
                            else
                            {
                                sendSocket(MTCR_SERVER_PASSWORD_INCORRECT, "", sckHandler[i]);
                            }

                            continue;
                        }
                        break;
                    case MTCR_CLIENT_CHANGE_NICKNAME:
                        cout << "Client send: Change Nickname" << endl;
                        if(sckHandlerInUse[i] == HANDLER_JOINED)
                        {
                            lenNickname = (int)strReceived[6];
                            strNickname = (char *)malloc((lenNickname + 1) * sizeof(char));
                            memcpy(strNickname, strReceived + 7, lenNickname);
                            strNickname[lenNickname] = '\0';

                            if(strcmp(strNickname, pwreqNick) == 0 || isNicknameExists(strNickname))
                            {
                                sendSocket(MTCR_SERVER_CHANGE_NICKNAME_FAIL, "", sckHandler[i]);
                            }
                            else
                            {
                                sckHandlerInUse[i] = HANDLER_JOINED;
                                strcpy(lstNickname[i], strNickname);
                                sendSocket(MTCR_SERVER_JOIN_SUCCESSED, "", sckHandler[i]);
                            }

                            continue;
                        }
                        break;
                    case MTCR_CLIENT_SEND_MESSAGE:
                        cout << "Client send: Send Message" << endl;
                        if(sckHandlerInUse[i] == HANDLER_JOINED)
                        {
                            intColorID = strReceived[6];

                            lenMessage = (strReceived[7] << 8) + strReceived[8];
                            strMessage = (char *)malloc((lenMessage + 1) * sizeof(char));
                            memcpy(strMessage, strReceived + 9, lenMessage);
                            strMessage[lenMessage] = '\0';

                            if(intColorID > 6 || intColorID < 3) intColorID = 4;
                            if(strcmp(lstNickname[i], pwreqNick) == 0) intColorID = 2;

                            strSend = (char *)malloc((5 + lenMessage + 1) * sizeof(char));

                            strSend[0] = i & 0xFF;
                            strSend[1] = intColorID & 0xFF;
                            strSend[2] = (lenMessage >> 8) & 0xFF;
                            strSend[3] = lenMessage & 0xFF;
                            memcpy(strSend + 4, strMessage, lenMessage);
                            strSend[5 + lenMessage] = '\0';

                            for(j = 0; j < MAX_CONNECTION; ++j)
                            {
                                if(sckHandlerInUse[j] == HANDLER_JOINED)
                                {
                                    sendSocket(MTCR_SERVER_MSG_SEND, strSend, sckHandler[j]);
                                }
                            }

                            continue;
                        }
                        break;
                    case MTCR_CLIENT_SEND_PRIVATE:
                        cout << "Client send: Send Private Message" << endl;
                        if(sckHandlerInUse[i] == HANDLER_JOINED)
                        {
                            intToID = strReceived[6];

                            if(sckHandlerInUse[intToID] != HANDLER_JOINED)
                            {
                                sendSocket(MTCR_SERVER_MSG_SEND_PRIVATE_FAILED, "", sckHandler[i]);
                                continue;
                            }

                            intColorID = strReceived[7];

                            lenMessage = (strReceived[8] << 8) + strReceived[9];
                            strMessage = (char *)malloc((lenMessage + 1) * sizeof(char));
                            memcpy(strMessage, strReceived + 10, lenMessage);
                            strMessage[lenMessage] = '\0';

                            if(intColorID > 6 || intColorID < 3) intColorID = 4;
                            if(strcmp(lstNickname[i], pwreqNick) == 0) intColorID = 2;

                            strSend = (char *)malloc((5 + lenMessage + 1) * sizeof(char));

                            strSend[0] = i & 0xFF;
                            strSend[1] = intToID & 0xFF;
                            strSend[2] = intColorID & 0xFF;
                            strSend[3] = (lenMessage >> 8) & 0xFF;
                            strSend[4] = lenMessage & 0xFF;
                            memcpy(strSend + 5, strMessage, lenMessage);
                            strSend[5 + lenMessage] = '\0';

                            sendSocket(MTCR_SERVER_MSG_SEND, strSend, sckHandler[intToID]);

                            continue;
                        }
                        break;
                    case MTCR_CLIENT_GET_ONLINE_LIST:
                        cout << "Client send: Get Online List" << endl;
                        if(sckHandlerInUse[i] == HANDLER_JOINED)
                        {
                            onlineCount = 0;
                            lenOnlineList = 1;

                            for(j = 0; j < MAX_CONNECTION; ++j)
                            {
                                if(sckHandlerInUse[j] == HANDLER_JOINED)
                                {
                                    ++onlineCount;
                                    lenOnlineList += 2 + strlen(lstNickname[j]);
                                }
                            }

                            strOnlineList = (char *)malloc((lenOnlineList + 1) * sizeof(char));

                            strOnlineList[0] = onlineCount & 0xFF;

                            for(j = 0, k = 1; j < MAX_CONNECTION; ++j)
                            {
                                if(sckHandlerInUse[j] == HANDLER_JOINED)
                                {
                                    strOnlineList[k++] = j & 0xFF;
                                    strOnlineList[k++] = strlen(lstNickname[j]) & 0xFF;
                                    memcpy(strOnlineList + k, lstNickname[j], strlen(lstNickname[j]));
                                    k += strlen(lstNickname[j]);
                                }
                            }

                            strOnlineList[k] = '\0';

                            sendSocket(MTCR_SERVER_ONLINE_LIST, strOnlineList, sckHandler[i]);

                            continue;
                        }
                        break;
                    }
                }
                else cout << "Client sent an non-MTCR socket!" << endl;

                sendSocket(MTCR_SERVER_CANNOT_UNDERSTAND, "Sorry but I could not understand what you want to do.", sckHandler[i]);
            }
            else if(lenReceived == 0)
            {
                sckHandlerInUse[i] = HANDLER_EOF;
            }
        }
    }
}

void handleSocketCleanup()
{
    int i;

    for(i = 0; i < MAX_CONNECTION; ++i)
    {
        if(sckHandlerInUse[i] == HANDLER_EOF)
        {
            cout << "Connection of " << i << " is closed." << endl;
            closesocket(sckHandler[i]);
            sckHandlerInUse[i] = HANDLER_CLOSED;
        }
    }
}

int handleKeyboardEvents()
{
    char chKey;
    if(kbhit())
    {
        chKey = getch();
        if(chKey >= 'A' && chKey <= 'Z') chKey += 'a' - 'A';

        if(chKey == 'q') return 0; // Quit the program.

        switch(keyStatus)
        {
        case 0:
            if(chKey == 'i') keyStatus++; else keyStatus = 0; break;
        case 1:
            if(chKey == 'l') keyStatus++; else keyStatus = 0; break;
        case 2:
            if(chKey == 'o') keyStatus++; else keyStatus = 0; break;
        case 3:
            if(chKey == 'v') keyStatus++; else keyStatus = 0; break;
        case 4:
            if(chKey == 'e') keyStatus++; else keyStatus = 0; break;
        case 5:
            if(chKey == 'm') keyStatus++; else keyStatus = 0; break;
        case 6:
            if(chKey == 't') cout << "I love you, too! <3" << endl;
            keyStatus = 0;
            break;
        default:
            keyStatus = 0;
        }
    }
    return 1;
}
