/************************************************************************
 * network.c      Low-level networking routines                         *
 * Copyright (C)  1998-2022  Ben Webb                                   *
 *                Email: benwebb@users.sf.net                           *
 *                WWW: https://dopewars.sourceforge.io/                 *
 *                                                                      *
 * This program is free software; you can redistribute it and/or        *
 * modify it under the terms of the GNU General Public License          *
 * as published by the Free Software Foundation; either version 2       *
 * of the License, or (at your option) any later version.               *
 *                                                                      *
 * This program is distributed in the hope that it will be useful,      *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of       *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the        *
 * GNU General Public License for more details.                         *
 *                                                                      *
 * You should have received a copy of the GNU General Public License    *
 * along with this program; if not, write to the Free Software          *
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston,               *
 *                   MA  02111-1307, USA.                               *
 ************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef NETWORKING

#ifdef CYGWIN
#include <winsock2.h>           /* For network functions */
#include <windows.h>            /* For datatypes such as BOOL */
#include "winmain.h"
#else
#include <sys/types.h>          /* For size_t etc. */
#include <sys/socket.h>         /* For struct sockaddr etc. */
#include <netinet/in.h>         /* For struct sockaddr_in etc. */
#include <arpa/inet.h>          /* For socklen_t */
#include <pwd.h>                /* For getpwuid */
#include <string.h>             /* For memcpy, strlen etc. */
#ifdef HAVE_UNISTD_H
#include <unistd.h>             /* For close(), various types and
                                 * constants */
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>              /* For fcntl() */
#endif
#include <netdb.h>              /* For gethostbyname() */
#endif /* CYGWIN */

#include <glib.h>
#include <errno.h>              /* For errno and Unix error codes */
#include <stdlib.h>             /* For exit() and atoi() */
#include <stdio.h>              /* For perror() */

#include "error.h"
#include "network.h"
#include "nls.h"

/* Maximum sizes (in bytes) of read and write buffers - connections should
 * be dropped if either buffer is filled */
#define MAXREADBUF   (32768)
#define MAXWRITEBUF  (65536)

/* SOCKS5 authentication method codes */
typedef enum {
  SM_NOAUTH = 0,                /* No authentication required */
  SM_GSSAPI = 1,                /* GSSAPI */
  SM_USERPASSWD = 2             /* Username/password authentication */
} SocksMethods;

static gboolean StartSocksNegotiation(NetworkBuffer *NetBuf,
                                      gchar *RemoteHost,
                                      unsigned RemotePort);
static gboolean StartConnect(int *fd, const gchar *bindaddr, gchar *RemoteHost,
                             unsigned RemotePort, gboolean *doneOK,
                             LastError **error);

#ifdef CYGWIN

void StartNetworking()
{
  WSADATA wsaData;
  LastError *error;
  GString *errstr;

  if (WSAStartup(MAKEWORD(1, 0), &wsaData) != 0) {
    error = NewError(ET_WINSOCK, WSAGetLastError(), NULL);
    errstr = g_string_new("");
    g_string_assign_error(errstr, error);
    g_log(NULL, G_LOG_LEVEL_CRITICAL, _("Cannot initialize WinSock (%s)!"),
          errstr->str);
    g_string_free(errstr, TRUE);
    FreeError(error);
    exit(EXIT_FAILURE);
  }
}

void StopNetworking()
{
  WSACleanup();
}

void SetReuse(SOCKET sock)
{
  BOOL i = TRUE;

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&i,
                 sizeof(i)) == -1) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
}

void SetBlocking(SOCKET sock, gboolean blocking)
{
  unsigned long param;

  param = blocking ? 0 : 1;
  ioctlsocket(sock, FIONBIO, &param);
}

#else

void StartNetworking()
{
}

void StopNetworking()
{
}

void SetReuse(int sock)
{
  int i = 1;

  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i)) == -1) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }
}

void SetBlocking(int sock, gboolean blocking)
{
  fcntl(sock, F_SETFL, blocking ? 0 : O_NONBLOCK);
}

#endif /* CYGWIN */

static gboolean FinishConnect(int fd, LastError **error);

static void NetBufCallBack(NetworkBuffer *NetBuf, gboolean CallNow)
{
  if (NetBuf && NetBuf->CallBack) {
    (*NetBuf->CallBack) (NetBuf, NetBuf->status != NBS_PRECONNECT,
                         (NetBuf->status == NBS_CONNECTED
                          && NetBuf->WriteBuf.DataPresent)
                         || (NetBuf->status == NBS_SOCKSCONNECT
                             && NetBuf->negbuf.DataPresent)
                         || NetBuf->WaitConnect, TRUE, CallNow);
  }
}

static void NetBufCallBackStop(NetworkBuffer *NetBuf)
{
  if (NetBuf && NetBuf->CallBack) {
    (*NetBuf->CallBack) (NetBuf, FALSE, FALSE, FALSE, FALSE);
  }
}

static void InitConnBuf(ConnBuf *buf)
{
  buf->Data = NULL;
  buf->Length = 0;
  buf->DataPresent = 0;
}

static void FreeConnBuf(ConnBuf *buf)
{
  g_free(buf->Data);
  InitConnBuf(buf);
}

/* 
 * Initializes the passed network buffer, ready for use. Messages sent
 * or received on the buffered connection will be terminated by the
 * given character, and if they end in "StripChar" it will be removed
 * before the messages are sent or received.
 */
void InitNetworkBuffer(NetworkBuffer *NetBuf, char Terminator,
                       char StripChar, SocksServer *socks)
{
  NetBuf->fd = -1;
  NetBuf->ioch = NULL;
  NetBuf->InputTag = 0;
  NetBuf->CallBack = NULL;
  NetBuf->CallBackData = NULL;
  NetBuf->Terminator = Terminator;
  NetBuf->StripChar = StripChar;
  InitConnBuf(&NetBuf->ReadBuf);
  InitConnBuf(&NetBuf->WriteBuf);
  InitConnBuf(&NetBuf->negbuf);
  NetBuf->WaitConnect = FALSE;
  NetBuf->status = NBS_PRECONNECT;
  NetBuf->socks = socks;
  NetBuf->host = NULL;
  NetBuf->userpasswd = NULL;
  NetBuf->error = NULL;
}

void SetNetworkBufferCallBack(NetworkBuffer *NetBuf, NBCallBack CallBack,
                              gpointer CallBackData)
{
  NetBufCallBackStop(NetBuf);
  NetBuf->CallBack = CallBack;
  NetBuf->CallBackData = CallBackData;
  NetBufCallBack(NetBuf, FALSE);
}

