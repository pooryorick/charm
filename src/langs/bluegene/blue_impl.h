#ifndef BLUE_IMPL_H
#define BLUE_IMPL_H

#include "conv-mach.h"
#include <stdlib.h>

#include "blue_types.h"
#include "blue_timing.h"
#include "blue_network.h"

/* alway use handler table per node */
#if ! defined(CMK_BLUEGENE_NODE) && ! defined(CMK_BLUEGENE_THREAD)
#define CMK_BLUEGENE_THREAD   1
#endif

/* define system parameters */
#define INBUFFER_SIZE	32

/* end of system parameters */

#define MAX_HANDLERS	100

class BGMach {
public:
  int x, y, z;         /* size of bluegene nodes in cube */
  int numCth, numWth;           /* number of threads */
  int stacksize;		/* bg thread stack size */
  int timingMethod;		/* timing method */
  char *traceroot;		/* bgTraceFile prefix */
  BigSimNetwork *network;	/* network setup */
public:
  BGMach() {  nullify(); }
  ~BGMach() { if (network) delete network; }
  void nullify() { x=y=z=0; numCth=numWth=0; stacksize=0; timingMethod = BG_ELAPSE; traceroot=NULL; network=new BlueGeneNetwork;}
  void setSize(int xx, int yy, int zz) 
	{ x=xx; y=yy; z=zz; }
  void getSize(int *xx, int *yy, int *zz) 
	{ *xx=x; *yy=y; *zz=z; }
  int numTh()
	{ return numCth + numWth; }
  int getNodeSize()  { return x*y*z; }
  int isWorkThread(int tid) { return tid>=0 && tid<numWth; }
  int read(char *file);
  void pup(PUP::er &p) { 
        p|x; p|y; p|z; p|numCth; p|numWth; 
	p|stacksize; p|timingMethod;
       }
};

// simulation state
// one copy per host machine processor  (Cpv)
class SimState {
public:
  // converse handlers
  int msgHandler;
  int nBcastMsgHandler;
  int tBcastMsgHandler;
  int exitHandler;
  int beginExitHandler;
  int bgStatCollectHandler;
  // state variables
  int inEmulatorInit;
  // simulation start timer
  double simStartTime;
};

CpvExtern(BGMach, bgMach);	/* BG machine size */
CpvExtern(SimState, simState);	/* simulation state variables */
CpvExtern(int, numNodes);	/* number of bg nodes on this PE */

typedef char ThreadType;
const char UNKNOWN_THREAD=0, COMM_THREAD=1, WORK_THREAD=2;

typedef bgQueue<int>  	    threadIDQueue;
typedef bgQueue<CthThread>  threadQueue;
typedef bgQueue<char *>     msgQueue;
//typedef CkQ<char *> 	    ckMsgQueue;
// use a queue sorted by recv time
typedef minMsgHeap 	    ckMsgQueue;
typedef CkQ<bgCorrectionMsg *> 	    bgCorrectionQ;
//typedef minHeap<bgCorrectionMsg *> 	    bgCorrectionQ;

/**
  definition of Handler Table;
  there are two kinds of handle tables: 
  one is node level, the other is at thread level
*/
class HandlerTable {
public:
  int          handlerTableCount; 
  BgHandlerInfo *  handlerTable;     
public:
  HandlerTable();
  inline int registerHandler(BgHandler h);
  inline void numberHandler(int idx, BgHandler h);
  inline void numberHandlerEx(int idx, BgHandlerEx h, void *userPtr);
  inline BgHandlerInfo* getHandle(int handler);
#if 0
  HandlerTable()
  {
    handlerTableCount = 1;
    handlerTable = (BgHandler *)malloc(MAX_HANDLERS * sizeof(BgHandler));
    for (int i=0; i<MAX_HANDLERS; i++) handlerTable[i] = defaultBgHandler;
  }
  inline int registerHandler(BgHandler h)
  {
    ASSERT(!cva(inEmulatorInit));
    /* leave 0 as blank, so it can report error luckily */
    int cur = handlerTableCount++;
    if (cur >= MAX_HANDLERS)
      CmiAbort("BG> HandlerID exceed the maximum.\n");
    handlerTable[cur] = h;
    return cur;
  }
  inline void numberHandler(int idx, BgHandler h)
  {
    ASSERT(!cva(inEmulatorInit));
    if (idx >= handlerTableCount || idx < 1)
      CmiAbort("BG> HandlerID exceed the maximum!\n");
    handlerTable[idx] = h;
  }
  inline BgHandlerInfo getHandle(int handler)
  {
#if 0
    if (handler >= handlerTableCount) {
      CmiPrintf("[%d] handler: %d handlerTableCount:%d. \n", tMYNODEID, handler, handlerTableCount);
      CmiAbort("Invalid handler!");
    }
#endif
    if (handler >= handlerTableCount) return NULL;
    return handlerTable[handler];
  }
#endif
};


