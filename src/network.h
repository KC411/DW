/************************************************************************
 * network.h      Header file for low-level networking routines         *
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

#ifndef __DP_NETWORK_H__
#define __DP_NETWORK_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/* Various includes necessary for select() calls */
#ifdef CYGWIN
#include <winsock2.h>
#include <windows.h>
#else
#include <sys/types.h>
/* Be careful not to include both sys/time.h and time.h on those systems
 * which don't like it */
#if TIME_WITH_SYS_TIME
#include <sys/time.h>
#include <time.h>
#else
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif
#endif

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#endif

#include <glib.h>

#include "error.h"

#ifdef NETWORKING
#include <curl/curl.h>

#ifndef SOCKET_ERROR
#define SOCKET_ERROR -1
#endif

#ifdef CYGWIN
/* Need GUI main loop to handle timers/events */
#include "gtkport/gtkport.h"
#else
#define dp_g_source_remove g_source_remove
#define dp_g_io_add_watch g_io_add_watch
#define dp_g_timeout_add g_timeout_add
#endif

typedef struct _CurlConnection {
  CURLM *multi;
  CURL *h;
  gboolean running;
  gchar *data;
  size_t data_size;
  char Terminator;              /* Character that separates messages */
  char StripChar;               /* Char that should be removed
                                 * from messages */
  GPtrArray *headers;

  guint timer_event;
  GSourceFunc timer_cb;
  GIOFunc socket_cb;
} CurlConnection;

typedef struct _ConnBuf {
  gchar *Data;                  /* bytes waiting to be read/written */
  gint Length;                  /* allocated length of the "Data" buffer */
  gint DataPresent;             /* number of bytes currently in "Data" */
} ConnBuf;

typedef struct _NetworkBuffer NetworkBuffer;

typedef void (*NBCallBack) (NetworkBuffer *NetBuf, gboolean Read,
                            gboolean Write, gboolean Exception,
                            gboolean CallNow);

typedef void (*NBUserPasswd) (NetworkBuffer *NetBuf, gpointer data);

/* Information about a SOCKS server */
typedef struct _SocksServer {
  gchar *name;                  /* hostname */
  int port;                     /* port number */
  int version;                  /* desired protocol version (usually
                                 * 4 or 5) */
  gboolean numuid;              /* if TRUE, send numeric user IDs rather
                                 * than names */
  char *user;                   /* if not blank, override the username
                                 * with this */
  gchar *authuser;              /* if set, the username for SOCKS5 auth */
  gchar *authpassword;          /* if set, the password for SOCKS5 auth */
} SocksServer;

/* The status of a network buffer */
typedef enum {
  NBS_PRECONNECT,               /* Socket is not yet connected */
  NBS_SOCKSCONNECT,             /* A CONNECT request is being sent to a
                                 * SOCKS server */
  NBS_CONNECTED                 /* Socket is connected */
} NBStatus;

/* Status of a SOCKS v5 negotiation */
typedef enum {
  NBSS_METHODS,                 /* Negotiation of available methods */
  NBSS_USERPASSWD,              /* Username-password request is being sent */
  NBSS_CONNECT                  /* CONNECT request is being sent */
} NBSocksStatus;

/* Handles reading and writing messages from/to a network connection */
struct _NetworkBuffer {
  int fd;                       /* File descriptor of the socket */
  GIOChannel *ioch;             /* GLib representation of the descriptor */
  gint InputTag;                /* Identifier for GLib event routines */
  NBCallBack CallBack;          /* Function called when the socket
                                 * status changes */
  gpointer CallBackData;        /* Data accessible to the callback
                                 * function */
  char Terminator;              /* Character that separates messages */
  char StripChar;               /* Char that should be removed
                                 * from messages */
  ConnBuf ReadBuf;              /* New data, waiting for the application */
  ConnBuf WriteBuf;             /* Data waiting to be written to the wire */
  ConnBuf negbuf;               /* Output for protocol negotiation
                                 * (e.g. SOCKS) */
  gboolean WaitConnect;         /* TRUE if a non-blocking connect is in
                                 * progress */
  NBStatus status;              /* Status of the connection (if any) */
  NBSocksStatus sockstat;       /* Status of SOCKS negotiation (if any) */
  SocksServer *socks;           /* If non-NULL, a SOCKS server to use */
  NBUserPasswd userpasswd;      /* Function to supply username and
                                 * password for SOCKS5 authentication */
  gpointer userpasswddata;      /* data to pass to the above function */
  gchar *host;                  /* If non-NULL, the host to connect to */
  unsigned port;                /* If non-NULL, the port to connect to */
  LastError *error;             /* Any error from the last operation */
};

