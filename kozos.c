#include "defines.h"
#include "kozos.h"
#include "intr.h"
#include "interrupt.h"
#include "syscall.h"
#include "memory.h"
#include "lib.h"

#define THREAD_NUM 6
#define PRIORITY_NUM 16
#define THREAD_NAME_SIZE 15

/* е╣еье├е╔бже│еєе╞ене╣е╚ */
typedef struct _kz_context {
  uint32 sp; /* е╣е┐е├епбже▌едеєе┐ */
} kz_context;

/* е┐е╣епбже│еєе╚еэб╝еыбже╓еэе├еп(TCB) */
typedef struct _kz_thread {
  struct _kz_thread *next;
  char name[THREAD_NAME_SIZE + 1]; /* е╣еье├е╔╠╛ */
  int priority;   /* ═е└ш┼┘ */
  char *stack;    /* е╣е┐е├еп */
  uint32 flags;   /* │╞╝яе╒еще░ */
#define KZ_THREAD_FLAG_READY (1 << 0)

  struct { /* е╣еье├е╔д╬е╣е┐б╝е╚бжеве├е╫(thread_init())д╦┼╧д╣е╤ещесб╝е┐ */
    kz_func_t func; /* е╣еье├е╔д╬еседеє┤╪┐Ї */
    int argc;       /* е╣еье├е╔д╬еседеє┤╪┐Їд╦┼╧д╣ argc */
    char **argv;    /* е╣еье├е╔д╬еседеє┤╪┐Їд╦┼╧д╣ argv */
  } init;

  struct { /* е╖е╣е╞ербже│б╝еы═╤е╨е├е╒еб */
    kz_syscall_type_t type;
    kz_syscall_param_t *param;
  } syscall;

  kz_context context; /* е│еєе╞ене╣е╚╛Ё╩є */
} kz_thread;

/* есе├е╗б╝е╕бже╨е├е╒еб */
typedef struct _kz_msgbuf {
  struct _kz_msgbuf *next;
  kz_thread *sender; /* есе├е╗б╝е╕дЄ┴ў┐од╖д┐е╣еье├е╔ */
  struct { /* есе├е╗б╝е╕д╬е╤ещесб╝е┐╩▌┬╕╬╬░ш */
    int size;
    char *p;
  } param;
} kz_msgbuf;

/* есе├е╗б╝е╕бже▄е├епе╣ */
typedef struct _kz_msgbox {
  kz_thread *receiver; /* ╝ї┐о┬╘д┴╛ї┬╓д╬е╣еье├е╔ */
  kz_msgbuf *head;
  kz_msgbuf *tail;

  /*
   * H8д╧16е╙е├е╚CPUд╩д╬д╟бд32е╙е├е╚└░┐Їд╦┬╨д╖д╞д╬╛ш╗╗╠┐╬сдм╠╡ддбедшд├д╞
   * ╣╜┬д┬╬д╬е╡еде║дмг▓д╬╬▀╛шд╦д╩д├д╞ддд╩ддд╚бд╣╜┬д┬╬д╬╟█╬єд╬едеєе╟е├епе╣
   * ╖╫╗╗д╟╛ш╗╗дм╗╚дядьд╞б╓___mulsi3дм╠╡ддб╫д╩д╔д╬еъеєепбжеиещб╝д╦д╩ды╛ь╣чдм
   * двдыбе(г▓д╬╬▀╛шд╩дщд╨е╖е╒е╚▒щ╗╗дм═°═╤д╡дьдыд╬д╟╠ф┬ъд╧╜╨д╩дд)
   * ┬╨║Ўд╚д╖д╞бде╡еде║дмг▓д╬╬▀╛шд╦д╩дыдшджд╦е└е▀б╝бжесеєе╨д╟─┤└░д╣дыбе
   * ┬╛╣╜┬д┬╬д╟╞▒══д╬еиещб╝дм╜╨д┐╛ь╣чд╦д╧бд╞▒══д╬┬╨╜шдЄд╣дыд│д╚бе
   */
  long dummy[1];
} kz_msgbox;

/* е╣еье├е╔д╬еье╟егб╝бженехб╝ */
static struct {
  kz_thread *head;
  kz_thread *tail;
} readyque[PRIORITY_NUM];

