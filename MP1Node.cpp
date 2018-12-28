/**********************************
 * FILE NAME: MP1Node.cpp
 *
 * DESCRIPTION: Membership protocol run by this Node.
 * 				Definition of MP1Node class functions.
 **********************************/

#include "MP1Node.h"

/**
 * Overloaded Constructor of the MP1Node class
 */
MP1Node::MP1Node(Member *member, Params *params, EmulNet *emul, Log *log,
                 Address *address) {
    for (int i = 0; i < 6; i++) {
        NULLADDR[i] = 0;
    }
    this->memberNode = member;
    this->emulNet = emul;
    this->log = log;
    this->par = params;
    this->memberNode->addr = *address;
    this->posInMemberList = 0;
}

/**
 * Destructor of the MP1Node class
 */
MP1Node::~MP1Node() {}

/**
 * FUNCTION NAME: recvLoop
 *
 * DESCRIPTION: This function receives message from the network and pushes into
 * the queue This function is called by a node to receive messages currently
 * waiting for it
 */
int MP1Node::recvLoop() {
    if (memberNode->bFailed) {
        return false;
    } else {
        return emulNet->ENrecv(&(memberNode->addr), enqueueWrapper, NULL, 1,
                               &(memberNode->mp1q));
    }
}

/**
 * FUNCTION NAME: enqueueWrapper
 *
 * DESCRIPTION: Enqueue the message from Emulnet into the queue
 */
int MP1Node::enqueueWrapper(void *env, char *buff, int size) {
    Queue q;
    return q.enqueue((queue<q_elt> *)env, (void *)buff, size);
}

/**
 * FUNCTION NAME: nodeStart
 *
 * DESCRIPTION: This function bootstraps the node
 * 				All initializations routines for a member.
 * 				Called by the application layer.
 */
void MP1Node::nodeStart(char *servaddrstr, short servport) {
    Address joinaddr;
    joinaddr = getJoinAddress();

    // Self booting routines
    if (initThisNode(&joinaddr) == -1) {
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "init_thisnode failed. Exit.");
#endif
        exit(1);
    }

    if (!introduceSelfToGroup(&joinaddr)) {
        finishUpThisNode();
#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Unable to join self to group. Exiting.");
#endif
        exit(1);
    }
    return;
}

/**
 * FUNCTION NAME: initThisNode
 *
 * DESCRIPTION: Find out who I am and start up
 */
int MP1Node::initThisNode(Address *joinaddr) {
    memberNode->bFailed = false;
    memberNode->inited = true;
    memberNode->inGroup = false;
    // node is up!
    memberNode->pingCounter = TFAIL;
    memberNode->timeOutCounter = TREMOVE;
    initMemberListTable(memberNode);
    return 0;
}

/**
 * FUNCTION NAME: introduceSelfToGroup
 *
 * DESCRIPTION: Join the distributed system
 */
int MP1Node::introduceSelfToGroup(Address *joinaddr) {
    if (0 == memcmp((char *)&(memberNode->addr.addr), (char *)&(joinaddr->addr),
                    sizeof(memberNode->addr.addr))) {
        // I am the group booter (first process to join the group). Boot up the
        // group

#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Starting up group...");
#endif
        memberNode->inGroup = true;
    } else {
        // create JOINREQ message: format of data is {struct Address myaddr}
        MessageHdr *msg;
        size_t msgsize = sizeof(MessageHdr);
        msg = new MessageHdr;
        msg->msgType = JOINREQ;
        msg->srcAddr = memberNode->addr;

#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Trying to join...");
#endif

        // send JOINREQ message to introducer member
        emulNet->ENsend(&memberNode->addr, joinaddr, (char *)msg, msgsize);

        free(msg);
    }

    return 1;
}

/**
 * FUNCTION NAME: finishUpThisNode
 *
 * DESCRIPTION: Wind up this node and clean up state
 */
int MP1Node::finishUpThisNode() {
    memberNode->bFailed = true;
    return 0;
}

/**
 * FUNCTION NAME: nodeLoop
 *
 * DESCRIPTION: Executed periodically at each member
 * 				Check your messages in queue and perform
 * membership protocol duties
 */
void MP1Node::nodeLoop() {
    if (memberNode->bFailed) {
        return;
    }

    // Check my messages
    checkMessages();

    // Wait until you're in the group...
    if (!memberNode->inGroup) {
        return;
    }

    // ...then jump in and share your responsibilites!
    nodeLoopOps();

    return;
}

