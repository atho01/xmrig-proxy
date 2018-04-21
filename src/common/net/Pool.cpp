/* XMRig
 * Copyright 2010      Jeff Garzik <jgarzik@pobox.com>
 * Copyright 2012-2014 pooler      <pooler@litecoinpool.org>
 * Copyright 2014      Lucas Jones <https://github.com/lucasjones>
 * Copyright 2014-2016 Wolf9466    <https://github.com/OhGodAPet>
 * Copyright 2016      Jay D Dee   <jayddee246@gmail.com>
 * Copyright 2017-2018 XMR-Stak    <https://github.com/fireice-uk>, <https://github.com/psychocrypt>
 * Copyright 2016-2018 XMRig       <https://github.com/xmrig>, <support@xmrig.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program. If not, see <http://www.gnu.org/licenses/>.
 */


#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


#include "common/net/Pool.h"


#ifdef APP_DEBUG
#   include "common/log/Log.h"
#endif


#ifdef _MSC_VER
#   define strncasecmp _strnicmp
#   define strcasecmp  _stricmp
#endif


static const char *algoNames[] = {
    "cryptonight",
#   ifndef XMRIG_NO_AEON
    "cryptonight-lite",
#   else
    nullptr,
#   endif
#   ifndef XMRIG_NO_SUMO
    "cryptonight-heavy",
#   else
    nullptr,
#   endif
#   ifndef XMRIG_NO_IPBC
    "cryptonight-ipbc",
#   else
    nullptr,
#   endif
};


static const char *algoNamesShort[] = {
    "cn",
#   ifndef XMRIG_NO_AEON
    "cn-lite",
#   else
    nullptr,
#   endif
#   ifndef XMRIG_NO_SUMO
    "cn-heavy",
#   else
    nullptr,
#   endif
#   ifndef XMRIG_NO_IPBC
    "cn-ipbc",
#   else
    nullptr,
#   endif
};


Pool::Pool() :
    m_nicehash(false),
    m_keepAlive(0),
    m_port(kDefaultPort),
    m_algorithm(xmrig::INVALID_ALGO),
    m_variant(xmrig::VARIANT_AUTO)
{
}


/**
 * @brief Parse url.
 *
 * Valid urls:
 * example.com
 * example.com:3333
 * stratum+tcp://example.com
 * stratum+tcp://example.com:3333
 *
 * @param url
 */
Pool::Pool(const char *url) :
    m_nicehash(false),
    m_keepAlive(0),
    m_port(kDefaultPort),
    m_algorithm(xmrig::INVALID_ALGO),
    m_variant(xmrig::VARIANT_AUTO)
{
    parse(url);
}


Pool::Pool(const char *host, uint16_t port, const char *user, const char *password, int keepAlive, bool nicehash, xmrig::Variant variant) :
    m_nicehash(nicehash),
    m_keepAlive(keepAlive),
    m_port(port),
    m_algorithm(xmrig::INVALID_ALGO),
    m_host(host),
    m_password(password),
    m_user(user),
    m_variant(variant)
{
    const size_t size = m_host.size() + 8;
    assert(size > 8);

    char *url = new char[size]();
    snprintf(url, size - 1, "%s:%d", m_host.data(), m_port);

    m_url = url;
}


const char *Pool::algoName(xmrig::Algo algorithm, bool shortName)
{
    if (algorithm == xmrig::INVALID_ALGO) {
        return "invalid";
    }

    return (shortName ? algoNamesShort : algoNames)[algorithm];
}


xmrig::Algo Pool::algorithm(const char *algo)
{
#   ifndef XMRIG_NO_AEON
    if (strcasecmp(algo, "cryptonight-light") == 0) {
        fprintf(stderr, "Algorithm \"cryptonight-light\" is deprecated, use \"cryptonight-lite\" instead\n");

        return xmrig::CRYPTONIGHT_LITE;
    }
#   endif

    const size_t size = sizeof(algoNames) / sizeof(algoNames[0]);

    assert(size == (sizeof(algoNamesShort) / sizeof(algoNamesShort[0])));

    for (size_t i = 0; i < size; i++) {
        if ((algoNames[i] && strcasecmp(algo, algoNames[i]) == 0) || (algoNamesShort[i] && strcasecmp(algo, algoNamesShort[i]) == 0)) {
            return static_cast<xmrig::Algo>(i);
        }
    }

    fprintf(stderr, "Unknown algorithm \"%s\" specified.\n", algo);
    return xmrig::INVALID_ALGO;
}