static kz_thread *current; /* елеьеєе╚бже╣еье├е╔ */
static kz_thread threads[THREAD_NUM]; /* е┐е╣епбже│еєе╚еэб╝еыбже╓еэе├еп */
static kz_handler_t handlers[SOFTVEC_TYPE_NUM]; /* │ф╣■д▀е╧еєе╔ещ */
static kz_msgbox msgboxes[MSGBOX_ID_NUM]; /* есе├е╗б╝е╕бже▄е├епе╣ */

//void dispatch(kz_context *context);

/* елеьеєе╚бже╣еье├е╔дЄеье╟егб╝бженехб╝длдщ╚┤дн╜╨д╣ */
static int getcurrent(void)
{
  if (current == NULL) {
    return -1;
  }
  if (!(current->flags & KZ_THREAD_FLAG_READY)) {
    /* д╣д╟д╦╠╡дд╛ь╣чд╧╠╡╗ы */
    return 1;
  }

  /* елеьеєе╚бже╣еье├е╔д╧╔мд║└ш╞мд╦двдыд╧д║д╩д╬д╟бд└ш╞мдлдщ╚┤дн╜╨д╣ */
  readyque[current->priority].head = current->next;
  if (readyque[current->priority].head == NULL) {
    readyque[current->priority].tail = NULL;
  }
  current->flags &= ~KZ_THREAD_FLAG_READY;
  current->next = NULL;

  return 0;
}

/* елеьеєе╚бже╣еье├е╔дЄеье╟егб╝бженехб╝д╦╖╥д▓ды */
static int putcurrent(void)
{
  if (current == NULL) {
    return -1;
  }
  if (current->flags & KZ_THREAD_FLAG_READY) {
    /* д╣д╟д╦═нды╛ь╣чд╧╠╡╗ы */
    return 1;
  }

  /* еье╟егб╝бженехб╝д╬╦Ў╚°д╦└▄┬│д╣ды */
  if (readyque[current->priority].tail) {
    readyque[current->priority].tail->next = current;
  } else {
    readyque[current->priority].head = current;
  }
  readyque[current->priority].tail = current;
  current->flags |= KZ_THREAD_FLAG_READY;

  return 0;
}

static void thread_end(void)
{
  kz_exit();
}

/* е╣еье├е╔д╬е╣е┐б╝е╚бжеве├е╫ */
static void thread_init(kz_thread *thp)
{
  /* е╣еье├е╔д╬еседеє┤╪┐ЇдЄ╕╞д╙╜╨д╣ */
  thp->init.func(thp->init.argc, thp->init.argv);
  thread_end();
}

/* е╖е╣е╞ербже│б╝еыд╬╜ш═¤(kz_run():е╣еье├е╔д╬╡п╞░) */
static kz_thread_id_t thread_run(kz_func_t func, char *name, int priority,
				 int stacksize, int argc, char *argv[])
{
  int i;
  kz_thread *thp;
  uint32 *sp;
  extern char userstack; /* еъеєелбже╣епеъе╫е╚д╟─ъ╡┴д╡дьдые╣е┐е├еп╬╬░ш */
  static char *thread_stack = &userstack;

  /* ╢їддд╞дддые┐е╣епбже│еєе╚еэб╝еыбже╓еэе├епдЄ╕б║ў */
  for (i = 0; i < THREAD_NUM; i++) {
    thp = &threads[i];
    if (!thp->init.func) /* ╕лд─длд├д┐ */
      break;
  }
  if (i == THREAD_NUM) /* ╕лд─длдщд╩длд├д┐ */
    return -1;

  memset(thp, 0, sizeof(*thp));

  /* е┐е╣епбже│еєе╚еэб╝еыбже╓еэе├еп(TCB)д╬└▀─ъ */
  strcpy(thp->name, name);
  thp->next     = NULL;
  thp->priority = priority;
  thp->flags    = 0;

  thp->init.func = func;
  thp->init.argc = argc;
  thp->init.argv = argv;

  /* е╣е┐е├еп╬╬░шдЄ│═╞└ */
  memset(thread_stack, 0, stacksize);
  thread_stack += stacksize;

  thp->stack = thread_stack; /* е╣е┐е├епдЄ└▀─ъ */

  /* е╣е┐е├епд╬╜щ┤№▓╜ */
  sp = (uint32 *)thp->stack;
  *(--sp) = (uint32)thread_end;

  /*
   * е╫еэе░ещербжележеєе┐дЄ└▀─ъд╣дыбе
   * е╣еье├е╔д╬═е└ш┼┘дме╝еэд╬╛ь╣чд╦д╧бд│ф╣■д▀╢╪╗▀е╣еье├е╔д╚д╣дыбе
   */
  *(--sp) = (uint32)thread_init | ((uint32)(priority ? 0 : 0xc0) << 24);

  *(--sp) = 0; /* ER6 */
  *(--sp) = 0; /* ER5 */
  *(--sp) = 0; /* ER4 */
  *(--sp) = 0; /* ER3 */
  *(--sp) = 0; /* ER2 */
  *(--sp) = 0; /* ER1 */

  /* е╣еье├е╔д╬е╣е┐б╝е╚бжеве├е╫(thread_init())д╦┼╧д╣░·┐Ї */
  *(--sp) = (uint32)thp;  /* ER0 */

  /* е╣еье├е╔д╬е│еєе╞ене╣е╚дЄ└▀─ъ */
  thp->context.sp = (uint32)sp;

  /* е╖е╣е╞ербже│б╝еыдЄ╕╞д╙╜╨д╖д┐е╣еье├е╔дЄеье╟егб╝бженехб╝д╦╠сд╣ */
  putcurrent();

  /* ┐╖╡м║ю└од╖д┐е╣еье├е╔дЄбдеье╟егб╝бженехб╝д╦└▄┬│д╣ды */
  current = thp;
  putcurrent();

  return (kz_thread_id_t)current;
}

