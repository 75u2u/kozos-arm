#include "defines.h"
#include "kozos.h"
#include "interrupt.h"
#include "lib.h"

/* システム・タスクとユーザ・タスクの起動 */
static int start_threads(int argc, char *argv[])
{
  kz_run(consdrv_main, "consdrv",  1, 0x200, 0, NULL);
  kz_run(command_main, "command",  8, 0x200, 0, NULL);

  kz_chpri(15); /* 優先順位を下げて，アイドルスレッドに移行する */
//  INTR_ENABLE; /* 割込み有効にする */
 
  while (1) {
	asm volatile ("wfi"); /* 省電力モードに移行 */
  }
 
  return 0;
}

int main(void)
{
//  INTR_DISABLE; /* 割込み無効にする */

  static char buf[64]; // buf

  puts("kozos boot succeed!\n");

  while(1) {
    gets(buf);
    if(!strcmp(buf, "echo")) {
	puts("Yes\n");
    } else {
	puts("No\n");
    }
  }

  /* OSの動作開始 */
  kz_start(start_threads, "idle", 0, 0x100, 0, NULL);
  /* ここには戻ってこない */

  return 0;
}
