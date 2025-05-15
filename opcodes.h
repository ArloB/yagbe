#ifndef OPCODES_H
#define OPCODES_H

/**
 * @brief Executes a non-prefixed Game Boy opcode.
 * Fetches operands if necessary, performs the operation, updates flags,
 * and returns the number of CPU cycles taken by the instruction.
 * Handles instruction timing, immediate IME scheduling, and CB prefix.
 * @param op The 8-bit opcode to execute.
 * @return The number of T-cycles the instruction took.
 */
uint8_t executeOp(uint8_t op);

#endif