#define cva CpvAccess
#define cta CtvAccess

class threadInfo;
CtvExtern(threadInfo *, threadinfo); 
class nodeInfo;
CpvExtern(nodeInfo *, nodeinfo); 
extern double (*timerFunc) (void);

#define tMYID		cta(threadinfo)->id
#define tMYGLOBALID	cta(threadinfo)->globalId
#define tTHREADTYPE	cta(threadinfo)->type
#define tMYNODE		cta(threadinfo)->myNode
#define tSTARTTIME	tMYNODE->startTime
#define tTIMERON	tMYNODE->timeron_flag
#define tCURRTIME	cta(threadinfo)->currTime
#define tHANDLETAB	cta(threadinfo)->handlerTable
#define tHANDLETABFNPTR	cta(threadinfo)->handlerTable.fnPtr
#define tHANDLETABUSERPTR	cta(threadinfo)->handlerTable.userPtr
#define tMYX		tMYNODE->x
#define tMYY		tMYNODE->y
#define tMYZ		tMYNODE->z
#define tMYNODEID	tMYNODE->id
#define tTIMELINEREC	tMYNODE->timelines[tMYID]
#define tTIMELINE	tMYNODE->timelines[tMYID].timeline
#define tINBUFFER	tMYNODE->inBuffer
#define tUSERDATA	tMYNODE->udata
#define tTHREADTABLE    tMYNODE->threadTable
#define tSTARTED        tMYNODE->started

extern int bgSize;

/*****************************************************************************
   used internally, define BG Node to real Processor mapping
*****************************************************************************/

class BlockMapInfo {
public:
  /* return the number of bg nodes on this physical emulator PE */
  inline static int numLocalNodes()
  {
    int n, m;
    n = bgSize / CmiNumPes();
    m = bgSize % CmiNumPes();
    if (CmiMyPe() < m) n++;
    return n;
  }

    /* map global serial number to (x,y,z) ++++ */
  inline static void Global2XYZ(int seq, int *x, int *y, int *z) {
    *x = seq / (cva(bgMach).y * cva(bgMach).z);
    *y = (seq - *x * cva(bgMach).y * cva(bgMach).z) / cva(bgMach).z;
    *z = (seq - *x * cva(bgMach).y * cva(bgMach).z) % cva(bgMach).z;
  }


    /* calculate global serial number of (x,y,z) ++++ */
  inline static int XYZ2Global(int x, int y, int z) {
    return x*(cva(bgMach).y * cva(bgMach).z) + y*cva(bgMach).z + z;
  }

    /* map (x,y,z) to emulator PE ++++ */
  inline static int XYZ2PE(int x, int y, int z) {
    return Global2PE(XYZ2Global(x,y,z));
  }

  inline static int XYZ2Local(int x, int y, int z) {
    return Global2Local(XYZ2Global(x,y,z));
  }

    /* local node index number to x y z ++++ */
  inline static void Local2XYZ(int num, int *x, int *y, int *z)  {
    Global2XYZ(Local2Global(num), x, y, z);
  }

    /* map global serial node number to PE ++++ */
  inline static int Global2PE(int num) { 
    int n = bgSize/CmiNumPes();
    int bn = bgSize%CmiNumPes();
    int start = 0; 
    int end = 0;
    for (int i=0; i< CmiNumPes(); i++) {
      end = start + n-1;
      if (i<bn) end++;
      if (num >= start && num <= end) return i;
      start = end+1;
    }
    CmiAbort("Global2PE: unknown pe!");
    return -1;
  }

