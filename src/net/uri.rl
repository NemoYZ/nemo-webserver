#include "net/uri.h"

#include <sstream>

#include "common/lexical_cast.h"

namespace nemo {
namespace net {
%%{
    # See RFC 3986: http://www.ietf.org/rfc/rfc3986.txt

    machine uri_parser;

    gen_delims = ":" | "/" | "?" | "#" | "[" | "]" | "@";
    sub_delims = "!" | "$" | "&" | "'" | "(" | ")" | "*" | "+" | "," | ";" | "=";
    reserved = gen_delims | sub_delims;
    unreserved = alpha | digit | "-" | "." | "_" | "~";
    pct_encoded = "%" xdigit xdigit;

    action marku { mark = fpc; }
    action markh { mark = fpc; }

    action save_scheme
    {
        uri->setScheme(StringArg(mark, fpc - mark));
        mark = NULL;
    }

    scheme = (alpha (alpha | digit | "+" | "-" | ".")*) >marku %save_scheme;

    action save_port
    {
        if (fpc != mark) {
            uri->setPort(LexicalCast<uint16_t>(mark));
        }
        mark = NULL;
    }
    action save_userinfo
    {
        if(mark) {
            uri->setUserInfo(StringArg(mark, fpc - mark));
        }
        mark = NULL;
    }
    action save_host
    {
        if (mark != NULL) {
            uri->setHost(StringArg(mark, fpc - mark));
        }
    }

    userinfo = (unreserved | pct_encoded | sub_delims | ":")*;
    dec_octet = digit | [1-9] digit | "1" digit{2} | 2 [0-4] digit | "25" [0-5];
    IPv4address = dec_octet "." dec_octet "." dec_octet "." dec_octet;
    h16 = xdigit{1,4};
    ls32 = (h16 ":" h16) | IPv4address;
    IPv6address = (                         (h16 ":"){6} ls32) |
                  (                    "::" (h16 ":"){5} ls32) |
                  ((             h16)? "::" (h16 ":"){4} ls32) |
                  (((h16 ":"){1} h16)? "::" (h16 ":"){3} ls32) |
                  (((h16 ":"){2} h16)? "::" (h16 ":"){2} ls32) |
                  (((h16 ":"){3} h16)? "::" (h16 ":"){1} ls32) |
                  (((h16 ":"){4} h16)? "::"              ls32) |
                  (((h16 ":"){5} h16)? "::"              h16 ) |
                  (((h16 ":"){6} h16)? "::"                  );
    IPvFuture = "v" xdigit+ "." (unreserved | sub_delims | ":")+;
    IP_literal = "[" (IPv6address | IPvFuture) "]";
    reg_name = (unreserved | pct_encoded | sub_delims)*;
    host = IP_literal | IPv4address | reg_name;
    port = digit*;

    authority = ( (userinfo %save_userinfo "@")? host >markh %save_host (":" port >markh %save_port)? ) >markh;

    action save_segment
    {
        mark = NULL;
    }

    action save_path
    {
        uri->setPath(StringArg(mark, fpc - mark));
        mark = NULL;
    }


#    pchar = unreserved | pct_encoded | sub_delims | ":" | "@";
# add (any -- ascii) support chinese
    pchar         = ( (any -- ascii ) | unreserved | pct_encoded | sub_delims | ":" | "@" ) ;
    segment = pchar*;
    segment_nz = pchar+;
    segment_nz_nc = (pchar - ":")+;

    action clear_segments
    {
    }

    path_abempty = (("/" segment))? ("/" segment)*;
    path_absolute = ("/" (segment_nz ("/" segment)*)?);
    path_noscheme = segment_nz_nc ("/" segment)*;
    path_rootless = segment_nz ("/" segment)*;
    path_empty = "";
    path = (path_abempty | path_absolute | path_noscheme | path_rootless | path_empty);

    action save_query
    {
        uri->setQuery(StringArg(mark, fpc - mark));
        mark = NULL;
    }
    action save_fragment
    {
        uri->setFragment(StringArg(mark, fpc - mark));
        mark = NULL;
    }

    query = (pchar | "/" | "?")* >marku %save_query;
    fragment = (pchar | "/" | "?")* >marku %save_fragment;

    hier_part = ("//" authority path_abempty > markh %save_path) | path_absolute | path_rootless | path_empty;

    relative_part = ("//" authority path_abempty) | path_absolute | path_noscheme | path_empty;
    relative_ref = relative_part ( "?" query )? ( "#" fragment )?;

    absolute_URI = scheme ":" hier_part ( "?" query )? ;
    # Obsolete, but referenced from HTTP, so we translate
    relative_URI = relative_part ( "?" query )?;

    URI = scheme ":" hier_part ( "?" query )? ( "#" fragment )?;
    URI_reference = URI | relative_ref;
    main := URI_reference;
    write data;
}%%

Uri::UniquePtr Uri::Create(StringArg uriStr) {
    Uri::UniquePtr uri = std::make_unique<Uri>();
    int cs = 0;
    const char* mark = 0;
    %% write init;
    const char *p = uriStr.data();
    const char *pe = p + uriStr.size();
    const char* eof = pe;
    %% write exec;
    if(cs == uri_parser_error) {
        return nullptr;
    } else if(cs >= uri_parser_first_final) {
        return uri;
    }
    return nullptr;
}

Uri::Uri() :
    port_(0) {
}

bool Uri::isDefaultPort() const {
    if(0 == port_) {
        return true;
    }
    if(scheme_ == "http"
            || scheme_ == "ws") {
        return port_ == 80;
    } else if(scheme_ == "https"
            || scheme_ == "wss") {
        return port_ == 443;
    }
    return false;
}

const String& Uri::getPath() const {
    static String sDefaultPath = "/";
    return path_.empty() ? sDefaultPath : path_;
}

uint16_t Uri::getPort() const {
    if(port_) {
        return port_;
    }
    if(scheme_ == "http"
        || scheme_ == "ws") {
        return 80;
    } else if(scheme_ == "https"
            || scheme_ == "wss") {
        return 443;
    }
    return port_;
}

std::ostream& Uri::dump(std::ostream& os) const {
    os << scheme_ << "://"
       << userInfo_
       << (userInfo_.empty() ? "" : "@")
       << host_
       << (isDefaultPort() ? "" : ":" + LexicalCast<String>(port_))
       << getPath()
       << (query_.empty() ? "" : "?")
       << query_
       << (fragment_.empty() ? "" : "#")
       << fragment_;
    return os;
}

String Uri::toString() const {
    std::stringstream ss;
    dump(ss);
    return ss.str();
}

IpAddress::UniquePtr Uri::createIpAddress() const {
    IpAddress::UniquePtr ipAddress = Address::LookupAnyIPAddress(host_);
    if (ipAddress) {
        ipAddress->setPort(getPort());
    }
    return ipAddress;
}

} // namespace net
} // namespace nemo