/* 
 * Sets the function used to obtain a username and password for SOCKS5
 * username/password authentication.
 */
void SetNetworkBufferUserPasswdFunc(NetworkBuffer *NetBuf,
                                    NBUserPasswd userpasswd, gpointer data)
{
  NetBuf->userpasswd = userpasswd;
  NetBuf->userpasswddata = data;
}

/* 
 * Sets up the given network buffer to handle data being sent/received
 * through the given socket.
 */
void BindNetworkBufferToSocket(NetworkBuffer *NetBuf, int fd)
{
  NetBuf->fd = fd;
#ifdef CYGIN
  NetBuf->ioch = g_io_channel_win32_new_socket(fd);
#else
  NetBuf->ioch = g_io_channel_unix_new(fd);
#endif

  SetBlocking(fd, FALSE);       /* We only deal with non-blocking sockets */
  NetBuf->status = NBS_CONNECTED;       /* Assume the socket is connected */
}

/* 
 * Returns TRUE if the pointer is to a valid network buffer, and it's
 * connected to an active socket.
 */
gboolean IsNetworkBufferActive(NetworkBuffer *NetBuf)
{
  return (NetBuf && NetBuf->fd >= 0);
}

gboolean StartNetworkBufferConnect(NetworkBuffer *NetBuf,
                                   const gchar *bindaddr,
                                   gchar *RemoteHost, unsigned RemotePort)
{
  gchar *realhost;
  unsigned realport;
  gboolean doneOK;

  ShutdownNetworkBuffer(NetBuf);

  if (NetBuf->socks) {
    realhost = NetBuf->socks->name;
    realport = NetBuf->socks->port;
  } else {
    realhost = RemoteHost;
    realport = RemotePort;
  }

  if (StartConnect(&NetBuf->fd, bindaddr, realhost, realport, &doneOK,
                   &NetBuf->error)) {
#ifdef CYGIN
    NetBuf->ioch = g_io_channel_win32_new_socket(NetBuf->fd);
#else
    NetBuf->ioch = g_io_channel_unix_new(NetBuf->fd);
#endif
    /* If we connected immediately, then set status, otherwise signal that 
     * we're waiting for the connect to complete */
    if (doneOK) {
      NetBuf->status = NetBuf->socks ? NBS_SOCKSCONNECT : NBS_CONNECTED;
      NetBuf->sockstat = NBSS_METHODS;
    } else {
      NetBuf->WaitConnect = TRUE;
    }

    if (NetBuf->socks
        && !StartSocksNegotiation(NetBuf, RemoteHost, RemotePort)) {
      return FALSE;
    }

    /* Notify the owner if necessary to check for the connection
     * completing and/or for data to be writeable */
    NetBufCallBack(NetBuf, FALSE);

    return TRUE;
  } else {
    return FALSE;
  }
}

/* 
 * Frees the network buffer's data structures (leaving it in the
 * 'initialized' state) and closes the accompanying socket.
 */
void ShutdownNetworkBuffer(NetworkBuffer *NetBuf)
{
  NetBufCallBackStop(NetBuf);

  if (NetBuf->fd >= 0) {
    CloseSocket(NetBuf->fd);
    g_io_channel_unref(NetBuf->ioch);
    NetBuf->fd = -1;
  }

  FreeConnBuf(&NetBuf->ReadBuf);
  FreeConnBuf(&NetBuf->WriteBuf);
  FreeConnBuf(&NetBuf->negbuf);

  FreeError(NetBuf->error);
  NetBuf->error = NULL;

  g_free(NetBuf->host);

  InitNetworkBuffer(NetBuf, NetBuf->Terminator, NetBuf->StripChar,
                    NetBuf->socks);
}

/* 
 * Updates the sets of read and write file descriptors to monitor
 * input to/output from the given network buffer. MaxSock is updated
 * with the highest-numbered file descriptor (plus 1) for use in a
 * later select() call.
 */
void SetSelectForNetworkBuffer(NetworkBuffer *NetBuf, fd_set *readfds,
                               fd_set *writefds, fd_set *errorfds,
                               int *MaxSock)
{
  if (!NetBuf || NetBuf->fd <= 0)
    return;
  FD_SET(NetBuf->fd, readfds);
  if (errorfds)
    FD_SET(NetBuf->fd, errorfds);
  if (NetBuf->fd >= *MaxSock)
    *MaxSock = NetBuf->fd + 1;
  if ((NetBuf->status == NBS_CONNECTED && NetBuf->WriteBuf.DataPresent)
      || (NetBuf->status == NBS_SOCKSCONNECT && NetBuf->negbuf.DataPresent)
      || NetBuf->WaitConnect) {
    FD_SET(NetBuf->fd, writefds);
  }
}

typedef enum {
  SEC_5FAILURE = 1,
  SEC_5RULESET = 2,
  SEC_5NETDOWN = 3,
  SEC_5UNREACH = 4,
  SEC_5CONNREF = 5,
  SEC_5TTLEXPIRED = 6,
  SEC_5COMMNOSUPP = 7,
  SEC_5ADDRNOSUPP = 8,

  SEC_REJECT = 91,
  SEC_NOIDENTD = 92,
  SEC_IDMISMATCH = 93,

  SEC_UNKNOWN = 200,
  SEC_AUTHFAILED,
  SEC_USERCANCEL,
  SEC_ADDRTYPE,
  SEC_REPLYVERSION,
  SEC_VERSION,
  SEC_NOMETHODS
} SocksErrorCode;

static ErrTable SocksErrStr[] = {
  /* SOCKS version 5 error messages */
  {SEC_5FAILURE, N_("SOCKS server general failure")},
  {SEC_5RULESET, N_("Connection denied by SOCKS ruleset")},
  {SEC_5NETDOWN, N_("SOCKS: Network unreachable")},
  {SEC_5UNREACH, N_("SOCKS: Host unreachable")},
  {SEC_5CONNREF, N_("SOCKS: Connection refused")},
  {SEC_5TTLEXPIRED, N_("SOCKS: TTL expired")},
  {SEC_5COMMNOSUPP, N_("SOCKS: Command not supported")},
  {SEC_5ADDRNOSUPP, N_("SOCKS: Address type not supported")},
  {SEC_NOMETHODS, N_("SOCKS server rejected all offered methods")},
  {SEC_ADDRTYPE, N_("Unknown SOCKS address type returned")},
  {SEC_AUTHFAILED, N_("SOCKS authentication failed")},
  {SEC_USERCANCEL, N_("SOCKS authentication canceled by user")},

  /* SOCKS version 4 error messages */
  {SEC_REJECT, N_("SOCKS: Request rejected or failed")},
  {SEC_NOIDENTD, N_("SOCKS: Rejected - unable to contact identd")},
  {SEC_IDMISMATCH,
   N_("SOCKS: Rejected - identd reports different user-id")},

  /* SOCKS errors due to protocol violations */
  {SEC_UNKNOWN, N_("Unknown SOCKS reply code")},
  {SEC_REPLYVERSION, N_("Unknown SOCKS reply version code")},
  {SEC_VERSION, N_("Unknown SOCKS server version")},
  {0, NULL}
};

