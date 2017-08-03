#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <errno.h>
#include <signal.h>
#include "MessagePacket.h"

#define ECHOMAX (1024)

/* クライアント（ユーザ）ID */
short IDCount = 1;

/* 参加者リストの内容 */
struct client {
  struct sockaddr_in clntAddr;
  char name[20];
  short nameSize;
  short clntID;
};

/* 参加者リスト */
struct clntList {
  struct client *data;
  struct clntList *following;
};

/* 参加者リストの先頭 */
struct clntList *clntList = {NULL};

/* ソケットディスクリプタ */
int sock;

/* SIGIO発生時のシグナルハンドラ */
void IOSignalHandler(int signo);

/* 新たなリストを追加する */
void insertList(struct clntList **pointer, struct sockaddr_in newClntAddr, char newName[20], short nameSize, short newClntID);

/* 指定したリストを削除する */
void deleteList(struct clntList **pointer);

int main(int argc, char *argv[])
{
  unsigned short servPort;        /* エコーサーバ(ローカル)のポート番号 */
  struct sockaddr_in servAddr;    /* エコーサーバ(ローカル)用アドレス構造体 */
  struct sigaction sigAction;     /* シグナルハンドラ設定用構造体 */

  /* 引数の数を確認する．*/
  if (argc != 2) {
    fprintf(stderr,"Usage: %s <Echo Port>\n", argv[0]);
    exit(1);
  }
  /* 第1引数からエコーサーバ(ローカル)のポート番号を取得する．*/
  servPort = atoi(argv[1]);
  
  /* メッセージの送受信に使うソケットを作成する．*/
  sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (sock < 0) {
    fprintf(stderr, "socket() failed\n");
    exit(1);
  }
  
  /* エコーサーバ(ローカル)用アドレス構造体へ必要な値を格納する．*/
  memset(&servAddr, 0, sizeof(servAddr));
  servAddr.sin_family      = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port        = htons(servPort);
  
  /* ソケットとエコーサーバ(ローカル)用アドレス構造体を結び付ける．*/
  if (bind(sock, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0) {
    fprintf(stderr, "bind() failed\n");
    exit(1);
  }
  
  /* シグナルハンドラを設定する．*/
  sigAction.sa_handler = IOSignalHandler;
  
  /* ハンドラ内でブロックするシグナルを設定する(全てのシグナルをブロックする)．*/
  if (sigfillset(&sigAction.sa_mask) < 0) {
    fprintf(stderr, "sigfillser() failed\n");
    exit(1);
  }
  
  /* シグナルハンドラに関するオプション指定は無し．*/
  sigAction.sa_flags = 0;
  
  /* シグナルハンドラ設定用構造体を使って，シグナルハンドラを登録する．*/
  if (sigaction(SIGIO, &sigAction, 0) < 0) {
    fprintf(stderr, "sigaction() failed\n");
    exit(1);
  }
  
  /* このプロセスがソケットに関するシグナルを受け取るための設定を行う．*/
  if (fcntl(sock, F_SETOWN, getpid()) < 0) {
    fprintf(stderr, "Unable to set process owner to us\n");
    exit(1);
  }
  
  /* ソケットに対してノンブロッキングと非同期I/Oの設定を行う．*/
  if (fcntl(sock, F_SETFL, O_NONBLOCK | FASYNC) < 0) {
    fprintf(stderr, "Unable to put the sock into nonblocking/async mode\n");
    exit(1);
  }
  
  /* エコーメッセージの受信と送信以外の処理をする．*/
  for (;;) {
  }
  
  /* ※このエコーサーバプログラムは，この部分には到達しない */
}

/* SIGIO 発生時のシグナルハンドラ */
void IOSignalHandler(int signo)
{
  struct sockaddr_in clntAddr; /* クライアント用アドレス構造体 */
  unsigned int clntAddrLen;    /* クライアント用アドレス構造体の長さ */
  char recvPktBuf[ECHOMAX];    /* パケット受信バッファ */
  char sendPktBuf[ECHOMAX];    /* パケット送信バッファ */
  char msgBuf[ECHOMAX - 4];    /* メッセージバッファの先頭番地 */
  int recvPktLen;              /* 受信パケットの長さ */
  int sendPktLen;              /* 送信パケットの長さ */
  int pktLen;                  /* 作成したパケットの長さ */
  int msgBufSize;              /* メッセージバッファの長さ */
  short msgID;                 /* メッセージの種類を示すID */
  short IDCount;               /* 割り振るクライアントID */
  char name[20];               /* 参加者の名前 */
  short nameSize;              /* 名前の文字列 */
  struct clntList **tmpList;   /* 参加者リスト */
  struct clntList **member;    /* ID確認用参加者リスト */
  short leaveID;               /* 退出するクライアント（ユーザ）のID */


  /* 受信データがなくなるまで，受信と送信を繰り返す．*/
  do {
    /* クライアント用アドレス構造体の長さを初期化する．*/
    clntAddrLen = sizeof(clntAddr);

    /* クライアントからメッセージを受信する．(※この呼び出しはブロックしない) */
    recvPktLen = recvfrom(sock, recvPktBuf, ECHOMAX, 0, (struct sockaddr *)&clntAddr, &clntAddrLen);

    /* 受信メッセージの長さを確認する．*/
    if (recvPktLen < 0) {
      /* errono が EWOULDBLOCK である場合，受信データが無くなったことを示す．*/
      /* EWOULDBLOCK は，許容できる唯一のエラー．*/
      if (errno != EWOULDBLOCK) {
        fprintf(stderr, "recvfrom() failed\n");
        exit(1);
      }
    } else {
      /* クライアントのIPアドレスを表示する．*/
      printf("Handling client %s\n", inet_ntoa(clntAddr.sin_addr));

      /* 受信メッセージの作成 */
      msgBufSize = Depacketize(recvPktBuf, recvPktLen, &msgID, msgBuf, 0);

      /* 受信したエコーメッセージをNULL文字で終端させる．*/
      msgBuf[msgBufSize] = '\0';

      /* 送信パケットを用意する */
      tmpList = &clntList;
      IDCount = 1;

      /* 参加要求を受け取った際の処理 */
      if (msgID == MSGID_JOIN_REQUEST) {
        while (*tmpList != NULL) {
          if (IDCount >= (*(*tmpList)->data).clntID) {
            tmpList = &((*tmpList)->following);
            IDCount++;
          } else {
            break;
          }
        }
        /* リストへ登録する情報を取得 */
        memcpy(&nameSize, &msgBuf[0], 2);
        memcpy(name, &msgBuf[2], 20);
        memcpy(&msgBuf[22], &IDCount, 2);
        /* リストを追加する */
        insertList(tmpList, clntAddr, name, nameSize, IDCount);
        pktLen = Packetize(MSGID_JOIN_RESPONSE , msgBuf, msgBufSize, sendPktBuf, ECHOMAX);
      } 

      /* 退出要求を受け取った際の処理を行う */
      if (msgID == MSGID_LEAVE_REQUEST ) {
        pktLen = Packetize(MSGID_LEAVE_RESPONSE, msgBuf, msgBufSize, sendPktBuf, ECHOMAX);
      }

      /* 通常チャット時の処理 */
      if (msgID == MSGID_CHAT_TEXT ){
        pktLen = Packetize(MSGID_CHAT_TEXT , msgBuf, msgBufSize, sendPktBuf, ECHOMAX);
      }

      /* 特定の参加者へのチャットを受け取った際の処理 */
      if (msgID == (short)MSGID_PRIVATE_CHAT_TEXT ){
        pktLen = Packetize((short)MSGID_PRIVATE_CHAT_TEXT  , msgBuf, msgBufSize, sendPktBuf, ECHOMAX);
      }

      if (msgID == (short)MSGID_USER_LIST_REQUEST ) {
        member = &clntList;
        /* 参加者リストを用意と送信処理を行う */ 
        while(*member != NULL) {
          /* この機能のとき限定で、参加者の名前とIDを格納する */
          memcpy(&msgBuf[0], &(*(*member)->data).nameSize, 2);
          memcpy(&msgBuf[2], (*(*member)->data).name, 20);
          memcpy(&msgBuf[24], &(*(*member)->data).clntID, 2);
          pktLen = Packetize((short)MSGID_USER_LIST_RESPONSE, msgBuf, msgBufSize, sendPktBuf, ECHOMAX);
          /* 参加者リストの送信 */
          tmpList = &clntList;
          while (*tmpList != NULL) {
            sendPktLen = sendto(sock, sendPktBuf, pktLen, 0, (struct sockaddr *)&((*tmpList)->data)->clntAddr, sizeof(((*tmpList)->data)->clntAddr));
            tmpList = &((*tmpList)->following);
          }

          /* 受信メッセージの長さと送信されたメッセージの長さが等しいことを確認する．*/
          if (recvPktLen != sendPktLen) {
            fprintf(stderr, "sendto() sent a defferent number of bytes than expected\n");
            exit(1);
          }
          member = &((*member)->following);
        }
      } else {
        /* 参加者リスト以外の送信 */
        tmpList = &clntList;
        while (*tmpList != NULL) {
          sendPktLen = sendto(sock, sendPktBuf, pktLen, 0, (struct sockaddr *)&((*tmpList)->data)->clntAddr, sizeof(((*tmpList)->data)->clntAddr));
          tmpList = &((*tmpList)->following);
        }
        /* 受信メッセージの長さと送信されたメッセージの長さが等しいことを確認する．*/
        if (recvPktLen != sendPktLen) {
          fprintf(stderr, "sendto() sent a defferent number of bytes than expected\n");
          exit(1);
        }
      }

      /* 退出者を参加者リストから削除する */
      if (msgID == MSGID_LEAVE_REQUEST ) {
        memcpy(&leaveID, &msgBuf[22], 2);
        tmpList = &clntList;
        while (leaveID != (*(*tmpList)->data).clntID) {
          tmpList = &((*tmpList)->following);
        }
        deleteList(tmpList);
      }

    }
  } while (recvPktLen >= 0);
}

/* 新たなリストを追加する */
void insertList(struct clntList **pointer, struct sockaddr_in newClntAddr, char newName[20], short newNameSize, short newClntID)
{
  struct clntList *newList;
  struct client *newdata;

  /* リストの値を用意する */
  newdata = (struct client *)malloc(sizeof(struct client));
  newdata->clntAddr = newClntAddr;
  strcpy(newdata->name, newName);
  newdata->nameSize = newNameSize;
  newdata->clntID = newClntID;

  /* リストを追加する */
  newList = (struct clntList *)malloc(sizeof(struct clntList));
  newList->data = newdata;
  newList->following = *pointer;
  *pointer = newList;
}

/* 指定したリストを削除する */
void deleteList(struct clntList **pointer)
{
  struct clntList *targetList;
  targetList = *pointer;
  *pointer = targetList->following;
  free((void *)targetList->data);
  free((void *)targetList);
}

/* 送信パケット生成関数 */
int Packetize(short msgID, char *msgBuf, short msgLen, char *pktBuf, int pktBufSize)
{
  if (strnlen(msgBuf, ECHOMAX - 4) <= ECHOMAX - 4) {
    memcpy(&pktBuf[0], &msgID, sizeof(short));
    memcpy(&pktBuf[2], &msgLen, sizeof(short));
    memcpy(&pktBuf[4], msgBuf, msgLen);
    return msgLen + 4;
  }
  printf("Message: <ERROR> too long.");
  return -1;
}

/* 受信メッセージ生成関数 */
int Depacketize(char *pktBuf, int pktLen,short *msgID, char *msgBuf, short msgBufSize)
{
  if (pktLen <= ECHOMAX) {
    memcpy(msgID, &pktBuf[0], sizeof(short));
    memcpy(&msgBufSize, &pktBuf[2], sizeof(short));
    memcpy(msgBuf, &pktBuf[4], msgBufSize);
    return msgBufSize;
  }
  printf("Message: <ERROR> too long.");
  return -1;
}