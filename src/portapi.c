/*
 * portapi.c - port common API
 *
 *  Copyright(C) 2002-2003 by Shiro Kawai (shiro@acm.org)
 *
 *  Permission to use, copy, modify, distribute this software and
 *  accompanying documentation for any purpose is hereby granted,
 *  provided that existing copyright notices are retained in all
 *  copies and that this notice is included verbatim in all
 *  distributions.
 *  This software is provided as is, without express or implied
 *  warranty.  In no circumstances the author(s) shall be liable
 *  for any damages arising out of the use of this software.
 *
 *  $Id: portapi.c,v 1.8 2003-02-05 09:50:00 shirok Exp $
 */

/* This file is included twice by port.c to define safe- and unsafe-
 * variant of port common APIs.
 *
 * The macro SHORTCUT allows 'safe' version to bypass lock/unlock
 * stuff by calling 'unsafe' version when the port is already locked by
 * the calling thread.
 */

#ifdef SAFE_PORT_OP
#define VMDECL        ScmVM *vm = Scm_VM()
#define LOCK(p)       PORT_LOCK(p, vm)
#define UNLOCK(p)     PORT_UNLOCK(p)
#define SAFE_CALL(p, exp) PORT_SAFE_CALL(p, exp)
#define SHORTCUT(p, unsafe) \
  do { if (PORT_LOCKED(p, vm)) { unsafe; }} while (0)
#else
#define VMDECL        /*none*/
#define LOCK(p)       /*none*/
#define UNLOCK(p)     /*none*/
#define SAFE_CALL(p, exp) (exp)
#define SHORTCUT(p, unsafe) /* none */
#endif

/* Convenience macro */

#define CLOSE_CHECK(port)                                               \
    do {                                                                \
        if (SCM_PORT_CLOSED_P(port)) {                                  \
            UNLOCK(p);                                                  \
            Scm_Error("I/O attempted on closed port: %S", (port));      \
        }                                                               \
    } while (0)

/*=================================================================
 * Putb
 */

#ifdef SAFE_PORT_OP
void Scm_Putb(ScmByte b, ScmPort *p)
#else
void Scm_PutbUnsafe(ScmByte b, ScmPort *p)
#endif
{
    VMDECL;
    SHORTCUT(p, Scm_PutbUnsafe(b, p); return);
    LOCK(p);
    CLOSE_CHECK(p);

    switch (SCM_PORT_TYPE(p)) {
    case SCM_PORT_FILE:
        if (p->src.buf.current >= p->src.buf.end) {
            SAFE_CALL(p, bufport_flush(p, 1));
        }
        SCM_ASSERT(p->src.buf.current < p->src.buf.end);
        *p->src.buf.current++ = b;
        if (p->src.buf.mode == SCM_PORT_BUFFER_NONE) {
            SAFE_CALL(p, bufport_flush(p, 1));
        }
        UNLOCK(p);
        break;
    case SCM_PORT_OSTR:
        SCM_DSTRING_PUTB(&p->src.ostr, b);
        UNLOCK(p);
        break;
    case SCM_PORT_PROC:
        SAFE_CALL(p, p->src.vt.Putb(b, p));
        UNLOCK(p);
        break;
    default:
        UNLOCK(p);
        Scm_Error("bad port type for output: %S", p);
    }
}

/*=================================================================
 * Putc
 */

