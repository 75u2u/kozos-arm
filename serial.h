#ifndef _SERIAL_H_INCLUDED_
#define _SERIAL_H_INCLUDED_

int serial_init(int index);                       /* ¥Ç¥Ð¥¤¥¹½é´ü²½ */
int serial_is_send_enable(int index);             /* Á÷¿®²ÄÇ½¤«¡© */
int serial_send_byte(int index, unsigned char b); /* £±Ê¸»úÁ÷¿® */
int serial_is_recv_enable(int index);             /* ¼õ¿®²ÄÇ½¤«¡© */
unsigned char serial_recv_byte(int index);        /* £±Ê¸»ú¼õ¿® */
int serial_intr_is_send_enable(int index);        /* Á÷¿®³ä¹þ¤ßÍ­¸ú¤«¡© */
void serial_intr_send_enable(int index);          /* Á÷¿®³ä¹þ¤ßÍ­¸ú²½ */
void serial_intr_send_disable(int index);         /* Á÷¿®³ä¹þ¤ßÌµ¸ú²½ */
int serial_intr_is_recv_enable(int index);        /* ¼õ¿®³ä¹þ¤ßÍ­¸ú¤«¡© */
void serial_intr_recv_enable(int index);          /* ¼õ¿®³ä¹þ¤ßÍ­¸ú²½ */
void serial_intr_recv_disable(int index);         /* ¼õ¿®³ä¹þ¤ßÌµ¸ú²½ */

/*GDBã‚¨ãƒŸãƒ¥ãƒ¬ãƒ¼ã‚¿å…¥å‡ºåŠ›*/
unsigned char sim_getc(void);
int sim_putc(unsigned char c);
#endif