static void SocksAppendError(GString *str, LastError *error)
{
  LookupErrorCode(str, error->code, SocksErrStr, _("SOCKS error code %d"));
}

static ErrorType ETSocks = { SocksAppendError, NULL };

static gboolean Socks5UserPasswd(NetworkBuffer *NetBuf)
{
  if (!NetBuf->userpasswd) {
    SetError(&NetBuf->error, &ETSocks, SEC_NOMETHODS, NULL);
    return FALSE;
  } else {
    /* Request a username and password (the callback function should in
     * turn call SendSocks5UserPasswd when it's done) */
    NetBuf->sockstat = NBSS_USERPASSWD;
    (*NetBuf->userpasswd) (NetBuf, NetBuf->userpasswddata);
    return TRUE;
  }
}

void SendSocks5UserPasswd(NetworkBuffer *NetBuf, gchar *user,
                          gchar *password)
{
  gchar *addpt;
  guint addlen;
  ConnBuf *conn;

  if (!user || !password || !user[0] || !password[0]) {
    SetError(&NetBuf->error, &ETSocks, SEC_USERCANCEL, NULL);
    NetBufCallBack(NetBuf, TRUE);
    return;
  }
  conn = &NetBuf->negbuf;
  addlen = 3 + strlen(user) + strlen(password);
  addpt = ExpandWriteBuffer(conn, addlen, &NetBuf->error);
  if (!addpt || strlen(user) > 255 || strlen(password) > 255) {
    SetError(&NetBuf->error, ET_CUSTOM, E_FULLBUF, NULL);
    NetBufCallBack(NetBuf, TRUE);
    return;
  }
  addpt[0] = 1;                 /* Subnegotiation version code */
  addpt[1] = strlen(user);
  strcpy(&addpt[2], user);
  addpt[2 + strlen(user)] = strlen(password);
  strcpy(&addpt[3 + strlen(user)], password);

  CommitWriteBuffer(NetBuf, conn, addpt, addlen);
}

static gboolean Socks5Connect(NetworkBuffer *NetBuf)
{
  gchar *addpt;
  guint addlen, hostlen;
  ConnBuf *conn;
  unsigned short int netport;

  conn = &NetBuf->negbuf;
  g_assert(NetBuf->host);
  hostlen = strlen(NetBuf->host);
  if (hostlen > 255)
    return FALSE;

  netport = htons(NetBuf->port);
  g_assert(sizeof(netport) == 2);

  addlen = hostlen + 7;
  addpt = ExpandWriteBuffer(conn, addlen, &NetBuf->error);
  if (!addpt)
    return FALSE;
  addpt[0] = 5;                 /* SOCKS version 5 */
  addpt[1] = 1;                 /* CONNECT */
  addpt[2] = 0;                 /* reserved - must be zero */
  addpt[3] = 3;                 /* Address type - FQDN */
  addpt[4] = hostlen;           /* Length of address */
  strcpy(&addpt[5], NetBuf->host);
  memcpy(&addpt[5 + hostlen], &netport, sizeof(netport));

  NetBuf->sockstat = NBSS_CONNECT;

  CommitWriteBuffer(NetBuf, conn, addpt, addlen);

  return TRUE;
}

static gboolean HandleSocksReply(NetworkBuffer *NetBuf)
{
  gchar *data;
  guchar addrtype;
  guint replylen;
  gboolean retval = TRUE;

  if (NetBuf->socks->version == 5) {
    if (NetBuf->sockstat == NBSS_METHODS) {
      data = GetWaitingData(NetBuf, 2);
      if (data) {
        retval = FALSE;
        if (data[0] != 5) {
          SetError(&NetBuf->error, &ETSocks, SEC_VERSION, NULL);
        } else if (data[1] == SM_NOAUTH) {
          retval = Socks5Connect(NetBuf);
        } else if (data[1] == SM_USERPASSWD) {
          retval = Socks5UserPasswd(NetBuf);
        } else {
          SetError(&NetBuf->error, &ETSocks, SEC_NOMETHODS, NULL);
        }
        g_free(data);
      }
    } else if (NetBuf->sockstat == NBSS_USERPASSWD) {
      data = GetWaitingData(NetBuf, 2);
      if (data) {
        retval = FALSE;
        if (data[1] != 0) {
          SetError(&NetBuf->error, &ETSocks, SEC_AUTHFAILED, NULL);
        } else {
          retval = Socks5Connect(NetBuf);
        }
        g_free(data);
      }
    } else if (NetBuf->sockstat == NBSS_CONNECT) {
      data = PeekWaitingData(NetBuf, 5);
      if (data) {
        retval = FALSE;
        addrtype = data[3];
        if (data[0] != 5) {
          SetError(&NetBuf->error, &ETSocks, SEC_VERSION, NULL);
        } else if (data[1] > 8) {
          SetError(&NetBuf->error, &ETSocks, SEC_UNKNOWN, NULL);
        } else if (data[1] != 0) {
          SetError(&NetBuf->error, &ETSocks, data[1], NULL);
        } else if (addrtype != 1 && addrtype != 3 && addrtype != 4) {
          SetError(&NetBuf->error, &ETSocks, SEC_ADDRTYPE, NULL);
        } else {
          retval = TRUE;
          replylen = 6;
          if (addrtype == 1)
            replylen += 4;      /* IPv4 address */
          else if (addrtype == 4)
            replylen += 16;     /* IPv6 address */
          else
            replylen += data[4];        /* FQDN */
          data = GetWaitingData(NetBuf, replylen);
          if (data) {
            NetBuf->status = NBS_CONNECTED;
            g_free(data);
            NetBufCallBack(NetBuf, FALSE);      /* status has changed */
          }
        }
      }
    }
    return retval;
  } else {
    data = GetWaitingData(NetBuf, 8);
    if (data) {
      retval = FALSE;
      if (data[0] != 0) {
        SetError(&NetBuf->error, &ETSocks, SEC_REPLYVERSION, NULL);
      } else {
        if (data[1] == 90) {
          NetBuf->status = NBS_CONNECTED;
          NetBufCallBack(NetBuf, FALSE);        /* status has changed */
          retval = TRUE;
        } else if (data[1] >= SEC_REJECT && data[1] <= SEC_IDMISMATCH) {
          SetError(&NetBuf->error, &ETSocks, data[1], NULL);
        } else {
          SetError(&NetBuf->error, &ETSocks, SEC_UNKNOWN, NULL);
        }
      }
      g_free(data);
    }
    return retval;
  }
}