/**
 * FUNCTION NAME: checkMessages
 *
 * DESCRIPTION: Check messages in the queue and call the respective message
 * handler
 */
void MP1Node::checkMessages() {
    void *ptr;
    int size;

    // Pop waiting messages from memberNode's mp1q
    while (!memberNode->mp1q.empty()) {
        ptr = memberNode->mp1q.front().elt;
        size = memberNode->mp1q.front().size;
        memberNode->mp1q.pop();
        recvCallBack((Member *)memberNode, (MessageHdr *)ptr, size);
    }
    return;
}

/**
 * FUNCTION NAME: recvCallBack
 *
 * DESCRIPTION: Message handler for different message types
 */
bool MP1Node::recvCallBack(Member *memberNode, MessageHdr *msg, int size) {
    switch (msg->msgType) {
        case JOINREQ:
            return handleJoinReq(msg, size);
        case JOINREP:
            return handleJoinReply(msg, size);
        case PING:
            return handlePing(msg, size);
        case ACK:
            return handleAck(msg, size);
        case PINGREQ:
            return handlePingReq(msg, size);
        case DELETE:
            return handleDelete(msg, size);
        default:
            break;
    }
    return false;
}

/**
 * FUNCTION NAME: handleJoinReq
 *
 * DESCRIPTION: Handle the JOINREQ message and update the memberlist
 */
bool MP1Node::handleJoinReq(MessageHdr *msg, int size) {
    if (isNullAddress(&msg->srcAddr)) {
        return false;
    }

    // check if the requested node be in memberlist
    bool existed = false;
    for (auto entry : memberNode->memberList) {
        if (entry.getid() == msg->srcAddr.addr[0]) {
            existed = true;
            break;
        }
    }

    // add the node into the memberlist
    if (!existed) {
        MemberListEntry entry = MemberListEntry((int)msg->srcAddr.addr[0],
                                                (short)msg->srcAddr.addr[4]);
        memberNode->memberList.push_back(entry);
    }

#ifdef DEBUGLOG
    log->logNodeAdd(&memberNode->addr, &msg->srcAddr);
#endif

    // send a JOINREP message to the requested node
    size_t msgSize = sizeof(MessageHdr);
    Address dstAddr = msg->srcAddr;
    msg = new MessageHdr;
    msg->msgType = JOINREP;
    msg->srcAddr = memberNode->addr;
    msg->memberList = memberNode->memberList;
    emulNet->ENsend(&memberNode->addr, &dstAddr, (char *)msg, msgSize);

    free(msg);
    return true;
}

/**
 * FUNCTION NAME: handleJoinReply
 *
 * DESCRIPTION: Handle the JOINREP message and update the memberlist
 */
bool MP1Node::handleJoinReply(MessageHdr *msg, int size) {
    if (isNullAddress(&msg->srcAddr)) {
        return false;
    }

    Address joinaddr = getJoinAddress();
    bool existed = false;
    for (auto localEntry : memberNode->memberList) {
        if (localEntry.getid() == msg->srcAddr.addr[0]) {
            existed = true;
            break;
        }
    }
    // add the src node to the memberList
    if (!existed) {
        MemberListEntry entry = MemberListEntry((int)msg->srcAddr.addr[0],
                                                (short)msg->srcAddr.addr[4]);
        memberNode->memberList.push_back(entry);
    }

    // join the group when first time
    if ((int)msg->srcAddr.addr[0] == (int)joinaddr.addr[0]) {
        memberNode->inGroup = true;

#ifdef DEBUGLOG
        log->LOG(&memberNode->addr, "Now in the group...");
#endif
    }

#ifdef DEBUGLOG
    log->logNodeAdd(&memberNode->addr, &msg->srcAddr);
#endif

    for (auto remoteEntry : msg->memberList) {
        existed = false;
        if (remoteEntry.getid() == *(int *)(memberNode->addr.addr)) {
            continue;
        }
        // select the non-existed node in local memberlist to send JOINREQ
        for (auto localEntry : memberNode->memberList) {
            if (localEntry.getid() == remoteEntry.getid()) {
                existed = true;
                break;
            }
        }
        if (!existed) {
            // add the non-existed node to the memberList
            memberNode->memberList.push_back(remoteEntry);
            // create the destination address
            Address dstAddr;
            dstAddr.init();
            dstAddr.addr[0] = (char)remoteEntry.getid();
            dstAddr.addr[4] = (char)remoteEntry.getport();
            // create the JOINREQ message
            msg = new MessageHdr;
            msg->msgType = JOINREQ;
            msg->srcAddr = memberNode->addr;
            msg->memberList = memberNode->memberList;
            emulNet->ENsend(&memberNode->addr, &dstAddr, (char *)msg, size);
            free(msg);
        }
    }
    return true;
}

