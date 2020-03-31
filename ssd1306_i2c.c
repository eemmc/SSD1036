#include "ssd1306_i2c.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#define SSD1306_I2C_ADDR 0x3C
#define BUFFER_LENGTH 1024

typedef struct {
    int handle;
    uint8_t command[2];
    uint8_t mapping[BUFFER_LENGTH + 1];
    struct i2c_msg message;
    struct i2c_rdwr_ioctl_data packet;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    pthread_t thread;
    int finished;
} SSD1306DriveMemory;


int ssd1306_i2c_command(SSD1306DriveMemory *mem, uint8_t command, uint8_t value){

    mem->command[0] = command;
    mem->command[1] = value;

    mem->message.len = 2;
    mem->message.buf = mem->command;

    if(ioctl(mem->handle, I2C_RDWR, &mem->packet) < 0){
        perror("Cannot to send data");
        return -1;
    }

    return 0;
}


int ssd1306_i2c_write(SSD1306DriveMemory *mem, uint8_t *buffer, uint16_t size){
    mem->message.len = size;
    mem->message.buf = buffer;

    if(ioctl(mem->handle, I2C_RDWR, &mem->packet) < 0){
        perror("Cannot to send data");
        return -1;
    }

    return 0;
}



void* ssd1306_i2c_display(void *self){
    SSD1306DriveMemory *mem = (SSD1306DriveMemory*)self;

    int ret = 0;
    while(!mem->finished){
        pthread_mutex_lock(&mem->mutex);

        /*[列范围(0-127)]*/
        ret |= ssd1306_i2c_command(mem, 0x00, 0x21);
        ret |= ssd1306_i2c_command(mem, 0x00, 0x00);
        ret |= ssd1306_i2c_command(mem, 0x00, 0x7F);
        /*[行范围(0-7)]*/
        ret |= ssd1306_i2c_command(mem, 0x00, 0x22);
        ret |= ssd1306_i2c_command(mem, 0x00, 0x00);
        ret |= ssd1306_i2c_command(mem, 0x00, 0x07);
        /*[开始传连续数据]*/
        ret |= ssd1306_i2c_write(mem, mem->mapping, sizeof(mem->mapping));

        if(ret != 0){
            perror("Unable to send pixel data");
        }

        pthread_cond_wait(&mem->cond, &mem->mutex);
        pthread_mutex_unlock(&mem->mutex);
    }

    return NULL;
}

int ssd1306_i2c_swap(void *self, uint8_t *buffer){
    SSD1306Drive *drive = (SSD1306Drive*)self;
    SSD1306DriveMemory *mem = (SSD1306DriveMemory*)drive->priv;

    pthread_mutex_lock(&mem->mutex);

    if(buffer != NULL){
        memcpy(mem->mapping + 1, buffer, BUFFER_LENGTH);
    }

    pthread_cond_signal(&mem->cond);
    pthread_mutex_unlock(&mem->mutex);

    return 0;
}


