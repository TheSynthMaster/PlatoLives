/* z80user.h - Interfaccia standard Lin Ke-Fong per PlatoLives CDC IST-II */
#ifndef __Z80USER_INCLUDED__
#define __Z80USER_INCLUDED__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct plato_microtutor plato_microtutor_t;
typedef struct Z80_STATE Z80_STATE;

uint8_t plato_microtutor_fetch_byte(void *context, uint16_t address);
uint8_t plato_microtutor_read_byte(void *context, uint16_t address);
void plato_microtutor_write_byte(void *context, uint16_t address, uint8_t value);
uint8_t plato_microtutor_input_byte(void *context, uint8_t port);
void plato_microtutor_output_byte(void *context, uint8_t port, uint8_t value);

#define Z80_FETCH_BYTE(address, x) \
{ \
    (x) = plato_microtutor_fetch_byte(context, (uint16_t)(address)); \
}

#define Z80_READ_BYTE(address, x) \
{ \
    (x) = plato_microtutor_read_byte(context, (uint16_t)(address)); \
}

#define Z80_READ_WORD(address, x) \
{ \
    uint16_t _a = (uint16_t)(address); \
    (x) = (uint16_t)plato_microtutor_read_byte(context, _a) | \
          ((uint16_t)plato_microtutor_read_byte(context, (uint16_t)(_a + 1u)) << 8); \
}

#define Z80_FETCH_WORD(address, x) Z80_READ_WORD((address), (x))

#define Z80_WRITE_BYTE(address, x) \
{ \
    plato_microtutor_write_byte(context, (uint16_t)(address), (uint8_t)(x)); \
}

#define Z80_WRITE_WORD(address, x) \
{ \
    uint16_t _a = (uint16_t)(address); \
    plato_microtutor_write_byte(context, _a, (uint8_t)(x)); \
    plato_microtutor_write_byte(context, (uint16_t)(_a + 1u), (uint8_t)((x) >> 8)); \
}

#define Z80_READ_WORD_INTERRUPT(address, x) Z80_READ_WORD((address), (x))
#define Z80_WRITE_WORD_INTERRUPT(address, x) Z80_WRITE_WORD((address), (x))

#define Z80_INPUT_BYTE(port, x) \
{ \
    (x) = plato_microtutor_input_byte(context, (uint8_t)(port)); \
}

#define Z80_OUTPUT_BYTE(port, x) \
{ \
    plato_microtutor_output_byte(context, (uint8_t)(port), (uint8_t)(x)); \
}

#ifdef __cplusplus
}
#endif

#endif