void InitNetworkBuffer(NetworkBuffer *NetBuf, char Terminator,
                       char StripChar, SocksServer *socks);
void SetNetworkBufferCallBack(NetworkBuffer *NetBuf, NBCallBack CallBack,
                              gpointer CallBackData);
void SetNetworkBufferUserPasswdFunc(NetworkBuffer *NetBuf,
                                    NBUserPasswd userpasswd,
                                    gpointer data);
gboolean IsNetworkBufferActive(NetworkBuffer *NetBuf);
void BindNetworkBufferToSocket(NetworkBuffer *NetBuf, int fd);
gboolean StartNetworkBufferConnect(NetworkBuffer *NetBuf,
                                   const gchar *bindaddr,
                                   gchar *RemoteHost, unsigned RemotePort);
void ShutdownNetworkBuffer(NetworkBuffer *NetBuf);
void SetSelectForNetworkBuffer(NetworkBuffer *NetBuf, fd_set *readfds,
                               fd_set *writefds, fd_set *errorfds,
                               int *MaxSock);
gboolean RespondToSelect(NetworkBuffer *NetBuf, fd_set *readfds,
                         fd_set *writefds, fd_set *errorfds,
                         gboolean *DoneOK);
gboolean NetBufHandleNetwork(NetworkBuffer *NetBuf, gboolean ReadReady,
                             gboolean WriteReady, gboolean ErrorReady,
                             gboolean *DoneOK);
gboolean ReadDataFromWire(NetworkBuffer *NetBuf);
gboolean WriteDataToWire(NetworkBuffer *NetBuf);
void QueueMessageForSend(NetworkBuffer *NetBuf, gchar *data);
gint CountWaitingMessages(NetworkBuffer *NetBuf);
gchar *GetWaitingMessage(NetworkBuffer *NetBuf);
void SendSocks5UserPasswd(NetworkBuffer *NetBuf, gchar *user,
                          gchar *password);
gchar *GetWaitingData(NetworkBuffer *NetBuf, int numbytes);
gchar *PeekWaitingData(NetworkBuffer *NetBuf, int numbytes);
gchar *ExpandWriteBuffer(ConnBuf *conn, int numbytes, LastError **error);
void CommitWriteBuffer(NetworkBuffer *NetBuf, ConnBuf *conn, gchar *addpt,
                       guint addlen);

#define DOPE_CURL_ERROR dope_curl_error_quark()
GQuark dope_curl_error_quark(void);

#define DOPE_CURLM_ERROR dope_curlm_error_quark()
GQuark dope_curlm_error_quark(void);

void CurlInit(CurlConnection *conn);
void CurlCleanup(CurlConnection *conn);
gboolean OpenCurlConnection(CurlConnection *conn, char *URL, char *body,
                            GError **err);
void CloseCurlConnection(CurlConnection *conn);
gboolean CurlConnectionPerform(CurlConnection *conn, int *still_running,
                               GError **err);
gboolean CurlConnectionSocketAction(CurlConnection *conn, curl_socket_t fd,
                                    int action, int *still_running,
                                    GError **err);
char *CurlNextLine(CurlConnection *conn, char *ch);
void SetCurlCallback(CurlConnection *conn, GSourceFunc timer_cb,
                     GIOFunc socket_cb);

int CreateTCPSocket(LastError **error);
gboolean BindTCPSocket(int sock, const gchar *addr, unsigned port,
                       LastError **error);
void StartNetworking(void);
void StopNetworking(void);

#ifdef CYGWIN
#define CloseSocket(sock) closesocket(sock)
void SetReuse(SOCKET sock);
void SetBlocking(SOCKET sock, gboolean blocking);
#else
#define CloseSocket(sock) close(sock)
void SetReuse(int sock);
void SetBlocking(int sock, gboolean blocking);
#endif

void AddB64Enc(GString *str, gchar *unenc);

#endif /* NETWORKING */

#endif /* __DP_NETWORK_H__ */