/* е╖е╣е╞ербже│б╝еыд╬╜ш═¤(kz_exit():е╣еье├е╔д╬╜к╬╗) */
static int thread_exit(void)
{
  /*
   * ╦▄═шд╩дще╣е┐е├епдт▓Є╩№д╖д╞║╞═°═╤д╟дндыдшджд╦д╣д┘днд└дм╛╩╬мбе
   * д│д╬д┐дсбде╣еье├е╔дЄ╔╤╚╦д╦└╕└обж╛├╡юд╣дыдшджд╩д│д╚д╧╕╜╛їд╟д╟днд╩ддбе
   */
  puts(current->name);
  puts(" EXIT.\n");
  memset(current, 0, sizeof(*current));
  return 0;
}

/* е╖е╣е╞ербже│б╝еыд╬╜ш═¤(kz_wait():е╣еье├е╔д╬╝┬╣╘╕в╩№┤■) */
static int thread_wait(void)
{
  putcurrent();
  return 0;
}

/* е╖е╣е╞ербже│б╝еыд╬╜ш═¤(kz_sleep():е╣еье├е╔д╬е╣еъб╝е╫) */
static int thread_sleep(void)
{
  return 0;
}

/* е╖е╣е╞ербже│б╝еыд╬╜ш═¤(kz_wakeup():е╣еье├е╔д╬ежезедепбжеве├е╫) */
static int thread_wakeup(kz_thread_id_t id)
{
  /* ежезедепбжеве├е╫дЄ╕╞д╙╜╨д╖д┐е╣еье├е╔дЄеье╟егб╝бженехб╝д╦╠сд╣ */
  putcurrent();

  /* ╗╪─ъд╡дьд┐е╣еье├е╔дЄеье╟егб╝бженехб╝д╦└▄┬│д╖д╞ежезедепбжеве├е╫д╣ды */
  current = (kz_thread *)id;
  putcurrent();

  return 0;
}

/* е╖е╣е╞ербже│б╝еыд╬╜ш═¤(kz_getid():е╣еье├е╔ID╝ш╞└) */
static kz_thread_id_t thread_getid(void)
{
  putcurrent();
  return (kz_thread_id_t)current;
}

/* е╖е╣е╞ербже│б╝еыд╬╜ш═¤(kz_chpri():е╣еье├е╔д╬═е└ш┼┘╩╤╣╣) */
static int thread_chpri(int priority)
{
  int old = current->priority;
  if (priority >= 0)
    current->priority = priority; /* ═е└ш┼┘╩╤╣╣ */
  putcurrent(); /* ┐╖д╖дд═е└ш┼┘д╬еье╟егб╝бженехб╝д╦╖╥до─╛д╣ */
  return old;
}

