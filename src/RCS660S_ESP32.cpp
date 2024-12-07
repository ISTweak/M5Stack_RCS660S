/*
 * RC-S660/S for Arduino
 * vtakaken
 */
#include <Arduino.h>
#include "RCS660S_ESP32.h"

RCS660S::RCS660S(Stream &serial)
{
    _serial = &serial;
    this->timeout = 1000;
    this->bseq = 0x0;
}

// RCS620S互換ラッパー
int RCS660S::cardCommand(
    const uint8_t *command,
    uint8_t command_len,
    uint8_t *response,
    uint8_t *response_len)
{
    uint8_t buf[1024];
    uint16_t buf_len;

    buf[0] = 0xFF; // CLA
    buf[1] = 0xC2; // INS
    buf[2] = 0x00; // P1
    buf[3] = 0x01; // P2
    buf[4] = 0x1A; // Lc
    // Data Object
    buf[5] = 0x5F; // Tag:Timer
    buf[6] = 0x46; // Tag:Timer
    buf[7] = 0x04; // Length
    buf[8] = 0x60; // Timer Value : 0xEA60 = 60000us = 60ms
    buf[9] = 0xEA;
    buf[10] = 0x01;
    buf[11] = 0x00;
    // Data Object
    buf[12] = 0x95;                       // Tag:Transceive
    buf[13] = (uint8_t)(command_len + 2); // Length
    buf[14] = (uint8_t)(command_len + 2); // Length(dummy)?
    memcpy(buf + 15, command, command_len);
    buf[15 + command_len] = 0x00;

    write_apdu(buf, 15 + command_len + 1);
    receive_ccid_response(buf, &buf_len);

    // CCID Header check
    if (buf[0] != 0x83 || buf[7] != 0x02)
    {
        Serial.print("Invalid CCID response message header.");
        return 0;
    }

    // PCSC Response Check
    if (memcmp(buf + 10, "\xC0\x03\x00\x90\x00", 5))
    {
        Serial.print("Invalid PCSC ManageSessionCommand Response.");
        return 0;
    }

    *response_len = (uint8_t)(buf[23] - 1);
    memcpy(response, buf + 25, *response_len);

    return 1;
}

int RCS660S::send_ccid_command(const uint8_t *command, uint16_t command_len)
{
    uint8_t dcs;
    uint8_t buf[290];

    dcs = calcDCS(command, command_len);
    buf[0] = 0x00; /* preamble */
    buf[1] = 0x00; /* start code */
    buf[2] = 0xff;
    buf[3] = (uint8_t)((command_len >> 8) & 0xff); /* LEN */
    buf[4] = (uint8_t)(command_len & 0xff);
    buf[5] = (uint8_t) - (buf[3] + buf[4]); /* LCS */
    memcpy(buf + 6, command, command_len);  /* PD0~PDn */
    buf[6 + command_len] = dcs;             /* DCS */
    buf[6 + command_len + 1] = 0x00;        /* postamble */
    writeSerial(buf, 6 + command_len + 2);
    return 1;
}

int RCS660S::receive_ccid_response(uint8_t *response, uint16_t *response_len)
{
    int rc;
    uint8_t buf[290];
    uint16_t buf_len;
    uint16_t pd_len;
    uint16_t need_len;

    /* read response command header */
    buf_len = 0;
    rc = readSerial(buf, 6);
    if (!rc)
    {
        Serial.println("uart read timeout");
        return 0;
    }

    /* check header */
    if (memcmp(buf, "\x00\x00\xff", 3) != 0)
    {
        Serial.println("Invalid response header");
        return 0;
    }
    else if (((buf[3] + buf[4] + buf[5]) & 0xff) != 0)
    {
        Serial.println("Invalid response header(LCS)");
        return 0;
    }

    pd_len = ((uint16_t)buf[3] << 8) + buf[4];
    if (pd_len > 282)
    {
        Serial.println("Too long response length");
        return 0;
    }

    need_len = pd_len + 2;
    readSerial(buf + 6, need_len);

    // TODO:checkは入れてない

    // ヘッダを除いてデータコピー
    memcpy(response, buf + 6, pd_len);
    *response_len = pd_len;

    Serial.printf("ccid_res(%d) : ", pd_len);
    printHexArray(response, pd_len);

    return 1;
}

int RCS660S::receive_ack()
{
    int rc;
    uint8_t ack_buf[7];
    rc = readSerial(ack_buf, 7);
    if (!rc)
    {
        Serial.println("Error read serial");
        return 0;
    }

    if (memcmp(ack_buf, "\x00\x00\xff\x00\x00\xff\x00", 7) != 0)
    {
        /* not ack */
        Serial.println("Not ACK");
        return 0;
    }

    return 1;
}

int RCS660S::abort_command()
{
    uint8_t abort[10]{
        0x72,                   /* bMessageType */
        0x00, 0x00, 0x00, 0x00, /* dwLength */
        0x00,                   /* bSlot */
        0x00,                   /* bSeq */
        0x00, 0x00, 0x00};      /* abRFU */
    uint8_t response_buf[1024];
    uint16_t response_len;

    abort[6] = ++bseq; // bSeq
    send_ccid_command(abort, sizeof(abort));
    receive_ack();
    receive_ccid_response(response_buf, &response_len);
    return 0;
}

