#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <unistd.h>
#include "MessagePacket.h"

#define ECHOMAX (1024)

/* キーボードからの文字列入力・エコーサーバへの送信処理関数 */
int SendEchoMessage(int sock, char name[20], short nameSize, struct sockaddr_in *pServAddr, short ID, int *privateFlag);

/* ソケットからのメッセージ受信・表示処理関数 */
int ReceiveEchoMessage(int sock, struct sockaddr_in *pServAddr, short *ID);

int main(int argc, char *argv[])
{
  char selection[2] = "0";      /* 入室するかの確認 */
  char myName[20];              /* 自分の名前（チャットで利用するユーザ名） */
  short nameSize;               /* 名前の文字列 */
  char servIP[20];		          /* エコーサーバのIPアドレス */
  char servPort[10];            /* エコーサーバのポート番号 */

  int sock;			                /* ソケットディスクリプタ */
  struct sockaddr_in servAddr;	/* エコーサーバ用アドレス構造体 */

  int maxDescriptor;		        /* select関数が扱うディスクリプタの最大値 */
  fd_set fdSet;			            /* select関数が扱うディスクリプタの集合 */
  struct timeval tout;		      /* select関数におけるタイムアウト設定用構造体 */
  
  char pktBuf[ECHOMAX];         /* 受信パケットバッファの先頭番地 */
  char msgBuf[ECHOMAX - 4];     /* メッセージバッファの先頭番地 */
  int pktBufSize;               /* 送信用パケットバッファのサイズ */
  int sendPktLen;               /* 送信したパケット長 */
  short ID = 0;                 /* クライアントのID */
  int privateFlag = 0;            /* プライベートチャットのフラグ */
  
  while (strcmp(selection, "2") != 0) {
    /* 初期選択肢 */
    printf("1:Login, 2:Close\nPlease enter(1 or 2): ");
    scanf("%s", selection);

    if (atoi(selection) == 1) {
      printf("Please enter the IP address of the server: ");
      scanf("%s", servIP);
      printf("Please enter port number: ");
      scanf("%s", servPort);
      printf("Your Name: ");
      scanf("%s", myName);
      printf("\n");
      while(getchar() != '\n');

      /* メッセージの送受信に使うソケットを作成する．*/
      sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
      if (sock < 0) {
       fprintf(stderr, "socket() failed");
       exit(1);
     }

      /* エコーサーバ用アドレス構造体へ必要な値を格納する．*/
      memset(&servAddr, 0, sizeof(servAddr));		       /* 構造体をゼロで初期化 */
      servAddr.sin_family	= AF_INET;		               /* インターネットアドレスファミリ */
      servAddr.sin_addr.s_addr	= inet_addr(servIP);	 /* サーバのIPアドレス */
      servAddr.sin_port		= htons(atoi(servPort));     /* サーバのポート番号 */

      /* select関数で処理するディスクリプタの最大値として，ソケットの値を設定する．*/
     maxDescriptor = sock;

      /* 参加要求用パケットを作成 */
     nameSize = strlen(myName);
     memcpy(&msgBuf[0], &nameSize, 2);
     memcpy(&msgBuf[2], myName, 20);
     memcpy(&msgBuf[22], &ID, 2);
     pktBufSize = Packetize(MSGID_JOIN_REQUEST, msgBuf, 24, pktBuf, ECHOMAX);

      /* エコーサーバへ参加要求を送信する．*/
     sendPktLen = sendto(sock, pktBuf, pktBufSize, 0, (struct sockaddr*)&servAddr, sizeof(servAddr));

      /* 送信されるべき文字列の長さと送信されたメッセージの長さが等しいことを確認する．*/
     if (pktBufSize != sendPktLen) {
       fprintf(stderr, "sendto() sent a different number of bytes than expected.");
       return -1;
     }

     /* 追加機能の表示 */
     printf("--------------------\n");
     printf("Option function\n");
     printf("・When sending a message to an individual -> /private\n");
     printf("・When checking a member who is entering -> /member\n");
     printf("--------------------\n");
     printf("\n");

      /* 文字列入力・メッセージ送信，およびメッセージ受信・表示処理ループ */
     for (;;) {

	     /* ディスクリプタの集合を初期化して，キーボートとソケットを設定する．*/
	     FD_ZERO(&fdSet);	            /* ゼロクリア */
	     FD_SET(STDIN_FILENO, &fdSet); /* キーボード(標準入力)用ディスクリプタを設定 */
	     FD_SET(sock, &fdSet);	        /* ソケットディスクリプタを設定 */

	     /* タイムアウト値を設定する．*/
	     tout.tv_sec  = 2;       /* 秒 */
	     tout.tv_usec = 0;       /* マイクロ秒 */

	     /* 各ディスクリプタに対する入力があるまでブロックする．*/
       if (select(maxDescriptor + 1, &fdSet, NULL, NULL, &tout) == 0) {
	       /* タイムアウト */
         continue;
       }

	     /* キーボードからの入力を確認する．*/
       if (FD_ISSET(STDIN_FILENO, &fdSet)) {
	       /* キーボードからの入力があるので，文字列を読み込み，エコーサーバへ送信する．*/
         if (SendEchoMessage(sock, myName, strlen(myName), &servAddr, ID, &privateFlag)< 0) {
           break;
         }
       }

	     /* ソケットからの入力を確認する．*/
       if (FD_ISSET(sock, &fdSet)) {
	       /* ソケットからの入力があるので，メッセージを受信し，表示する．*/
         if (ReceiveEchoMessage(sock, &servAddr, &ID) < 0) {
           break;
         }
       }
     }
   } 
   /* 終了 */
   else if (strcmp(selection, "2") == 0) {
    printf("Close\n");
  } 
  /* 指定された文字以外を入力したとき */
  else {
    printf("Please choose from the specified choices\nPlease enter(1 or 2)\n");
  }
}

/* ソケットを閉じ，プログラムを終了する．*/
close(sock);
exit(0);
}