/* е╖е╣е╞ербже│б╝еыд╬╜ш═¤(kz_kmalloc():╞░┼кесетеъ│═╞└) */
static void *thread_kmalloc(int size)
{
  putcurrent();
  return kzmem_alloc(size);
}

/* е╖е╣е╞ербже│б╝еыд╬╜ш═¤(kz_kfree():есетеъ▓Є╩№) */
static int thread_kmfree(char *p)
{
  kzmem_free(p);
  putcurrent();
  return 0;
}

/* есе├е╗б╝е╕д╬┴ў┐о╜ш═¤ */
static void sendmsg(kz_msgbox *mboxp, kz_thread *thp, int size, char *p)
{
  kz_msgbuf *mp;

  /* есе├е╗б╝е╕бже╨е├е╒ебд╬║ю└о */
  mp = (kz_msgbuf *)kzmem_alloc(sizeof(*mp));
  if (mp == NULL)
    kz_sysdown();
  mp->next       = NULL;
  mp->sender     = thp;
  mp->param.size = size;
  mp->param.p    = p;

  /* есе├е╗б╝е╕бже▄е├епе╣д╬╦Ў╚°д╦есе├е╗б╝е╕дЄ└▄┬│д╣ды */
  if (mboxp->tail) {
    mboxp->tail->next = mp;
  } else {
    mboxp->head = mp;
  }
  mboxp->tail = mp;
}

/* есе├е╗б╝е╕д╬╝ї┐о╜ш═¤ */
static void recvmsg(kz_msgbox *mboxp)
{
  kz_msgbuf *mp;
  kz_syscall_param_t *p;

  /* есе├е╗б╝е╕бже▄е├епе╣д╬└ш╞мд╦двдыесе├е╗б╝е╕дЄ╚┤дн╜╨д╣ */
  mp = mboxp->head;
  mboxp->head = mp->next;
  if (mboxp->head == NULL)
    mboxp->tail = NULL;
  mp->next = NULL;

  /* есе├е╗б╝е╕дЄ╝ї┐од╣дые╣еье├е╔д╦╩╓д╣├═дЄ└▀─ъд╣ды */
  p = mboxp->receiver->syscall.param;
  p->un.recv.ret = (kz_thread_id_t)mp->sender;
  if (p->un.recv.sizep)
    *(p->un.recv.sizep) = mp->param.size;
  if (p->un.recv.pp)
    *(p->un.recv.pp) = mp->param.p;

  /* ╝ї┐о┬╘д┴е╣еье├е╔д╧ддд╩дпд╩д├д┐д╬д╟бдNULLд╦╠сд╣ */
  mboxp->receiver = NULL;

  /* есе├е╗б╝е╕бже╨е├е╒ебд╬▓Є╩№ */
  kzmem_free(mp);
}

/* е╖е╣е╞ербже│б╝еыд╬╜ш═¤(kz_send():есе├е╗б╝е╕┴ў┐о) */
static int thread_send(kz_msgbox_id_t id, int size, char *p)
{
  kz_msgbox *mboxp = &msgboxes[id];

  putcurrent();
  sendmsg(mboxp, current, size, p); /* есе├е╗б╝е╕д╬┴ў┐о╜ш═¤ */

  /* ╝ї┐о┬╘д┴е╣еье├е╔дм┬╕║▀д╖д╞ддды╛ь╣чд╦д╧╝ї┐о╜ш═¤дЄ╣╘дж */
  if (mboxp->receiver) {
    current = mboxp->receiver; /* ╝ї┐о┬╘д┴е╣еье├е╔ */
    recvmsg(mboxp); /* есе├е╗б╝е╕д╬╝ї┐о╜ш═¤ */
    putcurrent(); /* ╝ї┐од╦дшдъ╞░║ю▓─╟╜д╦д╩д├д┐д╬д╟бде╓еэе├еп▓Є╜№д╣ды */
  }

  return size;
}

