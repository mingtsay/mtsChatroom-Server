#include "main.hpp"

#define CLIENT_STATE_CLOSED    0
#define CLIENT_STATE_CONNECTED 1
#define CLIENT_STATE_JOINED    2
#define CLIENT_STATE_PASSWORD  3

struct ClientData
{
    char nickname[256];
    char *ip;
    int state;
};

int key_status;
struct ClientData client_data[MAX_CONNECTION];
ChatroomSocket *my_socket;
const char *server_version = "1.0.0.0";
const char *client_version = "1.0.0.0";
const char *super_nickname = "mt";
const char *super_password = "lili40229";

int load_config(int);
void connected_event(int);
void data_arrival_event(int, const char *);
int keyboard_event();

int main()
{
    int i, listen_port = 83, is_running = 1;

    cout << "mt's Chatroom - Server" << endl;
    cout << "Copyright (c) 2010-2013 Ming Tsay. All rights reserved." << endl << endl;
    cout << "Keyboard events: Q = quit the program." << endl << endl;
    key_status = 0; // for easter egg

    cout << "Loading configure file..." << endl;
    listen_port = load_config(listen_port); // ���J�]�w��

    my_socket = new ChatroomSocket(listen_port, &connected_event, &data_arrival_event);

    if(my_socket->get_last_error() != WINSOCK_LASTERR_NONE) // �o�Ϳ��~
    {
        cout << "Error while initializing socket, perhaps the port " << listen_port << " is in use!" << endl;
        cout << "Server stopping..." << endl;
        return 0;
    }

    cout << "Server listened on port " << listen_port << "." << endl;

    for(i = 0; i < MAX_CONNECTION; ++i) // ��l�ƫȤ�ݸ��
    {
        client_data[i].state = CLIENT_STATE_CLOSED;
    }

    while(is_running) // �L���j�骽����L�ƥ�
    {
        my_socket->do_events();
        is_running = keyboard_event();
    }

    cout << "You pressed 'Q' for quit the program." << endl;
    cout << "Server is stopping..." << endl;

    delete my_socket;

    return 0;
}

int load_config(int default_port)
{
    int config_port = default_port;
    FILE *file_config = fopen("mtcrd.conf", "r");
    if(file_config) // �ɮצs�b
    {
        if(!feof(file_config)) fscanf(file_config, "%d", &config_port); // Ū���ɮ�
        fclose(file_config);
    }
    else // �ɮפ��s�b
    {
        file_config = fopen("mtcrd.conf", "w"); // �إ��ɮ�
        if(file_config)
        {
            fprintf(file_config, "%d", config_port); // �g�J�ɮ�
        }
        fclose(file_config);
    }

    if(config_port > 65535 || config_port < 1) // �ˬd port �i�Ω�
        return default_port;

    return config_port;
}

int nickname_exists(const char *string_nickname) // �ˬd�ʺ٬O�_�s�b
{
    int i;

    for(i = 0; i < MAX_CONNECTION; ++i)
    {
        if(client_data[i].state == CLIENT_STATE_JOINED)
        {
            if(strcmp(client_data[i].nickname, string_nickname) == 0) return 1; // ���ۦP��
        }
    }
    return 0; // �䤣��
}

void connected_event(int client_id) // �Ȥ�ݳs�W�F
{
    client_data[client_id].state = CLIENT_STATE_CONNECTED;
    client_data[client_id].ip = my_socket->get_handler_ip(client_id);
}