/* 
 * Reads and writes data if the network connection is ready. Sets the
 * various OK variables to TRUE if no errors occurred in the relevant
 * operations, and returns TRUE if data was read and is waiting for
 * processing.
 */
static gboolean DoNetworkBufferStuff(NetworkBuffer *NetBuf,
                                     gboolean ReadReady,
                                     gboolean WriteReady,
                                     gboolean ErrorReady, gboolean *ReadOK,
                                     gboolean *WriteOK, gboolean *ErrorOK)
{
  gboolean DataWaiting = FALSE, ConnectDone = FALSE;
  gboolean retval;

  *ReadOK = *WriteOK = *ErrorOK = TRUE;

  if (ErrorReady || NetBuf->error)
    *ErrorOK = FALSE;
  else if (NetBuf->WaitConnect) {
    if (WriteReady) {
      retval = FinishConnect(NetBuf->fd, &NetBuf->error);
      ConnectDone = TRUE;
      NetBuf->WaitConnect = FALSE;

      if (retval) {
        if (NetBuf->socks) {
          NetBuf->status = NBS_SOCKSCONNECT;
          NetBuf->sockstat = NBSS_METHODS;
        } else {
          NetBuf->status = NBS_CONNECTED;
        }
      } else {
        NetBuf->status = NBS_PRECONNECT;
        *WriteOK = FALSE;
      }
    }
  } else {
    if (WriteReady)
      *WriteOK = WriteDataToWire(NetBuf);

    if (ReadReady) {
      *ReadOK = ReadDataFromWire(NetBuf);
      if (NetBuf->ReadBuf.DataPresent > 0 &&
          NetBuf->status == NBS_SOCKSCONNECT) {
        if (!HandleSocksReply(NetBuf)
            || NetBuf->error) { /* From SendSocks5UserPasswd, possibly */
          *ErrorOK = FALSE;
        }
      }
      if (NetBuf->ReadBuf.DataPresent > 0 &&
          NetBuf->status != NBS_SOCKSCONNECT) {
        DataWaiting = TRUE;
      }
    }
  }

  if (!(*ErrorOK && *WriteOK && *ReadOK)) {
    /* We don't want to check the socket any more */
    NetBufCallBackStop(NetBuf);
    /* If there were errors, then the socket is now useless - so close it */
    CloseSocket(NetBuf->fd);
    NetBuf->fd = -1;
  } else if (ConnectDone) {
    /* If we just connected, then no need to listen for write-ready status
     * any more */
    NetBufCallBack(NetBuf, FALSE);
  } else if (WriteReady
             && ((NetBuf->status == NBS_CONNECTED
                  && NetBuf->WriteBuf.DataPresent == 0)
                 || (NetBuf->status == NBS_SOCKSCONNECT
                     && NetBuf->negbuf.DataPresent == 0))) {
    /* If we wrote out everything, then tell the owner so that the socket
     * no longer needs to be checked for write-ready status */
    NetBufCallBack(NetBuf, FALSE);
  }

  return DataWaiting;
}

/* 
 * Responds to a select() call by reading/writing data as necessary.
 * If any data were read, TRUE is returned. "DoneOK" is set TRUE
 * unless a fatal error (i.e. the connection was broken) occurred.
 */
gboolean RespondToSelect(NetworkBuffer *NetBuf, fd_set *readfds,
                         fd_set *writefds, fd_set *errorfds,
                         gboolean *DoneOK)
{
  gboolean ReadOK, WriteOK, ErrorOK;
  gboolean DataWaiting = FALSE;

  *DoneOK = TRUE;
  if (!NetBuf || NetBuf->fd <= 0)
    return DataWaiting;
  DataWaiting = DoNetworkBufferStuff(NetBuf, FD_ISSET(NetBuf->fd, readfds),
                                     FD_ISSET(NetBuf->fd, writefds),
                                     errorfds ? FD_ISSET(NetBuf->fd,
                                                         errorfds) : FALSE,
                                     &ReadOK, &WriteOK, &ErrorOK);
  *DoneOK = (WriteOK && ErrorOK && ReadOK);
  return DataWaiting;
}

gboolean NetBufHandleNetwork(NetworkBuffer *NetBuf, gboolean ReadReady,
                             gboolean WriteReady, gboolean ErrorReady,
                             gboolean *DoneOK)
{
  gboolean ReadOK, WriteOK, ErrorOK;
  gboolean DataWaiting = FALSE;

  *DoneOK = TRUE;
  if (!NetBuf || NetBuf->fd <= 0)
    return DataWaiting;

  DataWaiting = DoNetworkBufferStuff(NetBuf, ReadReady, WriteReady, ErrorReady,
                                     &ReadOK, &WriteOK, &ErrorOK);

  *DoneOK = (WriteOK && ErrorOK && ReadOK);
  return DataWaiting;
}

/* 
 * Returns the number of complete (terminated) messages waiting in the
 * given network buffer. This is the number of times that
 * GetWaitingMessage() can be safely called without it returning NULL.
 */
gint CountWaitingMessages(NetworkBuffer *NetBuf)
{
  ConnBuf *conn;
  gint i, msgs = 0;

  if (NetBuf->status != NBS_CONNECTED)
    return 0;

  conn = &NetBuf->ReadBuf;

  if (conn->Data)
    for (i = 0; i < conn->DataPresent; i++) {
      if (conn->Data[i] == NetBuf->Terminator)
        msgs++;
    }
  return msgs;
}

gchar *PeekWaitingData(NetworkBuffer *NetBuf, int numbytes)
{
  ConnBuf *conn;

  conn = &NetBuf->ReadBuf;
  if (!conn->Data || conn->DataPresent < numbytes)
    return NULL;
  else
    return conn->Data;
}

