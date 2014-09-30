/* SANE - Scanner Access Now Easy.

   Copyright (C) 2008  2012 by Louis Lagendijk

   This file is part of the SANE package.

   SANE is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   SANE is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
   License for more details.

   You should have received a copy of the GNU General Public License
   along with sane; see the file COPYING.  If not, write to the Free
   Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

   As a special exception, the authors of SANE give permission for
   additional uses of the libraries contained in this release of SANE.

   The exception is that, if you link a SANE library with other files
   to produce an executable, this does not by itself cause the
   resulting executable to be covered by the GNU General Public
   License.  Your use of that executable is in no way restricted on
   account of linking the SANE library code into it.

   This exception does not, however, invalidate any other reasons why
   the executable file might be covered by the GNU General Public
   License.

   If you submit changes to SANE to the maintainers to be included in
   a subsequent release, you agree by submitting the changes that
   those changes may be distributed with this exception intact.

   If you write modifications of your own for SANE, it is your choice
   whether to permit this exception to apply to your modifications.
   If you do not wish that, delete this exception notice.
*/
#undef BACKEND_NAME
#define BACKEND_NAME bjnp

#include  "../include/sane/config.h"
#include  "../include/sane/sane.h"

/*
 * Standard types etc
 */
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#include <unistd.h>
#include <stdio.h>
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

/* 
 * networking stuff
 */
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <net/if.h>
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>
#endif
#ifdef HAVE_SYS_SELSECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#include <errno.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "pixma_bjnp_private.h"
#include "pixma_bjnp.h"
/* #include "pixma_rename.h" */
#include "pixma.h"
#include "pixma_common.h"


/* static data */
static bjnp_device_t device[BJNP_NO_DEVICES];
static int bjnp_no_devices = 0;

/*
 * Private functions
 */

static void
u8tohex (char *string, const uint8_t *value, int len )
{
  int i;
  int x;
  const char hdigit[16] =
    { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd',
    'e', 'f'
  };
  for (i = 0; i < len; i++)
    {
      x = value[i];
      string[ 2 * i ] = hdigit[(x >> 4) & 0xf];
      string[ 2 * i + 1] = hdigit[x & 0xf];
    }
  string[2 * len ] = '\0';
}

static void
u32tohex (uint32_t x, char *str)
{
  uint8_t uint8[4];
   uint8[0]= (uint8_t) x >> 24;
   uint8[1] = (uint8_t)x >> 16;
   uint8[2] = (uint8_t)x >> 8;
   uint8[3] = (uint8_t)x ;
   u8tohex(str, uint8, 4);
}

static void
bjnp_hexdump (int level, const void *d_, unsigned len)
{
  const uint8_t *d = (const uint8_t *) (d_);
  unsigned ofs, c, plen;
  char line[100];               /* actually only 1+8+1+8*3+1+8*3+1 = 61 bytes needed */

  if (level > DBG_LEVEL)
    return;
  if (level == DBG_LEVEL)
    /* if debuglevel == exact match and buffer contains more than 3 lines, print 2 lines + .... */
    plen = (len > 64) ? 32: len;
  else
    plen = len;
  ofs = 0;
  while (ofs < plen)
    {
      char *p;
      line[0] = ' ';
      u32tohex (ofs, line + 1);
      line[9] = ':';
      p = line + 10;
      for (c = 0; c != 16 && (ofs + c) < plen; c++)
        {
          u8tohex (p, d + ofs + c, 1);
          p[2] = ' ';
          p += 3;
          if (c == 7)
            {
              p[0] = ' ';
              p++;
            }
        }
      p[0] = '\0';
      bjnp_dbg (level, "%s\n", line);
      ofs += c;
    }
  if (len > plen)
    bjnp_dbg(level, "......\n");
}

static int sa_is_equal( const bjnp_sockaddr_t * sa1, const bjnp_sockaddr_t * sa2)
{
  if ((sa1 == NULL) || (sa2 == NULL) )
    return 0;

  if (sa1->addr.sa_family == sa2-> addr.sa_family)
    {
      if( sa1 -> addr.sa_family == AF_INET)
        {
          if ( (sa1->ipv4.sin_port == sa2->ipv4.sin_port) &&
               (sa1->ipv4.sin_addr.s_addr == sa2->ipv4.sin_addr.s_addr))
            {
            return 1;
            }
        } 
#ifdef ENABLE_IPV6
      else if (sa1 -> addr.sa_family == AF_INET6 )
        {
          if ( (sa1-> ipv6.sin6_port == sa2->ipv6.sin6_port) &&
              (memcmp(&(sa1->ipv6.sin6_addr), &(sa2->ipv6.sin6_addr), sizeof(struct in6_addr)) == 0))
            {
              return 1;
            }
        } 
#endif
    }
    return 0; 
}

static int 
sa_size( const bjnp_sockaddr_t *sa)
{
  switch (sa -> addr.sa_family)
    {
      case AF_INET: 
        return (sizeof(struct sockaddr_in) );
#ifdef ENABLE_IPV6
      case AF_INET6:
        return (sizeof(struct sockaddr_in6) );
#endif
      default:
        /* should not occur */
        return sizeof( bjnp_sockaddr_t );
    }
}

static int
get_protocol_family( const bjnp_sockaddr_t *sa)
{
  switch (sa -> addr.sa_family)
    {
      case AF_INET:
        return PF_INET;
        break;
#ifdef ENABLE_IPV6
      case AF_INET6:
        return PF_INET6;
        break;
#endif
      default:
        /* should not occur */
        return -1;
    }
}

static void
get_address_info ( const bjnp_sockaddr_t *addr, char * addr_string, int *port)
{
  char tmp_addr[BJNP_HOST_MAX];
  if ( addr->addr.sa_family == AF_INET)
    {
      inet_ntop( AF_INET, &(addr -> ipv4.sin_addr.s_addr), addr_string, BJNP_HOST_MAX);
      *port = ntohs (addr->ipv4.sin_port);
    }
#ifdef ENABLE_IPV6
  else if (addr->addr.sa_family == AF_INET6)
    {
      inet_ntop( AF_INET6, addr -> ipv6.sin6_addr.s6_addr, tmp_addr, sizeof(tmp_addr) );

      if (IN6_IS_ADDR_LINKLOCAL( &(addr -> ipv6.sin6_addr) ) )
          sprintf(addr_string, "[%s%%%d]", tmp_addr, addr -> ipv6.sin6_scope_id);

      *port = ntohs (addr->ipv6.sin6_port);
    }
#endif
  else 
    {
      /* unknown address family, should not occur */
      strcpy(addr_string, "Unknown address family");
      *port = 0;
    }
}

static int
parse_IEEE1284_to_model (char *scanner_id, char *model)
{
/*
 * parses the  IEEE1284  ID of the scanner to retrieve make and model
 * of the scanner
 * Returns: 0 = not found
 *          1 = found, model is set
 */

  char s[BJNP_IEEE1284_MAX];
  char *tok;

  strcpy (s, scanner_id);
  model[0] = '\0';

  tok = strtok (s, ";");
  while (tok != NULL)
    {
      /* MDL contains make and model */

      if (strncmp (tok, "MDL:", 4) == 0)
	{
	  strcpy (model, tok + 4);
	  return 1;
	}
      tok = strtok (NULL, ";");
    }
  return 0;
}

static int
charTo2byte (char *d, const char *s, int len)
{
  /*
   * copy ASCII string to UTF-16 unicode string
   * len is length of destination buffer
   * Returns: number of characters copied
   */

  int done = 0;
  int copied = 0;
  int i;

  len = len / 2;
  for (i = 0; i < len; i++)
    {
      d[2 * i] = '\0';
      if (s[i] == '\0')
	{
	  done = 1;
	}
      if (done == 0)
	{
	  d[2 * i + 1] = s[i];
	  copied++;
	}
      else
	d[2 * i + 1] = '\0';
    }
  return copied;
}

static char *
getusername (void)
{
  static char noname[] = "sane_pixma";
  struct passwd *pwdent;

#ifdef HAVE_PWD_H
  if (((pwdent = getpwuid (geteuid ())) != NULL) && (pwdent->pw_name != NULL))
    return pwdent->pw_name;
#endif
  return noname;
}


static char *
determine_scanner_serial (const char *hostname, const char * mac_address, char *serial)
{
  char *dot;
  char copy[BJNP_HOST_MAX];

  /* determine a "serial number" for the scanner */
  /* if available we use the hostname or ipv4 address of the printer */
  /* if we only have a literal ipv6 address, we use the mac-address */

  strcpy(copy, hostname);
  while (strlen (copy) >= SHORT_HOSTNAME_MAX)
    {
      /* if this is a FQDN, not an ip-address, remove domain part of the name */
      if ((dot = strchr (copy, '.')) != NULL)
        {
	*dot = '\0';
        }
      else
        strcpy(copy, mac_address);
	break;
    }
  strcpy( serial, copy );
  return serial;
}

static int
bjnp_open_tcp (int devno)
{
  int sock;
  int val;
  bjnp_sockaddr_t *addr = device[devno].addr;
  char host[BJNP_HOST_MAX];
  int port;

  get_address_info( addr, host, &port);
  PDBG (bjnp_dbg (LOG_DEBUG, "bjnp_open_tcp: Setting up a TCP socket, dest: %s  port %d\n",
		   host, port ) );

  if ((sock = socket (get_protocol_family( addr ) , SOCK_STREAM, 0)) < 0)
    {
      PDBG (bjnp_dbg (LOG_CRIT, "bjnp_open_tcp: Can not create socket: %s\n",
		       strerror (errno)));
      return -1;
    }

  val = 1;
  setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof (val));

