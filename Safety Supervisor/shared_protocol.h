/* shared_protocol.h
 *
 * UART frame format between the main MCU (STM32G0B1CBT6) and the safety
 * supervisor (STM32G030F6P6TR).
 *
 * *** ARCHITECTURE UPDATE ***
 * Gating is now done via two INDEPENDENTLY-driven physical pins into an
 * AND gate (MCU_GATE_ENABLE from the main MCU, SUPERVISOR_GATE_ENABLE
 * from the supervisor) rather than the supervisor alone deciding both
 * pins. This UART link is still valuable - it lets each chip see the
 * other's reasoning for cross-diagnostics/logging - but the "approved"
 * field in proto_super_to_main_t is no longer the sole gating
 * mechanism. Physical pin state is now the ground truth for whether
 * boost is actually active; this link is a secondary/diagnostic channel.
 *
 * *** NOT YET CONFIRMED ***
 * Framing/field meaning below is a proposal for review, not a locked
 * spec.
 */
#ifndef SHARED_PROTOCOL_H
#define SHARED_PROTOCOL_H

#include <stdint.h>
#include "fault_codes.h"

#define PROTO_FRAME_SYNC        0xA5u
#define PROTO_TX_PERIOD_MS      20u   /* PLACEHOLDER */
#define PROTO_LINK_TIMEOUT_MS   100u  /* PLACEHOLDER */

/* Main MCU -> Supervisor */
typedef struct __attribute__((packed)) {
    uint8_t  sync;          /* PROTO_FRAME_SYNC */
    uint8_t  seq;
    uint8_t  want_boost;    /* 0/1 - main MCU's own decision, also reflected
                              * directly on the MCU_GATE_ENABLE pin */
    uint16_t main_adc;
    uint16_t sub_adc;
    uint16_t can_value;
    uint8_t  can_fresh;
    uint8_t  checksum;
} proto_main_to_super_t;

/* Supervisor -> Main MCU */
typedef struct __attribute__((packed)) {
    uint8_t  sync;
    uint8_t  seq_echo;
    uint8_t  approved;         /* informational now - supervisor's own
                                 * verdict, also reflected directly on the
                                 * SUPERVISOR_GATE_ENABLE pin */
    uint8_t  supervisor_fault;
    uint8_t  checksum;
} proto_super_to_main_t;

uint8_t proto_checksum(const uint8_t *bytes, uint32_t len);

#endif /* SHARED_PROTOCOL_H */