/* е╖е╣е╞ербже│б╝еыд╬╜ш═¤(kz_recv():есе├е╗б╝е╕╝ї┐о) */
static kz_thread_id_t thread_recv(kz_msgbox_id_t id, int *sizep, char **pp)
{
  kz_msgbox *mboxp = &msgboxes[id];

  if (mboxp->receiver) /* ┬╛д╬е╣еье├е╔дмд╣д╟д╦╝ї┐о┬╘д┴д╖д╞ддды */
    kz_sysdown();

  mboxp->receiver = current; /* ╝ї┐о┬╘д┴е╣еье├е╔д╦└▀─ъ */

  if (mboxp->head == NULL) {
    /*
     * есе├е╗б╝е╕бже▄е├епе╣д╦есе├е╗б╝е╕дм╠╡ддд╬д╟бде╣еье├е╔дЄ
     * е╣еъб╝е╫д╡д╗дыбе(е╖е╣е╞ербже│б╝еыдме╓еэе├епд╣ды)
     */
    return -1;
  }

  recvmsg(mboxp); /* есе├е╗б╝е╕д╬╝ї┐о╜ш═¤ */
  putcurrent(); /* есе├е╗б╝е╕дЄ╝ї┐од╟днд┐д╬д╟бдеье╟егб╝╛ї┬╓д╦д╣ды */

  return current->syscall.param->un.recv.ret;
}

/* е╖е╣е╞ербже│б╝еыд╬╜ш═¤(kz_setintr():│ф╣■д▀е╧еєе╔ещ┼╨╧┐) */
static int thread_setintr(softvec_type_t type, kz_handler_t handler)
{
  static void thread_intr(softvec_type_t type, unsigned long sp);

  /*
   * │ф╣■д▀дЄ╝їд▒╔╒д▒дыд┐дсд╦бде╜е╒е╚ежеиевбж│ф╣■д▀е┘епе┐д╦
   * OSд╬│ф╣■д▀╜ш═¤д╬╞■╕¤д╚д╩ды┤╪┐ЇдЄ┼╨╧┐д╣дыбе
   */
  softvec_setintr(type, thread_intr);

  handlers[type] = handler; /* OS┬ждлдщ╕╞д╙╜╨д╣│ф╣■д▀е╧еєе╔ещдЄ┼╨╧┐ */
  putcurrent();

  return 0;
}

static void call_functions(kz_syscall_type_t type, kz_syscall_param_t *p)
{
  /* е╖е╣е╞ербже│б╝еыд╬╝┬╣╘├цд╦currentдм╜ёдн┤╣дядыд╬д╟├э░╒ */
  switch (type) {
  case KZ_SYSCALL_TYPE_RUN: /* kz_run() */
    p->un.run.ret = thread_run(p->un.run.func, p->un.run.name,
			       p->un.run.priority, p->un.run.stacksize,
			       p->un.run.argc, p->un.run.argv);
    break;
  case KZ_SYSCALL_TYPE_EXIT: /* kz_exit() */
    /* TCBдм╛├╡юд╡дьдыд╬д╟бд╠сдъ├═дЄ╜ёдн╣■дєд╟д╧ддд▒д╩дд */
    thread_exit();
    break;
  case KZ_SYSCALL_TYPE_WAIT: /* kz_wait() */
    p->un.wait.ret = thread_wait();
    break;
  case KZ_SYSCALL_TYPE_SLEEP: /* kz_sleep() */
    p->un.sleep.ret = thread_sleep();
    break;
  case KZ_SYSCALL_TYPE_WAKEUP: /* kz_wakeup() */
    p->un.wakeup.ret = thread_wakeup(p->un.wakeup.id);
    break;
  case KZ_SYSCALL_TYPE_GETID: /* kz_getid() */
    p->un.getid.ret = thread_getid();
    break;
  case KZ_SYSCALL_TYPE_CHPRI: /* kz_chpri() */
    p->un.chpri.ret = thread_chpri(p->un.chpri.priority);
    break;
  case KZ_SYSCALL_TYPE_KMALLOC: /* kz_kmalloc() */
    p->un.kmalloc.ret = thread_kmalloc(p->un.kmalloc.size);
    break;
  case KZ_SYSCALL_TYPE_KMFREE: /* kz_kmfree() */
    p->un.kmfree.ret = thread_kmfree(p->un.kmfree.p);
    break;
  case KZ_SYSCALL_TYPE_SEND: /* kz_send() */
    p->un.send.ret = thread_send(p->un.send.id,
				 p->un.send.size, p->un.send.p);
    break;
  case KZ_SYSCALL_TYPE_RECV: /* kz_recv() */
    p->un.recv.ret = thread_recv(p->un.recv.id,
				 p->un.recv.sizep, p->un.recv.pp);
    break;
  case KZ_SYSCALL_TYPE_SETINTR: /* kz_setintr() */
    p->un.setintr.ret = thread_setintr(p->un.setintr.type,
				       p->un.setintr.handler);
    break;
  default:
    break;
  }
}