#if 0
  val = 1;
  setsockopt (sock, SOL_SOCKET, SO_REUSEPORT, &val, sizeof (val));

  val = 1;
#endif

  /*
   * Using TCP_NODELAY improves responsiveness, especially on systems
   * with a slow loopback interface...
   */

  val = 1;
  setsockopt (sock, IPPROTO_TCP, TCP_NODELAY, &val, sizeof (val));

/*
 * Close this socket when starting another process...
 */

  fcntl (sock, F_SETFD, FD_CLOEXEC); 

  if (connect
      (sock, &(addr->addr), sa_size(device[devno].addr) )!= 0)
    {
      PDBG (bjnp_dbg
	    (LOG_CRIT, "bjnp_open_tcp: Can not connect to scanner: %s\n",
	     strerror (errno)));
      return -1;
    }
  device[devno].tcp_socket = sock;
  return 0;
}

static int
split_uri (const char *devname, char *method, char *host, char *port,
	   char *args)
{
  char copy[1024];
  char *start;
  char next;
  int i;

  strcpy (copy, devname);
  start = copy;

/*
 * retrieve method
 */
  i = 0;
  while ((start[i] != '\0') && (start[i] != ':'))
    {
      i++;
    }

  if (((strncmp (start + i, "://", 3) != 0)) || (i > BJNP_METHOD_MAX -1 ))
    {
      PDBG (bjnp_dbg (LOG_NOTICE, "Can not find method in %s (offset %d)\n",
		       devname, i));
      return -1;
    }

  start[i] = '\0';
  strcpy (method, start);
  start = start + i + 3;

/*
 * retrieve host
 */

  if (start[0] == '[')
    {
      /* literal IPv6 address */

      char *end_of_address = strchr(start, ']'); 

      if ( ( end_of_address == NULL) || 
           ( (end_of_address[1] != ':') && (end_of_address[1] != '/' ) &&  (end_of_address[1] != '\0' )) ||
           ( (end_of_address - start) >= BJNP_HOST_MAX ) )
        {
          PDBG (bjnp_dbg (LOG_NOTICE, "Can not find hostname or address in %s\n", devname));
          return -1;
        }
      next = end_of_address[1];
      *end_of_address = '\0';
      strcpy(host, start + 1);
      start = end_of_address + 2;
    }
  else
    {
      i = 0;
      while ((start[i] != '\0') && (start[i] != '/') && (start[i] != ':'))
        {
          i++;
        }
      next = start[i];
      start[i] = '\0';
      if ((i == 0) || (i >= BJNP_HOST_MAX ) )
        {
          PDBG (bjnp_dbg (LOG_NOTICE, "Can not find hostname or address in %s\n", devname));
          return -1;
        }
      strcpy (host, start);
      start = start + i +1;
    }


/*
 * retrieve port number
 */

  if (next != ':')
    strcpy(port, "");
  else
    {
      char *end_of_port = strchr(start, '/');
      if (end_of_port == NULL) 
        { 
          next = '\0';
        }
      else
        {
          next = *end_of_port;
          *end_of_port = '\0';
        }
      if ((strlen(start) == 0) || (strlen(start) >= BJNP_PORT_MAX ) )
        {
          PDBG (bjnp_dbg (LOG_NOTICE, "Can not find port in %s (have \"%s\")\n", devname, start));
          return -1;
        }
      strcpy(port, start);
    }

/*
 * Retrieve arguments
 */

  if (next == '/')
    {
    i = strlen(start);
    if ( i >= BJNP_ARGS_MAX)
      {
        PDBG (bjnp_dbg (LOG_NOTICE, "Argument string too long in %s\n", devname));
      }
    strcpy (args, start);
    }
  else
    strcpy (args, "");
  return 0;
}



static void
set_cmd (int devno, struct BJNP_command *cmd, char cmd_code, int payload_len)
{
  /*
   * Set command buffer with command code, session_id and lenght of payload
   * Returns: sequence number of command
   */
  strncpy (cmd->BJNP_id, BJNP_STRING, sizeof (cmd->BJNP_id));
  cmd->dev_type = BJNP_CMD_SCAN;
  cmd->cmd_code = cmd_code;
  cmd->unknown1 = htons (0);
  if (devno == -1)
    {
      /* device not yet opened, use 0 for serial and session) */
      cmd->seq_no = htons (0);
      cmd->session_id = htons (0);
    }
  else
    {
      cmd->seq_no = htons (++(device[devno].serial));
      cmd->session_id = (cmd_code == CMD_UDP_POLL ) ? 0 : htons (device[devno].session_id);
      device[devno].last_cmd = cmd_code;
    }
  cmd->payload_len = htonl (payload_len);
}

static int
bjnp_setup_udp_socket ( const int dev_no )
{
  /*
   * Setup a udp socket for the given device
   * Returns the socket or -1 in case of error
   */

  int sockfd;
  char addr_string[256];
  int port;
  bjnp_sockaddr_t * addr = device[dev_no].addr;

  get_address_info( addr, addr_string, &port);

  PDBG (bjnp_dbg (LOG_DEBUG, "setup_udp_socket: Setting up a UDP socket, dest: %s  port %d\n",
		   addr_string, port ) );

  if ((sockfd = socket (get_protocol_family( addr ), SOCK_DGRAM, IPPROTO_UDP)) == -1)
    {
      PDBG (bjnp_dbg
	    (LOG_CRIT, "setup_udp_socket: can not open socket - %s\n",
	     strerror (errno)));
      return -1;
    }

  if (connect
      (sockfd, &(device[dev_no].addr->addr), sa_size(device[dev_no].addr) )!= 0)
    {
      PDBG (bjnp_dbg
	    (LOG_CRIT, "setup_udp_socket: connect failed- %s\n",
	     strerror (errno)));
      close(sockfd);
      return -1;
    }
  return sockfd;
}

static int
udp_command (const int dev_no, char *command, int cmd_len, char *response,
	     int resp_len)
{
  /* 
   * send udp command to given device and recieve the response`
   * returns: the legth of the response or -1 
   */
  int sockfd;
  struct timeval timeout;
  int result;
  int try, attempt;
  int numbytes;
  fd_set fdset;
  struct BJNP_command *resp = (struct BJNP_command *) response;
  struct BJNP_command *cmd = (struct BJNP_command *) command;
 
  if ( (sockfd = bjnp_setup_udp_socket(dev_no) ) == -1 )
    {
      PDBG (bjnp_dbg( LOG_CRIT, "udp_command: Can not setup socket\n") );
      return -1;
    }

  for (try = 0; try < BJNP_UDP_RETRY_MAX; try++)
    {
      if ((numbytes = send (sockfd, command, cmd_len, 0)) != cmd_len)
	{
	  PDBG (bjnp_dbg
		(LOG_NOTICE, "udp_command: Sent %d bytes, expected %d\n",
		 numbytes, cmd_len));
	  continue;
	}

      attempt = 0;

      /* wait for data to be received, ignore signals being received */
      /* skip late udp responses (they have an incorrect sequence number */
      do
	{
	  FD_ZERO (&fdset);
	  FD_SET (sockfd, &fdset);

	  timeout.tv_sec = BJNP_TIMEOUT_UDP;
	  timeout.tv_usec = 0; 
	}
      while (((result =
	       select (sockfd + 1, &fdset, NULL, NULL, &timeout)) <= 0)
	     && (errno == EINTR) && (attempt++ < BJNP_MAX_SELECT_ATTEMPTS) 
             && resp-> seq_no != cmd->seq_no);

      if (result <= 0)
	{
	  PDBG (bjnp_dbg
		(LOG_NOTICE, "udp_command: select failed: %s\n",
		 result == 0 ? "timed out" : strerror (errno)));
	  continue;
	}

      if ((numbytes = recv (sockfd, response, resp_len, 0)) == -1)
	{
	  PDBG (bjnp_dbg
		(LOG_NOTICE, "udp_command: recv failed: %s",
		 strerror (errno)));
	  continue;
	}
      close(sockfd);
      return numbytes;
    }

  /* no response even after retry */

  close(sockfd);
  PDBG (bjnp_dbg
        (LOG_CRIT, "udp_command: no data received\n" ) );
  return -1;
}