/* キーボードからの文字列入力・エコーサーバへのメッセージ送信処理関数　*/
int SendEchoMessage(int sock, char name[20], short nameSize, struct sockaddr_in *pServAddr, short ID, int *privateFlag)
{
  char pktBuf[ECHOMAX];     /* パケットバッファ */
  char msgBuf[ECHOMAX - 4]; /* エコーサーバへ送信する文字列 */
  char msg[ECHOMAX - 4];    /* メッセージ */
  int pktLen;               /* 受信パケットバッファに含まれる受信パケット長 */
  short msgSize;            /* メッセージバッファの文字列 */
  int sendPktLen;	          /* 送信メッセージの長さ */             
  short sendID;             /* プライベートチャットの送信先ID */

  /* キーボードからの入力を読み込む．(※改行コードも含まれる．) */
  if (fgets(msg, ECHOMAX - 4, stdin) == NULL) {
    /*「Control + D」が入力された．またはエラー発生．*/
    return -1;
  }
  
  /* msgBufの最後尾にある改行文字を削除する．*/
  msgSize = strlen(msg);
  strcpy(&msg[msgSize - 1], "\0");

  /* パケットを作成 */
  memcpy(&msgBuf[0], &nameSize, 2);
  memcpy(&msgBuf[2], name, 20);
  memcpy(&msgBuf[22], &ID, 2);

  /* 退出要求時 */
  if (strcmp(msg, "/quit") == 0) {
      pktLen = Packetize(MSGID_LEAVE_REQUEST, msgBuf, 24, pktBuf, ECHOMAX);
      printf("Logout\n");

  } 
  /* 特定の参加者に対するチャット送信時 */
  else if (strcmp(msg, "/private") == 0) {
    printf("Destination ID: ");
    scanf("%hd", &sendID);
    memcpy(&msgBuf[24], &sendID, 2);
    *privateFlag = 1;
    while(getchar() != '\n');
  } 
  /* 参加者情報要求時 */
  else if (strcmp(msg, "/member") == 0) {
    pktLen = Packetize((short)MSGID_USER_LIST_REQUEST, msgBuf, 26, pktBuf, ECHOMAX);
  } 
  /* 通常チャット時 */
  else {
    memcpy(&msgBuf[26], msg, msgSize);
    if (*privateFlag == 1) {
      pktLen = Packetize((short)MSGID_PRIVATE_CHAT_TEXT, msgBuf, msgSize + 26, pktBuf, ECHOMAX);
      *privateFlag = 0;
    } else {
      pktLen = Packetize(MSGID_CHAT_TEXT, msgBuf, msgSize + 26, pktBuf, ECHOMAX);
    }
  }

  /* パケットが正常に読み取れたかを確認する */
  if (pktLen < 0) {
    fprintf(stderr, "Packetize() failed");
    return -1;
  }

  /* エコーサーバへメッセージ(入力された文字列)を送信する．*/
  if (*privateFlag != 1) {
    sendPktLen = sendto(sock, pktBuf, pktLen, 0, (struct sockaddr*)pServAddr, sizeof(*pServAddr));
    
    /* 送信されるべき文字列の長さと送信されたメッセージの長さが等しいことを確認する．*/
    if (pktLen != sendPktLen) {
      fprintf(stderr, "sendto() sent a different number of bytes than expected");
      return -1;
    }
  }
  return 0;
}

/*
 * ソケットからのメッセージ受信・表示処理関数
 */