#ifdef SAFE_PORT_OP
void Scm_Putc(ScmChar c, ScmPort *p)
#else
void Scm_PutcUnsafe(ScmChar c, ScmPort *p)
#endif
{
    int nb;
    VMDECL;
    SHORTCUT(p, Scm_PutcUnsafe(c, p); return);
    LOCK(p);
    CLOSE_CHECK(p);
    
    switch (SCM_PORT_TYPE(p)) {
    case SCM_PORT_FILE:
        nb = SCM_CHAR_NBYTES(c);
        if (p->src.buf.current+nb > p->src.buf.end) {
            SAFE_CALL(p, bufport_flush(p, nb));
        }
        SCM_ASSERT(p->src.buf.current+nb <= p->src.buf.end);
        SCM_CHAR_PUT(p->src.buf.current, c);
        p->src.buf.current += nb;
        if (p->src.buf.mode == SCM_PORT_BUFFER_LINE) {
            if (c == '\n') {
                SAFE_CALL(p, bufport_flush(p, nb));
            }
        } else if (p->src.buf.mode == SCM_PORT_BUFFER_NONE) {
            SAFE_CALL(p, bufport_flush(p, nb));
        }
        UNLOCK(p);
        break;
    case SCM_PORT_OSTR:
        SCM_DSTRING_PUTC(&p->src.ostr, c);
        UNLOCK(p);
        break;
    case SCM_PORT_PROC:
        PORT_SAFE_CALL(p, p->src.vt.Putc(c, p));
        UNLOCK(p);
        break;
    default:
        UNLOCK(p);
        Scm_Error("bad port type for output: %S", p);
    }
}

/*=================================================================
 * Puts
 */

#ifdef SAFE_PORT_OP
void Scm_Puts(ScmString *s, ScmPort *p)
#else
void Scm_PutsUnsafe(ScmString *s, ScmPort *p)
#endif
{
    VMDECL;
    SHORTCUT(p, Scm_PutsUnsafe(s, p); return);
    LOCK(p);
    CLOSE_CHECK(p);
    
    switch (SCM_PORT_TYPE(p)) {
    case SCM_PORT_FILE:
        SAFE_CALL(p, bufport_write(p, SCM_STRING_START(s), SCM_STRING_SIZE(s)));
        
        if (p->src.buf.mode == SCM_PORT_BUFFER_LINE) {
            const char *cp = p->src.buf.current;
            while (cp-- > p->src.buf.buffer) {
                if (*cp == '\n') {
                    SAFE_CALL(p, bufport_flush(p, (int)(cp - p->src.buf.current)));
                    break;
                }
            }
        } else if (p->src.buf.mode == SCM_PORT_BUFFER_NONE) {
            SAFE_CALL(p, bufport_flush(p, 0));
        }
        UNLOCK(p);
        break;
    case SCM_PORT_OSTR:
        Scm_DStringAdd(&p->src.ostr, s);
        UNLOCK(p);
        break;
    case SCM_PORT_PROC:
        SAFE_CALL(p, p->src.vt.Puts(s, p));
        UNLOCK(p);
        break;
    default:
        UNLOCK(p);
        Scm_Error("bad port type for output: %S", p);
    }
}

/*=================================================================
 * Putz
 */

#ifdef SAFE_PORT_OP
void Scm_Putz(const char *s, int siz, ScmPort *p)
#else
void Scm_PutzUnsafe(const char *s, int siz, ScmPort *p)
#endif
{
    VMDECL;
    SHORTCUT(p, Scm_PutzUnsafe(s, siz, p); return);
    LOCK(p);
    CLOSE_CHECK(p);
    if (siz < 0) siz = strlen(s);
    switch (SCM_PORT_TYPE(p)) {
    case SCM_PORT_FILE:
        SAFE_CALL(p, bufport_write(p, s, siz));
        if (p->src.buf.mode == SCM_PORT_BUFFER_LINE) {
            const char *cp = p->src.buf.current;
            while (cp-- > p->src.buf.buffer) {
                if (*cp == '\n') {
                    SAFE_CALL(p, bufport_flush(p, (int)(cp - p->src.buf.current)));
                    break;
                }
            }
        } else if (p->src.buf.mode == SCM_PORT_BUFFER_NONE) {
            SAFE_CALL(p, bufport_flush(p, 0));
        }
        UNLOCK(p);
        break;
    case SCM_PORT_OSTR:
        Scm_DStringPutz(&p->src.ostr, s, siz);
        UNLOCK(p);
        break;
    case SCM_PORT_PROC:
        SAFE_CALL(p, p->src.vt.Putz(s, siz, p));
        UNLOCK(p);
        break;
    default:
        UNLOCK(p);
        Scm_Error("bad port type for output: %S", p);
    }
}

/*=================================================================
 * Flush
 */

