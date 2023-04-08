/**
 * ragel 有限状态机简介
 *  基础代码块 %% {...} %%
 *  1. 类型
 *      machine: 定义一个有限状态机
 *      action: 当匹配语句时执行的动作(有点回调的味道)
 *  2. 关键字
 *      fpc: 指向当前的字符的指针，等价于P
 *      fc: 当前的字符，相当于(*fpc)
 *      buffer: 要解析的字符串
 *      p: buffer的起始位置
 *      pe: 指向buffer的终止位置
 *      cs: 当前状态
 *      fbreak: 让fpc + 1, 保存状态到cs，然后跳出解析的循环(结束解析)
 *      ascii: ascii字符，0...127
 * 3. 运算符
 *      >: 匹配开始调用action
 *      %: 匹配末尾调用action
 *      @: 除末态以外的任何状态调用action
 *      --: 求差集
 *      -: 求差集
 *      |: 求并集
 *      &: 求交集
 *      +: 重复至少一次
 *      *: 重复至少0次
 *      :>: 连接两个状态，右边的状态优先级高于左边的
 * 4. 内置有限状态机
 *      any: 字母表中的任何字符
 */

#include "net/http/http_client_parser.h"
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#define LEN(AT, FPC) (FPC - buffer - parser->AT)                //
#define MARK(M,FPC) (parser->M = (FPC) - buffer)                //
#define PTR_TO(F) (buffer + parser->F)                          //
#define check(A, M, ...) if(!(A)) {  errno=0; goto error; }     //


/** machine **/
%%{
    machine httpclient_parser;

    action mark {MARK(mark, fpc); }                             #标记啥呢？没搞懂

    action start_field { MARK(field_start, fpc); }              #字段起始位置

    action write_field {                                        //当前解析字段长度
        parser->field_len = LEN(field_start, fpc);
    }

    action start_value { MARK(mark, fpc); }

    action write_content_len {                                  //body长度
        parser->content_len = strtol(PTR_TO(mark), NULL, 10);
    }

    action write_connection_close {                             //非长连接
        parser->close = 1;
    }

    action write_value {                                        //字符串http报文起始处                                 
        if(parser->http_field != NULL) {
            parser->http_field(parser->data, PTR_TO(field_start), parser->field_len, PTR_TO(mark), LEN(mark, fpc));
        }
    }

    action reason_phrase {                                      //原因字段
        if(parser->reason_phrase != NULL)
            parser->reason_phrase(parser->data, PTR_TO(mark), LEN(mark, fpc));
    }

    action status_code {                                        //状态码
        parser->status = strtol(PTR_TO(mark), NULL, 10);

        if(parser->status_code != NULL)
            parser->status_code(parser->data, PTR_TO(mark), LEN(mark, fpc));
    }

    action http_version {	                                    //http版本
        if(parser->http_version != NULL)
            parser->http_version(parser->data, PTR_TO(mark), LEN(mark, fpc));
    }

    action chunk_size {                                         //块大小
        parser->chunked = 1;
        parser->content_len = strtol(PTR_TO(mark), NULL, 16);
        parser->chunks_done = parser->content_len <= 0;

        if(parser->chunks_done && parser->last_chunk) {
            parser->last_chunk(parser->data, PTR_TO(mark), LEN(mark, fpc));
        } else if(parser->chunk_size != NULL) {
            parser->chunk_size(parser->data, PTR_TO(mark), LEN(mark, fpc));
        } // else skip it
    }

    action trans_chunked {                                      //分块解析
        parser->chunked = 1;
    }

    action done {                                               //解析完成
        parser->body_start = fpc - buffer + 1; 
        if(parser->header_done != NULL)
            parser->header_done(parser->data, fpc + 1, pe - fpc - 1);
        fbreak;
    }

# line endings
    CRLF = ("\r\n" | "\n");                                                 #回车

# character types
    CTL = (cntrl | 127);                                                    #不可打印字符
    tspecials = ("(" | ")" | "<" | ">" | "@" | "," | ";" | ":" | "\\" | "\"" | "/" | "[" | "]" | "?" | "=" | "{" | "}" | " " | "\t"); #特殊字符

# elements
    token = (ascii -- (CTL | tspecials));                                   #普通字符

    Reason_Phrase = (any -- CRLF)+ >mark %reason_phrase;                    #状态码的解析
    Status_Code = digit+ >mark %status_code;                                #状态码
    http_number = (digit+ "." digit+);                                      #http版本号
    HTTP_Version = ("HTTP/" http_number) >mark %http_version ;              #http版本字段
    Status_Line = HTTP_Version " " Status_Code " " Reason_Phrase :> CRLF;   #http响应报文第一行

    field_name = token+ >start_field %write_field;                          #字段名(keep-alive、content-length等)
    field_value = any* >start_value %write_value;                           #字段值
    fields = field_name ":" space* field_value :> CRLF;                     #字段

    content_length = (/Content-Length/i >start_field %write_field ":" space *   #body长度
            digit+ >start_value %write_content_len %write_value) CRLF;

    conn_close = (/Connection/i ":" space* /close/i) CRLF %write_connection_close;  #是否为长连接

    transfer_encoding_chunked = (/Transfer-Encoding/i >start_field %write_field     #分块传输
            ":" space* /chunked/i >start_value %write_value) CRLF @trans_chunked;

    message_header = transfer_encoding_chunked | conn_close | content_length | fields;

    Response = 	Status_Line (message_header)* CRLF;                                 #响应报文

    chunk_ext_val = token+;
    chunk_ext_name = token+;
    chunk_extension = (";" chunk_ext_name >start_field %write_field %start_value ("=" chunk_ext_val >start_value)? %write_value )*;
    chunk_size = xdigit+;
    Chunked_Header = chunk_size >mark %chunk_size chunk_extension :> CRLF;

main := (Response | Chunked_Header) @done;
}%%

