//
// Created by Ruben on 2024-10-28.
//

#include "tinyprotocol.h"

static const uint8_t *tlm_pointer[TINYPROTOCOL_MAX_CMD_NUM] = {NULL};
static uint8_t tlm_pointer_size[TINYPROTOCOL_MAX_CMD_NUM] = {0};
static uint8_t tlm_current_channel = 0;
static uint8_t tlm_buffer_idx = 0;

static uint8_t tc_size[TINYPROTOCOL_MAX_CMD_NUM] = {0};
static uint8_t tlcmd_buffer_idx = 0;
static uint8_t tlcmd_current = 0;

static uint8_t rx_buffer[TINYPROTOCOL_RX_BUFF_SIZE] = {0};
tinyprotocol_rx_fsm current_state = TINYPROTOCOL_FSM_IDLE;

static uint8_t tc_send_buffer_tmp[TINYPROTOCOL_MAX_PACKET_SIZE] = {0};

uint8_t calculate_crc(const uint8_t* buffer, uint8_t buffer_size) {
    uint8_t crc = crc_init_value;

    for (int i = 0; i < buffer_size; i++)
        crc = crc_lookup_table[buffer[i] ^ crc];

    return crc ^ crc_xor_value;
}

int16_t TINYPROTOCOL_ParseByte(const struct TINYPROTOCOL_Config *cfg, uint8_t byte) {
    switch(current_state) {
        case TINYPROTOCOL_FSM_IDLE:
            if ((byte & 0x80) != 0) {
                // Filter out the MSB
                tlm_current_channel = (byte & 0x7F);
                tlm_buffer_idx = 0;
                current_state = TINYPROTOCOL_FSM_TLM_REQ_RECEIVED;
            } else {
                tlcmd_current = byte;
                tlcmd_buffer_idx = 0;
                current_state = TINYPROTOCOL_FSM_TC_RECEIVED;

                rx_buffer[tlcmd_buffer_idx ++] = tlcmd_current;
            }
            break;
        case TINYPROTOCOL_FSM_TLM_REQ_RECEIVED:
            // If CRC is invalid, force TLM_ACK channel
            if (calculate_crc(&tlm_current_channel, 1) != byte)
                tlm_current_channel = 0;
            else
                current_state = TINYPROTOCOL_FSM_IDLE;
            break;
        case TINYPROTOCOL_FSM_TC_RECEIVED:
            if (tlcmd_buffer_idx == tc_size[tlcmd_current] - 1) {
                if (calculate_crc(rx_buffer, tc_size[tlcmd_current]) == byte) {
                    cfg->TINYPROTOCOL_ProcessTelecommand(rx_buffer[0], &rx_buffer[1], tc_size[tlcmd_current] - 2);
                }
            } else {
                rx_buffer[tlcmd_buffer_idx ++] = byte;
            }

            break;
    }

    return ETINYPROTOCOL_SUCCESS;
}

int16_t TINYPROTOCOL_RegisterTelecommand(uint8_t cmd, uint8_t size) {
    if(tc_size[cmd] > 0)
        return -ETINYPROTOCOL_TC_USED;

    tc_size[cmd] = size;
    return ETINYPROTOCOL_SUCCESS;
}

int16_t TINYPROTOCOL_SendTelecommand(const struct TINYPROTOCOL_Config *cfg, uint8_t command, const uint8_t* buffer, uint8_t size) {
    if (size > TINYPROTOCOL_MAX_PACKET_SIZE - 2)
        return -ETINYPROTOCOL_OVERFLOW;

    tc_send_buffer_tmp[0] = command;

    for(uint8_t i = 0; i < size; i ++) {
        tc_send_buffer_tmp[i+1] = buffer[i];
    }

    tc_send_buffer_tmp[size + 1] = calculate_crc(tc_send_buffer_tmp, size + 1);
    return cfg->TINYPROTOCOL_WriteBuffer(tc_send_buffer_tmp, size + 2);;
}

int16_t TINYPROTOCOL_RegisterTelemetryChannel(uint8_t tlm_channel, const uint8_t* ptr, uint8_t size) {
    if (tlm_channel > TINYPROTOCOL_MAX_CMD_NUM)
        return -ETINYPROTOCOL_TLM_INVALID_CHANNEL;

    if (tlm_channel == TINYPROTOCOL_TLM_ACK || tlm_pointer_size[tlm_channel] > 0)
        return -ETINYPROTOCOL_TLM_CHANNEL_USED;

    tlm_pointer[tlm_channel] = ptr;
    tlm_pointer_size[tlm_channel] = size;

    return ETINYPROTOCOL_SUCCESS;
}

int16_t TINYPROTOCOL_SendTelemetryRequest(const struct TINYPROTOCOL_Config *cfg, uint8_t tlm_req) {
    // Telemetry requests must have the most significant bit set to 1.s
    tlm_req |= 0x80;

    // Calculate crc and create buffer with proper content.
    uint8_t crc = calculate_crc(&tlm_req, 1);
    uint8_t buf[2] = {tlm_req, crc};

    return cfg->TINYPROTOCOL_WriteBuffer(buf, 2);
}

int16_t TINYPROTOCOL_ReadNextTelemetryByte(uint8_t *byte) {
    if(tlm_current_channel > TINYPROTOCOL_MAX_CMD_NUM)
        return -ETINYPROTOCOL_TLM_INVALID_CHANNEL;

    if (tlm_buffer_idx >= tlm_pointer_size[tlm_current_channel])
        return -ETINYPROTOCOL_OVERFLOW;

    *byte = tlm_pointer[tlm_current_channel][tlm_buffer_idx++];
    return ETINYPROTOCOL_SUCCESS;
}