static int
get_scanner_id (const int dev_no, char *model)
{
  /*
   * get scanner identity
   * Sets model (make and model)
   * Return 0 on success, -1 in case of errors
   */

  struct BJNP_command cmd;
  struct IDENTITY *id;
  char scanner_id[BJNP_IEEE1284_MAX];
  int resp_len;
  char resp_buf[BJNP_RESP_MAX];
  int id_len;

  /* set defaults */

  strcpy (model, "Unidentified scanner");

  set_cmd (dev_no, &cmd, CMD_UDP_GET_ID, 0);

  PDBG (bjnp_dbg (LOG_DEBUG2, "Get scanner identity\n"));
  PDBG (bjnp_hexdump (LOG_DEBUG2, (char *) &cmd,
		       sizeof (struct BJNP_command)));

  if ( ( resp_len =  udp_command (dev_no, (char *) &cmd, sizeof (struct BJNP_command),
		 resp_buf, BJNP_RESP_MAX) ) < (int)sizeof(struct BJNP_command) )
    {
      PDBG (bjnp_dbg (LOG_DEBUG, "Failed to retrieve scanner identity:\n"));
      return -1;
    }
  PDBG (bjnp_dbg (LOG_DEBUG2, "scanner identity:\n"));
  PDBG (bjnp_hexdump (LOG_DEBUG2, resp_buf, resp_len));

  id = (struct IDENTITY *) resp_buf;

  /* truncate string to be safe */
  id_len = htons( id-> id_len ) - sizeof(id->id_len);
  id->id[id_len] = '\0';
  strcpy (scanner_id, id->id);

  PDBG (bjnp_dbg (LOG_INFO, "Scanner identity string = %s - lenght = %d\n", scanner_id, id_len));

  /* get make&model from IEEE1284 id  */

  if (model != NULL)
  {
    parse_IEEE1284_to_model (scanner_id, model);
    PDBG (bjnp_dbg (LOG_INFO, "Scanner model = %s\n", model));
  }
  return 0;
}

static int
get_scanner_name(const bjnp_sockaddr_t *scanner_sa, char *host)
{
  /*
   * Parse identify command responses to ip-address
   * and hostname
   */

  struct addrinfo *results;
  struct addrinfo *result;
  char ip_address[BJNP_HOST_MAX];
  int port;
  int error; 
  int match = 0;
  int level;
  char service[64];

#ifdef ENABLE_IPV6
  if ( ( scanner_sa -> addr.sa_family == AF_INET6 ) &&
       ( IN6_IS_ADDR_LINKLOCAL( &(scanner_sa -> ipv6.sin6_addr ) ) ) )
    level = BJNP_ADDRESS_IS_LINK_LOCAL;
  else
#endif
    level = BJNP_ADDRESS_IS_GLOBAL;

  get_address_info( scanner_sa, ip_address, &port );

  /* do reverse name lookup, if hostname can not be found return ip-address */

  if( (error = getnameinfo( &(scanner_sa -> addr) , sa_size( scanner_sa), 
                  host, BJNP_HOST_MAX , NULL, 0, NI_NAMEREQD) ) != 0 )
    {
      PDBG (bjnp_dbg(LOG_INFO, "Name for %s not found : %s\n", 
                      ip_address, gai_strerror(error) ) );
      strcpy(host, ip_address);
      return level;
    }
  else
    {
      sprintf(service, "%d", port);
      /* some buggy routers return rubbish if reverse lookup fails, so 
       * we do a forward lookup on the received name to see if the result matches */

      if (getaddrinfo(host , service, NULL, &results) == 0) 
        {
          result = results;

          while (result != NULL) 
            {
               if(sa_is_equal( scanner_sa, (bjnp_sockaddr_t *)result-> ai_addr))
                 {
                     /* found match, good */
                     PDBG (bjnp_dbg (LOG_INFO, 
                              "Forward lookup for %s succeeded, using as hostname\n", host));
                    match = 1;
                    level = BJNP_ADDRESS_HAS_FQDN;
                    break;
                 }
              result = result-> ai_next;
            }
          freeaddrinfo(results);

          if (match != 1) 
            {
              PDBG (bjnp_dbg (LOG_INFO, 
                 "Forward lookup for %s succeeded, IP-address does not match, using IP-address %s instead\n", 
                 host, ip_address));
              strcpy (host, ip_address);
            }
         } 
       else 
         {
           /* forward lookup failed, use ip-address */
           PDBG ( bjnp_dbg (LOG_INFO, "Forward lookup of %s failed, using IP-address", ip_address));
           strcpy (host, ip_address);
         }
    }
  return level;
}

static int create_broadcast_socket( const bjnp_sockaddr_t * local_addr )
{
  int sockfd = -1;
  int broadcast = 1;
  int ipv6_v6only = 1;


 if ((sockfd = socket (local_addr-> addr.sa_family, SOCK_DGRAM, 0)) == -1)
    {
      PDBG (bjnp_dbg
            (LOG_CRIT, "create_broadcast_socket: can not open socket - %s",
             strerror (errno)));
      return -1;
    }

  /* Set broadcast flag on socket */

  if (setsockopt
      (sockfd, SOL_SOCKET, SO_BROADCAST, (const char *) &broadcast,
       sizeof (broadcast)) != 0)
    {
      PDBG (bjnp_dbg
            (LOG_CRIT,
             "create_broadcast_socket: setting socket option SO_BROADCAST failed - %s",
             strerror (errno)));
      close (sockfd);
      return -1;
    };

  /* For an IPv6 socket, bind to v6 only so a V6 socket can co-exist with a v4 socket */
  if ( (local_addr -> addr.sa_family == AF_INET6) && ( setsockopt
      (sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (const char *) &ipv6_v6only,
       sizeof (ipv6_v6only)) != 0) )
    {
      PDBG (bjnp_dbg
            (LOG_CRIT,
             "create_broadcast_socket: setting socket option IPV6_V6ONLY failed - %s",
             strerror (errno)));
      close (sockfd);
      return -1;
    };

  if (bind
      (sockfd, &(local_addr->addr),
       (socklen_t) sa_size( local_addr)) != 0)
    {
      PDBG (bjnp_dbg
            (LOG_CRIT,
             "create_broadcast_socket: bind socket to local address failed - %s\n",
             strerror (errno)));
      close (sockfd);
      return -1;
    }
  return sockfd;
}

static int 
prepare_socket(const char *if_name, const bjnp_sockaddr_t *local_sa, 
               const bjnp_sockaddr_t *broadcast_sa, bjnp_sockaddr_t * dest_sa)
{
  /*
   * Prepare a socket for broadcast or multicast
   * Input:
   * if_name: the name of the interface
   * local_sa: local address to use
   * broadcast_sa: broadcast address to use, if NULL we use all hosts
   * dest_sa: (write) where to return destination address of broadcast
   * retuns: open socket or -1
   */

  int socket = -1;
  bjnp_sockaddr_t local_sa_copy;

  if ( local_sa == NULL )
    {
      PDBG (bjnp_dbg (LOG_DEBUG, 
                       "%s is not a valid IPv4 interface, skipping...\n",
                       if_name));
      return -1;
    }

  memset( &local_sa_copy, 0, sizeof(local_sa_copy) );
  memcpy( &local_sa_copy, local_sa, sa_size(local_sa) );

  switch( local_sa_copy.addr.sa_family )
    {
      case AF_INET:
        {
          local_sa_copy.ipv4.sin_port = htons(BJNP_PORT_SCAN);

          if (local_sa_copy.ipv4.sin_addr.s_addr == htonl (INADDR_LOOPBACK) )
            {
              /* not a valid interface */

              PDBG (bjnp_dbg (LOG_DEBUG, 
                               "%s is not a valid IPv4 interface, skipping...\n",
	                       if_name));
              return -1;
            }


          /* send broadcasts to the broadcast address of the interface */

          memcpy(dest_sa, broadcast_sa, sa_size(dest_sa) );
          dest_sa -> ipv4.sin_port = htons(BJNP_PORT_SCAN);
          if ( (socket = create_broadcast_socket( &local_sa_copy) ) != -1) 
            {
               PDBG (bjnp_dbg (LOG_INFO, "%s is IPv4 capable, sending broadcast, socket = %d\n",
                      if_name, socket));
            }
          else
            {
              PDBG (bjnp_dbg (LOG_INFO, "%s is IPv4 capable, but failed to create a socket.\n",
                    if_name));
              return -1;
            }
        }
        break;
#ifdef ENABLE_IPV6
      case AF_INET6:
        {
          local_sa_copy.ipv6.sin6_port = htons(BJNP_PORT_SCAN);

          if (IN6_IS_ADDR_LOOPBACK( &(local_sa_copy.ipv6.sin6_addr) ) )
            {
              /* not a valid interface */

              PDBG (bjnp_dbg (LOG_DEBUG, 
                               "%s is not a valid IPv6 interface, skipping...\n",
	                       if_name));
              return -1;
            }
          else
            {
              dest_sa -> ipv6.sin6_family = AF_INET6;
              dest_sa -> ipv6.sin6_port = htons(BJNP_PORT_SCAN);
              inet_pton(AF_INET6, "ff02::1", dest_sa -> ipv6.sin6_addr.s6_addr);
              if ( (socket = create_broadcast_socket( &local_sa_copy ) ) != -1) 
                {
                   PDBG (bjnp_dbg (LOG_INFO, "%s is IPv6 capable, sending broadcast, socket = %d\n",
                          if_name, socket));
                }
              else
                {
                  PDBG (bjnp_dbg (LOG_INFO, "%s is IPv6 capable, but failed to create a socket.\n",
                        if_name));
                  return -1;
                }
            }
          } 
          break;
#endif

      default:
        socket = -1;
    }
  return socket;
}

static int
bjnp_send_broadcast (int sockfd, const bjnp_sockaddr_t * broadcast_addr, 
                     struct BJNP_command cmd, int size)
{
  int num_bytes;

  /* set address to send packet to */
  /* usebroadcast address of interface */

  if ((num_bytes = sendto (sockfd, &cmd, size, 0,
			  &(broadcast_addr->addr),
			  sa_size( broadcast_addr)) ) != size)
    {
      PDBG (bjnp_dbg (LOG_INFO,
		       "bjnp_send_broadcast: Socket: %d: sent only %x = %d bytes of packet, error = %s\n",
		       sockfd, num_bytes, num_bytes, strerror (errno)));
      /* not allowed, skip this interface */

      return -1;
    }
  return sockfd;
}

