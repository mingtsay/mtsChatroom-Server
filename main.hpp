#ifndef __MAIN_HPP__
#define __MAIN_HPP__

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <conio.h>
#include <winsock2.h>
#include "mtcr_sck.hpp"

#define new_char(x) (char*)malloc((x)*sizeof(char))

#define MAX_CONNECTION 255

#define HANDLER_CLOSED    0
#define HANDLER_CONNECTED 1
#define HANDLER_EOF       2

#define WINSOCK_LASTERR_NONE   0
#define WINSOCK_LASTERR_WSA    1
#define WINSOCK_LASTERR_SOCK   2
#define WINSOCK_LASTERR_BIND   3
#define WINSOCK_LASTERR_LISTEN 4
#define WINSOCK_LASTERR_HANDLE 5

using namespace std;

class ChatroomSocket
{
private:
    struct MySocket
    {
        SOCKET the_socket;
        sockaddr_in information;
        int state;
    };
    WSADATA wsa_data;
    WORD socket_version;
    struct MySocket socket_listener, socket_handler[MAX_CONNECTION];
    u_long socket_mode;
    int last_error;
    char *tmp; // �Ω󱵦���ƼȦs
    void (*event_connected)(int);
    void (*event_data_arrival)(int, const char *);

    int get_free_handler()
    {
        int i;

        for(i = 0; i < MAX_CONNECTION; ++i)
        {
            if(socket_handler[i].state == HANDLER_CLOSED) // ���Ŷ��� Handler
            {
                return i;
            }
        }

        return -1; // �䤣��Ŷ��� Handler
    }

    void check_new_connection()
    {
        struct MySocket socket_tmp;
        int socket_tmp_info_size, handler_id;

        socket_tmp_info_size = sizeof(sockaddr_in);
        socket_tmp.the_socket = accept(socket_listener.the_socket, (sockaddr *)&socket_tmp.information, &socket_tmp_info_size);

        if(socket_tmp.the_socket != INVALID_SOCKET) // ���s���s�u
        {
            handler_id = get_free_handler(); // ��@�ӪŶ��� Handler
            if(handler_id == -1) // �S���Ŷ��� Handler �F
            {
                last_error = WINSOCK_LASTERR_HANDLE;
                return;
            }

            socket_handler[handler_id].the_socket = socket_tmp.the_socket;
            socket_handler[handler_id].information = socket_tmp.information;
            socket_handler[handler_id].state = HANDLER_CONNECTED;

            if(event_connected != NULL)
            {
                event_connected(handler_id); // �I�s�s�J�ƥ�
            }
        }
    }

    void check_data_arrival()
    {
        int i, data_arrival_size;

        for(i = 0; i < MAX_CONNECTION; ++i)
        {
            if(socket_handler[i].state != HANDLER_CLOSED) // �B�z�H�s�u���s�u
            {
                data_arrival_size = recv(socket_handler[i].the_socket, tmp, MTCR_MAX_SOCKET_LENGTH, 0);
                if(data_arrival_size == 0) // ���w�����s�u
                {
                    socket_handler[i].state = HANDLER_EOF; // �Хܬ�������
                }
                else if(data_arrival_size > 0) // ����ƶǶi��
                {
                    if(event_data_arrival != NULL)
                    {
                        event_data_arrival(i, tmp); // �I�s��ƶǤJ�ƥ�
                    }
                }
            }
        }
    }

    void check_closed_connection()
    {
        int i;

        for(i = 0; i < MAX_CONNECTION; ++i)
        {
            if(socket_handler[i].state == HANDLER_EOF) // �B�z���������s�u
            {
                closesocket(socket_handler[i].the_socket); // �����s�u
                socket_handler[i].state = HANDLER_CLOSED;
            }
        }
    }