int ssd1306_i2c_init(void *self, const char *filename){
    SSD1306Drive *drive = (SSD1306Drive*)self;
    SSD1306DriveMemory *mem = (SSD1306DriveMemory*)drive->priv;

    if((mem->handle = open(filename, O_RDWR)) < 0){
        perror("Cannot to open i2c control file");
        return -1;
    }

    int ret = 0;
    /* [关闭屏幕] */
    ret |= ssd1306_i2c_command(mem, 0x00, 0xAE);
    /* [刷新率(?列数)] */
    ret |= ssd1306_i2c_command(mem, 0x00, 0x5D);
    ret |= ssd1306_i2c_command(mem, 0x00, 0x80);
    /* [复用值(?行数)] */
    ret |= ssd1306_i2c_command(mem, 0x00, 0xA8);
    ret |= ssd1306_i2c_command(mem, 0x00, 0x3F);
    /*[显示偏移(0偏移)]*/
    ret |= ssd1306_i2c_command(mem, 0x00, 0xD3);
    ret |= ssd1306_i2c_command(mem, 0x00, 0x00);
    /*[起始行(0x40|行索引值)]*/
    ret |= ssd1306_i2c_command(mem, 0x00, 0x40 | 0x00);
    /*[内部电源稳压]*/
    ret |= ssd1306_i2c_command(mem, 0x00, 0x8D);
    ret |= ssd1306_i2c_command(mem, 0x00, 0x14);
    /*[内存模式(水平向右)]*/
    ret |= ssd1306_i2c_command(mem, 0x00, 0x20);
    ret |= ssd1306_i2c_command(mem, 0x00, 0x00);
    /*[内存映射模式]*/
    ret |= ssd1306_i2c_command(mem, 0x00, 0xA0 | 0x01);
    /*[水平方向扫描]*/
    ret |= ssd1306_i2c_command(mem, 0x00, 0xC8);
    /*[信号管脚配置]*/
    ret |= ssd1306_i2c_command(mem, 0x00, 0xDA);
    ret |= ssd1306_i2c_command(mem, 0x00, 0x12);
    /*[亮度设置]*/
    ret |= ssd1306_i2c_command(mem, 0x00, 0x81);
    ret |= ssd1306_i2c_command(mem, 0x00, 0xCF);
    /*[设置预充电周期的持续时间]*/
    ret |= ssd1306_i2c_command(mem, 0x00, 0xD9);
    ret |= ssd1306_i2c_command(mem, 0x00, 0xF1);
    /*[调整V COMH调节器输出]*/
    ret |= ssd1306_i2c_command(mem, 0x00, 0xDB);
    ret |= ssd1306_i2c_command(mem, 0x00, 0x40);
    /*[恢复显存显示]*/
    ret |= ssd1306_i2c_command(mem, 0x00, 0xA4);
    /*[正色显示|反色显示]*/
    ret |= ssd1306_i2c_command(mem, 0x00, 0xA6);
    /*[打开屏幕]*/
    ret |= ssd1306_i2c_command(mem, 0x00, 0xAF);

    if(ret != 0){
        perror("Unable to send header data");
        return -1;
    }

    pthread_create(&mem->thread, NULL, &ssd1306_i2c_display, mem);
    pthread_detach(mem->thread);

    return 0;
}


int ssd1306_i2c_free(void *self){
    SSD1306Drive *drive = (SSD1306Drive*)self;
    if(drive != NULL){
        SSD1306DriveMemory *mem;
        if(drive->priv != NULL){
            mem = (SSD1306DriveMemory*)drive->priv;

            mem->finished = 1;
            pthread_mutex_lock(&mem->mutex);
            pthread_cond_signal(&mem->cond);
            pthread_mutex_unlock(&mem->mutex);

            ssd1306_i2c_command(mem, 0x00, 0xAE);

            pthread_mutex_destroy(&mem->mutex);
            pthread_cond_destroy(&mem->cond);

            close(mem->handle);
            free(mem);
        }

        free(drive);
    }

    return 0;
}


SSD1306Drive *ssd1306_i2c_new(void){
    SSD1306Drive *drive;
    SSD1306DriveMemory *mem;

    drive = (SSD1306Drive*)malloc(sizeof (SSD1306Drive));
    memset(drive, 0, sizeof (SSD1306Drive));
    mem = (SSD1306DriveMemory*)malloc(sizeof (SSD1306DriveMemory));
    memset(mem, 0, sizeof (SSD1306DriveMemory));

    mem->mapping[0]    = 0x40;
    mem->message.addr  = SSD1306_I2C_ADDR;
    mem->message.flags = 0;
    mem->packet.msgs   = &mem->message;
    mem->packet.nmsgs  = 1;

    mem->finished = 0;
    pthread_mutex_init(&mem->mutex, NULL);
    pthread_cond_init(&mem->cond, NULL);

    drive->priv = mem;
    drive->init = &ssd1306_i2c_init;
    drive->swap = &ssd1306_i2c_swap;
    drive->free = &ssd1306_i2c_free;

    return drive;
}