static void
bjnp_finish_job (int devno)
{
/* 
 * Signal end of scanjob to scanner
 */

  char resp_buf[BJNP_RESP_MAX];
  int resp_len;
  struct BJNP_command cmd;

  set_cmd (devno, &cmd, CMD_UDP_CLOSE, 0);

  PDBG (bjnp_dbg (LOG_DEBUG2, "Finish scanjob\n"));
  PDBG (bjnp_hexdump
	(LOG_DEBUG2, (char *) &cmd, sizeof (struct BJNP_command)));
  resp_len =
    udp_command (devno, (char *) &cmd, sizeof (struct BJNP_command), resp_buf,
		 BJNP_RESP_MAX);

  if (resp_len != sizeof (struct BJNP_command))
    {
      PDBG (bjnp_dbg
	    (LOG_INFO,
	     "Received %d characters on close scanjob command, expected %d\n",
	     resp_len, (int) sizeof (struct BJNP_command)));
      return;
    }
  PDBG (bjnp_dbg (LOG_DEBUG2, "Finish scanjob response\n"));
  PDBG (bjnp_hexdump (LOG_DEBUG2, resp_buf, resp_len));

}

#ifdef PIXMA_BJNP_USE_STATUS
static int
bjnp_poll_scanner (int devno, char type,char *hostname, char *user, SANE_Byte *status, int size)
{
/* 
 * send details of user to the scanner
 */

  char cmd_buf[BJNP_CMD_MAX];
  char resp_buf[BJNP_RESP_MAX];
  int resp_len;
  int len = 0;			/* payload length */
  int buf_len;			/* length of the whole command  buffer */
  struct POLL_DETAILS *poll;
  struct POLL_RESPONSE *response;
  char user_host[256]; 
  time_t t;
  int user_host_len;

  poll = (struct POLL_DETAILS *) cmd_buf;
  memset( poll, 0, sizeof( struct POLL_DETAILS));
  memset( &resp_buf, 0, sizeof( resp_buf) );


  /* create payload */
  poll->type = htons(type);

  user_host_len =  sizeof( poll -> extensions.type2.user_host);
  snprintf(user_host, (user_host_len /2) ,"%s  %s", user, hostname);
  user_host[ user_host_len /2 + 1] = '\0';

  switch( type) {
    case 0:
      len = 80;
      break;
    case 1:
      charTo2byte(poll->extensions.type1.user_host, user_host, user_host_len);
      len = 80;
      break;
    case 2:
      poll->extensions.type2.dialog = htonl(device[devno].dialog);     
      charTo2byte(poll->extensions.type2.user_host, user_host, user_host_len); 
      poll->extensions.type2.unknown_1 = htonl(0x14);
      poll->extensions.type2.unknown_2 = htonl(0x10); 
      t = time (NULL);
      strftime (poll->extensions.type2.ascii_date, 
                sizeof (poll->extensions.type2.ascii_date), 
               "%Y%m%d%H%M%S", localtime (&t));
      len = 116;
      break;
    case 5:
      poll->extensions.type5.dialog = htonl(device[devno].dialog);     
      charTo2byte(poll->extensions.type5.user_host, user_host, user_host_len); 
      poll->extensions.type5.unknown_1 = htonl(0x14);
      poll->extensions.type5.key = htonl(device[devno].status_key); 
      len = 100;
      break;
    default:
      PDBG (bjnp_dbg (LOG_INFO, "bjnp_poll_scanner: unknown packet type: %d\n", type));
      return -1;
  }; 
  /* we can only now set the header as we now know the length of the payload */
  set_cmd (devno, (struct BJNP_command *) cmd_buf, CMD_UDP_POLL,
	   len);

  buf_len = len + sizeof(struct BJNP_command);
  PDBG (bjnp_dbg (LOG_DEBUG2, "Poll details (type %d)\n", type));
  PDBG (bjnp_hexdump (LOG_DEBUG2, cmd_buf,
		       buf_len));

  resp_len = udp_command (devno, cmd_buf, buf_len,  resp_buf, BJNP_RESP_MAX);

  if (resp_len > 0)
    {
      PDBG (bjnp_dbg (LOG_DEBUG2, "Poll details response:\n"));
      PDBG (bjnp_hexdump (LOG_DEBUG2, resp_buf, resp_len));
      response = (struct POLL_RESPONSE *) resp_buf;

      device[devno].dialog = ntohl( response -> dialog );

      if ( response -> result[3] == 1 )
        {
          return BJNP_RESTART_POLL;
        }
      if ( (response -> result[2] & 0x80) != 0) 
        {
          memcpy( status, response->status, size);
          PDBG( bjnp_dbg(LOG_INFO, "received button status!\n"));
	  PDBG (bjnp_hexdump( LOG_DEBUG2, status, size ));
	  device[devno].status_key = ntohl( response -> key );
          return  size;
        }
    }
  return 0;
}
#endif

static void
bjnp_send_job_details (int devno, char *hostname, char *user, char *title)
{
/* 
 * send details of scanjob to scanner
 */

  char cmd_buf[BJNP_CMD_MAX];
  char resp_buf[BJNP_RESP_MAX];
  int resp_len;
  struct JOB_DETAILS *job;
  struct BJNP_command *resp;

  /* send job details command */

  set_cmd (devno, (struct BJNP_command *) cmd_buf, CMD_UDP_JOB_DETAILS,
	   sizeof (*job) - sizeof (struct BJNP_command));

  /* create payload */

  job = (struct JOB_DETAILS *) (cmd_buf);
  charTo2byte (job->unknown, "", sizeof (job->unknown));
  charTo2byte (job->hostname, hostname, sizeof (job->hostname));
  charTo2byte (job->username, user, sizeof (job->username));
  charTo2byte (job->jobtitle, title, sizeof (job->jobtitle));

  PDBG (bjnp_dbg (LOG_DEBUG2, "Job details\n"));
  PDBG (bjnp_hexdump (LOG_DEBUG2, cmd_buf,
		       (sizeof (struct BJNP_command) + sizeof (*job))));

  resp_len = udp_command (devno, cmd_buf,
			  sizeof (struct JOB_DETAILS), resp_buf,
			  BJNP_RESP_MAX);

  if (resp_len > 0)
    {
      PDBG (bjnp_dbg (LOG_DEBUG2, "Job details response:\n"));
      PDBG (bjnp_hexdump (LOG_DEBUG2, resp_buf, resp_len));
      resp = (struct BJNP_command *) resp_buf;
      device[devno].session_id = ntohs (resp->session_id);
    }
}

static int
bjnp_get_scanner_mac_address ( int devno, char *mac_address )
{
/* 
 * send discover to scanner
 */

  char cmd_buf[BJNP_CMD_MAX];
  char resp_buf[BJNP_RESP_MAX];
  int resp_len;
  struct DISCOVER_RESPONSE *resp = (struct DISCOVER_RESPONSE * )&resp_buf;;

  /* send job details command */

  set_cmd (devno, (struct BJNP_command *) cmd_buf, CMD_UDP_DISCOVER, 0);
  resp_len = udp_command (devno, cmd_buf,
			  sizeof (struct BJNP_command), resp_buf,
			  BJNP_RESP_MAX);

  if (resp_len > 0)
    {
      PDBG (bjnp_dbg (LOG_DEBUG2, "Discover response:\n"));
      PDBG (bjnp_hexdump (LOG_DEBUG2, resp_buf, resp_len));
      u8tohex( mac_address, resp -> mac_addr, sizeof( resp -> mac_addr ) ); 
      return 0;
    }
  return -1;
}

static int
bjnp_write (int devno, const SANE_Byte * buf, size_t count)
{
/*
 * This function writes scandata to the scanner. 
 * Returns: number of bytes written to the scanner
 */
  int sent_bytes;
  int terrno;
  struct SCAN_BUF bjnp_buf;

  if (device[devno].scanner_data_left)
    PDBG (bjnp_dbg
	  (LOG_CRIT, "bjnp_write: ERROR: scanner data left = 0x%lx = %ld\n",
	   (unsigned long) device[devno].scanner_data_left,
	   (unsigned long) device[devno].scanner_data_left));

  /* set BJNP command header */

  set_cmd (devno, (struct BJNP_command *) &bjnp_buf, CMD_TCP_SEND, count);
  memcpy (bjnp_buf.scan_data, buf, count);
  PDBG (bjnp_dbg (LOG_DEBUG, "bjnp_write: sending 0x%lx = %ld bytes\n",
		   (unsigned long) count, (unsigned long) count);
	PDBG (bjnp_hexdump (LOG_DEBUG2, (char *) &bjnp_buf,
			     sizeof (struct BJNP_command) + count)));

  if ((sent_bytes =
       send (device[devno].tcp_socket, &bjnp_buf,
	     sizeof (struct BJNP_command) + count, 0)) <
      (ssize_t) (sizeof (struct BJNP_command) + count))
    {
      /* return result from write */
      terrno = errno;
      PDBG (bjnp_dbg (LOG_CRIT, "bjnp_write: Could not send data!\n"));
      errno = terrno;
      return sent_bytes;
    }
  /* correct nr of bytes sent for length of command */

  else if (sent_bytes != (int) (sizeof (struct BJNP_command) + count))
    {
      errno = EIO;
      return -1;
    }
  return count;
}