    char *ip_to_string(u_long socket_ip)
    {
        char *string_ip = new char(16);
        u_long tmp_ip = socket_ip;
        int a, b, c, d;

        a = tmp_ip & 0xFF;

        tmp_ip >>= 8;
        b = tmp_ip & 0xFF;

        tmp_ip >>= 8;
        c = tmp_ip & 0xFF;

        tmp_ip >>= 8;
        d = tmp_ip & 0xFF;

        sprintf(string_ip, "%d.%d.%d.%d", a, b, c, d);
        return string_ip;
    }

public:
    ChatroomSocket(int port, void (*custom_event_connected)(int), void (*custom_event_data_arrival)(int, const char *))
    {
        int i;

        last_error = WINSOCK_LASTERR_NONE;

        // ��l�� WSA
        socket_version = MAKEWORD(2, 2);
        if(WSAStartup(socket_version, &wsa_data) != 0) // ����
        {
            last_error = WINSOCK_LASTERR_WSA;
            return;
        }

        // ��l�� Listener Socket
        socket_listener.the_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if(socket_listener.the_socket == INVALID_SOCKET) // ����
        {
            last_error = WINSOCK_LASTERR_SOCK;
            WSACleanup();
            return;
        }

        // �]�w Listener ���ݩ�
        socket_listener.information.sin_family = AF_INET; // ���ں���
        socket_listener.information.sin_addr.s_addr = INADDR_ANY; // ����IP
        socket_listener.information.sin_port = htons(port); // �s�� Port

        // Bind port
        if(bind(socket_listener.the_socket, (SOCKADDR *)(&socket_listener.information), sizeof(sockaddr_in)) == SOCKET_ERROR) // ����
        {
            last_error = WINSOCK_LASTERR_BIND;
            WSACleanup();
            return;
        }

        // �}�l listen
        listen(socket_listener.the_socket, 1);
        socket_mode = 1; // �ҰʫD�P�B
        ioctlsocket(socket_listener.the_socket, FIONBIO, &socket_mode);

        // ��l�� Handler �����A
        for(i = 0; i < MAX_CONNECTION; ++i)
        {
            socket_handler[i].state = HANDLER_CLOSED;
        }

        // ��l�Ʊ�����ƥ��ܼ�
        tmp = new_char(MTCR_MAX_SOCKET_LENGTH + 1);

        // ���s�s�u�ɪ��ƥ�
        event_connected = custom_event_connected;

        // �������ƪ��B�z���
        event_data_arrival = custom_event_data_arrival;
    }

    int get_last_error()
    {
        int e = last_error;
        last_error = WINSOCK_LASTERR_NONE;
        return e;
    }

    void do_events()
    {
        check_new_connection();
        check_data_arrival();
        check_closed_connection();
    }

    int send_data(int handler_id, const char *data_to_send)
    {
        if(!(handler_id >= 0 && handler_id < MAX_CONNECTION)) return 0; // Handler �s���W�X�d��
        if(socket_handler[handler_id].state == HANDLER_CLOSED) return 0; // Handler ���ӴN�S���s�u
        send(socket_handler[handler_id].the_socket, data_to_send, strlen(data_to_send), 0); // �e���
        return 1;
    }

    int close_handler(int handler_id)
    {
        if(!(handler_id >= 0 && handler_id < MAX_CONNECTION)) return 0; // Handler �s���W�X�d��
        if(socket_handler[handler_id].state == HANDLER_CLOSED) return 0; // Handler ���ӴN�S���s�u
        closesocket(socket_handler[handler_id].the_socket); // �����s�u
        socket_handler[handler_id].state = HANDLER_CLOSED;
        return 1;
    }

    char *get_handler_ip(int handler_id)
    {
        if(!(handler_id >= 0 && handler_id < MAX_CONNECTION)) return NULL; // Handler �s���W�X�d��
        if(socket_handler[handler_id].state == HANDLER_CLOSED) return NULL; // Handler ���ӴN�S���s�u
        return ip_to_string(socket_handler[handler_id].information.sin_addr.s_addr);
    }

    ~ChatroomSocket()
    {
        closesocket(socket_listener.the_socket);
        WSACleanup();
    }
};

#endif
