// SDL_net (TCP only) on POSIX sockets: the engine's link to its frontend.
#include "hwhaiku.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <stdlib.h>
#include <string.h>

struct IPaddress { Uint32 host; Uint16 port; };	// both in network byte order

struct TCPsocket {	// layout as SDLh.pas' TTCPSocket
	int ready;
	int channel;
	IPaddress remote, local;
	int sflag;	// 1: listening server socket
};

struct SocketSet { int count, max; TCPsocket** sockets; };

HW_API int SDLNet_Init() { return 0; }
HW_API void SDLNet_Quit() {}

HW_API int SDLNet_ResolveHost(IPaddress* addr, const char* host, Uint16 port)
{
	addr->port = htons(port);
	if (!host) {
		addr->host = INADDR_ANY;
		return 0;
	}
	in_addr a;
	if (inet_aton(host, &a)) {
		addr->host = a.s_addr;
		return 0;
	}
	hostent* he = gethostbyname(host);
	if (!he) {
		addr->host = INADDR_NONE;
		hw_set_error("can't resolve %s", host);
		return -1;
	}
	memcpy(&addr->host, he->h_addr, 4);
	return 0;
}

HW_API TCPsocket* SDLNet_TCP_Open(IPaddress* ip)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		hw_set_error("socket failed");
		return NULL;
	}
	sockaddr_in sa = {};
	sa.sin_family = AF_INET;
	sa.sin_addr.s_addr = ip->host;
	sa.sin_port = ip->port;
	TCPsocket* s = (TCPsocket*)calloc(1, sizeof(TCPsocket));
	s->channel = fd;
	if (ip->host == INADDR_ANY) {
		int yes = 1;
		setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
		if (bind(fd, (sockaddr*)&sa, sizeof(sa)) < 0 || listen(fd, 5) < 0) {
			hw_set_error("can't listen on port %d", ntohs(ip->port));
			close(fd);
			free(s);
			return NULL;
		}
		s->sflag = 1;
		s->local = *ip;
	} else {
		if (connect(fd, (sockaddr*)&sa, sizeof(sa)) < 0) {
			hw_set_error("can't connect to port %d", ntohs(ip->port));
			close(fd);
			free(s);
			return NULL;
		}
		int yes = 1;
		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
		s->remote = *ip;
	}
	return s;
}

HW_API TCPsocket* SDLNet_TCP_Accept(TCPsocket* server)
{
	if (!server || !server->sflag)
		return NULL;
	server->ready = 0;
	sockaddr_in sa;
	socklen_t len = sizeof(sa);
	int fd = accept(server->channel, (sockaddr*)&sa, &len);
	if (fd < 0)
		return NULL;
	TCPsocket* s = (TCPsocket*)calloc(1, sizeof(TCPsocket));
	s->channel = fd;
	s->remote = {sa.sin_addr.s_addr, sa.sin_port};
	return s;
}

HW_API int SDLNet_TCP_Send(TCPsocket* s, const void* data, int len)
{
	int sent = 0;
	while (sent < len) {
		ssize_t n = send(s->channel, (const char*)data + sent, len - sent, 0);
		if (n <= 0)
			break;
		sent += n;
	}
	return sent;
}

HW_API int SDLNet_TCP_Recv(TCPsocket* s, void* data, int maxlen)
{
	s->ready = 0;
	return (int)recv(s->channel, data, maxlen, 0);
}

HW_API void SDLNet_TCP_Close(TCPsocket* s)
{
	if (!s)
		return;
	close(s->channel);
	free(s);
}

HW_API SocketSet* SDLNet_AllocSocketSet(int max)
{
	SocketSet* set = (SocketSet*)calloc(1, sizeof(SocketSet));
	set->max = max;
	set->sockets = (TCPsocket**)calloc(max, sizeof(TCPsocket*));
	return set;
}

HW_API void SDLNet_FreeSocketSet(SocketSet* set)
{
	if (!set)
		return;
	free(set->sockets);
	free(set);
}

HW_API int SDLNet_AddSocket(SocketSet* set, TCPsocket* s)
{
	for (int i = 0; i < set->count; i++)
		if (set->sockets[i] == s)
			return set->count;	// the engine adds its socket every frame
	if (set->count >= set->max) {
		hw_set_error("socket set is full");
		return -1;
	}
	set->sockets[set->count++] = s;
	return set->count;
}

HW_API int SDLNet_CheckSockets(SocketSet* set, Uint32 timeout)
{
	fd_set fds;
	FD_ZERO(&fds);
	int maxfd = -1;
	for (int i = 0; i < set->count; i++) {
		FD_SET(set->sockets[i]->channel, &fds);
		if (set->sockets[i]->channel > maxfd)
			maxfd = set->sockets[i]->channel;
	}
	timeval tv = {(time_t)(timeout / 1000), (suseconds_t)((timeout % 1000) * 1000)};
	int n = select(maxfd + 1, &fds, NULL, NULL, &tv);
	if (n <= 0)
		return n;
	for (int i = 0; i < set->count; i++)
		set->sockets[i]->ready = FD_ISSET(set->sockets[i]->channel, &fds);
	return n;
}
