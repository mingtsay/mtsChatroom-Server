#include <cstdlib>
#include <cstring>
#include "mtcr_sck.hpp"

byte *createMTCRSocket(byte sckType, const char *sckData)
{
    byte *newMTCRSocket;
    int sckLength;

    sckLength = 6 + strlen(sckData);

    newMTCRSocket = (byte *)malloc((sckLength + 1) * sizeof(byte));

    newMTCRSocket[0] = 'M';
    newMTCRSocket[1] = 'T';
    newMTCRSocket[2] = 'C';
    newMTCRSocket[3] = 'R';

    newMTCRSocket[4] = MTCR_PROTOCOL_VERSION;

    newMTCRSocket[5] = sckType;

    memcpy(newMTCRSocket + 6, sckData, strlen(sckData));

    newMTCRSocket[sckLength] = '\0';

    return newMTCRSocket;
}