#ifdef SAFE_PORT_OP
void Scm_Flush(ScmPort *p)
#else
void Scm_FlushUnsafe(ScmPort *p)
#endif
{
    VMDECL;
    SHORTCUT(p, Scm_FlushUnsafe(p); return);
    LOCK(p);
    CLOSE_CHECK(p);
    switch (SCM_PORT_TYPE(p)) {
    case SCM_PORT_FILE:
        SAFE_CALL(p, bufport_flush(p, 0));
        UNLOCK(p);
        break;
    case SCM_PORT_OSTR:
        UNLOCK(p);
        break;
    case SCM_PORT_PROC:
        SAFE_CALL(p, p->src.vt.Flush(p));
        UNLOCK(p);
        break;
    default:
        UNLOCK(p);
        Scm_Error("bad port type for output: %S", p);
    }
}

/*=================================================================
 * Ungetc & PeekChar
 */

#ifdef SAFE_PORT_OP
void Scm_Ungetc(ScmChar c, ScmPort *p)
#else
void Scm_UngetcUnsafe(ScmChar c, ScmPort *p)
#endif
{
    VMDECL;
    SHORTCUT(p, Scm_UngetcUnsafe(c, p); return);
    LOCK(p);
    SCM_UNGETC(c, p);
    UNLOCK(p);
}

#ifdef SAFE_PORT_OP
ScmChar Scm_Peekc(ScmPort *p)
#else
ScmChar Scm_PeekcUnsafe(ScmPort *p)
#endif
{
    ScmChar ch;
    VMDECL;
    SHORTCUT(p, return Scm_PeekcUnsafe(p));
    LOCK(p);
    if ((ch = SCM_PORT_UNGOTTEN(p)) == SCM_CHAR_INVALID) {
        ch = Scm_GetcUnsafe(p);
        SCM_PORT_UNGOTTEN(p) = ch;
    }
    UNLOCK(p);
    return ch;
}

/*=================================================================
 * Ungetb & PeekByte
 */

#ifdef SAFE_PORT_OP
void Scm_Ungetb(int b, ScmPort *p)
#else
void Scm_UngetbUnsafe(int b, ScmPort *p)
#endif
{
    VMDECL;
    SHORTCUT(p, Scm_UngetbUnsafe(b, p); return);
    LOCK(p);
    /* caller must ensure ungetb is called only at the fresh state */
    p->scratch[0] = b;
    p->scrcnt = 1;
    UNLOCK(p);
}

#ifdef SAFE_PORT_OP
int Scm_Peekb(ScmPort *p)
#else
int Scm_PeekbUnsafe(ScmPort *p)
#endif
{
    int b;
    VMDECL;
    SHORTCUT(p, return Scm_PeekbUnsafe(p));
    LOCK(p);
    if (p->scrcnt > 0) {
        b = (unsigned char)p->scratch[0];
    } else {
        SCM_GETB(b, p);
        if (b >= 0) {
            if (p->scrcnt > 0) {
                /* unshift scratch buffer */
                int i;
                SCM_ASSERT(p->scrcnt < SCM_CHAR_MAX_BYTES);
                for (i=p->scrcnt; i>0; i--) {
                    p->scratch[i] = p->scratch[i-1];
                }
                p->scratch[0] = b;
                p->scrcnt++;
            } else {
                p->scratch[0] = b;
                p->scrcnt = 1;
            }
        }
    }
    UNLOCK(p);
    return b;
}

/*=================================================================
 * Getb
 */

#ifndef SHIFT_SCRATCH  /* we need to define this only once */
/* shift scratch buffer content */
#define SHIFT_SCRATCH(p, off) \
   do { int i_; for (i_=0; i_ < (p)->scrcnt; i_++) (p)->scratch[i_]=(p)->scratch[i_+(off)]; } while (0)

/* handle the case that there's remaining data in the scratch buffer */
static int getb_scratch(ScmPort *p)
{
    int b = (unsigned char)p->scratch[0];
    p->scrcnt--;
    SHIFT_SCRATCH(p, 1);
    return b;
}

