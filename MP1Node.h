/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Header file of MP1Node class.
 **********************************/

#ifndef _MP1NODE_H_
#define _MP1NODE_H_

#include "EmulNet.h"
#include "Log.h"
#include "Member.h"
#include "Params.h"
#include "Queue.h"
#include "stdincludes.h"

/**
 * Macros
 */
#define TREMOVE 20
#define TFAIL 4

/**
 * Message Types
 */
enum MsgTypes { JOINREQ, JOINREP, DELETE, PING, PINGREQ, ACK };

/**
 * STRUCT NAME: MessageHdr
 *
 * DESCRIPTION: Header and content of a message
 */
typedef struct MessageHdr {
    enum MsgTypes msgType;
    vector<MemberListEntry> memberList;
    Address srcAddr;
    Address endAddr;
} MessageHdr;

/**
 * CLASS NAME: MP1Node
 *
 * DESCRIPTION: Class implementing Membership protocol functionalities for
 * failure detection
 */
class MP1Node {
   private:
    EmulNet *emulNet;
    Log *log;
    Params *par;
    Member *memberNode;
    bool finishedPING;
    Address pingTarget;
    size_t posInMemberList;
    char NULLADDR[6];

   public:
    MP1Node(Member *, Params *, EmulNet *, Log *, Address *);
    Member *getMemberNode() { return memberNode; }
    int recvLoop();
    static int enqueueWrapper(void *env, char *buff, int size);
    void nodeStart(char *servaddrstr, short serverport);
    int initThisNode(Address *joinaddr);
    int introduceSelfToGroup(Address *joinAddress);
    int finishUpThisNode();
    void nodeLoop();
    void checkMessages();
    bool recvCallBack(Member *memberNode, MessageHdr *data, int size);
    void nodeLoopOps();
    int isNullAddress(Address *addr);
    Address getJoinAddress();
    void initMemberListTable(Member *memberNode);
    virtual ~MP1Node();
    bool deleteNode(Address *deleteNodeAddr);
    bool sendFailedNode();
    bool startPing();
    bool startPingREQ();
    // subFunctions for recvCallBack
    bool handleJoinReq(MessageHdr *msg, int size);
    bool handleJoinReply(MessageHdr *msg, int size);
    bool handlePing(MessageHdr *msg, int size);
    bool handlePingReq(MessageHdr *msg, int size);
    bool handleAck(MessageHdr *msg, int size);
    bool handleDelete(MessageHdr *msg, int size);
};

#endif /* _MP1NODE_H_ */