static int
bjnp_send_read_request (int devno)
{
/*
 * This function reads responses from the scanner.  
 * Returns: 0 on success, else -1
 *  
 */
  int sent_bytes;
  int terrno;
  struct BJNP_command bjnp_buf;

  if (device[devno].scanner_data_left)
    PDBG (bjnp_dbg
	  (LOG_CRIT,
	   "bjnp_send_read_request: ERROR scanner data left = 0x%lx = %ld\n",
	   (unsigned long) device[devno].scanner_data_left,
	   (unsigned long) device[devno].scanner_data_left));

  /* set BJNP command header */

  set_cmd (devno, (struct BJNP_command *) &bjnp_buf, CMD_TCP_REQ, 0);

  PDBG (bjnp_dbg (LOG_DEBUG, "bjnp_send_read_req sending command\n"));
  PDBG (bjnp_hexdump (LOG_DEBUG2, (char *) &bjnp_buf,
		       sizeof (struct BJNP_command)));

  if ((sent_bytes =
       send (device[devno].tcp_socket, &bjnp_buf, sizeof (struct BJNP_command),
	     0)) < 0)
    {
      /* return result from write */
      terrno = errno;
      PDBG (bjnp_dbg
	    (LOG_CRIT, "bjnp_send_read_request: Could not send data!\n"));
      errno = terrno;
      return -1;
    }
  return 0;
}

static SANE_Status
bjnp_recv_header (int devno, size_t *payload_size )
{
/*
 * This function receives the response header to bjnp commands.
 * devno device number
 * size: return value for data size returned by scanner 
 * Returns: 
 * SANE_STATUS_IO_ERROR when any IO error occurs
 * SANE_STATUS_GOOD in case no errors were encountered
 */
  struct BJNP_command resp_buf;
  fd_set input;
  struct timeval timeout;
  int recv_bytes;
  int terrno;
  int result;
  int fd;
  int attempt;

  PDBG (bjnp_dbg
	(LOG_DEBUG, "bjnp_recv_header: receiving response header\n") );
  fd = device[devno].tcp_socket;

  *payload_size = 0;
  attempt = 0;
  do
    {
      /* wait for data to be received, ignore signals being received */
      FD_ZERO (&input);
      FD_SET (fd, &input);

      timeout.tv_sec = BJNP_TIMEOUT_TCP;
      timeout.tv_usec = 0;
    }
  while ( ( (result = select (fd + 1, &input, NULL, NULL, &timeout)) <= 0) &&
	 (errno == EINTR) && (attempt++ < BJNP_MAX_SELECT_ATTEMPTS));

  if (result < 0)
    {
      terrno = errno;
      PDBG (bjnp_dbg (LOG_CRIT,
		       "bjnp_recv_header: could not read response header (select): %s!\n",
		       strerror (terrno)));
      errno = terrno;
      return SANE_STATUS_IO_ERROR;
    }
  else if (result == 0)
    {
      terrno = errno;
      PDBG (bjnp_dbg (LOG_CRIT,
		       "bjnp_recv_header: could not read response header (select timed out)!\n" ) );
      errno = terrno;
      return SANE_STATUS_IO_ERROR;
    }

  /* get response header */

  if ((recv_bytes =
       recv (fd, (char *) &resp_buf,
	     sizeof (struct BJNP_command),
	     0)) != sizeof (struct BJNP_command))
    {
      terrno = errno;
      PDBG (bjnp_dbg (LOG_CRIT,
		       "bjnp_recv_header: (recv) could not read response header, received %d bytes!\n",
		       recv_bytes));
      PDBG (bjnp_dbg
	    (LOG_CRIT, "bjnp_recv_header: (recv) error: %s!\n",
	     strerror (terrno)));
      errno = terrno;
      return SANE_STATUS_IO_ERROR;
    }

  if (resp_buf.cmd_code != device[devno].last_cmd)
    {
      PDBG (bjnp_dbg
	    (LOG_CRIT,
	     "bjnp_recv_header:ERROR, Received response has cmd code %d, expected %d\n",
	     resp_buf.cmd_code, device[devno].last_cmd));
      return SANE_STATUS_IO_ERROR;
    }

  if (ntohs (resp_buf.seq_no) != (uint16_t) device[devno].serial)
    {
      PDBG (bjnp_dbg
	    (LOG_CRIT,
	     "bjnp_recv_header:ERROR, Received response has serial %d, expected %d\n",
	     (int) ntohs (resp_buf.seq_no), (int) device[devno].serial));
      return SANE_STATUS_IO_ERROR;
    }

  /* got response header back, retrieve length of scanner data */


  *payload_size = ntohl (resp_buf.payload_len);
  PDBG (bjnp_dbg
	(LOG_DEBUG, "TCP response header(scanner data = %ld bytes):\n",
	 *payload_size) );
  PDBG (bjnp_hexdump
	(LOG_DEBUG2, (char *) &resp_buf, sizeof (struct BJNP_command)));
  return SANE_STATUS_GOOD;
}

static int
bjnp_init_device_structure(int dn, bjnp_sockaddr_t *sa )
{
  /* initialize device structure */

  char name[BJNP_HOST_MAX];

  device[dn].open = 0;
#ifdef PIXMA_BJNP_USE_STATUS
  device[dn].polling_status = BJNP_POLL_STOPPED; 
  device[dn].dialog = 0;
  device[dn].status_key = 0;
#endif
  device[dn].tcp_socket = -1;

  device[dn].addr = (bjnp_sockaddr_t *) malloc(sizeof ( bjnp_sockaddr_t) );
  memset( device[dn].addr, 0, sizeof( bjnp_sockaddr_t ) );  
  memcpy(device[dn].addr, sa, sa_size((bjnp_sockaddr_t *)sa) );
  device[dn].address_level = get_scanner_name(sa, name);
  device[dn].session_id = 0;
  device[dn].serial = -1;
  device[dn].bjnp_timeout = 0;
  device[dn].scanner_data_left = 0;
  device[dn].last_cmd = 0;
  device[dn].blocksize = 2048;	/* safe assumption, we start low */
  device[dn].last_block = 0;
  /* fill mac_address */

  if (bjnp_get_scanner_mac_address(dn, device[dn].mac_address) != 0 )
    {
      PDBG (bjnp_dbg
            (LOG_CRIT, "Cannot read mac address, skipping this scanner\n"  ) );
      return -1;
    }
  return 0;
}

static void
bjnp_free_device_structure( int dn)
{
  if (device[dn].addr != NULL)
    {
    free (device[dn].addr );
    device[dn].addr = NULL;
    }
  device[dn].open = 0;
}

static SANE_Status
bjnp_recv_data (int devno, SANE_Byte * buffer, size_t * len)
{
/*
 * This function receives the responses to the write commands.
 * NOTE: len may not exceed SSIZE_MAX (as that is max for recv)
 * Returns: number of bytes of payload received from device
 */

  fd_set input;
  struct timeval timeout;
  ssize_t recv_bytes;
  int terrno;
  int result;
  int fd;
  int attempt;

  PDBG (bjnp_dbg (LOG_DEBUG, "bjnp_recv_data: receiving response data\n"));
  fd = device[devno].tcp_socket;

  PDBG (bjnp_dbg
	(LOG_DEBUG, "bjnp_recv_data: read response payload (%ld bytes max)\n",
	 (unsigned long) *len));

  attempt = 0;
  do
    {
      /* wait for data to be received, retry on a signal being received */
      FD_ZERO (&input);
      FD_SET (fd, &input);
      timeout.tv_sec = BJNP_TIMEOUT_TCP;
      timeout.tv_usec = 0;
    }
  while (((result = select (fd + 1, &input, NULL, NULL, &timeout)) <= 0) &&
	 (errno == EINTR) && (attempt++ < BJNP_MAX_SELECT_ATTEMPTS));

  if (result < 0)
    {
      terrno = errno;
      PDBG (bjnp_dbg (LOG_CRIT,
		       "bjnp_recv_data: could not read response payload (select): %s!\n",
		       strerror (errno)));
      errno = terrno;
      *len = 0;
      return SANE_STATUS_IO_ERROR;
    }
  else if (result == 0)
    {
      terrno = errno;
      PDBG (bjnp_dbg (LOG_CRIT,
		       "bjnp_recv_data: could not read response payload (select timed out): %s!\n",
		       strerror (terrno)));
      errno = terrno;
      *len = 0;
      return SANE_STATUS_IO_ERROR;
    }

  if ((recv_bytes = recv (fd, buffer, *len, 0)) < 0)
    {
      terrno = errno;
      PDBG (bjnp_dbg (LOG_CRIT,
		       "bjnp_recv_data: could not read response payload (recv): %s!\n",
		       strerror (errno)));
      errno = terrno;
      *len = 0;
      return SANE_STATUS_IO_ERROR;
    }
  PDBG (bjnp_dbg (LOG_DEBUG2, "Received TCP response payload (%ld bytes):\n",
		   (unsigned long) recv_bytes));
  PDBG (bjnp_hexdump (LOG_DEBUG2, buffer, recv_bytes));

  *len = recv_bytes;
  return SANE_STATUS_GOOD;
}