/* handle the case that there's an ungotten char */
static int getb_ungotten(ScmPort *p)
{
    SCM_CHAR_PUT(p->scratch, p->ungotten);
    p->scrcnt = SCM_CHAR_NBYTES(p->ungotten);
    p->ungotten = SCM_CHAR_INVALID;
    return getb_scratch(p);
}
#endif /*SHIFT_SCRATCH*/

/* Getb body */
#ifdef SAFE_PORT_OP
int Scm_Getb(ScmPort *p)
#else
int Scm_GetbUnsafe(ScmPort *p)
#endif
{
    int b = 0, r = 0;
    VMDECL;
    SHORTCUT(p, return Scm_GetbUnsafe(p));
    LOCK(p);
    CLOSE_CHECK(p);

    /* check if there's "pushded back" stuff */
    if (p->scrcnt) {
        b = getb_scratch(p);
    } else if (p->ungotten != SCM_CHAR_INVALID) {
        b = getb_ungotten(p);
    } else {
        switch (SCM_PORT_TYPE(p)) {
        case SCM_PORT_FILE:
            if (p->src.buf.current >= p->src.buf.end) {
                SAFE_CALL(p, r = bufport_fill(p, 1, FALSE));
                if (r == 0) {
                    UNLOCK(p);
                    return EOF;
                }
            }
            b = (unsigned char)*p->src.buf.current++;
            break;
        case SCM_PORT_ISTR:
            if (p->src.istr.current >= p->src.istr.end) b = EOF;
            else b = (unsigned char)*p->src.istr.current++;
            break;
        case SCM_PORT_PROC:
            SAFE_CALL(p, b = p->src.vt.Getb(p));
            break;
        default:
            UNLOCK(p);
            Scm_Error("bad port type for output: %S", p);
        }
    }
    UNLOCK(p);
    return b;
}

/*=================================================================
 * Getc
 */

/* handle the case that there's data in scratch area */
#ifdef SAFE_PORT_OP
#define GETC_SCRATCH getc_scratch
static int getc_scratch(ScmPort *p)
#else
#define GETC_SCRATCH getc_scratch_unsafe
static int getc_scratch_unsafe(ScmPort *p)
#endif
{
    char tbuf[SCM_CHAR_MAX_BYTES];
    int nb = SCM_CHAR_NFOLLOWS(p->scratch[0]), ch, i, curr = p->scrcnt;
    int r = 0;
    
    memcpy(tbuf, p->scratch, curr);
    p->scrcnt = 0;
    for (i=curr; i<=nb; i++) {
        SAFE_CALL(p, r = Scm_Getb(p));
        if (r == EOF) {
            UNLOCK(p);
            Scm_Error("encountered EOF in middle of a multibyte character from port %S", p);
        }
        tbuf[i] = (char)r;
    }
    SCM_CHAR_GET(tbuf, ch);
    return ch;
}