gchar *GetWaitingData(NetworkBuffer *NetBuf, int numbytes)
{
  ConnBuf *conn;
  gchar *data;

  conn = &NetBuf->ReadBuf;
  if (!conn->Data || conn->DataPresent < numbytes)
    return NULL;

  data = g_new(gchar, numbytes);
  memcpy(data, conn->Data, numbytes);

  memmove(&conn->Data[0], &conn->Data[numbytes],
          conn->DataPresent - numbytes);
  conn->DataPresent -= numbytes;

  return data;
}

/* 
 * Reads a complete (terminated) message from the network buffer. The
 * message is removed from the buffer, and returned as a null-terminated
 * string (the network terminator is removed). If no complete message is
 * waiting, NULL is returned. The string is dynamically allocated, and
 * so must be g_free'd by the caller.
 */
gchar *GetWaitingMessage(NetworkBuffer *NetBuf)
{
  ConnBuf *conn;
  int MessageLen;
  char *SepPt;
  gchar *NewMessage;

  conn = &NetBuf->ReadBuf;
  if (!conn->Data || !conn->DataPresent || NetBuf->status != NBS_CONNECTED) {
    return NULL;
  }
  SepPt = memchr(conn->Data, NetBuf->Terminator, conn->DataPresent);
  if (!SepPt)
    return NULL;
  *SepPt = '\0';
  MessageLen = SepPt - conn->Data + 1;
  SepPt--;
  if (NetBuf->StripChar && *SepPt == NetBuf->StripChar)
    *SepPt = '\0';
  NewMessage = g_new(gchar, MessageLen);

  memcpy(NewMessage, conn->Data, MessageLen);
  if (MessageLen < conn->DataPresent) {
    memmove(&conn->Data[0], &conn->Data[MessageLen],
            conn->DataPresent - MessageLen);
  }
  conn->DataPresent -= MessageLen;
  return NewMessage;
}

/* 
 * Reads any waiting data on the given network buffer's TCP/IP connection
 * into the read buffer. Returns FALSE if the connection was closed, or
 * if the read buffer's maximum size was reached.
 */
gboolean ReadDataFromWire(NetworkBuffer *NetBuf)
{
  ConnBuf *conn;
  int CurrentPosition, BytesRead;

  conn = &NetBuf->ReadBuf;
  CurrentPosition = conn->DataPresent;
  while (1) {
    if (CurrentPosition >= conn->Length) {
      if (conn->Length == MAXREADBUF) {
        SetError(&NetBuf->error, ET_CUSTOM, E_FULLBUF, NULL);
        return FALSE;           /* drop connection */
      }
      if (conn->Length == 0)
        conn->Length = 256;
      else
        conn->Length *= 2;
      if (conn->Length > MAXREADBUF)
        conn->Length = MAXREADBUF;
      conn->Data = g_realloc(conn->Data, conn->Length);
    }
    BytesRead = recv(NetBuf->fd, &conn->Data[CurrentPosition],
                     conn->Length - CurrentPosition, 0);
    if (BytesRead == SOCKET_ERROR) {
#ifdef CYGWIN
      int Error = WSAGetLastError();

      if (Error == WSAEWOULDBLOCK)
        break;
      else {
        SetError(&NetBuf->error, ET_WINSOCK, Error, NULL);
        return FALSE;
      }
#else
      if (errno == EAGAIN)
        break;
      else if (errno != EINTR) {
        SetError(&NetBuf->error, ET_ERRNO, errno, NULL);
        return FALSE;
      }
#endif
    } else if (BytesRead == 0) {
      return FALSE;
    } else {
      CurrentPosition += BytesRead;
      conn->DataPresent = CurrentPosition;
    }
  }
  return TRUE;
}

gchar *ExpandWriteBuffer(ConnBuf *conn, int numbytes, LastError **error)
{
  int newlen;

  newlen = conn->DataPresent + numbytes;
  if (newlen > conn->Length) {
    conn->Length *= 2;
    conn->Length = MAX(conn->Length, newlen);
    if (conn->Length > MAXWRITEBUF)
      conn->Length = MAXWRITEBUF;
    if (newlen > conn->Length) {
      if (error)
        SetError(error, ET_CUSTOM, E_FULLBUF, NULL);
      return NULL;
    }
    conn->Data = g_realloc(conn->Data, conn->Length);
  }

  return (&conn->Data[conn->DataPresent]);
}

void CommitWriteBuffer(NetworkBuffer *NetBuf, ConnBuf *conn,
                       gchar *addpt, guint addlen)
{
  conn->DataPresent += addlen;

  /* If the buffer was empty before, we may need to tell the owner to
   * check the socket for write-ready status */
  if (NetBuf && addpt == conn->Data)
    NetBufCallBack(NetBuf, FALSE);
}

/* 
 * Writes the null-terminated string "data" to the network buffer, ready
 * to be sent to the wire when the network connection becomes free. The
 * message is automatically terminated. Fails to write the message without
 * error if the buffer reaches its maximum size (although this error will
 * be detected when an attempt is made to write the buffer to the wire).
 */
void QueueMessageForSend(NetworkBuffer *NetBuf, gchar *data)
{
  gchar *addpt;
  guint addlen;
  ConnBuf *conn;

  conn = &NetBuf->WriteBuf;

  if (!data)
    return;
  addlen = strlen(data) + 1;
  addpt = ExpandWriteBuffer(conn, addlen, NULL);
  if (!addpt)
    return;

  memcpy(addpt, data, addlen);
  addpt[addlen - 1] = NetBuf->Terminator;

  CommitWriteBuffer(NetBuf, conn, addpt, addlen);
}

static void SetNetworkError(LastError **error) {
#ifdef CYGWIN
  SetError(error, ET_WINSOCK, WSAGetLastError(), NULL);
#else
  SetError(error, ET_HERRNO, h_errno, NULL);
#endif
}

static struct hostent *LookupHostname(const gchar *host, LastError **error)
{
  struct hostent *he;

  if ((he = gethostbyname(host)) == NULL && error) {
    SetNetworkError(error);
  }
  return he;
}

