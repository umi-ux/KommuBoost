/* protocol.h
 *
 * UART frame format between the main MCU (STM32G0B1CBT6, runs the boost
 * decision logic) and the safety supervisor (STM32G030F6P6TR, physically
 * owns FORCE_PT/GATE_ENABLE and has final veto over boost).
 *
 * *** NOT YET CONFIRMED WITH TING ***
 * The schematic only shows a UART_TX/UART_RX wire between the two chips.
 * Everything below - framing, field meaning, timing - is a proposal for
 * review, not a locked spec. Flag this explicitly before relying on it.
 *
 * Design intent:
 *   - Main MCU periodically tells the supervisor what it sees and
 *     whether it currently wants boost.
 *   - Supervisor runs its OWN independent copy of the input checks
 *     (it has its own MAIN_ADC/SUB_ADC taps per the schematic) and only
 *     drives FORCE_PT/GATE_ENABLE high if it independently agrees.
 *   - Supervisor's reply carries its own fault code, so a
 *     supervisor-side fault surfaces in the main MCU's fault log too,
 *     instead of just silently denying boost.
 */
#ifndef SHARED_PROTOCOL_H
#define SHARED_PROTOCOL_H

#include <stdint.h>
#include "fault_codes.h"

#define PROTO_FRAME_SYNC        0xA5u
#define PROTO_TX_PERIOD_MS      20u   /* PLACEHOLDER - how often main MCU sends a status frame */
#define PROTO_LINK_TIMEOUT_MS   100u  /* PLACEHOLDER - no valid frame this long = link fault */

/* Main MCU -> Supervisor */
typedef struct __attribute__((packed)) {
    uint8_t  sync;          /* PROTO_FRAME_SYNC */
    uint8_t  seq;           /* increments every frame - lets supervisor spot drops/staleness */
    uint8_t  want_boost;    /* 0/1 - main MCU's decision, NOT yet approved */
    uint16_t main_adc;
    uint16_t sub_adc;
    uint16_t can_value;
    uint8_t  can_fresh;
    uint8_t  checksum;      /* 8-bit sum of all preceding bytes */
} proto_main_to_super_t;

/* Supervisor -> Main MCU */
typedef struct __attribute__((packed)) {
    uint8_t  sync;
    uint8_t  seq_echo;         /* echoes the seq it's responding to */
    uint8_t  approved;         /* 0/1 - supervisor's own checks + agreement to boost */
    uint8_t  supervisor_fault; /* fault_code_t, cast to uint8_t (0 = FAULT_NONE) */
    uint8_t  checksum;
} proto_super_to_main_t;

uint8_t proto_checksum(const uint8_t *bytes, uint32_t len);

#endif /* SHARED_PROTOCOL_H */