/**
 * FUNCTION NAME: handlePingReq
 *
 * DESCRIPTION: Send a PING message to dstAddr node for detecting the node
 */
bool MP1Node::handlePingReq(MessageHdr *msg, int size) {
    if (isNullAddress(&msg->srcAddr) && isNullAddress(&msg->endAddr)) {
        return false;
    }

    Address end = msg->srcAddr;
    Address dstAddr = msg->endAddr;

    // create and send the PING message
    msg = new MessageHdr;
    msg->msgType = PING;
    msg->srcAddr = memberNode->addr;
    msg->endAddr = end;
    emulNet->ENsend(&memberNode->addr, &dstAddr, (char *)msg, size);

    free(msg);
    return true;
}

/**
 * FUNCTION NAME: handlePing
 *
 * DESCRIPTION: Send back an ACK message
 */
bool MP1Node::handlePing(MessageHdr *msg, int size) {
    if (isNullAddress(&msg->srcAddr) && isNullAddress(&msg->endAddr)) {
        return false;
    }

    Address end = msg->endAddr;
    Address dstAddr = msg->srcAddr;

    // create and send the ACK message
    msg = new MessageHdr;
    msg->msgType = ACK;
    msg->srcAddr = memberNode->addr;
    msg->endAddr = end;
    emulNet->ENsend(&memberNode->addr, &dstAddr, (char *)msg, size);

    free(msg);
    return true;
}

/**
 * FUNCTION NAME: handleAck
 *
 * DESCRIPTION: send the ACK msg to the des
 */
bool MP1Node::handleAck(MessageHdr *msg, int size) {
    if (isNullAddress(&msg->srcAddr) && isNullAddress(&msg->endAddr)) {
        return false;
    }

    if (0 == memcmp((char *)&(memberNode->addr.addr),
                    (char *)&(msg->endAddr.addr),
                    sizeof(memberNode->addr.addr))) {
        finishedPING = true;
    } else {
        // forward the message to the destination node
        Address end = msg->endAddr;
        msg = new MessageHdr;
        msg->msgType = ACK;
        msg->srcAddr = memberNode->addr;
        msg->endAddr = end;
        emulNet->ENsend(&memberNode->addr, &end, (char *)msg, size);
        free(msg);
    }
    return true;
}

/**
 * FUNCTION NAME: handleDelete
 *
 * DESCRIPTION: delete the related node from memberlist
 */
bool MP1Node::handleDelete(MessageHdr *msg, int size) {
    if (isNullAddress(&msg->endAddr)) {
        return false;
    }

    Address *deleteNodeAddr = &msg->endAddr;
    deleteNode(deleteNodeAddr);

    free(msg);
    return true;
}

/**
 * FUNCTION NAME: deleteNode
 *
 * DESCRIPTION: implementation for deleting the node
 */
bool MP1Node::deleteNode(Address *deleteNodeAddr) {
    for (size_t i = 0; i < memberNode->memberList.size(); i++) {
        if ((int)deleteNodeAddr->addr[0] == memberNode->memberList[i].getid()) {
            memberNode->memberList.erase(memberNode->memberList.begin() + i);
            log->logNodeRemove(&memberNode->addr, deleteNodeAddr);
            break;
        }
    }
    return true;
}

/**
 * FUNCTION NAME: sendFailedNode
 *
 * DESCRIPTION: send out the info about the failed node to total network
 */
bool MP1Node::sendFailedNode() {
    for (auto localEntry : memberNode->memberList) {
        // create the dst address
        Address dstAddr;
        dstAddr.init();
        dstAddr.addr[0] = (char)localEntry.getid();
        dstAddr.addr[4] = (char)localEntry.getport();
        // create DELETE message
        MessageHdr *msg = new MessageHdr;
        size_t msgsize = sizeof(MessageHdr);
        msg->msgType = DELETE;
        msg->srcAddr = memberNode->addr;
        msg->endAddr = pingTarget;
        emulNet->ENsend(&memberNode->addr, &dstAddr, (char *)msg, msgsize);
        free(msg);
    }
    return true;
}