gboolean StartSocksNegotiation(NetworkBuffer *NetBuf, gchar *RemoteHost,
                               unsigned RemotePort)
{
  guint num_methods;
  ConnBuf *conn;
  struct hostent *he;
  gchar *addpt;
  guint addlen, i;
  struct in_addr *haddr;
  unsigned short int netport;
  gchar *username = NULL;

#ifdef CYGWIN
  DWORD bufsize;
#else
  struct passwd *pwd;
#endif

  conn = &NetBuf->negbuf;

  if (NetBuf->socks->version == 5) {
    num_methods = 1;
    if (NetBuf->userpasswd)
      num_methods++;
    addlen = 2 + num_methods;
    addpt = ExpandWriteBuffer(conn, addlen, &NetBuf->error);
    if (!addpt)
      return FALSE;
    addpt[0] = 5;               /* SOCKS version 5 */
    addpt[1] = num_methods;
    i = 2;
    addpt[i++] = SM_NOAUTH;
    if (NetBuf->userpasswd)
      addpt[i++] = SM_USERPASSWD;

    g_free(NetBuf->host);
    NetBuf->host = g_strdup(RemoteHost);
    NetBuf->port = RemotePort;

    CommitWriteBuffer(NetBuf, conn, addpt, addlen);

    return TRUE;
  }

  he = LookupHostname(RemoteHost, &NetBuf->error);
  if (!he)
    return FALSE;

  if (NetBuf->socks->user && NetBuf->socks->user[0]) {
    username = g_strdup(NetBuf->socks->user);
  } else {
#ifdef CYGWIN
    bufsize = 0;
    WNetGetUser(NULL, username, &bufsize);
    if (GetLastError() != ERROR_MORE_DATA) {
      SetError(&NetBuf->error, ET_WIN32, GetLastError(), NULL);
      return FALSE;
    } else {
      username = g_malloc(bufsize);
      if (WNetGetUser(NULL, username, &bufsize) != NO_ERROR) {
        SetError(&NetBuf->error, ET_WIN32, GetLastError(), NULL);
        return FALSE;
      }
    }
#else
    if (NetBuf->socks->numuid) {
      username = g_strdup_printf("%d", getuid());
    } else {
      pwd = getpwuid(getuid());
      if (!pwd || !pwd->pw_name)
        return FALSE;
      username = g_strdup(pwd->pw_name);
    }
#endif
  }
  addlen = 9 + strlen(username);

  haddr = (struct in_addr *)he->h_addr;
  g_assert(sizeof(struct in_addr) == 4);

  netport = htons(RemotePort);
  g_assert(sizeof(netport) == 2);

  addpt = ExpandWriteBuffer(conn, addlen, &NetBuf->error);
  if (!addpt)
    return FALSE;

  addpt[0] = 4;                 /* SOCKS version */
  addpt[1] = 1;                 /* CONNECT */
  memcpy(&addpt[2], &netport, sizeof(netport));
  memcpy(&addpt[4], haddr, sizeof(struct in_addr));
  strcpy(&addpt[8], username);
  g_free(username);
  addpt[addlen - 1] = '\0';

  CommitWriteBuffer(NetBuf, conn, addpt, addlen);

  return TRUE;
}

static gboolean WriteBufToWire(NetworkBuffer *NetBuf, ConnBuf *conn)
{
  int CurrentPosition, BytesSent;

  if (!conn->Data || !conn->DataPresent)
    return TRUE;
  if (conn->Length == MAXWRITEBUF) {
    SetError(&NetBuf->error, ET_CUSTOM, E_FULLBUF, NULL);
    return FALSE;
  }
  CurrentPosition = 0;
  while (CurrentPosition < conn->DataPresent) {
    BytesSent = send(NetBuf->fd, &conn->Data[CurrentPosition],
                     conn->DataPresent - CurrentPosition, 0);
    if (BytesSent == SOCKET_ERROR) {
#ifdef CYGWIN
      int Error = WSAGetLastError();

      if (Error == WSAEWOULDBLOCK)
        break;
      else {
        SetError(&NetBuf->error, ET_WINSOCK, Error, NULL);
        return FALSE;
      }
#else
      if (errno == EAGAIN)
        break;
      else if (errno != EINTR) {
        SetError(&NetBuf->error, ET_ERRNO, errno, NULL);
        return FALSE;
      }
#endif
    } else {
      CurrentPosition += BytesSent;
    }
  }
  if (CurrentPosition > 0 && CurrentPosition < conn->DataPresent) {
    memmove(&conn->Data[0], &conn->Data[CurrentPosition],
            conn->DataPresent - CurrentPosition);
  }
  conn->DataPresent -= CurrentPosition;
  return TRUE;
}

/* 
 * Writes any waiting data in the network buffer to the wire. Returns
 * TRUE on success, or FALSE if the buffer's maximum length is
 * reached, or the remote end has closed the connection.
 */
gboolean WriteDataToWire(NetworkBuffer *NetBuf)
{
  if (NetBuf->status == NBS_SOCKSCONNECT) {
    return WriteBufToWire(NetBuf, &NetBuf->negbuf);
  } else {
    return WriteBufToWire(NetBuf, &NetBuf->WriteBuf);
  }
}

static size_t MetaConnWriteFunc(void *contents, size_t size, size_t nmemb,
                                void *userp)
{
  size_t realsize = size * nmemb;
  CurlConnection *conn = (CurlConnection *)userp;
 
  conn->data = g_realloc(conn->data, conn->data_size + realsize + 1);
  memcpy(&(conn->data[conn->data_size]), contents, realsize);
  conn->data_size += realsize;
  conn->data[conn->data_size] = 0;
 
  return realsize;
}

static size_t MetaConnHeaderFunc(char *contents, size_t size, size_t nmemb,
                                 void *userp)
{
  size_t realsize = size * nmemb;
  CurlConnection *conn = (CurlConnection *)userp;

  gchar *str = g_strchomp(g_strndup(contents, realsize));
  g_ptr_array_add(conn->headers, (gpointer)str);
  return realsize;
}

void CurlInit(CurlConnection *conn)
{
  curl_global_init(CURL_GLOBAL_DEFAULT);
  conn->multi = curl_multi_init();
  conn->h = curl_easy_init();
  conn->running = FALSE;
  conn->Terminator = '\n';
  conn->StripChar = '\r';
  conn->data_size = 0;
  conn->headers = NULL;
  conn->timer_cb = NULL;
  conn->socket_cb = NULL;
}

void CloseCurlConnection(CurlConnection *conn)
{
  if (conn->running) {
    curl_multi_remove_handle(conn->multi, conn->h);
    g_free(conn->data);
    conn->data_size = 0;
    conn->running = FALSE;
    g_ptr_array_free(conn->headers, TRUE);
    conn->headers = NULL;
  }
}

void CurlCleanup(CurlConnection *conn)
{
  if (conn->running) {
    CloseCurlConnection(conn);
  }
  curl_easy_cleanup(conn->h);
  curl_multi_cleanup(conn->multi);
  curl_global_cleanup();
}

