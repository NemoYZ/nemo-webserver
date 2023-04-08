#pragma once

#include "net/http/http11_common.h"

typedef struct httpclient_parser { 
  int cs;                       ///< 解析结果
  size_t body_start;            ///< 报文体开始处
  int content_len;              ///< body大小
  int status;                   ///< 状态码
  int chunked;                  ///< 是否分块解析
  int chunks_done;              ///< 当前块解析完成?
  int close;                    ///< 是否为长连接
  size_t nread;                 ///< 已经解析长度
  size_t mark;                  ///< 标记
  size_t field_start;           ///< 当前解析字段开始(keep-alive、content-length等)
  size_t field_len;             ///< 当前解析字段长度

  void *data;                   ///< 要解析的字符串

  field_cb http_field;
  element_cb reason_phrase;
  element_cb status_code;
  element_cb chunk_size;
  element_cb http_version;
  element_cb header_done;
  element_cb last_chunk;
  
  
} httpclient_parser;

int httpclient_parser_init(httpclient_parser *parser);
int httpclient_parser_finish(httpclient_parser *parser);
int httpclient_parser_execute(httpclient_parser *parser, const char *data, size_t len, size_t off);
int httpclient_parser_has_error(httpclient_parser *parser);
int httpclient_parser_is_finished(httpclient_parser *parser);

#define httpclient_parser_nread(parser) (parser)->nread 