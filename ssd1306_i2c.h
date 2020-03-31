#ifndef SSD1306_I2C_H
#define SSD1306_I2C_H

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int (*init)(void *self, const char *filename);
    int (*swap)(void *self, uint8_t *buffer);
    int (*free)(void *self);

    void *priv;
} SSD1306Drive;

SSD1306Drive *ssd1306_i2c_new(void);

#ifdef __cplusplus
}
#endif

#endif // SSD1306_I2C_H