/**
 * FUNCTION NAME: startPing
 *
 * DESCRIPTION: Ping someone in a new cycle
 */
bool MP1Node::startPing() {
    // create pingTarget Address
    pingTarget.init();
    pingTarget.addr[0] = (char)memberNode->memberList[posInMemberList].getid();
    pingTarget.addr[4] =
        (char)memberNode->memberList[posInMemberList].getport();

    memberNode->pingCounter = TFAIL;
    finishedPING = false;
    // memberList and dst is not needed becasuse we just wanna an ACK from
    // the
    MessageHdr *msg = new MessageHdr;
    size_t msgsize = sizeof(MessageHdr);
    msg->msgType = PING;
    msg->srcAddr = memberNode->addr;
    msg->endAddr = memberNode->addr;
    emulNet->ENsend(&memberNode->addr, &pingTarget, (char *)msg, msgsize);
    free(msg);

    posInMemberList++;

    return true;
}

/**
 * FUNCTION NAME: startPingREQ
 *
 * DESCRIPTION: Start one cycle run during time period
 */
bool MP1Node::startPingREQ() {
    // create pingREQtarget Address
    Address pingREQtarget;
    pingREQtarget.init();
    pingREQtarget.addr[0] =
        (char)memberNode->memberList[posInMemberList].getid();
    pingREQtarget.addr[4] =
        (char)memberNode->memberList[posInMemberList].getport();

    MessageHdr *msg = new MessageHdr;
    size_t msgsize = sizeof(MessageHdr);
    msg->msgType = PING;
    msg->srcAddr = memberNode->addr;
    msg->endAddr = pingTarget;
    emulNet->ENsend(&memberNode->addr, &pingREQtarget, (char *)msg, msgsize);
    free(msg);

    memberNode->pingCounter = TFAIL;
    posInMemberList++;
    return true;
}

/**
 * FUNCTION NAME: nodeLoopOps
 *
 * DESCRIPTION: Check if any node hasn't responded within a timeout period
 * and then delete the nodes Propagate your membership list
 */
void MP1Node::nodeLoopOps() {
    if (memberNode->memberList.empty()) {
        return;
    }

    if (memberNode->timeOutCounter == 0) {
        memberNode->timeOutCounter = TREMOVE;

        if (!finishedPING) {
            // remove the failed node and send out information
            if (!deleteNode(&pingTarget)) {
                exit(1);
            }
            if (!sendFailedNode()) {
                exit(1);
            }
        }
    }

    if (posInMemberList == memberNode->memberList.size()) {
        // random time seed to boost performance
        srand(time(NULL));
        random_shuffle(memberNode->memberList.begin(),
                       memberNode->memberList.end());
        posInMemberList = 0;
    }

    if (memberNode->timeOutCounter == TREMOVE) {
        if (!startPing()) {
            exit(1);
        }
    }

    if (memberNode->timeOutCounter < TREMOVE && !finishedPING &&
        memberNode->pingCounter == 0) {
        if (!startPingREQ()) {
            exit(1);
        }
    }

    memberNode->pingCounter--;
    memberNode->timeOutCounter--;
    return;
}

/**
 * FUNCTION NAME: isNullAddress
 *
 * DESCRIPTION: Function checks if the address is NULL
 */
int MP1Node::isNullAddress(Address *addr) {
    return (memcmp(addr->addr, NULLADDR, 6) == 0 ? 1 : 0);
}

/**
 * FUNCTION NAME: getJoinAddress
 *
 * DESCRIPTION: Returns the Address of the coordinator
 */
Address MP1Node::getJoinAddress() {
    Address joinaddr;

    memset(&joinaddr, 0, sizeof(Address));
    *(int *)(&joinaddr.addr) = 1;
    *(short *)(&joinaddr.addr[4]) = 0;

    return joinaddr;
}

/**
 * FUNCTION NAME: initMemberListTable
 *
 * DESCRIPTION: Initialize the membership list
 */
void MP1Node::initMemberListTable(Member *memberNode) {
    memberNode->memberList.clear();
}