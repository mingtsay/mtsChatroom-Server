#include <cstdlib>
#include <cstring>
#include "mtcr_sck.hpp"

char *create_MTCR_socket(int socket_type, const char *socket_data)
{
    char *new_MTCR_socket;
    int socket_length;

    socket_length = 6 + strlen(socket_data);

    new_MTCR_socket = (char *)malloc((socket_length + 1) * sizeof(byte));

    new_MTCR_socket[0] = 'M';
    new_MTCR_socket[1] = 'T';
    new_MTCR_socket[2] = 'C';
    new_MTCR_socket[3] = 'R';

    new_MTCR_socket[4] = MTCR_PROTOCOL_VERSION;

    new_MTCR_socket[5] = socket_type;

    memcpy(new_MTCR_socket + 6, socket_data, strlen(socket_data));

    new_MTCR_socket[socket_length] = '\0';

    return new_MTCR_socket;
}

int parse_MTCR_socket(const char *socket_data, int *type, char **data)
{
    int data_length = strlen(socket_data) - 6;

    *data = (char *)malloc((data_length + 1) * sizeof(char));

    if(
       socket_data[0] == 'M' &&
       socket_data[1] == 'T' &&
       socket_data[2] == 'C' &&
       socket_data[3] == 'R' &&
       socket_data[4] == MTCR_PROTOCOL_VERSION
       )
    {
        *type = socket_data[5];
        memcpy(*data, socket_data + 6, data_length);
        (*data)[data_length] = '\0';

        return 1;
    }
    return 0;
}