gboolean HandleCurlMultiReturn(CurlConnection *conn, CURLMcode mres,
                               GError **err)
{
  struct CURLMsg *m;
  if (mres != CURLM_OK && mres != CURLM_CALL_MULTI_PERFORM) {
    CloseCurlConnection(conn);
    g_set_error_literal(err, DOPE_CURLM_ERROR, mres, curl_multi_strerror(mres));
    return FALSE;
  }

  do {
    int msgq = 0;
    m = curl_multi_info_read(conn->multi, &msgq);
    if (m && m->msg == CURLMSG_DONE && m->data.result != CURLE_OK) {
      CloseCurlConnection(conn);
      g_set_error_literal(err, DOPE_CURL_ERROR, m->data.result,
                          curl_easy_strerror(m->data.result));
      return FALSE;
    }
  } while(m);
  
  return TRUE;
}

gboolean CurlConnectionPerform(CurlConnection *conn, int *still_running,
                               GError **err)
{
  CURLMcode mres = curl_multi_perform(conn->multi, still_running);
  return HandleCurlMultiReturn(conn, mres, err);
}

gboolean CurlConnectionSocketAction(CurlConnection *conn, curl_socket_t fd,
                                    int action, int *still_running,
                                    GError **err)
{
  CURLMcode mres = curl_multi_socket_action(conn->multi, fd, action,
                                            still_running);
  return HandleCurlMultiReturn(conn, mres, err);
}

GQuark dope_curl_error_quark(void)
{
  return g_quark_from_static_string("dope-curl-error-quark");
}

GQuark dope_curlm_error_quark(void)
{
  return g_quark_from_static_string("dope-curlm-error-quark");
}

gboolean CurlEasySetopt1(CURL *curl, CURLoption option, void *arg, GError **err)
{
  CURLcode res = curl_easy_setopt(curl, option, arg);
  if (res == CURLE_OK) {
    return TRUE;
  } else {
    g_set_error_literal(err, DOPE_CURL_ERROR, res, curl_easy_strerror(res));
    return FALSE;
  }
}

#ifdef CYGWIN
/* Set the path to TLS CA certificates. Without this, curl connections
   to the metaserver may fail on Windows as it cannot verify the
   certificate.
 */
static gboolean SetCaInfo(CurlConnection *conn, GError **err)
{
  gchar *bindir, *cainfo;
  gboolean ret;

  /* Point to a .crt file in the same directory as dopewars.exe */
  bindir = GetBinaryDir();
  cainfo = g_strdup_printf("%s\\ca-bundle.crt", bindir);
  g_free(bindir);

  ret = CurlEasySetopt1(conn->h, CURLOPT_CAINFO, cainfo, err);
  g_free(cainfo);
  return ret;
}
#endif

gboolean OpenCurlConnection(CurlConnection *conn, char *URL, char *body,
                            GError **err)
{
  /* If the previous connect hung for so long that it's still active, then
   * break the connection before we start a new one */
  if (conn->running) {
    CloseCurlConnection(conn);
  }

  if (conn->h) {
    int still_running;
    CURLMcode mres;
    if (body && !CurlEasySetopt1(conn->h, CURLOPT_COPYPOSTFIELDS, body, err)) {
      return FALSE;
    }

    if (!CurlEasySetopt1(conn->h, CURLOPT_URL, URL, err)
        || !CurlEasySetopt1(conn->h, CURLOPT_WRITEFUNCTION, MetaConnWriteFunc,
                            err)
        || !CurlEasySetopt1(conn->h, CURLOPT_WRITEDATA, conn, err)
        || !CurlEasySetopt1(conn->h, CURLOPT_HEADERFUNCTION,
                            MetaConnHeaderFunc, err)
#ifdef CYGWIN
        || !SetCaInfo(conn, err)
#endif
        || !CurlEasySetopt1(conn->h, CURLOPT_HEADERDATA, conn, err)) {
      return FALSE;
    }

    mres = curl_multi_add_handle(conn->multi, conn->h);
    if (mres != CURLM_OK && mres != CURLM_CALL_MULTI_PERFORM) {
      g_set_error_literal(err, DOPE_CURLM_ERROR, mres,
                          curl_multi_strerror(mres));
      return FALSE;
    }
    conn->data = g_malloc(1);
    conn->data_size = 0;
    conn->headers = g_ptr_array_new_with_free_func(g_free);
    conn->running = TRUE;
    if (conn->timer_cb) {
      /* If we set a callback, we must not do _perform, but wait for the cb */
      return TRUE;
    } else {
      return CurlConnectionPerform(conn, &still_running, err);
    }
  } else {
    g_set_error_literal(err, DOPE_CURLM_ERROR, 0, _("Could not init curl"));
    return FALSE;
  }
  return TRUE;
}

char *CurlNextLine(CurlConnection *conn, char *ch)
{
  char *sep_pt;
  if (!ch) return NULL;
  sep_pt = strchr(ch, conn->Terminator);
  if (sep_pt) {
    *sep_pt = '\0';
    if (sep_pt > ch && sep_pt[-1] == conn->StripChar) {
      sep_pt[-1] = '\0';
    }
    sep_pt++;
  }
  return sep_pt;
}

/* Information associated with a specific socket */
typedef struct _SockData {
  GIOChannel *ch;
  guint ev;
} SockData;

static int timer_function(CURLM *multi, long timeout_ms, void *userp)
{
  CurlConnection *g = userp;

  if (g->timer_event) {
    dp_g_source_remove(g->timer_event);
    g->timer_event = 0;
  }

  /* -1 means we should just delete our timer. */
  if (timeout_ms >= 0) {
    g->timer_event = dp_g_timeout_add(timeout_ms, g->timer_cb, g);
  }
  return 0;
}

/* Clean up the SockData structure */
static void remsock(SockData *f)
{
  if (!f) {
    return;
  }
  if (f->ev) {
    dp_g_source_remove(f->ev);
  }
  g_io_channel_unref(f->ch);
  g_free(f);
}

/* Assign information to a SockData structure */
static void setsock(SockData *f, curl_socket_t s, CURL *e, int act,
                    CurlConnection *g)
{
  GIOCondition kind =
    ((act & CURL_POLL_IN) ? G_IO_IN : 0) |
    ((act & CURL_POLL_OUT) ? G_IO_OUT : 0);

  if (f->ev) {
    dp_g_source_remove(f->ev);
  }
  f->ev = dp_g_io_add_watch(f->ch, kind, g->socket_cb, g);
}

