/*
 * RC-S660/S for Arduino
 * vtakaken
 */

#ifndef RCS660S_H_
#define RCS660S_H_

class RCS660S
{
public:
    RCS660S(Stream &serial = Serial1);

    int initDevice(void);
    int polling(uint16_t systemCode = 0xffff);
    int cardCommand(
        const uint8_t *command,
        uint8_t commandLen,
        uint8_t response[1024],
        uint8_t *responseLen);

    // private:
    Stream *_serial;

    uint8_t calcDCS(
        const uint8_t *data,
        uint16_t len);

    void writeSerial(
        const uint8_t *data,
        uint16_t len);

    int readSerial(
        uint8_t *data,
        uint16_t len);

    void flushSerial(void);

    int checkTimeout(unsigned long t0);

    void printHexArray(const uint8_t *data, size_t len);

    int send_ccid_command(
        const uint8_t *command,
        uint16_t command_len);

    int receive_ccid_response(
        uint8_t *response,
        uint16_t *response_len);

    int receive_ack(void);
    int abort_command(void);

    int write_apdu(const uint8_t *data, uint32_t data_len);

    int read_rapdu(uint8_t *data, uint32_t *data_len);

public:
    unsigned long timeout;
    uint8_t bseq;
    uint8_t idm[8];
    uint8_t pmm[8];
};

#endif