int ReceiveEchoMessage(int sock, struct sockaddr_in *pServAddr, short *ID)
{
  struct sockaddr_in fromAddr; /* メッセージ送信元用アドレス構造体 */
  unsigned int fromAddrLen;    /* メッセージ送信元用アドレス構造体の長さ */
  char pktBuf[ECHOMAX];	       /* パケット送受信バッファ */
  int recvPktLen;	             /* 受信メッセージの長さ */
  short msgID = MSGID_NONE;    /* メッセージID */
  char msgBuf[ECHOMAX - 4];    /* メッセージバッファ */
  char msg[ECHOMAX - 4];       /* チャット本文 */
  short msgBufSize;            /* メッセージバッファ長 */
  char name[20];               /* 送信元のクライアント名 */
  short nameSize;              /* クライアント名の長さ */
  short clntID;                /* チャットの送信元ID */
  short recvID;                /* プライベートチャットの送信先ID */

  /* エコーメッセージ送信元用アドレス構造体の長さを初期化する．*/
  fromAddrLen = sizeof(fromAddr);

  /* エコーメッセージを受信する．*/
  recvPktLen = recvfrom(sock, pktBuf, ECHOMAX, 0, (struct sockaddr*)&fromAddr, &fromAddrLen);
  if (recvPktLen < 0) {
    fprintf(stderr, "recvfrom() failed");
    return -1;
  }

  /* エコーメッセージの送信元がエコーサーバであることを確認する．*/
  if (fromAddr.sin_addr.s_addr != pServAddr->sin_addr.s_addr) {
    fprintf(stderr,"Error: received a packet from unknown source.\n");
    return -1;
  }

  /* パケットを読み込む */
  msgBufSize = Depacketize(pktBuf, recvPktLen, &msgID, msgBuf, 0);
  if (msgBufSize < 0) {
    fprintf(stderr, "Dpacketize() failed");
    return -1;
  }

  /* 受信したエコーメッセージをNULL文字で終端させる. */
  memcpy(&nameSize, &msgBuf[0], 2);
  memcpy(name, &msgBuf[2], nameSize);
  memcpy(&clntID, &msgBuf[22], 2);
  name[nameSize] = '\0';
  
  /* 参加要求への応答 */
  if (msgID == MSGID_JOIN_RESPONSE) {
    printf("--- %s has entered the chat room．---\n", name);
    if (*ID == 0) {
      *ID = clntID;
    }
  }
  
  /* 退出要求への応答 */
  if (msgID == MSGID_LEAVE_RESPONSE) {
    printf("--- %s has left the chat room．---\n", name);
    if (*ID == clntID) {
      printf("\n");
      *ID = 0;
      return -2;
    }
  }

  /* 通常チャットの受信 */
  if (msgID == MSGID_CHAT_TEXT) {
    memcpy(msg, &msgBuf[26], msgBufSize - 26);
    msg[msgBufSize - 26] = '\0';
    printf("Name: %s\n", name);
    printf("ID: %d\n", clntID);
    printf("Message: %s\n", msg);
    printf("--------------------\n");
  } 

  /* プライベートチャットの受信 */
  if (msgID == (short)MSGID_PRIVATE_CHAT_TEXT) {
    memcpy(&recvID, &msgBuf[24], 2);
    memcpy(msg, &msgBuf[26], msgBufSize - 26);
    msg[msgBufSize - 26] = '\0';
    if (*ID == recvID || *ID == clntID) {
      printf("[PRIVATE_CHAT]\n");
      printf("Name: %s\n", name);
      printf("ID: %d\n", clntID);
      printf("Message: %s\n", msg);
      printf("--------------------\n");
    }
  }

  /* 参加者リストの取得 */
  if (msgID == (short)MSGID_USER_LIST_RESPONSE) {
    if (*ID == clntID) {
      memcpy(&recvID, &msgBuf[24], 2);
      printf("ID: %d  Name: %s\n", recvID, name);
    }
  }

  return 0;
}

/* 送信パケット生成関数 */
int Packetize(short msgID, char *msgBuf, short msgLen, char *pktBuf, int pktBufSize)
{
  if (strnlen(msgBuf, ECHOMAX - 4) <= ECHOMAX - 4) {
    memcpy(&pktBuf[0], &msgID, 2);
    memcpy(&pktBuf[2], &msgLen, 2);
    memcpy(&pktBuf[4], msgBuf, msgLen);
    return msgLen + 4;
  }
  printf("Message: <ERROR> too long.");
  return -1;
}

/* 受信メッセージ生成関数 */
int Depacketize(char *pktBuf, int pktLen, short *msgID, char *msgBuf, short msgBufSize)
{
  if (pktLen <= ECHOMAX) {
    memcpy(msgID, &pktBuf[0], 2);
    memcpy(&msgBufSize, &pktBuf[2], 2);
    memcpy(msgBuf, &pktBuf[4], msgBufSize);
    return msgBufSize;
  }
  printf("Message: <ERROR> too long.");
  return -1;
}