    /* map global serial node ID to local node array index  ++++ */
  inline static int Global2Local(int num) { 
    int n = bgSize/CmiNumPes();
    int bn = bgSize%CmiNumPes();
    int start = 0; 
    int end = 0;
    for (int i=0; i< CmiNumPes(); i++) {
      end = start + n-1;
      if (i<bn) end++;
      if (num >= start && num <= end) return num-start;
      start = end+1;
    }
    CmiAbort("Global2Local:unknown pe!");
    return -1;
  }

    /* map local node index to global serial node id ++++ */
  inline static int Local2Global(int num) { 
    int n = bgSize/CmiNumPes();
    int bn = bgSize%CmiNumPes();
    int start = 0; 
    int end = 0;
    for (int i=0; i< CmiMyPe(); i++) {
      end = start + n-1;
      if (i<bn) end++;
      start = end+1;
    }
    return start+num;
  }
};

class CyclicMapInfo {
public:
  /* return the number of bg nodes on this physical emulator PE */
  inline static int numLocalNodes()
  {
    int n, m;
    n = bgSize / CmiNumPes();
    m = bgSize % CmiNumPes();
    if (CmiMyPe() < m) n++;
    return n;
  }

    /* map global serial number to (x,y,z) ++++ */
  inline static void Global2XYZ(int seq, int *x, int *y, int *z) {
    *x = seq / (cva(bgMach).y * cva(bgMach).z);
    *y = (seq - *x * cva(bgMach).y * cva(bgMach).z) / cva(bgMach).z;
    *z = (seq - *x * cva(bgMach).y * cva(bgMach).z) % cva(bgMach).z;
  }


    /* calculate global serial number of (x,y,z) ++++ */
  inline static int XYZ2Global(int x, int y, int z) {
    return x*(cva(bgMach).y * cva(bgMach).z) + y*cva(bgMach).z + z;
  }

    /* map (x,y,z) to emulator PE ++++ */
  inline static int XYZ2PE(int x, int y, int z) {
    return Global2PE(XYZ2Global(x,y,z));
  }

  inline static int XYZ2Local(int x, int y, int z) {
    return Global2Local(XYZ2Global(x,y,z));
  }

    /* local node index number to x y z ++++ */
  inline static void Local2XYZ(int num, int *x, int *y, int *z)  {
    Global2XYZ(Local2Global(num), x, y, z);
  }

    /* map global serial node number to PE ++++ */
  inline static int Global2PE(int num) { return num % CmiNumPes(); }

    /* map global serial node ID to local node array index  ++++ */
  inline static int Global2Local(int num) { return num/CmiNumPes(); }

    /* map local node index to global serial node id ++++ */
  inline static int Local2Global(int num) { return CmiMyPe()+num*CmiNumPes();}
};


/*****************************************************************************
      NodeInfo:
        including a group of functions defining the mapping, terms used here:
        XYZ: (x,y,z)
        Global:  map (x,y,z) to a global serial number
        Local:   local index of this nodeinfo in the emulator's node 
*****************************************************************************/
class nodeInfo: public CyclicMapInfo  {
public:
  int id;
  int x,y,z;
  msgQueue     inBuffer;	/* emulate the fix-size inbuffer */
  CmmTable     msgBuffer;	/* buffer when inBuffer is full */
  CthThread   *threadTable;	/* thread table for both work and comm threads*/
  threadInfo  **threadinfo;
  threadQueue *commThQ;		/* suspended comm threads queue */
  ckMsgQueue   nodeQ;		/* non-affinity msg queue */
  ckMsgQueue  *affinityQ;	/* affinity msg queue for each work thread */
  double       startTime;	/* start time for a thread */
  double       nodeTime;	/* node time to coordinate thread times */
  short        lastW;           /* last worker thread assigned msg */
  char         started;		/* flag indicate if this node is started */
  char        *udata;		/* node specific data pointer */
  char 	       timeron_flag;	/* true if timer started */
 
  HandlerTable handlerTable; /* node level handler table */
#if BLUEGENE_TIMING
  // for timing
  BgTimeLineRec *timelines;
  bgCorrectionQ cmsg;
#endif
public:
  nodeInfo();
  ~nodeInfo();
  /**
   *  add a message to this bluegene node's inbuffer queue
   */
  void addBgNodeInbuffer(char *msgPtr);
  /**
   *  add a message to this bluegene node's non-affinity queue
   */
  void addBgNodeMessage(char *msgPtr);
  /**
   *  called by comm thread to poll inBuffer
   */
  char * getFullBuffer();
};	// end of nodeInfo

