#ifndef K_OP_H
#define K_OP_H

// data type sizes
static const int
    k_byte_s = sizeof(char),
    k_short_s = sizeof(short),
    k_int_s = sizeof(int),
    k_long_s = sizeof(long),
    k_float_s = sizeof(float),
    k_double_s = sizeof(double);

// data type opcodes
#define K_TYPE_BYTE 0
#define K_TYPE_SHORT 1
#define K_TYPE_INT 2
#define K_TYPE_LONG 3
#define K_TYPE_BOOL 4
#define K_TYPE_FLOAT 5
#define K_TYPE_DOUBLE 6
#define K_TYPE_STRING 7

// data type opcode flags
#define K_TYPE_UNSIGNED 1
#define K_TYPE_DEFAULT 2

// command opcodes
#define K_CMD_GET 0 // SELECT
#define K_CMD_SET 1 // UPDATE
#define K_CMD_ADD 2 // INSERT
#define K_CMD_DEL 3 // DELETE
#define K_CMD_REM 4 // DROP
#define K_CMD_NEW 5 // CREATE

// sub command opcode
#define K_Q_WHERE 0 // WHERE str(x) comp(op) val(?)
#define K_Q_LIMIT 1 // LIMIT int(x)

#endif // K_OP_H