/** Data **/
%% write data;

int httpclient_parser_init(httpclient_parser *parser)  {
    int cs = 0;

    %% write init;

    parser->cs = cs;
    parser->body_start = 0;
    parser->content_len = -1;
    parser->chunked = 0;
    parser->chunks_done = 0;
    parser->mark = 0;
    parser->nread = 0;
    parser->field_len = 0;
    parser->field_start = 0;    
    parser->close = 0;

    return(1);
}


/** exec **/
int httpclient_parser_execute(httpclient_parser *parser, const char *buffer, size_t len, size_t off)  
{
    parser->nread = 0;
    parser->mark = 0;
    parser->field_len = 0;
    parser->field_start = 0;

    const char *p, *pe;
    int cs = parser->cs;

    assert(off <= len && "offset past end of buffer");

    p = buffer+off;
    pe = buffer+len;

    assert(*pe == '\0' && "pointer does not end on NUL");
    assert(pe - p == (int)len - (int)off && "pointers aren't same distance");


    %% write exec;

    parser->cs = cs;
    parser->nread += p - (buffer + off);

    assert(p <= pe && "buffer overflow after parsing execute");
    assert(parser->nread <= len && "nread longer than length");
    assert(parser->body_start <= len && "body starts after buffer end");
    check(parser->mark < len, "mark is after buffer end");
    check(parser->field_len <= len, "field has length longer than whole buffer");
    check(parser->field_start < len, "field starts after buffer end");

    //if(parser->body_start) {
    //    /* final \r\n combo encountered so stop right here */
    //    parser->nread++;
    //}

    return(parser->nread);

error:
    return -1;
}

int httpclient_parser_finish(httpclient_parser *parser)
{
    int cs = parser->cs;

    parser->cs = cs;

    if (httpclient_parser_has_error(parser) ) {
        return -1;
    } else if (httpclient_parser_is_finished(parser) ) {
        return 1;
    } else {
        return 0;
    }
}

int httpclient_parser_has_error(httpclient_parser *parser) {
    return parser->cs == httpclient_parser_error;
}

int httpclient_parser_is_finished(httpclient_parser *parser) {
    return parser->cs == httpclient_parser_first_final;
}