bool Pool::isEqual(const Pool &other) const
{
    return (m_nicehash     == other.m_nicehash
            && m_keepAlive == other.m_keepAlive
            && m_port      == other.m_port
            && m_algorithm == other.m_algorithm
            && m_host      == other.m_host
            && m_password  == other.m_password
            && m_url       == other.m_url
            && m_user      == other.m_user
            && m_variant   == other.m_variant);
}


bool Pool::parse(const char *url)
{
    assert(url != nullptr);

    const char *p = strstr(url, "://");
    const char *base = url;

    if (p) {
        if (strncasecmp(url, "stratum+tcp://", 14)) {
            return false;
        }

        base = url + 14;
    }

    if (!strlen(base) || *base == '/') {
        return false;
    }

    m_url = url;
    if (base[0] == '[') {
        return parseIPv6(base);
    }

    const char *port = strchr(base, ':');
    if (!port) {
        m_host = base;
        return true;
    }

    const size_t size = port++ - base + 1;
    char *host        = new char[size]();
    memcpy(host, base, size - 1);

    m_host = host;
    m_port = static_cast<uint16_t>(strtol(port, nullptr, 10));

    return true;
}


bool Pool::setUserpass(const char *userpass)
{
    const char *p = strchr(userpass, ':');
    if (!p) {
        return false;
    }

    char *user = new char[p - userpass + 1]();
    strncpy(user, userpass, p - userpass);

    m_user     = user;
    m_password = p + 1;

    return true;
}


void Pool::adjust(xmrig::Algo algorithm)
{
    if (!isValid()) {
        return;
    }

    if (m_algorithm == xmrig::INVALID_ALGO) {
        m_algorithm = algorithm;
    }

    if (strstr(m_host.data(), ".nicehash.com")) {
        m_keepAlive = false;
        m_nicehash  = true;
    }

    if (strstr(m_host.data(), ".minergate.com")) {
        m_keepAlive = false;
    }
}


void Pool::setVariant(int variant)
{
   switch (variant) {
   case xmrig::VARIANT_AUTO:
   case xmrig::VARIANT_V0:
   case xmrig::VARIANT_V1:
       m_variant = static_cast<xmrig::Variant>(variant);
       break;

   default:
       assert(false);
       break;
   }
}


xmrig::Variant Pool::variant() const
{
    switch (m_algorithm) {
    case xmrig::CRYPTONIGHT_HEAVY:
        return xmrig::VARIANT_V0;

    case xmrig::CRYPTONIGHT_IPBC:
        return xmrig::VARIANT_V1;

    default:
        break;
    }

    return m_variant;
}


#ifdef APP_DEBUG
void Pool::print() const
{
    LOG_NOTICE("url:       %s", m_url.data());
    LOG_DEBUG ("host:      %s", m_host.data());
    LOG_DEBUG ("port:      %d", static_cast<int>(m_port));
    LOG_DEBUG ("user:      %s", m_user.data());
    LOG_DEBUG ("pass:      %s", m_password.data());
    LOG_DEBUG ("algo:      %s/%d", algoName(m_algorithm), static_cast<int>(variant()));
    LOG_DEBUG ("nicehash:  %d", static_cast<int>(m_nicehash));
    LOG_DEBUG ("keepAlive: %d", m_keepAlive);
}
#endif


bool Pool::parseIPv6(const char *addr)
{
    const char *end = strchr(addr, ']');
    if (!end) {
        return false;
    }

    const char *port = strchr(end, ':');
    if (!port) {
        return false;
    }

    const size_t size = end - addr;
    char *host        = new char[size]();
    memcpy(host, addr + 1, size - 1);

    m_host = host;
    m_port = static_cast<uint16_t>(strtol(port + 1, nullptr, 10));

    return true;
}