void data_arrival_event(int client_id, const char *string_data) // �Ȥ�ݶǸ�ƹL�ӤF
{
    int i, j;
    int type, type_send = -1;
    int client_length, server_length, nickname_length, password_length;
    int color_id, to_id, message_length, count_online, online_length;
    char *data, *string_send, *string_nickname, *string_password, *string_message;
    char *string_empty = new char(1); string_empty[0] = '\0';
    char string_unknown[] = "Sorry but I could not understand what you want to do.";

    if(parse_MTCR_socket(string_data, &type, &data)) // ���R���
    {
        switch(type)
        {
        case MTCR_CLIENT_GET_VERSION: // ���o����
            client_length = strlen(client_version);
            server_length = strlen(server_version);

            string_send = new char(client_length + server_length + 3);

            string_send[0] = client_length;
            string_send[client_length + 1] = server_length;

            memcpy(string_send + 1, client_version, client_length);
            memcpy(string_send + client_length + 2, server_version, server_length);

            string_send[client_length + server_length + 2] = '\0';

            type_send = MTCR_SERVER_VERSION;
            break;
        case MTCR_CLIENT_JOIN: // �[�J��ѫ�
            if(client_data[client_id].state == CLIENT_STATE_CONNECTED)
            {
                nickname_length = data[0];

                string_nickname = new char(nickname_length + 1);
                memcpy(string_nickname, data + 1, nickname_length);
                string_nickname[nickname_length] = '\0';

                if(nickname_exists(string_nickname)) // �ˬd�ʺ٦s�b
                {
                    type_send = MTCR_SERVER_JOIN_FAILED;
                    string_send = string_empty;

                    client_data[client_id].state = CLIENT_STATE_CONNECTED;
                }
                else if(strcmp(super_nickname, string_nickname) == 0) // �n�K�X���ʺ�
                {
                    type_send = MTCR_SERVER_PASSWORD_REQUIRED;
                    string_send = string_empty;

                    strcpy(client_data[client_id].nickname, string_nickname);
                    client_data[client_id].state = CLIENT_STATE_PASSWORD;
                }
                else // �i�ϥΪ��ʺ�
                {
                    strcpy(client_data[client_id].nickname, string_nickname);
                    client_data[client_id].state = CLIENT_STATE_JOINED;

                    string_send = new char(nickname_length + 3);
                    string_send[0] = client_id;
                    string_send[1] = nickname_length;
                    memcpy(string_send + 2, string_nickname, nickname_length);
                    string_send[nickname_length + 2] = '\0';

                    for(i = 0; i < MAX_CONNECTION; ++i) // �i�D�Ҧ��H���s�ϥΪ̥[�J
                    {
                        if(client_data[client_id].state == CLIENT_STATE_JOINED)
                        {
                            my_socket->send_data(i, create_MTCR_socket(MTCR_SERVER_USER_JOINED, string_send));
                        }
                    }

                    delete [] string_send;

                    type_send = MTCR_SERVER_JOIN_SUCCESSED;
                    string_send = string_empty;
                }

                delete [] string_nickname;
            }
            break;
        case MTCR_CLIENT_PASSWORD: // �n�J�K�X
            if(client_data[client_id].state == CLIENT_STATE_PASSWORD)
            {
                password_length = data[0];

                string_password = new char(password_length + 1);
                memcpy(string_password, data + 1, password_length);
                string_password[password_length] = '\0';

                if(strcmp(super_password, string_password) == 0)
                {
                    type_send = MTCR_SERVER_JOIN_SUCCESSED;
                    string_send = string_empty;

                    client_data[client_id].state = CLIENT_STATE_JOINED;
                }
                else
                {
                    type_send = MTCR_SERVER_PASSWORD_INCORRECT;
                    string_send = string_empty;
                }

                delete [] string_password;
            }
            break;
        case MTCR_CLIENT_CHANGE_NICKNAME: // ��ʺ�
            if(client_data[client_id].state == CLIENT_STATE_JOINED)
            {
                nickname_length = data[0];

                string_nickname = new char(nickname_length + 1);
                memcpy(string_nickname, data + 1, nickname_length);
                string_nickname[nickname_length] = '\0';

                if(nickname_exists(string_nickname) || strcmp(super_nickname, string_nickname) == 0) // �ʺ٤��i��
                {
                    type_send = MTCR_SERVER_CHANGE_NICKNAME_FAIL;
                    string_send = string_empty;
                }
                else // �ʺ٥i�ϥ�
                {
                    strcpy(client_data[client_id].nickname, string_nickname);

                    string_send = new char(nickname_length + 3);
                    string_send[0] = client_id;
                    string_send[1] = nickname_length;
                    memcpy(string_send + 2, string_nickname, nickname_length);
                    string_send[nickname_length + 2] = '\0';

                    for(i = 0; i < MAX_CONNECTION; ++i) // �i�D�Ҧ��H�L��s�ʺ�
                    {
                        if(client_data[client_id].state == CLIENT_STATE_JOINED)
                        {
                            my_socket->send_data(i, create_MTCR_socket(MTCR_SERVER_USER_CHANGE_NICKNAME, string_send));
                        }
                    }

                    delete [] string_send;

                    type_send = MTCR_SERVER_CHANGE_NICKNAME_OKAY;
                    string_send = string_empty;
                }

                delete [] string_nickname;
            }
            break;
        case MTCR_CLIENT_SEND_MESSAGE: // �e�T��
            if(client_data[client_id].state == CLIENT_STATE_JOINED)
            {
                color_id = data[0];
                message_length = (data[1] << 8) + data[2];

                string_message = new char(message_length + 1);
                memcpy(string_message, data + 3, message_length);
                string_message[message_length] = '\0';

                string_send = new char(message_length + 5);
                string_send[0] = client_id;
                string_send[1] = color_id;
                string_send[2] = (message_length >> 8) & 0xFF;
                string_send[3] = message_length & 0xFF;
                memcpy(string_send + 4, string_message, message_length);
                string_send[message_length + 4] = '\0';

                for(i = 0; i < MAX_CONNECTION; ++i) // �ǰT�����C�ӤH
                {
                    if(client_data[i].state == CLIENT_STATE_JOINED)
                    {
                        my_socket->send_data(i, create_MTCR_socket(MTCR_SERVER_MSG_SEND, string_send));
                    }
                }

                delete [] string_send;
            }
            break;
        case MTCR_CLIENT_SEND_PRIVATE: // �Ǩp�H�T��
            if(client_data[client_id].state == CLIENT_STATE_JOINED)
            {
                color_id = data[0];
                to_id = data[1];
                message_length = (data[2] << 8) + data[3];

                if(client_data[to_id].state == CLIENT_STATE_JOINED) // �i�H��
                {
                    string_message = new char(message_length + 1);
                    memcpy(string_message, data + 4, message_length);
                    string_message[message_length] = '\0';

                    string_send = new char();
                    my_socket->send_data(to_id, create_MTCR_socket(MTCR_SERVER_MSG_SEND_PRIVATE, string_send));
                    delete [] string_send;

                    type_send = MTCR_SERVER_MSG_SEND_PRIVATE_OKAY;
                    string_send = string_empty;
                }
                else // �����
                {
                    type_send = MTCR_SERVER_MSG_SEND_PRIVATE_FAILED;
                    string_send = string_empty;
                }
            }
            break;
        case MTCR_CLIENT_GET_ONLINE_LIST: // ���o�u�W�ϥΪ̲M��
            if(client_data[client_id].state == CLIENT_STATE_JOINED)
            {
                // �p��H���٦��ʺ٪���
                count_online = 0;
                online_length = 1;
                for(i = 0; i < MAX_CONNECTION; ++i)
                {
                    if(client_data[i].state == CLIENT_STATE_JOINED)
                    {
                        ++count_online;
                        online_length += 2 + strlen(client_data[i].nickname);
                    }
                }

                // �ǳƦr��
                string_send = new char(online_length + 1);
                string_send[0] = count_online;

                // �ǳƲM��
                for(i = 0, j = 1; i < MAX_CONNECTION; ++i)
                {
                    if(client_data[i].state == CLIENT_STATE_JOINED)
                    {
                        string_send[j++] = i;
                        string_send[j++] = strlen(client_data[i].nickname);
                        memcpy(string_send + j, client_data[i].nickname, strlen(client_data[i].nickname));
                        j += strlen(client_data[i].nickname);
                    }
                }

                type_send = MTCR_SERVER_ONLINE_LIST;
            }
            break;
        }
    }

    delete [] data;

    if(type_send == -1)
    {
        // �Ȥ�ݶǤF���A���ݤ������T��
        type_send = MTCR_SERVER_CANNOT_UNDERSTAND;
        string_send = string_unknown;
    }

    // �ǰe�T��
    my_socket->send_data(client_id, create_MTCR_socket(type_send, string_send));
    delete [] string_send;
}

int keyboard_event()
{
    char keypress;

    if(kbhit()) // ��L�Q���U�F
    {
        keypress = getch();
        if(keypress >= 'a' && keypress <= 'z') keypress -= 'a' - 'A'; // ��j�g

        if(keypress == 'Q') return 0; // �����{��

        switch(key_status) // for easter egg
        {
        case 0:
            if(keypress == 'I') key_status++; else key_status = 0; break;
        case 1:
            if(keypress == 'L') key_status++; else key_status = 0; break;
        case 2:
            if(keypress == 'O') key_status++; else key_status = 0; break;
        case 3:
            if(keypress == 'V') key_status++; else key_status = 0; break;
        case 4:
            if(keypress == 'E') key_status++; else key_status = 0; break;
        case 5:
            if(keypress == 'M') key_status++; else key_status = 0; break;
        case 6:
            if(keypress == 'T') cout << "I love you, too! <3" << endl;
        default:
            key_status = 0;
        }
    }
    return 1;
}