/* Getc body */
#ifdef SAFE_PORT_OP
int Scm_Getc(ScmPort *p)
#else
int Scm_GetcUnsafe(ScmPort *p)
#endif
{
    int first, nb, c = 0, r = 0;
    VMDECL;
    SHORTCUT(p, return Scm_GetcUnsafe(p));
    LOCK(p);
    CLOSE_CHECK(p);
    if (p->scrcnt > 0) {
        r = GETC_SCRATCH(p);
        UNLOCK(p);
        return r;
    }
    if (p->ungotten != SCM_CHAR_INVALID) {
        c = p->ungotten;
        p->ungotten = SCM_CHAR_INVALID;
        UNLOCK(p);
        return c;
    }

    switch (SCM_PORT_TYPE(p)) {
    case SCM_PORT_FILE:
        if (p->src.buf.current >= p->src.buf.end) {
            SAFE_CALL(p, r = bufport_fill(p, 1, FALSE));
            if (r == 0) {
                UNLOCK(p);
                return EOF;
            }
        }
        first = (unsigned char)*p->src.buf.current++;
        nb = SCM_CHAR_NFOLLOWS(first);
        if (nb > 0) {
            if (p->src.buf.current + nb > p->src.buf.end) {
                /* The buffer doesn't have enough bytes to consist a char.
                   move the incomplete char to the scratch buffer and try
                   to fetch the rest of the char. */
                int rest, filled = 0; 
                p->scrcnt = (unsigned char)(p->src.buf.end - p->src.buf.current + 1);
                memcpy(p->scratch, p->src.buf.current-1, p->scrcnt);
                p->src.buf.current = p->src.buf.end;
                rest = nb + 1 - p->scrcnt;
                for (;;) {
                    SAFE_CALL(p, filled = bufport_fill(p, rest, FALSE));
                    if (filled <= 0) {
                        /* TODO: make this behavior customizable */
                        UNLOCK(p);
                        Scm_Error("encountered EOF in middle of a multibyte character from port %S", p);
                    }
                    if (filled >= rest) {
                        memcpy(p->scratch+p->scrcnt, p->src.buf.current, rest);
                        p->scrcnt += rest;
                        p->src.buf.current += rest;
                        break;
                    } else {
                        memcpy(p->scratch+p->scrcnt, p->src.buf.current, filled);
                        p->scrcnt += filled;
                        p->src.buf.current = p->src.buf.end;
                        rest -= filled;
                    }
                }
                SCM_CHAR_GET(p->scratch, c);
                p->scrcnt = 0;
            } else {
                SCM_CHAR_GET(p->src.buf.current-1, c);
                p->src.buf.current += nb;
            }
        } else {
            c = first;
            if (c == '\n') p->src.buf.line++;
        }
        UNLOCK(p);
        return c;
    case SCM_PORT_ISTR:
        if (p->src.istr.current >= p->src.istr.end) {
            UNLOCK(p);
            return EOF;
        }
        first = (unsigned char)*p->src.istr.current++;
        nb = SCM_CHAR_NFOLLOWS(first);
        if (nb > 0) {
            if (p->src.istr.current + nb > p->src.istr.end) {
                /* TODO: make this behavior customizable */
                UNLOCK(p);
                Scm_Error("encountered EOF in middle of a multibyte character from port %S", p);
            }
            SCM_CHAR_GET(p->src.istr.current-1, c);
            p->src.istr.current += nb;
        } else {
            c = first;
        }
        UNLOCK(p);
        return c;
    case SCM_PORT_PROC:
        SAFE_CALL(p, c = p->src.vt.Getc(p));
        UNLOCK(p);
        return c;
    default:
        UNLOCK(p);
        Scm_Error("bad port type for output: %S", p);
    }
    return 0;/*dummy*/
}

#undef GETC_SCRATCH

/*=================================================================
 * Getz - block read.
 *   If the buffering mode is BUFFER_FULL, this reads BUFLEN bytes
 *   unless it reaches EOF.  Otherwise, this reads less than BUFLEN
 *   if the data is not immediately available.
 */

#ifdef SAFE_PORT_OP
#define GETZ_SCRATCH getz_scratch
static int getz_scratch(char *buf, int buflen, ScmPort *p)
#else
#define GETZ_SCRATCH getz_scratch_unsafe
static int getz_scratch_unsafe(char *buf, int buflen, ScmPort *p)
#endif
{
    int i, n = 0;
    if (p->scrcnt >= buflen) {
        memcpy(buf, p->scratch, buflen);
        p->scrcnt -= buflen;
        SHIFT_SCRATCH(p, buflen);
        return buflen;
    } else {
        memcpy(buf, p->scratch, p->scrcnt);
        i = p->scrcnt;
        p->scrcnt = 0;
        SAFE_CALL(p, n = Scm_Getz(buf+i, buflen-i, p));
        return i + n;
    }
}

#ifndef GETZ_ISTR               /* common part */
#define GETZ_ISTR getz_istr
static int getz_istr(ScmPort *p, char *buf, int buflen)
{
    int siz;
    if (p->src.istr.current + buflen >= p->src.istr.end) {
        if (p->src.istr.current >= p->src.istr.end) return EOF;
        siz = (int)(p->src.istr.end - p->src.istr.current);
        memcpy(buf, p->src.istr.current, siz);
        p->src.istr.current = p->src.istr.end;
        return siz;
    } else {
        memcpy(buf, p->src.istr.current, buflen);
        p->src.istr.current += buflen;
        return buflen;
    }
}
#endif /*!GETZ_ISTR*/