static BJNP_Status
bjnp_allocate_device (SANE_String_Const devname, 
                      SANE_Int * dn, char *res_host)
{
  char method[BJNP_METHOD_MAX]; 
  char host[BJNP_HOST_MAX];
  char port[BJNP_PORT_MAX] = "";
  char args[BJNP_ARGS_MAX];
  struct addrinfo *res, *cur;
  struct addrinfo hints;
  int result;
  int i;

  PDBG (bjnp_dbg (LOG_DEBUG, "bjnp_allocate_device(%s) %d\n", devname, bjnp_no_devices));

  if (split_uri (devname, method, host, port, args) != 0)
    {
      return BJNP_STATUS_INVAL;
    }

  if (strlen (args) != 0)
    {
      PDBG (bjnp_dbg
	    (LOG_CRIT,
	     "URI may not contain userid, password or aguments: %s\n",
	     devname));

      return BJNP_STATUS_INVAL;
    }
  if (strcmp (method, BJNP_METHOD) != 0)
    {
      PDBG (bjnp_dbg
	    (LOG_CRIT, "URI %s contains invalid method: %s\n", devname,
	     method));
      return BJNP_STATUS_INVAL;
    }

  if (strlen(port) == 0)
    {
      sprintf( port, "%d", BJNP_PORT_SCAN );
    }
 
  hints.ai_flags = 0;
#ifdef ENABLE_IPV6
  hints.ai_family = AF_UNSPEC;
#else
  hints.ai_family = AF_INET;
#endif
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_protocol = 0;
  hints.ai_addrlen = 0;
  hints.ai_addr = NULL;
  hints.ai_canonname = NULL;
  hints.ai_next = NULL;

  result = getaddrinfo (host, port, &hints, &res );
  if (result != 0 ) 
    {
      PDBG (bjnp_dbg (LOG_CRIT, "Cannot resolve host: %s port %s\n", host, port));
      return SANE_STATUS_INVAL;
    }

  /* Check if a device number is already allocated to any of the scanner's addresses */

  cur = res;
  while( cur != NULL)
    {
      /* create a new device structure for this address */
      
      if (bjnp_no_devices == BJNP_NO_DEVICES)
        {
          PDBG (bjnp_dbg
    	    (LOG_CRIT,
    	     "Too many devices, ran out of device structures, can not add %s\n",
    	     devname));
          freeaddrinfo(res);
          return BJNP_STATUS_INVAL;
        }
      if (bjnp_init_device_structure( bjnp_no_devices, (bjnp_sockaddr_t *)cur -> ai_addr) != 0)
        {
          /* giving up on this address, try next one if any */
          break;
        }
      for (i = 0; i < bjnp_no_devices; i++)
        {
          /* we check for matching addresses as wel as matching mac_addresses as */
          /* an IPv6 host can have multiple adresses */

          if ( (sa_is_equal( device[i].addr, (bjnp_sockaddr_t *)cur -> ai_addr) ) ||
               ( strcmp( device[i].mac_address, device[bjnp_no_devices].mac_address ) == 0 ) )
            {
              if ( device[i].address_level < device[bjnp_no_devices].address_level ) 
                {
                  /* use the new address instead as it is better */
                  free (device[i].addr);
                  device[i].addr = device[bjnp_no_devices].addr;
                  device[bjnp_no_devices].addr = NULL;
                  device[i].address_level = device[bjnp_no_devices].address_level;
                }
              freeaddrinfo(res); 
              *dn = i;
              bjnp_free_device_structure( bjnp_no_devices);
              return BJNP_STATUS_ALREADY_ALLOCATED;
            }
        }
      cur = cur->ai_next;
    }
  freeaddrinfo(res);

  PDBG (bjnp_dbg (LOG_INFO, "Scanner not yet in our list, added it: %s:%s\n", host, port));

  /* return hostname if required */

  if (res_host != NULL)
    {
      strcpy (res_host, host);
    }
  *dn = bjnp_no_devices;

  /* Commit new device structure */

  bjnp_no_devices++;

  return BJNP_STATUS_GOOD;
}

static void add_scanner(SANE_Int *dev_no, 
                        const char *uri, 
			SANE_Status (*attach_bjnp)
			              (SANE_String_Const devname,
			               SANE_String_Const makemodel,
			               SANE_String_Const serial,
			               const struct pixma_config_t *
			               const pixma_devices[]),
			 const struct pixma_config_t *const pixma_devices[])

{
  char scanner_host[BJNP_HOST_MAX];
  char serial[BJNP_SERIAL_MAX];
  char makemodel[BJNP_IEEE1284_MAX];

  /* Allocate device structure for scanner */
  switch (bjnp_allocate_device (uri, dev_no, scanner_host))
    {
      case BJNP_STATUS_GOOD:
        if (get_scanner_id (*dev_no, makemodel) != 0)
          {
            PDBG (bjnp_dbg (LOG_CRIT, "Cannot read scanner make & model: %s\n", 
                             uri));
          }
        else
          {
          /*
           * inform caller of found scanner
           */

           determine_scanner_serial (scanner_host, device[*dev_no].mac_address, serial);
           attach_bjnp (uri, makemodel,
                        serial, pixma_devices);
          }
        break;
      case BJNP_STATUS_ALREADY_ALLOCATED:
        PDBG (bjnp_dbg (LOG_NOTICE, "Scanner at %s was added before, good!\n",
	                 uri));
        break;

      case BJNP_STATUS_INVAL:
        PDBG (bjnp_dbg (LOG_NOTICE, "Scanner at %s can not be added\n",
	                 uri));
        break;
    }
}


/*
 * Public functions
 */

/** Initialize sanei_bjnp.
 *
 * Call this before any other sanei_bjnp function.
 */
extern void
sanei_bjnp_init (void)
{
  DBG_INIT();
  bjnp_no_devices = 0;
}

/** 
 * Find devices that implement the bjnp protocol
 *
 * The function attach is called for every device which has been found.
 *
 * @param attach attach function
 *
 * @return SANE_STATUS_GOOD - on success (even if no scanner was found)
 */