/* е╖е╣е╞ербже│б╝еыд╬╜ш═¤ */
static void syscall_proc(kz_syscall_type_t type, kz_syscall_param_t *p)
{
  /*
   * е╖е╣е╞ербже│б╝еыдЄ╕╞д╙╜╨д╖д┐е╣еье├е╔дЄеье╟егб╝бженехб╝длдщ
   * │░д╖д┐╛ї┬╓д╟╜ш═¤┤╪┐ЇдЄ╕╞д╙╜╨д╣бед│д╬д┐дсе╖е╣е╞ербже│б╝еыдЄ
   * ╕╞д╙╜╨д╖д┐е╣еье├е╔дЄд╜д╬д▐д▐╞░║ю╖╤┬│д╡д╗д┐дд╛ь╣чд╦д╧бд
   * ╜ш═¤┤╪┐Їд╬╞т╔Їд╟ putcurrent() дЄ╣╘дж╔м═╫дмдвдыбе
   */
  getcurrent();
  call_functions(type, p);
}

/* е╡б╝е╙е╣бже│б╝еыд╬╜ш═¤ */
static void srvcall_proc(kz_syscall_type_t type, kz_syscall_param_t *p)
{
  /*
   * е╖е╣е╞ербже│б╝еыд╚е╡б╝е╙е╣бже│б╝еыд╬╜ш═¤┤╪┐Їд╬╞т╔Їд╟бд
   * е╖е╣е╞ербже│б╝еыд╬╝┬╣╘д╖д┐е╣еье├е╔IDдЄ╞└дыд┐дсд╦ current дЄ
   * ╗▓╛╚д╖д╞ддды╔Ї╩мдмдвдъ(д┐д╚дид╨ thread_send() д╩д╔)бд
   * current дм╗─д├д╞дддыд╚╕э╞░║юд╣дыд┐дс NULL д╦└▀─ъд╣дыбе
   * е╡б╝е╙е╣бже│б╝еыд╧ thread_intrvec() ╞т╔Їд╬│ф╣■д▀е╧еєе╔ещ╕╞д╙╜╨д╖д╬
   * ▒ф─╣д╟╕╞д╨дьд╞дддыд╧д║д╟д╩д╬д╟бд╕╞д╙╜╨д╖╕хд╦ thread_intrvec() д╟
   * е╣е▒е╕ехб╝еъеєе░╜ш═¤дм╣╘дядьбдcurrent д╧║╞└▀─ъд╡дьдыбе
   */
  current = NULL;
  call_functions(type, p);
}

/* е╣еье├е╔д╬е╣е▒е╕ехб╝еъеєе░ */
static void schedule(void)
{
  int i;

  /*
   * ═е└ш╜ч░╠д╬╣тдд╜ч(═е└ш┼┘д╬┐Ї├═д╬╛од╡дд╜ч)д╦еье╟егб╝бженехб╝дЄ╕лд╞бд
   * ╞░║ю▓─╟╜д╩е╣еье├е╔дЄ╕б║ўд╣дыбе
   */
  for (i = 0; i < PRIORITY_NUM; i++) {
    if (readyque[i].head) /* ╕лд─длд├д┐ */
      break;
  }
  if (i == PRIORITY_NUM) /* ╕лд─длдщд╩длд├д┐ */
    kz_sysdown();

  current = readyque[i].head; /* елеьеєе╚бже╣еье├е╔д╦└▀─ъд╣ды */
}

static void syscall_intr(void)
{
  syscall_proc(current->syscall.type, current->syscall.param);
}

static void softerr_intr(void)
{
  puts(current->name);
  puts(" DOWN.\n");
  getcurrent(); /* еье╟егб╝енехб╝длдщ│░д╣ */
  thread_exit(); /* е╣еье├е╔╜к╬╗д╣ды */
}