#ifdef SAFE_PORT_OP
int Scm_Getz(char *buf, int buflen, ScmPort *p)
#else
int Scm_GetzUnsafe(char *buf, int buflen, ScmPort *p)
#endif
{
    int siz = 0, r = 0;
    VMDECL;
    SHORTCUT(p, return Scm_GetzUnsafe(buf, buflen, p));
    LOCK(p);
    CLOSE_CHECK(p);

    if (p->scrcnt) {
        r = GETZ_SCRATCH(buf, buflen, p);
        UNLOCK(p);
        return r;
    }
    if (p->ungotten != SCM_CHAR_INVALID) {
        p->scrcnt = SCM_CHAR_NBYTES(p->ungotten);
        SCM_CHAR_PUT(p->scratch, p->ungotten);
        p->ungotten = SCM_CHAR_INVALID;
        r = GETZ_SCRATCH(buf, buflen, p);
        UNLOCK(p);
        return r;
    }

    switch (SCM_PORT_TYPE(p)) {
    case SCM_PORT_FILE:
        SAFE_CALL(p, siz = bufport_read(p, buf, buflen));
        UNLOCK(p);
        if (siz == 0) return EOF;
        else return siz;
    case SCM_PORT_ISTR:
        r = GETZ_ISTR(p, buf, buflen);
        UNLOCK(p);
        return r;
    case SCM_PORT_PROC:
        SAFE_CALL(p, r = p->src.vt.Getz(buf, buflen, p));
        UNLOCK(p);
        return r;
    default:
        UNLOCK(p);
        Scm_Error("bad port type for output: %S", p);
    }
    return -1;                  /* dummy */
}

#undef GETZ_SCRATCH

/*=================================================================
 * ReadLine
 *   Reads up to EOL or EOF.  
 */

/* Readline can be optimized by directly scanning the buffer instead of
   reading one char at a time, bypassing mb->char->mb conversion and
   DString creation.   There was a some attempt to implement it; see
   port.c, v1.69 for details.  For now, I drop that. */

#ifdef SAFE_PORT_OP
ScmObj Scm_ReadLine(ScmPort *p)
#else
ScmObj Scm_ReadLineUnsafe(ScmPort *p)
#endif
{
    int c1, c2;
    ScmDString ds;
    VMDECL;
    SHORTCUT(p, return Scm_ReadLineUnsafe(p));

    Scm_DStringInit(&ds);
    LOCK(p);
    SAFE_CALL(p, c1 = Scm_GetcUnsafe(p));
    if (c1 == EOF) {
        UNLOCK(p);
        return SCM_EOF;
    }
    for (;;) {
        if (c1 == EOF || c1 == '\n') break;
        if (c1 == '\r') {
            SAFE_CALL(p, c2 = Scm_GetcUnsafe(p));
            if (c2 == EOF || c2 == '\n') break;
            Scm_UngetcUnsafe(c2, p);
            break;
        }
        SCM_DSTRING_PUTC(&ds, c1);
        SAFE_CALL(p, c1 = Scm_GetcUnsafe(p));
    }
    UNLOCK(p);
    return Scm_DStringGet(&ds);
}

/*=================================================================
 * CharReady
 */

#ifdef SAFE_PORT_OP
int Scm_CharReady(ScmPort *p)
#else
int Scm_CharReadyUnsafe(ScmPort *p)
#endif
{
    int r = 0;
    VMDECL;
    SHORTCUT(p, return Scm_CharReadyUnsafe(p));
    if (!SCM_IPORTP(p)) Scm_Error("input port required, but got %S", p);
    LOCK(p);
    if (SCM_PORT_UNGOTTEN(p) != SCM_CHAR_INVALID) r = TRUE;
    else {
        switch (SCM_PORT_TYPE(p)) {
        case SCM_PORT_FILE:
            if (p->src.buf.current < p->src.buf.end) r = TRUE;
            else if (p->src.buf.ready == NULL) r = TRUE;
            else {
                SAFE_CALL(p, r = (p->src.buf.ready(p) != SCM_FD_WOULDBLOCK));
            }
            break;
        case SCM_PORT_PROC:
            SAFE_CALL(p, r = p->src.vt.Ready(p));
            break;
        default:
            r = TRUE;
        }
    }
    UNLOCK(p);
    return r;
}

