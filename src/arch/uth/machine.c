#include <stdio.h>
#include <math.h>
#include "converse.h"

static char *DeleteArg(argv)
  char **argv;
{
  char *res = argv[0];
  if (res==0) { CmiError("Bad arglist."); exit(1); }
  while (*argv) { argv[0]=argv[1]; argv++; }
  return res;
}

int CountArgs(argv)
  char **argv;
{
  int n = 0;
  while (*argv) { n++; argv++; }
  return n;
}

/***********************************************************************
 *
 * Abort function:
 *
 ************************************************************************/

void CmiAbort(char *message)
{
  CmiError(message);
  exit(1);
}

/*****************************************************************************
 *
 * Module variables
 * 
 ****************************************************************************/

typedef void *Fifo;

int        Cmi_mype;
int        Cmi_myrank;
int        Cmi_numpes;
int        Cmi_nodesize;
int        Cmi_stacksize = 64000;
char     **CmiArgv;
CmiStartFn CmiStart;
int        CmiUsched;
CthThread *CmiThreads;
Fifo      *CmiQueues;
int       *CmiBarred;
int        CmiNumBarred=0;

Fifo FIFO_Create();
CpvDeclare(Fifo, CmiLocalQueue);

/******************************************************************************
 *
 * Load-Balancer needs
 *
 * These neighbour functions impose a (possibly incomplete)
 * hypercube on the machine.
 *
 *****************************************************************************/


long CmiNumNeighbours(node)
int node;
{
  int bit, count=0;
  bit = 1;
  while (1) {
    int neighbour = node ^ bit;
    if (neighbour < CmiNumPes()) count++;
    bit = bit<<1; 
    if (bit > CmiNumPes()) break;
  }
  return count;
}

int CmiGetNodeNeighbours(node, neighbours)
int node, *neighbours;
{
  int bit, count=0;
  bit = 1;
  while (1) {
    int neighbour = node ^ bit;
    if (neighbour < CmiNumPes()) neighbours[count++] = neighbour;
    bit = bit<<1; 
    if (bit > CmiNumPes()) break;
  }
  return count;
}
 
int CmiNeighboursIndex(node, nbr)
int node, nbr;
{
  int bit, count=0;
  bit = 1;
  while (1) {
    int neighbour = node ^ bit;
    if (neighbour < CmiNumPes()) { if (nbr==neighbour) return count; count++; }
    bit = bit<<=1; 
    if (bit > CmiNumPes()) break;
  }
  return(-1);
}


/*****************************************************************************
 *
 * Comm handles are nonexistent in uth version
 *
 *****************************************************************************/

int CmiAsyncMsgSent(c)
CmiCommHandle c ;
{
  return 1;
}

void CmiReleaseCommHandle(c)
CmiCommHandle c ;
{
}

/********************* CONTEXT-SWITCHING FUNCTIONS ******************/

static void CmiNext()
{
  CthThread t; int index; int orig;
  index = (CmiMyPe()+1) % CmiNumPes();
  orig = index;
  while (1) {
    t = CmiThreads[index];
    if ((t)&&(!CmiBarred[index])) break;
    index = (index+1) % CmiNumPes();
    if (index == orig) exit(0);
  }
  Cmi_mype = index;
  CthResume(t);
}

void CmiExit()
{
  CmiThreads[CmiMyPe()] = 0;
  CmiFree(CthSelf());
  CmiNext();
}

void *CmiGetNonLocal()
{
  CmiThreads[CmiMyPe()] = CthSelf();
  CmiNext();
  return 0;
}

void CmiNotifyIdle()
{
  CmiThreads[CmiMyPe()] = CthSelf();
  CmiNext();
}

void CmiNodeBarrier()
{
  int i;
  CmiNumBarred++;
  CmiBarred[CmiMyPe()] = 1;
  if (CmiNumBarred == CmiNumPes()) {
    for (i=0; i<CmiNumPes(); i++) CmiBarred[i]=0;
    CmiNumBarred=0;
  }
  CmiGetNonLocal();
}

CmiNodeLock CmiCreateLock()
{
  return (CmiNodeLock)malloc(sizeof(int));
}

void CmiLock(CmiNodeLock lk)
{
  while (*lk) CmiGetNonLocal();
  *lk = 1;
}

void CmiUnlock(CmiNodeLock lk)
{
  if (*lk==0) {
    CmiError("CmiNodeLock not locked, can't unlock.");
    exit(1);
  }
  *lk = 0;
}

int CmiTryLock(CmiNodeLock lk)
{
  if (*lk==0) { *lk=1; return 0; }
  return -1;
}

void CmiDestroyLock(CmiNodeLock lk)
{
  free(lk);
}



/********************* MESSAGE SEND FUNCTIONS ******************/

void CmiSyncSendFn(destPE, size, msg)
int destPE;
int size;
char * msg;
{
  char *buf = (char *)CmiAlloc(size);
  memcpy(buf,msg,size);
  FIFO_EnQueue(CmiQueues[destPE],buf);
}

