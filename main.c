#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <time.h>

#include "ffmpeg_slicer.h"
#include "ssd1306_i2c.h"

#define OLED_WIDTH  128
#define OLED_HEIGHT 64

typedef struct {
    Slicer *slicer;
    SSD1306Drive *driver;
    uint8_t buffer[1024];
    int scale_width;
    int scale_height;
} Memory;


uint8_t filtering_pixel(uint8_t pixel){
    if(pixel > 0xEF){
        return 1;
    }else if(pixel <= 0x10){
        return 0;
    }

    int v = rand() & 0xFF;
    if( v >= pixel){
        return 0;
    }else{
        return 1;
    }
}


int display_frame(void *pointer, uint8_t *buffer, int linesize){

    Memory *refs = (Memory*)pointer;

    if(refs->driver != NULL){
        memset(refs->buffer, 0, 1024);

        int l = (OLED_WIDTH - refs->scale_width) / 2;
        int t = (OLED_HEIGHT - refs->scale_height) / 2;
        int r = l + refs->scale_width;
        int b = t + refs->scale_height;

        uint8_t sp;
        int i, n, p, ps, py, x , y;
        for(p = 0; p < 8; p ++){
            ps = 8 * p;

            if(ps > b){
                break;
            }else if(ps + 8 < t){
                continue;
            }

            i = p * 128;
            for(py = 0; py < 8; py++){
                y = ps + py;

                if(y > b){
                    break;
                } else if( y < t){
                    continue;
                }

                n = y - t;
                for(x = l; x < r; x++){
                    sp = filtering_pixel(buffer[n * linesize + (x - l)]);
                    if( sp != 0){
                        refs->buffer[i + x] |= 1 << py;
                    }
                }
            }
        }

        refs->driver->swap(refs->driver, refs->buffer);
    }

    return 0;
}


int main(int argc, char **argv)
{
    if(argc != 3){
        fprintf(stderr, "Usage: %s <i2c_dev> <video_file>\n", argv[0]);
        return 1;
    }

    srand((unsigned int)time(NULL));

    Memory *refs = (Memory*)malloc(sizeof (Memory));
    refs->driver = ssd1306_i2c_new();
    refs->slicer = slicer_new();

    if(refs->driver->init(refs->driver, argv[1]) != 0){
        fprintf(stderr, "SSD1306 Drive init Failed!\n");
        goto END;
    }


    if(refs->slicer->init(refs->slicer, argv[2]) != 0){
        fprintf(stderr, "Slicer init Failed!\n");
        goto END;
    }


    double scale;
    if(refs->slicer->width > refs->slicer->height){
        scale = OLED_HEIGHT / (double)refs->slicer->height;
        refs->scale_width  = (int)(refs->slicer->width * scale);
        refs->scale_height = OLED_HEIGHT;
    }else{
        scale = OLED_WIDTH / (double)refs->slicer->width;
        refs->scale_width  = OLED_WIDTH;
        refs->scale_height = (int)(refs->slicer->height * scale);
    }

    snprintf(refs->slicer->command, sizeof(refs->slicer->command),
             "scale=%d:%d", refs->scale_width, refs->scale_height);

    if(refs->slicer->loop(refs->slicer, &display_frame, refs) != 0){
        fprintf(stderr, "Slicer parse video Failed!\n");
        goto END;
    }

END:
    if(refs->slicer != NULL){
        refs->slicer->free(refs->slicer);
    }

    if(refs->driver != NULL){
        refs->driver->free(refs->driver);
    }

    free(refs);

    return 0;
}