int RCS660S::write_apdu(const uint8_t *data, uint32_t data_len)
{
    uint32_t buf_len;
    uint8_t buf[1024];

    /* make CCID message */
    buf[0] = 0x6b; /* bMessageType = PC_to_RDR_Escape */
    buf[1] = (uint8_t)(data_len & 0xff);
    buf[2] = (uint8_t)((data_len >> 8) & 0xff);
    buf[3] = (uint8_t)((data_len >> 16) & 0xff);
    buf[4] = (uint8_t)((data_len >> 24) & 0xff);
    buf[5] = 0x00;   /* bSlot */
    buf[6] = ++bseq; /* bSeq */
    buf[7] = 0x00;
    buf[8] = 0x00;
    buf[9] = 0x00;
    memcpy(&buf[10], data, data_len);
    buf_len = 10 + data_len;

    // Debug
    Serial.printf("ccid_com(%d) : ", buf_len);
    printHexArray(buf, buf_len);

    /* send CCID command*/
    send_ccid_command(buf, buf_len);
    receive_ack();

    return 0;
}

int RCS660S::read_rapdu(uint8_t *data, uint32_t *data_len)
{
    uint8_t buf[1024];
    uint16_t buf_len;

    receive_ccid_response(buf, &buf_len);

    return 0;
}

int RCS660S::initDevice(void)
{
    uint8_t buf[1024];
    uint16_t buf_len;

    // Reset
    Serial1.write(0x01);
    delay(20);

    // PC_to_RDR_Abort
    abort_command();

    // APDU : End Transparent Session
    write_apdu((const uint8_t *)"\xFF\xC2\x00\x00\x02\x82\x00", 7);
    receive_ccid_response(buf, &buf_len);

    // APDU : Start Transparent Session
    write_apdu((const uint8_t *)"\xFF\xC2\x00\x00\x02\x81\x00", 7);
    receive_ccid_response(buf, &buf_len);

    // APDU : Switch Protocol
    write_apdu((const uint8_t *)"\xFF\xC2\x00\x02\x04\x8F\x02\x03\x00", 9);
    receive_ccid_response(buf, &buf_len);

    // APDU : Transparent Exchange Transmission and Reception Flag
    write_apdu((const uint8_t *)"\xFF\xC2\x00\x01\x04\x90\x02\x00\x1C", 9);
    receive_ccid_response(buf, &buf_len);

    // APDU : Transparent Exchange Transmission Bit framing
    write_apdu((const uint8_t *)"\xFF\xC2\x00\x01\x03\x91\x01\x00", 8);
    receive_ccid_response(buf, &buf_len);

    // APDU : Manage Session Set Parameters
    write_apdu((const uint8_t *)"\xFF\xC2\x00\x00\x06\xFF\x6E\x03\x05\x01\x89", 11);
    receive_ccid_response(buf, &buf_len);

    // APDU Manage Session Turn On RF Field
    write_apdu((const uint8_t *)"\xFF\xC2\x00\x00\x02\x84\x00\x00", 8);
    receive_ccid_response(buf, &buf_len);

    return 1;
}

int RCS660S::polling(uint16_t systemCode)
{
    int ret;
    uint8_t command[] =
        {
            0xFF, // CLA
            0xC2, // INS
            0x00, // P1
            0x01, // P2
            0x0F, // Lc
            // Data Object
            0x5F, 0x46,             // Tag:Timer
            0x04,                   // Length
            0x60, 0xEA, 0x00, 0x00, // Timer Value
            // Data Object
            0x95,       // Tag:Transceive
            0x06, 0x06, // Length
            0x00,       // Command
            0xFF, 0xFF, // SystemCode
            0x00, 0x00};

    uint8_t buf[1024];
    uint16_t buf_len;

    command[16] = (uint8_t)((systemCode >> 8) & 0xff);
    command[17] = (uint8_t)((systemCode >> 0) & 0xff);
    write_apdu(command, sizeof(command));
    receive_ccid_response(buf, &buf_len);

    // no card
    if (buf_len != 44)
    {
        return 0;
    }

    memcpy(this->idm, buf + 26, 8);
    memcpy(this->pmm, buf + 34, 8);

    return 1;
}

uint8_t RCS660S::calcDCS(const uint8_t *data, uint16_t len)
{
    uint8_t sum = 0;
    uint16_t i;

    for (i = 0; i < len; i++)
    {
        sum += data[i];
    }

    return (uint8_t) - (sum & 0xff);
}

void RCS660S::writeSerial(
    const uint8_t *data,
    uint16_t len)
{
    _serial->write(data, len);

#ifdef UART_DEBUG
    Serial.print("WriteSerial : ");
    printHexArray(data, len);
#endif
}

int RCS660S::readSerial(
    uint8_t *data,
    uint16_t len)
{
    uint16_t nread = 0;
    unsigned long t0 = millis();

    while (nread < len)
    {
        if (checkTimeout(t0))
        {
            return 0;
        }

        if (_serial->available() > 0)
        {
            data[nread] = _serial->read();
            nread++;
        }
    }

#ifdef UART_DEBUG
    Serial.print("ReadSerial  : ");
    printHexArray(data, len);
#endif

    return 1;
}

void RCS660S::flushSerial(void)
{
    _serial->flush();
}

int RCS660S::checkTimeout(unsigned long t0)
{
    unsigned long t = millis();

    if ((t - t0) >= this->timeout)
    {
        return 1;
    }

    return 0;
}

void RCS660S::printHexArray(const uint8_t *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (data[i] < 0x10)
        {
            // 1桁の場合は先頭に0をつける
            Serial.print("0");
        }
        Serial.print(data[i], HEX); // 16進数で出力
        if (i < len - 1)
        {
            Serial.print(" "); // 最後の要素でなければスペースを出力
        }
    }
    Serial.println(); // 改行を追加
}
