# SEXP (S-expressions) Basic Transport Support
#
# See: http://people.csail.mit.edu/rivest/sexp.html
#

import re

WHITESPACE='[ \n\t\v\r\f]+'
re_ws = re.compile(WHITESPACE)
DIGITS='[0-9]+'
re_digit = re.compile(DIGITS)

def skip_whitespace(string, pos):
    m = re_ws.match(string, pos)
    if m:
        return m.start()
    else:
        return pos

def sexp_match(string, ch, pos):
    pos = skip_whitespace(string,pos)
    if string[pos] == ch:
        return pos+1
    else:
        raise ValueError("expect '%s'" % ch)

def sexp_parse_simple_string(string, pos):
    pos = skip_whitespace(string,pos)
    m = re_digit.match(string, pos)
    if m:
        length = int(string[m.start():m.end()],10)
        pos = sexp_match(string, ':', m.end())
        return (string[pos:pos+length], pos+length)
    else:
        raise ValueError('expect digit')

def sexp_parse_list(string,pos):
    l = []
    while True:
        pos = skip_whitespace(string,pos)
        if string[pos] == ')':
            return (l, pos)
        else:
            (sexp, pos) = sexp_parse(string,pos)
            l.append(sexp)

def sexp_parse(string, pos=0):
    pos = skip_whitespace(string,pos)
    if string[pos] == '(':
        (l, pos) = sexp_parse_list(string,pos+1)
        pos = sexp_match(string, ')', pos)
        return (l, pos)
    elif string[pos] == '[':
        pos = skip_whitespace(string,pos)
        (dsp, pos) = sexp_parse_simple_string(string,pos+1)
        pos = sexp_match(string, ']', pos)
        pos = skip_whitespace(string,pos)
        (ss, pos) = sexp_parse_simple_string(string, pos)
        return ((dsp, ss), pos)
    else:
        return sexp_parse_simple_string(string, pos)

def sexp(string):
    (sexp, pos) = sexp_parse(string)
    return sexp