extern SANE_Status
sanei_bjnp_find_devices (const char **conf_devices,
			 SANE_Status (*attach_bjnp)
			 (SANE_String_Const devname,
			  SANE_String_Const makemodel,
			  SANE_String_Const serial,
			  const struct pixma_config_t *
			  const pixma_devices[]),
			 const struct pixma_config_t *const pixma_devices[])
{
  int numbytes = 0;
  struct BJNP_command cmd;
  unsigned char resp_buf[2048];
  struct DISCOVER_RESPONSE *disc_resp = ( struct DISCOVER_RESPONSE *) & resp_buf; 
  int socket_fd[BJNP_SOCK_MAX];
  int no_sockets;
  int i;
  int attempt;
  int last_socketfd = 0;
  fd_set fdset;
  fd_set active_fdset;
  struct timeval timeout;
  char scanner_host[256]; 
  char uri[256];
  int dev_no;
  bjnp_sockaddr_t broadcast_addr[BJNP_SOCK_MAX];
  bjnp_sockaddr_t scanner_sa; 
  socklen_t socklen;

  memset( broadcast_addr, 0, sizeof( broadcast_addr) );
  memset( &scanner_sa, 0 ,sizeof( scanner_sa ) );
  PDBG (bjnp_dbg (LOG_INFO, "sanei_bjnp_find_devices:\n"));
  bjnp_no_devices = 0;

  for (i=0; i < BJNP_SOCK_MAX; i++)
    {
      socket_fd[i] = -1;
    }
  /* First add devices from config file */

  if (conf_devices[0] == NULL)
    PDBG (bjnp_dbg( LOG_DEBUG, "No devices specified in configuration file.\n" ) );

  for (i = 0; conf_devices[i] != NULL; i++)
    {
      PDBG (bjnp_dbg
	    (LOG_DEBUG, "Adding scanner from pixma.conf: %s\n", conf_devices[i]));
      add_scanner(&dev_no, conf_devices[i], attach_bjnp, pixma_devices);
    }
  PDBG (bjnp_dbg
	(LOG_DEBUG,
	 "Added all configured scanners, now do auto detection...\n"));

  /*
   * Send UDP DISCOVER to discover scanners and return the list of scanners found
   */

  FD_ZERO (&fdset);
  set_cmd (-1, &cmd, CMD_UDP_DISCOVER, 0);

  no_sockets = 0;
#ifdef HAVE_IFADDRS_H
  {
    struct ifaddrs *interfaces = NULL;
    struct ifaddrs *interface;
    getifaddrs (&interfaces);

    /* create a socket for each suitable interface */

    interface = interfaces;
    while ((no_sockets < BJNP_SOCK_MAX) && (interface != NULL))
      {
        if ( ! (interface -> ifa_flags & IFF_POINTOPOINT) &&  
            ( (socket_fd[no_sockets] = 
                      prepare_socket( interface -> ifa_name, 
                                      (bjnp_sockaddr_t *) interface -> ifa_addr, 
                                      (bjnp_sockaddr_t *) interface -> ifa_broadaddr,
                                      &broadcast_addr[no_sockets] ) ) != -1 ) )
          {
            /* track highest used socket for later use in select */
            if (socket_fd[no_sockets] > last_socketfd)
              {
                last_socketfd = socket_fd[no_sockets];
              }
            FD_SET (socket_fd[no_sockets], &fdset);
            no_sockets++;
          }
        interface = interface->ifa_next;
      }  
    freeifaddrs (interfaces);
  }
#else
  /* we have no easy way to find interfaces with their broadcast addresses. */
  /* use global broadcast and all-hosts instead */
  {
    bjnp_sockaddr_t local;
    bjnp_sockaddr_t bc_addr;

    memset( &local, 0, sizeof( local) );
    local.ipv4.sin_family = AF_INET;
    local.ipv4.sin_addr.s_addr = htonl (INADDR_ANY);

    bc_addr.ipv4.sin_family = AF_INET;
    bc_addr.ipv4.sin_port = htons(BJNP_PORT_SCAN);
    bc_addr.ipv4.sin_addr.s_addr = htonl (INADDR_BROADCAST);

    socket_fd[no_sockets] = prepare_socket( "any_interface", 
                                   &local, 
                                   &bc_addr,  
                                   &broadcast_addr[no_sockets] ); 
    if (socket_fd[no_sockets] >= 0)
      {
        FD_SET (socket_fd[no_sockets], &fdset);
        if (socket_fd[no_sockets] > last_socketfd)
          {
            last_socketfd = socket_fd[no_sockets];
          }
        no_sockets++;
      }
#ifdef ENABLE_IPV6
    local.ipv6.sin6_family = AF_INET6;
    local.ipv6.sin6_addr = in6addr_any;

    socket_fd[no_sockets] = prepare_socket( "any_interface", 
                                   &local, 
                                   NULL,
                                   &broadcast_addr[no_sockets] ); 
    if (socket_fd[no_sockets] >= 0)
      {
        FD_SET (socket_fd[no_sockets], &fdset);
        if (socket_fd[no_sockets] > last_socketfd)
          {
            last_socketfd = socket_fd[no_sockets];
          }
        no_sockets++;
      }
#endif 
  }
#endif

  /* send BJNP_MAX_BROADCAST_ATTEMPTS broadcasts on each prepared socket */
  for (attempt = 0; attempt < BJNP_MAX_BROADCAST_ATTEMPTS; attempt++)
    {
      for ( i=0; i < no_sockets; i++)
        {
          bjnp_send_broadcast ( socket_fd[i], &broadcast_addr[i], cmd, sizeof (cmd));
	}
      /* wait for some time between broadcast packets */
      usleep (BJNP_BROADCAST_INTERVAL * BJNP_USLEEP_MS);
    }

  /* wait for a UDP response */

  timeout.tv_sec = 0;
  timeout.tv_usec = BJNP_BC_RESPONSE_TIMEOUT * BJNP_USLEEP_MS;


  active_fdset = fdset;

  while (select (last_socketfd + 1, &active_fdset, NULL, NULL, &timeout) > 0)
    {
      PDBG (bjnp_dbg (LOG_DEBUG, "Select returned, time left %d.%d....\n",
		       (int) timeout.tv_sec, (int) timeout.tv_usec));
      for (i = 0; i < no_sockets; i++)
	{
	  if (FD_ISSET (socket_fd[i], &active_fdset))
	    {
              socklen =  sizeof(scanner_sa);
	      if ((numbytes =
		   recvfrom (socket_fd[i], resp_buf, sizeof (resp_buf), 0, 
                             &(scanner_sa.addr), &socklen ) ) == -1)
		{
		  PDBG (bjnp_dbg
			(LOG_INFO, "find_devices: no data received"));
		  break;
		}
	      else
		{
		  PDBG (bjnp_dbg (LOG_DEBUG2, "Discover response:\n"));
		  PDBG (bjnp_hexdump (LOG_DEBUG2, &resp_buf, numbytes));

		  /* check if something sensible is returned */
		  if ( (numbytes < (int)sizeof (struct BJNP_command)) ||
		       (strncmp ("BJNP", disc_resp-> response.BJNP_id, 4) != 0))
		    {
		      /* not a valid response, assume not a scanner  */

                      char bjnp_id[5];
                      strncpy(bjnp_id,  disc_resp-> response.BJNP_id, 4);
                      bjnp_id[4] = '\0';
                      PDBG (bjnp_dbg (LOG_INFO, 
                        "Invalid discover response! Length = %d, Id = %s\n",
                        numbytes, bjnp_id ) );
		      break;
		    }
                  if ( ! ((disc_resp -> response.dev_type) & 0x80) )
                    {
                      /* not a response, a command from somebody else or */
                      /* a discover command that we generated */
                      break;
                    }
		};

	      /* scanner found, get IP-address or hostname */
              get_scanner_name( &scanner_sa, scanner_host);

	      /* construct URI */
	      sprintf (uri, "%s://%s:%d", BJNP_METHOD, scanner_host,
		       BJNP_PORT_SCAN);

              add_scanner( &dev_no, uri, attach_bjnp, pixma_devices); 

	    }
	}
      active_fdset = fdset;
      timeout.tv_sec = 0;
      timeout.tv_usec = BJNP_BC_RESPONSE_TIMEOUT * BJNP_USLEEP_MS;
    }
  PDBG (bjnp_dbg (LOG_DEBUG, "scanner discovery finished...\n"));

  for (i = 0; i < no_sockets; i++)
    close (socket_fd[i]);

  return SANE_STATUS_GOOD;
}

/** Open a BJNP device.
 *
 * The device is opened by its name devname and the device number is
 * returned in dn on success.  
 *
 * Device names consist of an URI                                     
 * Where:
 * type = bjnp
 * hostname = resolvable name or IP-address
 * port = 8612 for a scanner
 * An example could look like this: bjnp://host.domain:8612
 *
 * @param devname name of the device to open
 * @param dn device number
 *
 * @return
 * - SANE_STATUS_GOOD - on success
 * - SANE_STATUS_ACCESS_DENIED - if the file couldn't be accessed due to
 *   permissions
 * - SANE_STATUS_INVAL - on every other error
 */

extern SANE_Status
sanei_bjnp_open (SANE_String_Const devname, SANE_Int * dn)
{
  int result;

  PDBG (bjnp_dbg (LOG_INFO, "sanei_bjnp_open(%s, %d):\n", devname, *dn));

  result = bjnp_allocate_device (devname, dn, NULL);
  if ( (result != BJNP_STATUS_GOOD) && (result != BJNP_STATUS_ALREADY_ALLOCATED ) )
    return SANE_STATUS_INVAL; 

  return sanei_bjnp_activate( *dn);;
}

/** Close a BJNP device.
 * 
 * @param dn device number
 */

void
sanei_bjnp_close (SANE_Int dn)
{
  PDBG (bjnp_dbg (LOG_INFO, "sanei_bjnp_close(%d):\n", dn));
  sanei_bjnp_deactivate(dn);
  device[dn].open = 0;
}

/** Activate BJNP device connection
 *
 * @param dn device number
 */

SANE_Status
sanei_bjnp_activate (SANE_Int dn)
{
  char hostname[256];
  char pid_str[64];

  PDBG (bjnp_dbg (LOG_INFO, "sanei_bjnp_activate (%d)\n", dn));

  gethostname (hostname, 256);
  hostname[255] = '\0';
  sprintf (pid_str, "Process ID = %d", getpid ());

  bjnp_send_job_details (dn, hostname, getusername (), pid_str);

  if (bjnp_open_tcp (dn) != 0)
    {
      return SANE_STATUS_INVAL;
    }

  return SANE_STATUS_GOOD;
}

/** Deactivate BJNP device connection
 *
 * @paran dn device number
 */

SANE_Status
sanei_bjnp_deactivate (SANE_Int dn)
{
  PDBG (bjnp_dbg (LOG_INFO, "sanei_bjnp_deactivate (%d)\n", dn));

  bjnp_finish_job (dn);

  if ( device[dn].tcp_socket != -1)
    {
      close (device[dn].tcp_socket);
      device[dn].tcp_socket = -1;
    }
  return SANE_STATUS_GOOD;
}

/** Set the timeout for interrupt reads.
 *  we do not use it for bulk reads!
 * @param timeout the new timeout in ms
 */
extern void
sanei_bjnp_set_timeout (SANE_Int devno, SANE_Int timeout)
{
  PDBG (bjnp_dbg (LOG_INFO, "bjnp_set_timeout to %d\n",
		   timeout));

  device[devno].bjnp_timeout = timeout;
}

/** Initiate a bulk transfer read.
 *
 * Read up to size bytes from the device to buffer. After the read, size
 * contains the number of bytes actually read.
 *
 * @param dn device number
 * @param buffer buffer to store read data in
 * @param size size of the data
 *
 * @return 
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_EOF - if zero bytes have been read
 * - SANE_STATUS_IO_ERROR - if an error occured during the read
 * - SANE_STATUS_INVAL - on every other error
 *
 */

