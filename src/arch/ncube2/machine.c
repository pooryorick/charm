/***************************************************************************
 * RCS INFORMATION:
 *
 *	$RCSfile$
 *	$Author$	$Locker$		$State$
 *	$Revision$	$Date$
 *
 ***************************************************************************
 * DESCRIPTION:
 *
 ***************************************************************************
 * REVISION HISTORY:
 *
 * $Log$
 * Revision 2.8  1995-10-10 06:10:58  jyelon
 * removed program_name
 *
 * Revision 2.7  1995/09/29  09:50:07  jyelon
 * CmiGet-->CmiDeliver, added protos, etc.
 *
 * Revision 2.6  1995/09/20  16:00:16  gursoy
 * made the arg of CmiFree and CmiSize void*
 *
 * Revision 2.5  1995/09/07  22:59:57  gursoy
 * added CpvInitialize calls for Cmi_mype etc for the sake of compleeteness
 *
 * Revision 2.4  1995/09/07  22:51:52  gursoy
 * Cmi_mype Cmi_numpe and CmiLocalQueue are accessed thru macros now
 *
 * Revision 2.3  1995/07/03  17:58:04  gursoy
 * changed charm_main to user_main
 *
 * Revision 2.2  1995/06/13  16:01:23  gursoy
 * fixed a minor syntax error
 *
 * Revision 2.1  1995/06/09  21:23:01  gursoy
 * Cpv macros moved to converse
 *
 * Revision 2.0  1995/06/08  16:39:47  gursoy
 * Reorganized directory structure
 *
 * Revision 1.3  1995/05/04  22:19:02  sanjeev
 * Mc to Cmi changes
 *
 * Revision 1.2  1994/11/22  16:56:31  sanjeev
 * Replaced main by SetupCharm
 *
 * Revision 1.1  1994/11/03  17:35:37  brunner
 * Initial revision
 *
 ***************************************************************************/
static char ident[] = "@(#)$Header$";

#include <stdio.h>
#include "converse.h"

#define FLIPBIT(node,bitnumber) (node ^ (1 << bitnumber))


CpvDeclare(int, Cmi_mype);
CpvDeclare(int,  Cmi_numpe);
CpvDeclare(void*, CmiLocalQueue);

static int Cmi_dim;

static int process, host, cflag, source, type;
static double uclockinitvalue;
extern double amicclk();




void *CmiAlloc(size)
int size;
{
char *res;
res =(char *)malloc(size+8);
if (res==0) printf("Memory allocation failed.");
((int *)res)[0]=size;
return (void *)(res+8);
}

int CmiSize(blk)
void *blk;
{
return ((int *)( (char *)blk)  -8))[0];
}

void CmiFree(blk)
void *blk;
{
free( ((char *)blk) - 8);
}





/**************************  TIMER FUNCTIONS **************************/

double CmiTimer()
{
    return ( (amicclk() - uclockinitvalue) / 1000000.0 );
}




static void CmiTimerInit()
{
     uclockinitvalue = amicclk();
}


int CmiAsyncMsgSent(c)
CmiCommHandle c ;
{
    return 1;
}


void CmiReleaseCommHandle(c)
CmiCommHandle c ;
{
}




/********************* MESSAGE RECEIVE FUNCTIONS ******************/

void *CmiGetNonLocal()
{
        void *env;
        int  msglength;

        type = MSG_TYPE;
        source = -1;  /* dont care */

        if ( (msglength = ntest(&source, &type)) > 0)
        {
               env = (void *)  CmiAlloc(msglength); 
               if (env == 0) 
                  CmiPrintf("*** ERROR *** Memory Allocation Failed.\n");
               CmiSyncReceive(msglength, env);
               return env;
        }
        else
		return 0;
}


CmiSyncReceive(size, buffer)
int size;
char *buffer;
{
    nread(buffer, size, &source, &type, &cflag);
}


/********************* MESSAGE SEND FUNCTIONS ******************/

void CmiSyncSendFn(destPE, size, msg)
int destPE;
int size;
char * msg;
{
    nwrite(msg, size, destPE, MSG_TYPE, &cflag);
}


CmiCommHandle CmiAsyncSendFn(destPE, size, msg)   /* same as sync send for ncube */
int destPE;
int size;
char * msg;
{
    nwrite(msg, size, destPE, MSG_TYPE, &cflag);
    return 0 ;
}


void CmiFreeSendFn(destPE, size, msg)
     int destPE, size;
     char *msg;
{
    CmiSyncSendFn(destPE, size, msg);
    CmiFree(msg);
}

/*********************** BROADCAST FUNCTIONS **********************/


void CmiSyncBroadcastFn(size, msg)	/* ALL_EXCEPT_ME  */
int size;
char * msg;
{
	int i;

	for (i=0; i<CpvAccess(Cmi_numpe); i++)
		if (i != CpvAccess(Cmi_mype))
			nwrite(msg, size, i, MSG_TYPE, &cflag);
}


CmiCommHandle CmiAsyncBroadcastFn(size, msg)	/* ALL_EXCEPT_ME  */
int size;
char * msg;
{
/* Same as sync broadcast for now */
	int i;

	for (i=0; i<CpvAccess(Cmi_numpe); i++)
		if (i != CpvAccess(Cmi_mype))
			nwrite(msg, size, i, MSG_TYPE, &cflag);
	return 0 ;
}

void CmiFreeBroadcastFn(size, msg)
    int size;
    char *msg;
{
    CmiSyncBroadcastFn(size,msg);
    CmiFree(msg);
}

void CmiSyncBroadcastAllFn(size, msg)
int size;
char * msg;
{
    	int dest = 0xffff;
	nwrite(msg, size, dest, MSG_TYPE, &cflag); 
}


CmiCommHandle CmiAsyncBroadcastAllFn(size, msg)
int size;
char * msg;
{
    	int dest = 0xffff;
	nwrite(msg, size, dest, MSG_TYPE, &cflag); 
	return 0 ;
}

void CmiFreeBroadcastAllFn(size, msg)
int size;
char * msg;
{
	int dest = 0xffff;
	nwrite(msg, size, dest, MSG_TYPE, &cflag); 
	CmiFree(msg) ; 
}





/**********************  LOAD BALANCER NEEDS **********************/

long CmiNumNeighbours(node)
int node;
{
    return Cmi_dim;
}


CmiGetNodeNeighbours(node, neighbours)
int node, *neighbours;
{
    int i;

    for (i = 0; i < Cmi_dim; i++)
	neighbours[i] = FLIPBIT(node,i);
}




int CmiNeighboursIndex(node, neighbour)
int node, neighbour;
{
    int index = 0;
    int linenum = node ^ neighbour;

    while (linenum > 1)
    {
	linenum = linenum >> 1;
	index++;
    }
    return index;
}



/************************** SETUP ***********************************/

void CmiInitMc(argv)
char *argv[];
{
    CpvInitialize(int, Cmi_mype);
    CpvInitialize(int, Cmi_numpe);
    CpvInitialize(void*, CmiLocalQueue);

    whoami(&CpvAccess(Cmi_mype), &process, &host, &Cmi_dim);
    CpvAccess(Cmi_numpe) = (1 << Cmi_dim) ;


    CpvAccess(CmiLocalQueue)= (void *) FIFO_Create();
    CmiSpanTreeInit();
    CmiTimerInit();
}



void CmiExit()
{}


void CmiDeclareArgs()
{}


main(argc,argv)
int argc;
char *argv[];
{
    user_main(argc,argv);
}

