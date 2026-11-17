// SPDX-License-Identifier: GPL-3.0-or-later
// lwIP's socket header on Linux: the POSIX ones it mirrors. LWIP_IPV6 stays
// undefined: the httpd shim listens on IPv4.
#pragma once

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