extern SANE_Status
sanei_bjnp_read_bulk (SANE_Int dn, SANE_Byte * buffer, size_t * size)
{
  SANE_Status result;
  SANE_Status error;
  size_t recvd;
  size_t more; 
  size_t requested;

  PDBG (bjnp_dbg
	(LOG_INFO, "bjnp_read_bulk(%d, bufferptr, 0x%lx = %ld)\n", dn,
	 (unsigned long) *size, (unsigned long) *size));

  recvd = 0;
  requested = *size;

  PDBG (bjnp_dbg
	(LOG_DEBUG, "bjnp_read_bulk: 0x%lx = %ld bytes available at start\n", 
	 (unsigned long) device[dn].scanner_data_left,
	 (unsigned long) device[dn].scanner_data_left ) );

  while ( (recvd < requested) && !( device[dn].last_block && (device[dn].scanner_data_left == 0)) )

    {
      PDBG (bjnp_dbg
	    (LOG_DEBUG,
	     "Received 0x%lx = %ld bytes, backend requested 0x%lx = %ld bytes\n",
	     (unsigned long) recvd, (unsigned long) recvd, 
	     (unsigned long) requested, (unsigned long)requested ));

      if (device[dn].scanner_data_left == 0)
        {
	  /* send new read request */

          PDBG (bjnp_dbg (LOG_DEBUG,
                          "No (more) scanner data available, requesting more( blocksize = %ld =%lx\n",
                          (long int) device[dn].blocksize, (long int) device[dn].blocksize ));

          if ((error = bjnp_send_read_request (dn)) != SANE_STATUS_GOOD)
            {
              *size = recvd;
              return SANE_STATUS_IO_ERROR;
            }
          if ( ( error = bjnp_recv_header (dn, &(device[dn].scanner_data_left) )  ) != SANE_STATUS_GOOD)
            {
              *size = recvd;
              return SANE_STATUS_IO_ERROR;
            }
          /* correct blocksize if applicable */ 

          device[dn].blocksize = MAX (device[dn].blocksize, device[dn].scanner_data_left);

          if ( device[dn].scanner_data_left < device[dn].blocksize)
            {
              /* the scanner will not react at all to a read request, when no more data is available */
              /* we now determine end of data by comparing the payload size to the maximun blocksize */
              /* this block is shorter than blocksize, so after this block we are done */
        
              device[dn].last_block = 1;
            }
          if ( device[dn].scanner_data_left == 0 )
            {
              break;
            }
        }

      PDBG (bjnp_dbg (LOG_DEBUG, "Scanner reports 0x%lx = %ld bytes available\n",
		 (unsigned long) device[dn].scanner_data_left,
		 (unsigned long) device[dn].scanner_data_left));

     /* read as many bytes as needed and available */

      more = MIN( device[dn].scanner_data_left, (requested - recvd) );

      PDBG (bjnp_dbg
	    (LOG_DEBUG,
	     "reading 0x%lx = %ld (of max 0x%lx = %ld) bytes\n",
	     (unsigned long) more, 
	     (unsigned long) more,
	     (unsigned long) device[dn].scanner_data_left, 
	     (unsigned long) device[dn].scanner_data_left) );

      result = bjnp_recv_data (dn, buffer + recvd, &more);
      if (result != SANE_STATUS_GOOD)
	{
	  *size = recvd;
	  return SANE_STATUS_IO_ERROR;
	}
      PDBG (bjnp_dbg (LOG_DEBUG, "Requested %ld bytes, received: %ld\n", 
            MIN( device[dn].scanner_data_left, (requested - recvd) ), more) );

      device[dn].scanner_data_left = device[dn].scanner_data_left - more;
      recvd = recvd + more;
    }

  PDBG (bjnp_dbg (LOG_DEBUG, "returning %ld bytes, backend expexts %ld\n", 
        recvd, *size ) );
  *size = recvd;
  if ( *size == 0 )
    return SANE_STATUS_EOF;
  return SANE_STATUS_GOOD;
}

/** Initiate a bulk transfer write.
 *
 * Write up to size bytes from buffer to the device. After the write size
 * contains the number of bytes actually written.
 *
 * @param dn device number
 * @param buffer buffer to write to device
 * @param size size of the data
 *
 * @return 
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_IO_ERROR - if an error occured during the write
 * - SANE_STATUS_INVAL - on every other error
 */

extern SANE_Status
sanei_bjnp_write_bulk (SANE_Int dn, const SANE_Byte * buffer, size_t * size)
{
  ssize_t sent;
  size_t recvd;
  uint32_t buf;
  size_t payload_size;

  PDBG (bjnp_dbg
	(LOG_INFO, "bjnp_write_bulk(%d, bufferptr, 0x%lx = %ld)\n", dn,
	 (unsigned long) *size, (unsigned long) *size));
  sent = bjnp_write (dn, buffer, *size);
  if (sent < 0)
    return SANE_STATUS_IO_ERROR;
  if (sent != (int) *size)
    {
      PDBG (bjnp_dbg
	    (LOG_CRIT, "Sent only %ld bytes to scanner, expected %ld!!\n",
	     (unsigned long) sent, (unsigned long) *size));
      return SANE_STATUS_IO_ERROR;
    }

  if (bjnp_recv_header (dn, &payload_size) != SANE_STATUS_GOOD)
    {
      PDBG (bjnp_dbg (LOG_CRIT, "Could not read response to command!\n"));
      return SANE_STATUS_IO_ERROR;
    }

  if (payload_size != 4)
    {
      PDBG (bjnp_dbg (LOG_CRIT,
		       "Scanner length of write confirmation = 0x%lx bytes = %ld, expected %d!!\n",
		       (unsigned long) payload_size,
		       (unsigned long) payload_size, 4));
      return SANE_STATUS_IO_ERROR;
    }
  recvd = payload_size;
  if ((bjnp_recv_data (dn, (unsigned char *) &buf, &recvd) !=
       SANE_STATUS_GOOD) || (recvd != payload_size))
    {
      PDBG (bjnp_dbg (LOG_CRIT,
		       "Could not read length of data confirmed by device\n"));
      return SANE_STATUS_IO_ERROR;
    }
  recvd = ntohl (buf);
  if (recvd != *size)
    {
      PDBG (bjnp_dbg
	    (LOG_CRIT, "Scanner confirmed %ld bytes, expected %ld!!\n",
	     (unsigned long) recvd, (unsigned long) *size));
      return SANE_STATUS_IO_ERROR;
    }
  /* we can expect data from the scanner again */

  device[dn].last_block = 0;

  return SANE_STATUS_GOOD;
}

/** Initiate a interrupt transfer read.
 *
 * Read up to size bytes from the interrupt endpoint from the device to
 * buffer. After the read, size contains the number of bytes actually read.
 *
 * @param dn device number
 * @param buffer buffer to store read data in
 * @param size size of the data
 *
 * @return 
 * - SANE_STATUS_GOOD - on succes
 * - SANE_STATUS_EOF - if zero bytes have been read
 * - SANE_STATUS_IO_ERROR - if an error occured during the read
 * - SANE_STATUS_INVAL - on every other error
 *
 */

extern SANE_Status
sanei_bjnp_read_int (SANE_Int dn, SANE_Byte * buffer, size_t * size)
{
#ifndef PIXMA_BJNP_USE_STATUS
  PDBG (bjnp_dbg
	(LOG_INFO, "bjnp_read_int(%d, bufferptr, 0x%lx = %ld):\n", dn,
	 (unsigned long) *size, (unsigned long) *size));

  memset (buffer, 0, *size);
  sleep (1);
  return SANE_STATUS_IO_ERROR;
#else

  char hostname[256];
  int resp_len;
  int timeout;
  int seconds;
  
  PDBG (bjnp_dbg
	(LOG_INFO, "bjnp_read_int(%d, bufferptr, 0x%lx = %ld):\n", dn,
	 (unsigned long) *size, (unsigned long) *size));

  memset (buffer, 0, *size);

  gethostname (hostname, 32);
  hostname[32] = '\0';


  switch (device[dn].polling_status)
    {
    case BJNP_POLL_STOPPED:

      /* establish dialog */

      if ( (bjnp_poll_scanner (dn, 0, hostname, getusername (), buffer, *size ) != 0) ||
           (bjnp_poll_scanner (dn, 1, hostname, getusername (), buffer, *size ) != 0) )
        {
	  PDBG (bjnp_dbg (LOG_NOTICE, "Failed to setup read_intr dialog with device!\n"));
          device[dn].dialog = 0;
          device[dn].status_key = 0;
          return SANE_STATUS_IO_ERROR;
        }
      device[dn].polling_status = BJNP_POLL_STARTED;

    case BJNP_POLL_STARTED:
      /* we use only seonds accuracy between poll attempts */
      timeout = device[dn].bjnp_timeout /1000;
     
      do
        {
          if ( (resp_len = bjnp_poll_scanner (dn, 2, hostname, getusername (), buffer, *size ) ) < 0 )
            {
              PDBG (bjnp_dbg (LOG_NOTICE, "Restarting polling dialog!\n"));
              device[dn].polling_status = BJNP_POLL_STOPPED;
              *size = 0;
              return SANE_STATUS_EOF;
            }
          *size = (size_t) resp_len;
          if ( resp_len > 0 )
            {
              device[dn].polling_status = BJNP_POLL_STATUS_RECEIVED;

              /* this is a bit of a hack, but the scanner does not like */
              /* us to continue using the existing tcp socket */

              sanei_bjnp_deactivate(dn);
              sanei_bjnp_activate(dn);

              return SANE_STATUS_GOOD;
            }
          seconds = timeout > 2 ? 2 : timeout;
          sleep(seconds);
          timeout = timeout - seconds;
        } while ( timeout > 0 ) ;
      break;
    case BJNP_POLL_STATUS_RECEIVED:
       if ( (resp_len = bjnp_poll_scanner (dn, 5, hostname, getusername (), buffer, *size ) ) < 0 )
        {
          PDBG (bjnp_dbg (LOG_NOTICE, "Restarting polling dialog!\n"));
          device[dn].polling_status = BJNP_POLL_STOPPED;
          *size = 0;
          break;
        }
    }
  return SANE_STATUS_EOF;
#endif
}