/* │ф╣■д▀╜ш═¤д╬╞■╕¤┤╪┐Ї */
static void thread_intr(softvec_type_t type, unsigned long sp)
{
  /* елеьеєе╚бже╣еье├е╔д╬е│еєе╞ене╣е╚дЄ╩▌┬╕д╣ды */
  current->context.sp = sp;

  /*
   * │ф╣■д▀д┤д╚д╬╜ш═¤дЄ╝┬╣╘д╣дыбе
   * SOFTVEC_TYPE_SYSCALL, SOFTVEC_TYPE_SOFTERR д╬╛ь╣чд╧
   * syscall_intr(), softerr_intr() дме╧еєе╔ещд╦┼╨╧┐д╡дьд╞дддыд╬д╟бд
   * д╜дьдщдм╝┬╣╘д╡дьдыбе
   * д╜дь░╩│░д╬╛ь╣чд╧бдkz_setintr()д╦дшд├д╞ецб╝е╢┼╨╧┐д╡дьд┐е╧еєе╔ещдм
   * ╝┬╣╘д╡дьдыбе
   */
  if (handlers[type])
    handlers[type]();

  schedule(); /* е╣еье├е╔д╬е╣е▒е╕ехб╝еъеєе░ */

  /*
   * е╣еье├е╔д╬е╟еге╣е╤е├е┴
   * (dispatch()┤╪┐Їд╬╦▄┬╬д╧startup.sд╦двдъбдеве╗еєе╓ещд╟╡н╜╥д╡дьд╞ддды)
   */
  //dispatch(&current->context);
  /* д│д│д╦д╧╩╓д├д╞д│д╩дд */
}

void kz_start(kz_func_t func, char *name, int priority, int stacksize,
	      int argc, char *argv[])
{
  kzmem_init(); /* ╞░┼кесетеъд╬╜щ┤№▓╜ */

  /*
   * ░╩╣▀д╟╕╞д╙╜╨д╣е╣еье├е╔┤╪╧вд╬ещеде╓ещеъ┤╪┐Їд╬╞т╔Їд╟ current дЄ
   * ╕лд╞ддды╛ь╣чдмдвдыд╬д╟бдcurrent дЄ NULL д╦╜щ┤№▓╜д╖д╞дкдпбе
   */
  current = NULL;

  memset(readyque, 0, sizeof(readyque));
  memset(threads,  0, sizeof(threads));
  memset(handlers, 0, sizeof(handlers));
  memset(msgboxes, 0, sizeof(msgboxes));

  /* │ф╣■д▀е╧еєе╔ещд╬┼╨╧┐ */
  thread_setintr(SOFTVEC_TYPE_SYSCALL, syscall_intr); /* е╖е╣е╞ербже│б╝еы */
  thread_setintr(SOFTVEC_TYPE_SOFTERR, softerr_intr); /* е└ежеє═╫░°╚п└╕ */

  /* е╖е╣е╞ербже│б╝еы╚п╣╘╔╘▓─д╩д╬д╟─╛└▄┤╪┐ЇдЄ╕╞д╙╜╨д╖д╞е╣еье├е╔║ю└од╣ды */
  current = (kz_thread *)thread_run(func, name, priority, stacksize,
				    argc, argv);

  /* ║╟╜щд╬е╣еье├е╔дЄ╡п╞░ */
  //dispatch(&current->context);

  /* д│д│д╦д╧╩╓д├д╞д│д╩дд */
}

void kz_sysdown(void)
{
  puts("system error!\n");
  while (1)
    ;
}

/* е╖е╣е╞ербже│б╝еы╕╞д╙╜╨д╖═╤ещеде╓ещеъ┤╪┐Ї */
void kz_syscall(kz_syscall_type_t type, kz_syscall_param_t *param)
{
  current->syscall.type  = type;
  current->syscall.param = param;
  //asm volatile ("trapa #0"); /* е╚еще├е╫│ф╣■д▀╚п╣╘ */
  asm volatile ("svc #0"); /* уГИуГйуГГуГЧхЙ▓ш╛╝уБ┐чЩ║шбМ */
}

/* е╡б╝е╙е╣бже│б╝еы╕╞д╙╜╨д╖═╤ещеде╓ещеъ┤╪┐Ї */
void kz_srvcall(kz_syscall_type_t type, kz_syscall_param_t *param)
{
  srvcall_proc(type, param);
}