/*****************************************************************************
      ThreadInfo:  each thread has a thread private threadInfo structure.
      It has a local id, a global serial id. 
      myNode: point to the nodeInfo it belongs to.
      currTime: is the elapse time for this thread;
      me:   point to the CthThread converse thread handler.
*****************************************************************************/

class threadInfo {
public:
  short id;
//  int globalId;
  ThreadType  type;		/* worker or communication thread */
  CthThread me;			/* Converse thread handler */
  nodeInfo *myNode;		/* the node belonged to */
  double  currTime;		/* thread timer */

#if  CMK_BLUEGENE_THREAD
  HandlerTable   handlerTable;      /* thread level handler table */
#endif

public:
  threadInfo(int _id, ThreadType _type, nodeInfo *_node): 
  	id(_id), type(_type), myNode(_node), currTime(0.0) 
  {
//    if (id != -1) globalId = nodeInfo::Local2Global(_node->id)*(cva(numCth)+cva(numWth))+_id;
  }
  inline void setThread(CthThread t) { me = t; }
  inline CthThread getThread() const { return me; }
  virtual void run() { CmiAbort("run not imlplemented"); }
}; 

class workThreadInfo : public threadInfo {
public:
  workThreadInfo(int _id, ThreadType _type, nodeInfo *_node): 
        threadInfo(_id, _type, _node) {}
  void addAffMessage(char *msgPtr);        ///  add msg to affinity queue
  void run();
};

class commThreadInfo : public threadInfo {
public:
  commThreadInfo(int _id, ThreadType _type, nodeInfo *_node): 
        threadInfo(_id, _type, _node) {}
  void run();
};

// functions

double BgGetCurTime();
char ** BgGetArgv();
int     BgGetArgc();
void    startVTimer();
void    stopVTimer();

char * getFullBuffer();
void   addBgNodeMessage(char *msgPtr);
void   addBgThreadMessage(char *msgPtr, int threadID);
void   BgProcessMessage(char *msg);


/* blue gene debug */

#define BLUEGENE_DEBUG 0

#if BLUEGENE_DEBUG
/**Controls amount of debug messages: 1 (the lowest priority) is 
extremely verbose, 2 shows most procedure entrance/exits, 
3 shows most communication, and 5 only shows rare or unexpected items.
Displaying lower priority messages doesn't stop higher priority ones.
*/
#define BLUEGENE_DEBUG_PRIO 2
#define BLUEGENE_DEBUG_LOG 1 /**Controls whether output goes to log file*/

extern FILE *bgDebugLog;
# define BGSTATE_I(prio,args) if ((prio)>=BLUEGENE_DEBUG_PRIO) {\
	fprintf args ; fflush(bgDebugLog); }
# define BGSTATE(prio,str) \
	BGSTATE_I(prio,(bgDebugLog,"[%.3f]> "str"\n",CmiWallTimer()))
# define BGSTATE1(prio,str,a) \
	BGSTATE_I(prio,(bgDebugLog,"[%.3f]> "str"\n",CmiWallTimer(),a))
# define BGSTATE2(prio,str,a,b) \
	BGSTATE_I(prio,(bgDebugLog,"[%.3f]> "str"\n",CmiWallTimer(),a,b))
# define BGSTATE3(prio,str,a,b,c) \
	BGSTATE_I(prio,(bgDebugLog,"[%.3f]> "str"\n",CmiWallTimer(),a,b,c))
# define BGSTATE4(prio,str,a,b,c,d) \
	BGSTATE_I(prio,(bgDebugLog,"[%.3f]> "str"\n",CmiWallTimer(),a,b,c,d))
#else
# define BLUEGENE_DEBUG_LOG 0
# define BGSTATE(n,x) /*empty*/
# define BGSTATE1(n,x,a) /*empty*/
# define BGSTATE2(n,x,a,b) /*empty*/
# define BGSTATE3(n,x,a,b,c) /*empty*/
# define BGSTATE4(n,x,a,b,c,d) /*empty*/
#endif

#endif