CmiCommHandle CmiAsyncSendFn(destPE, size, msg) 
int destPE;
int size;
char * msg;
{
  char *buf = (char *)CmiAlloc(size);
  memcpy(buf,msg,size);
  FIFO_EnQueue(CmiQueues[destPE],buf);
  return 0;
}

void CmiFreeSendFn(destPE, size, msg)
int destPE;
int size;
char * msg;
{
  FIFO_EnQueue(CmiQueues[destPE], msg);
}

void CmiSyncBroadcastFn(size, msg)
int size;
char * msg;
{
  int i;
  for(i=0; i<CmiNumPes(); i++)
    if (i != CmiMyPe()) CmiSyncSendFn(i,size,msg);
}

CmiCommHandle CmiAsyncBroadcastFn(size, msg)
int size;
char * msg;
{
  CmiSyncBroadcastFn(size, msg);
  return 0;
}

void CmiFreeBroadcastFn(size, msg)
int size;
char * msg;
{
  CmiSyncBroadcastFn(size, msg);
  CmiFree(msg);
}

void CmiSyncBroadcastAllFn(size, msg)
int size;
char * msg;
{
  int i;
  for(i=0; i<CmiNumPes(); i++)
    CmiSyncSendFn(i,size,msg);
}

CmiCommHandle CmiAsyncBroadcastAllFn(size, msg)
int size;
char * msg;
{
  CmiSyncBroadcastAllFn(size,msg);
  return 0 ;
}

void CmiFreeBroadcastAllFn(size, msg)
int size;
char * msg;
{
  int i;
  for(i=0; i<CmiNumPes(); i++)
    if (i!=CmiMyPe()) CmiSyncSendFn(i,size,msg);
  FIFO_EnQueue(CpvAccess(CmiLocalQueue),msg);
}



/************************** SETUP ***********************************/

static void CmiParseArgs(argv)
char **argv;
{
  char **argp;
  
  for (argp=argv; *argp; ) {
    if ((strcmp(*argp,"++stacksize")==0)&&(argp[1])) {
      DeleteArg(argp);
      Cmi_stacksize = atoi(*argp);
      DeleteArg(argp);
    } else if ((strcmp(*argp,"+p")==0)&&(argp[1])) {
      Cmi_numpes = atoi(argp[1]);
      argp+=2;
    } else if (sscanf(*argp, "+p%d", &CmiNumPes()) == 1) {
      argp+=1;
    } else argp++;
  }
  
  if (CmiNumPes()<1) {
    printf("Error: must specify number of processors to simulate with +pXXX\n",CmiNumPes());
    exit(1);
  }
}

static char **CopyArgvec(char **src)
{
  int argc; char **argv;
  for (argc=0; src[argc]; argc++);
  argv = (char **)malloc((argc+1)*sizeof(char *));
  memcpy(argv, src, (argc+1)*sizeof(char *));
  return argv;
}

char **CmiInitPE()
{
  int argc; char **argv;
  argv = CopyArgvec(CmiArgv);
  CpvAccess(CmiLocalQueue) = CmiQueues[CmiMyPe()];
  CmiTimerInit();
  ConverseCommonInit(argv);
  return argv;
}

void CmiCallMain()
{
  char **argv;
  int argc;
  argv = CmiInitPE();
  for (argc=0; argv[argc]; argc++);
  CmiStart(argc, argv);
  if (CmiUsched==0) CsdScheduler(-1);
  ConverseExit();
}

void ConverseExit()
{
  ConverseCommonExit();
  CmiThreads[CmiMyPe()] = 0;
  CmiNext();
}

void ConverseInit(argc,argv,fn,usched,initret)
int argc;
char **argv;
CmiStartFn fn;
int usched, initret;
{
  CthThread t; int stacksize, i;
  
#if CMK_USE_HP_MAIN_FIX
#if FOR_CPLUS
  _main(argc,argv);
#endif
#endif
  
  CmiArgv = CopyArgvec(argv);
  CmiStart = fn;
  CmiUsched = usched;
  CmiParseArgs(argv);
  CthInit(argv);
  CpvInitialize(void*, CmiLocalQueue);
  CmiThreads = (CthThread *)CmiAlloc(CmiNumPes()*sizeof(CthThread));
  CmiBarred  = (int       *)CmiAlloc(CmiNumPes()*sizeof(int));
  CmiQueues  = (Fifo      *)CmiAlloc(CmiNumPes()*sizeof(Fifo));
  
  /* Create threads for all PE except PE 0 */
  for(i=0; i<CmiNumPes(); i++) {
    t = (i==0) ? CthSelf() : CthCreate(CmiCallMain, 0, Cmi_stacksize);
    CmiThreads[i] = t;
    CmiBarred[i] = 0;
    CmiQueues[i] = FIFO_Create();
  }
  Cmi_mype = 0;
  argv = CmiInitPE();
  if (initret==0) {
    fn(CountArgs(argv), argv);
    if (usched==0) CsdScheduler(-1);
    ConverseExit();
  }
}