/* Initialize a new SockData structure */
static void addsock(curl_socket_t s, CURL *easy, int action, CurlConnection *g)
{
  SockData *fdp = g_malloc0(sizeof(SockData));

#ifdef CYGIN
  fdp->ch = g_io_channel_win32_new_socket(s);
#else
  fdp->ch = g_io_channel_unix_new(s);
#endif
  setsock(fdp, s, easy, action, g);
  curl_multi_assign(g->multi, s, fdp);
}

static int socket_function(CURL *easy, curl_socket_t s, int  what, void *userp,
                           void *socketp)
{
  CurlConnection *g = userp;
  SockData *fdp = socketp;
  if (what == CURL_POLL_REMOVE) {
    remsock(fdp);
  } else if (!fdp) {
    addsock(s, easy, what, g);
  } else {
    setsock(fdp, s, easy, what, g);
  }
  return 0;
}

void SetCurlCallback(CurlConnection *conn, GSourceFunc timer_cb,
                     GIOFunc socket_cb)
{
  conn->timer_event = 0;
  conn->timer_cb = timer_cb;
  conn->socket_cb = socket_cb;

  curl_multi_setopt(conn->multi, CURLMOPT_TIMERFUNCTION, timer_function);
  curl_multi_setopt(conn->multi, CURLMOPT_TIMERDATA, conn);
  curl_multi_setopt(conn->multi, CURLMOPT_SOCKETFUNCTION, socket_function);
  curl_multi_setopt(conn->multi, CURLMOPT_SOCKETDATA, conn);
}

int CreateTCPSocket(LastError **error)
{
  int fd;

  fd = socket(AF_INET, SOCK_STREAM, 0);

  if (fd == SOCKET_ERROR && error) {
    SetNetworkError(error);
  }

  return fd;
}

gboolean BindTCPSocket(int sock, const gchar *addr, unsigned port,
                       LastError **error)
{
  struct sockaddr_in bindaddr;
  int retval;
  struct hostent *he;

  bindaddr.sin_family = AF_INET;
  bindaddr.sin_port = htons(port);
  if (addr && addr[0]) {
    he = LookupHostname(addr, error);
    if (!he) {
      return FALSE;
    }
    bindaddr.sin_addr = *((struct in_addr *)he->h_addr);
  } else {
    bindaddr.sin_addr.s_addr = INADDR_ANY;
  }
  memset(bindaddr.sin_zero, 0, sizeof(bindaddr.sin_zero));

  retval =
      bind(sock, (struct sockaddr *)&bindaddr, sizeof(struct sockaddr));

  if (retval == SOCKET_ERROR && error) {
    SetNetworkError(error);
  }

  return (retval != SOCKET_ERROR);
}

gboolean StartConnect(int *fd, const gchar *bindaddr, gchar *RemoteHost,
                      unsigned RemotePort, gboolean *doneOK, LastError **error)
{
  struct sockaddr_in ClientAddr;
  struct hostent *he;

  if (doneOK)
    *doneOK = FALSE;
  he = LookupHostname(RemoteHost, error);
  if (!he)
    return FALSE;

  *fd = CreateTCPSocket(error);
  if (*fd == SOCKET_ERROR)
    return FALSE;

  if (bindaddr && bindaddr[0] && !BindTCPSocket(*fd, bindaddr, 0, error)) {
    return FALSE;
  }

  ClientAddr.sin_family = AF_INET;
  ClientAddr.sin_port = htons(RemotePort);
  ClientAddr.sin_addr = *((struct in_addr *)he->h_addr);
  memset(ClientAddr.sin_zero, 0, sizeof(ClientAddr.sin_zero));

  SetBlocking(*fd, FALSE);

  if (connect(*fd, (struct sockaddr *)&ClientAddr,
              sizeof(struct sockaddr)) == SOCKET_ERROR) {
#ifdef CYGWIN
    int errcode = WSAGetLastError();

    if (errcode == WSAEWOULDBLOCK)
      return TRUE;
    else if (error)
      SetError(error, ET_WINSOCK, errcode, NULL);
#else
    if (errno == EINPROGRESS)
      return TRUE;
    else if (error)
      SetError(error, ET_ERRNO, errno, NULL);
#endif
    CloseSocket(*fd);
    *fd = -1;
    return FALSE;
  } else {
    if (doneOK)
      *doneOK = TRUE;
  }
  return TRUE;
}

gboolean FinishConnect(int fd, LastError **error)
{
  int errcode;

#ifdef CYGWIN
  errcode = WSAGetLastError();
  if (errcode == 0)
    return TRUE;
  else {
    if (error) {
      SetError(error, ET_WINSOCK, errcode, NULL);
    }
    return FALSE;
  }
#else
#ifdef HAVE_SOCKLEN_T
  socklen_t optlen;
#else
  int optlen;
#endif

  optlen = sizeof(errcode);
  if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &errcode, &optlen) == -1) {
    errcode = errno;
  }
  if (errcode == 0)
    return TRUE;
  else {
    if (error) {
      SetError(error, ET_ERRNO, errcode, NULL);
    }
    return FALSE;
  }
#endif /* CYGWIN */
}

static void AddB64char(GString *str, int c)
{
  if (c < 0)
    return;
  else if (c < 26)
    g_string_append_c(str, c + 'A');
  else if (c < 52)
    g_string_append_c(str, c - 26 + 'a');
  else if (c < 62)
    g_string_append_c(str, c - 52 + '0');
  else if (c == 62)
    g_string_append_c(str, '+');
  else
    g_string_append_c(str, '/');
}

/* 
 * Adds the plain text string "unenc" to the end of the GString "str",
 * using the Base64 encoding scheme.
 */
void AddB64Enc(GString *str, gchar *unenc)
{
  guint i;
  long value = 0;

  if (!unenc || !str)
    return;
  for (i = 0; i < strlen(unenc); i++) {
    value <<= 8;
    value |= (unsigned char)unenc[i];
    if (i % 3 == 2) {
      AddB64char(str, (value >> 18) & 0x3F);
      AddB64char(str, (value >> 12) & 0x3F);
      AddB64char(str, (value >> 6) & 0x3F);
      AddB64char(str, value & 0x3F);
      value = 0;
    }
  }
  if (i % 3 == 1) {
    AddB64char(str, (value >> 2) & 0x3F);
    AddB64char(str, (value << 4) & 0x3F);
    g_string_append(str, "==");
  } else if (i % 3 == 2) {
    AddB64char(str, (value >> 10) & 0x3F);
    AddB64char(str, (value >> 4) & 0x3F);
    AddB64char(str, (value << 2) & 0x3F);
    g_string_append_c(str, '=');
  }
}

#endif /* NETWORKING */