/*=================================================================
 * PortSeek
 */

#ifndef SEEK_ISTR               /* common part */
#define SEEK_ISTR seek_istr
static off_t seek_istr(ScmPort *p, off_t o, int whence, int nomove)
{
    off_t r;
    if (nomove) {
        r = (off_t)(p->src.istr.current - p->src.istr.start);
    } else {
        long z = (long)o;
        if (whence == SEEK_CUR) {
            z += (long)(p->src.istr.current - p->src.istr.start);
        } else if (whence == SEEK_END) {
            z += (long)(p->src.istr.end - p->src.istr.start);
        }
        if (z < 0 || z > (long)(p->src.istr.end - p->src.istr.start)) {
            r = (off_t)-1;
        } else {
            p->src.istr.current = p->src.istr.start + z;
            r = (off_t)(p->src.istr.current - p->src.istr.start);
        }
    }
    return r;
}
#endif /*SEEK_ISTR*/

#ifdef SAFE_PORT_OP
ScmObj Scm_PortSeek(ScmPort *p, ScmObj off, int whence)
#else
ScmObj Scm_PortSeekUnsafe(ScmPort *p, ScmObj off, int whence)
#endif
{
    off_t r = (off_t)-1, o = Scm_IntegerToOffset(off);
    int nomove = (whence == SEEK_CUR && o == 0);
    VMDECL;
    SHORTCUT(p, return Scm_PortSeekUnsafe(p, off, whence));
    if (SCM_PORT_CLOSED_P(p)) {
        Scm_Error("attempt to seek on closed port: %S", p);
    }
    LOCK(p);
    switch (SCM_PORT_TYPE(p)) {
    case SCM_PORT_FILE:
        /* NB: we might be able to skip calling seeker if we keep the
           # of bytes read or write so far, but such count may be off
           when the port has been experienced an error condition. */
        /* NB: the following doesn't work if we have bidirectional port.
           In such case we need to keep whether the last call of buffer
           handling routine was input or output. */
        if (!p->src.buf.seeker) break;
        if (nomove) {
            SAFE_CALL(p, r = p->src.buf.seeker(p, 0, SEEK_CUR));
            if (SCM_PORT_DIR(p)&SCM_PORT_INPUT) {
                r -= (off_t)(p->src.buf.end - p->src.buf.current);
            } else {
                r += (off_t)(p->src.buf.current - p->src.buf.buffer);
            }
        } else {
            /* NB: possible optimization: the specified position is within
               the current buffer, we can avoid calling seeker. */
            if (SCM_PORT_DIR(p)&SCM_PORT_INPUT) {
                if (whence == SEEK_CUR) {
                    o -= (off_t)(p->src.buf.end - p->src.buf.current);
                }
                p->src.buf.current = p->src.buf.end; /* invalidate buffer */
                SAFE_CALL(p, r = p->src.buf.seeker(p, o, whence));
            } else {
                SAFE_CALL(p, bufport_flush(p, 0));
                SAFE_CALL(p, r = p->src.buf.seeker(p, o, whence));
            }
        }
        break;
    case SCM_PORT_ISTR:
        r = SEEK_ISTR(p, o, whence, nomove);
        break;
    case SCM_PORT_OSTR:
        if (nomove) {
            r = (off_t)Scm_DStringSize(&(p->src.ostr));
        } else {
            /* Not supported yet */
            r = (off_t)-1;
        }
        break;
    case SCM_PORT_PROC:
        if (p->src.vt.Seek) {
            SAFE_CALL(p, r = p->src.vt.Seek(p, o, whence));
        }
        break;
    default:
    }
    UNLOCK(p);
    if (r == (off_t)-1) return SCM_FALSE;
    else return Scm_OffsetToInteger(r);
}

#undef VMDECL
#undef LOCK
#undef UNLOCK
#undef SAFE_CALL
#undef SHORTCUT
#undef CLOSE_CHECK

