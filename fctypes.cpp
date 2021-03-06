#include "fctypes.h"
#include "Entities.h" //getHtmlEntity()
#include "Xml.h"
#include "XmlNode.h"
#include "utf8_fast.h"
#include "Log.h"
#include "Errno.h"
#include <fcntl.h>
#include <sys/time.h>
#include "gbmemcpy.h"


char *strncasestr( char *haystack, int32_t haylen, const char *needle){
	int32_t matchLen = 0;
	int32_t needleLen = strlen(needle);
	for (int32_t i = 0; i < haylen;i++){
		char c1 = to_lower_a(haystack[i]);
		char c2 = to_lower_a(needle[matchLen]);
		if ( c1 != c2 ){
			// no match
			matchLen = 0;
			continue;
		}
		// we matched another character
		matchLen++;
		if (matchLen < needleLen) continue;
		
		// we've matched the whole string
		return haystack + i - matchLen + 1;
	}
	return NULL;
}

char *strnstr( const char *haystack, const char *needle, int32_t haystackLen ) {
	int32_t matchLen = 0;
	int32_t needleLen = strlen( needle );
	for ( int32_t i = 0; i < haystackLen; ++i ) {
		char c1 = ( haystack[ i ] );
		char c2 = ( needle[ matchLen ] );
		if ( c1 != c2 ) {
			// no match
			if (matchLen != 0) {
				i -= matchLen;
				matchLen = 0;
			}
			continue;
		}

		// we matched another character
		matchLen++;
		if ( matchLen < needleLen ) {
			continue;
		}

		// we've matched the whole string
		return const_cast<char*>( haystack + i - matchLen + 1 );
	}

	return NULL;
}

const char *strnstrn(const char *haystack, int32_t haystackLen, const char *needle, int32_t needleLen) {
	//glibc has a nice function for this. It is presumably fast
	return (const char*)memmem(haystack,haystackLen,needle,needleLen);
}

// . this stores a "n" into "s" and returns the # of bytes written into "s"
// . it also puts commas into the number
// . it now also NULL terminates bytes written into "s"
int32_t ulltoa ( char *s , uint64_t n ) {
	// if n is zero, it's easy
	if ( n == 0LL ) { *s++='0'; *s='\0'; return 1; }
	// a hunk is a number in [0,999]
	int32_t hunks[10]; 
	int32_t lastHunk = -1;
	// . get the hunks
	// . the first hunk we get is called the "lowest hunk"
	// . "lastHunk" is called the "highest hunk"
	for ( int32_t i = 0 ; i < 10 ; i++ ) {
		hunks[i] = n % 1000;
		n /= 1000;
		if ( hunks[i] != 0 ) lastHunk = i;
	}
	// remember start of buf for calculating # bytes written
	char *start = s;
	// print the hunks separated by comma
	for ( int32_t i = lastHunk ; i >= 0 ; i-- ) {
		// pad all hunks except highest hunk with zeroes
		if ( i != lastHunk ) sprintf ( s , "%03" PRId32 , hunks[i] );
		else                 sprintf ( s , "%" PRId32 , hunks[i] );
		s += strlen(s);
		// comma after all hunks but lowest hunk
		if ( i != 0 ) *s++ = ',';
	}
	// null terminate it
	*s = '\0';
	// return # of bytes stored into "s"
	return s - start;
}

int32_t atol2 ( const char *s, int32_t len ) {
	// skip over spaces
	const char *end = s + len;
	while ( s < end && is_wspace_a ( *s ) ) s++;
	// return 0 if all spaces
	if ( s == end ) return 0;
	int32_t i   = 0;
	int32_t val = 0;
	bool negative = false;
	if ( s[0] == '-' ) { negative = true; i++; }
	while ( i < len && is_digit(s[i]) ) val = val * 10 + ( s[i++] - '0' );
	if ( negative ) return -val;
	return val;
}

int64_t atoll1 ( const char *s ) {
	return atoll ( s );
}

int64_t atoll2 ( const char *s, int32_t len ) {
	// skip over spaces
	const char *end = s + len;
	while ( s < end && is_wspace_a ( *s ) ) s++;
	// return 0 if all spaces
	if ( s == end ) return 0;
	int32_t i   = 0;
	int64_t val = 0LL;
	bool negative = false;
	if ( s[0] == '-' ) { negative = true; i++; }
	while ( i < len && is_digit(s[i]) ) val = val * 10LL + ( s[i++] - '0');
	if ( negative ) return -val;
	return val;
}

double atof2 ( const char *s, int32_t len ) {
	// skip over spaces
	const char *end = s + len;
	while ( s < end && is_wspace_a ( *s ) ) { s++; len--; }
	// return 0 if all spaces
	if ( s == end ) return 0;
	char tmpBuf[128];
	if ( len >= 128 ) len = 127;
	//strncpy ( dst , s , len );

	const char *p = s;
	const char *srcEnd = s + len;
	char *dst = tmpBuf;
	// remove commas
	for ( ; p < srcEnd ; p++ ) {
		// skip commas
		if ( *p == ',' ) continue;
		// otherwise store it
		*dst++ = *p;
	}
	// null term
	*dst = '\0';
	//buf[len] = '\0';
	return atof ( tmpBuf );
}

// convert hex ascii string into binary at "dst"
void hexToBin ( const char *src , int32_t srcLen , char *dst ) {
	const char *srcEnd = src + srcLen;
	for ( ; src && src < srcEnd ; ) {
		*dst  = htob(*src++);
		*dst <<= 4;
		*dst |= htob(*src++);
		dst++;
	}
	// sanity check
	if ( src != srcEnd ) { gbshutdownAbort(true); }
}

void binToHex ( const unsigned char *src , int32_t srcLen , char *dst ) {
	const unsigned char *srcEnd = src + srcLen;
	for ( ; src && src < srcEnd ; ) {
		*dst++ = btoh(*src>>4);
		*dst++ = btoh(*src&15);
		src++;
	}
	// always null term!
	*dst = '\0';
	// sanity check
	if ( src != srcEnd ) { gbshutdownAbort(true); }
}


// . like strstr but haystack may not be NULL terminated
// . needle, however, IS null terminated
char *strncasestr ( char *haystack , const char *needle , int32_t haystackSize ) {
	int32_t needleSize = strlen(needle);
	int32_t n = haystackSize - needleSize ;
	for ( int32_t i = 0 ; i <= n ; i++ ) {
		// keep looping if first chars do not match
		if ( to_lower_a(haystack[i]) != to_lower_a(needle[0]) ) {
			continue;
		}

		// if needle was only 1 char it's a match
		if ( ! needle[1] ) {
			return &haystack[i];
		}

		// compare the whole strings now
		if ( strncasecmp ( &haystack[i] , needle , needleSize ) == 0 ) {
			return &haystack[i];
		}
	}
	return NULL;
}

// . like strstr but haystack may not be NULL terminated
// . needle, however, IS null terminated
char *strncasestr ( char *haystack , const char *needle , 
		    int32_t haystackSize, int32_t needleSize ) {
	int32_t n = haystackSize - needleSize ;
	for ( int32_t i = 0 ; i <= n ; i++ ) {
		// keep looping if first chars do not match
		if ( to_lower_a(haystack[i]) != to_lower_a(needle[0]) ) 
			continue;
		// if needle was only 1 char it's a match
		if ( ! needle[1] ) return &haystack[i];
		// compare the whole strings now
		if ( strncasecmp ( &haystack[i] , needle , needleSize ) == 0 ) 
			return &haystack[i];			
	}
	return NULL;
}

// independent of case
char *gb_strcasestr ( char *haystack , const char *needle ) {
	int32_t needleSize   = strlen(needle);
	int32_t haystackSize = strlen(haystack);
	int32_t n = haystackSize - needleSize ;
	for ( int32_t i = 0 ; i <= n ; i++ ) {
		// keep looping if first chars do not match
		if ( to_lower_a(haystack[i]) != to_lower_a(needle[0]) ) 
			continue;
		// if needle was only 1 char it's a match
		if ( ! needle[1] ) return &haystack[i];
		// compare the whole strings now
		if ( strncasecmp ( &haystack[i] , needle , needleSize ) == 0 ) 
			return &haystack[i];			
	}
	return NULL;
}


char *gb_strncasestr ( char *haystack , int32_t haystackSize , const char *needle ) {
	// temp term
	char c = haystack[haystackSize];
	haystack[haystackSize] = '\0';
	char *res = gb_strcasestr ( haystack , needle );
	haystack[haystackSize] = c;
	return res;
}

// . if "doSpecial" is true, then we don't touch &lt;, &gt; and &amp;
int32_t htmlDecode( char *dst, const char *src, int32_t srcLen, bool doSpecial ) {
	//special-case optimization
	if ( srcLen == 0 ) {
		return 0;
	}

	char * const start  = dst;
	const char * const srcEnd = src + srcLen;
	for ( ; src < srcEnd ; ) {
		
		if ( *src != '&' ) {
			*dst++ = *src++;
		} else {
			// Ok, we have an ampersand. So decode it into unicode/utf8, do a few special
			// checks, and in general store the resulting string in dst[]
		
			// store decoded entity char into dst[j]
			uint32_t codepoint[2];
			int32_t codepointCount;
			int32_t utf8Len=0;

			// "skip" is how many bytes the entites was in "src"
			int32_t skip = getHtmlEntity(src, srcEnd - src, codepoint, &codepointCount, &utf8Len);

			// If the entity is invalid/unknown then store it as text

			//@todo BR: Temporary fix for named html entities where the utf8 length is 
			// longer than the html entity name. This causes problems for XmlDoc that
			// calls this function with the same buffer as input and output
			if ( skip == 0 || utf8Len > skip) {
				//todo: if doSpecial then make it an &amp;
				// but the decoding is done in-place (bad idea) so we cannot expand the output
				*dst++ = *src++;
				continue;
			}

			// . special mapping
			// . make &lt; and &gt; special so Xml::set() still works
			// . and make &amp; special so we do not screw up summaries
			if ( doSpecial ) {
				if ( codepoint[0] == '<' || codepoint[0] == '>' || codepoint[0] == '&' ) {
					int32_t entityLen = 4;
					const char* entityStr = "";
	
					if (codepoint[0] == '<') {
						entityStr = "&lt;";
					} else if (codepoint[0] == '>') {
						entityStr = "&gt;";
					} else {
						entityStr = "&amp;";
						entityLen = 5;
					}
	
					//fixme: this mangles &nvlt; and &nvgt; because it only takes the < or > character and drops the long-vertical-link-overlay codepoint
					memcpy(dst, entityStr, entityLen);
					src += skip;
					dst += entityLen;
					continue;
				}
	
				/// @todo verify if we need to replace " with '
	
				// some tags have &quot; in their value strings
				// so we have to preserve that!
				// use curling quote:
				//http://www.dwheeler.com/essays/quotes-test-utf-8.html
				// curling double and single quotes resp:
				// &ldquo; &rdquo; &lsquo; &rdquo;
				if ( codepoint[0] == '\"' ) {
					*dst = '\'';
					dst++;
					src += skip;
					continue;
				}
			}

			int32_t totalUtf8Bytes = 0;
			for ( int i=0; i<codepointCount; i++) {
				// . store it into "dst" in utf8 format
				int32_t numBytes = utf8Encode ( codepoint[i], dst );
				totalUtf8Bytes += numBytes;

				// sanity check. do not eat our tail if dst == src
				if ( totalUtf8Bytes > skip ) {
					gbshutdownAbort(true);
				}

				// advance dst ptr
				dst += numBytes;
			}

			// skip over the encoded entity in the source string
			src += skip;
		}
	}

	// NUL term
	*dst = '\0';

	return dst - start;
}

// . entity-ize a string so it's safe for html output
// . store "t" into "s" and return bytes stored
// . does bounds checking
char *htmlEncode ( char *dst, char *dstend, const char *src, const char *srcend ) {
	for ( ; src < srcend ; src++ ) {
		if ( dst + 7 >= dstend ) {
			*dst = '\0';
			return dst;
		}

		if ( *src == '"' ) {
			*dst++ = '&';
			*dst++ = '#';
			*dst++ = '3';
			*dst++ = '4';
			*dst++ = ';';
			continue;
		}
		if ( *src == '<' ) {
			*dst++ = '&';
			*dst++ = 'l';
			*dst++ = 't';
			*dst++ = ';';
			continue;
		}
		if ( *src == '>' ) {
			*dst++ = '&';
			*dst++ = 'g';
			*dst++ = 't';
			*dst++ = ';';
			continue;
		}
		if ( *src == '&' ) {
			*dst++ = '&';
			*dst++ = 'a';
			*dst++ = 'm';
			*dst++ = 'p';
			*dst++ = ';';
			continue;
		}
		if ( *src == '#' ) {
			*dst++ = '&';
			*dst++ = '#';
			*dst++ = '0';
			*dst++ = '3';
			*dst++ = '5';
			*dst++ = ';';
			continue;
		}
		*dst++ = *src;		
	}
	*dst = '\0';
	return dst;
}



//Note: there is a safer version in GbUtil.* that writes to a SafeBuf.
// . convert "-->%22 , &-->%26, +-->%2b, space-->+, ?-->%3f is that it?
// . convert so we can display as a cgi PARAMETER within a url
// . used by HttPage2 (cached web page) to encode the query into a url
// . used by PageRoot to do likewise
// . returns bytes written into "d" not including terminating \0
int32_t urlEncode ( char *d , int32_t dlen , const char *s , int32_t slen ) {
	char *dstart = d;
	// subtract 1 to make room for a terminating \0
	char *dend = d + dlen - 1;
	const char *send = s + slen;
	for ( ; s < send && d < dend ; s++ ) {
		// encode if not fit for display
		if ( ! is_ascii ( *s ) ) goto encode;
		switch ( *s ) {
		case ' ': goto encode;
		case '&': goto encode;
		case '"': goto encode;
		case '+': goto encode;
		case '%': goto encode;
		case '#': goto encode;
		// encoding < and > are more for displaying on an
		// html page than sending to an http server
		case '>': goto encode;
		case '<': goto encode;
		case '?': goto encode;
		}
		// otherwise, no need to encode
		*d++ = *s;
		continue;
	encode:
		// space to +
		if ( *s == ' ' && d + 1 < dend ) { *d++ = '+'; continue; }
		// break out if no room to encode
		if ( d + 2 >= dend ) break;
		*d++ = '%';
		// store first hex digit
		unsigned char v = ((unsigned char)*s)/16 ;
		if ( v < 10 ) v += '0';
		else          v += 'A' - 10;
		*d++ = v;
		// store second hex digit
		v = ((unsigned char)*s) & 0x0f ;
		if ( v < 10 ) v += '0';
		else          v += 'A' - 10;
		*d++ = v;
	}
	// NULL terminate it
	*d = '\0';
	// and return the length
	return d - dstart;
}

// . decodes "s/slen" and stores into "dest"
// . returns the number of bytes stored into "dest"
int32_t urlDecode ( char *dest , const char *s , int32_t slen ) {
	int32_t j = 0;
	for ( int32_t i = 0 ; i < slen ; i++ ) {
		if ( s[i] == '+' ) { dest[j++]=' '; continue; }
		dest[j++] = s[i];
		if ( s[i]  != '%'  ) continue;
		if ( i + 2 >= slen ) continue;
		// if two chars after are not hex chars, it's not an encoding
		if ( ! is_hex ( s[i+1] ) ) continue;
		if ( ! is_hex ( s[i+2] ) ) continue;
		// convert hex chars to values
		unsigned char a = htob ( s[i+1] ) * 16; 
		unsigned char b = htob ( s[i+2] )     ;
		dest[j-1] = (char) (a + b);
		i += 2;
	}
	return j;
}


int32_t urlDecodeNoZeroes ( char *dest , const char *s , int32_t slen ) {
	int32_t j = 0;
	for ( int32_t i = 0 ; i < slen ; i++ ) {
		if ( s[i] == '+' ) { dest[j++]=' '; continue; }
		dest[j++] = s[i];
		if ( s[i]  != '%'  ) continue;
		if ( i + 2 >= slen ) continue;
		// if two chars after are not hex chars, it's not an encoding
		if ( ! is_hex ( s[i+1] ) ) continue;
		if ( ! is_hex ( s[i+2] ) ) continue;
		// convert hex chars to values
		unsigned char a = htob ( s[i+1] ) * 16; 
		unsigned char b = htob ( s[i+2] )     ;
		// NO ZEROES! fixes &content= having decoded \0's in it
		// and setting our parms
		if ( a + b == 0 ) {
			log("fctypes: urlDecodeNoZeros encountered url "
			    "encoded zero. truncating http request.");
			return j; 
		}
		dest[j-1] = (char) (a + b);
		i += 2;
	}
	return j;
}


int64_t gettimeofdayInMilliseconds() {
	struct timeval tv;
	gettimeofday ( &tv , NULL );
	return ((int64_t)(tv.tv_usec/1000)+((int64_t)tv.tv_sec)*1000);
}

time_t getTime () {
	uint32_t now = gettimeofdayInMilliseconds() / 1000;
	return (time_t)now;
}



#include "HttpMime.h" // CT_HTML

// returns length of stripped content, but will set g_errno and return -1
// on error
int32_t stripHtml( char *content, int32_t contentLen, int32_t version ) {
	if ( ! content )
		return 0;
	if ( contentLen == 0 )
		return 0;

	// filter content if we should
	// keep this on the big stack so "content" still references something
	Xml tmpXml;
	// . get the content as xhtml (should be NULL terminated)
	// . parse as utf8 since all we are doing is messing with 
	//   the tags...content manipulation comes later
	if ( !tmpXml.set( content, contentLen, version, CT_HTML ) ) {
		return -1;
	}

	// go tag by tag
	int32_t     n       = tmpXml.getNumNodes();
	XmlNode *nodes   = tmpXml.getNodes();
	// Xml class may have converted to utf16
	content    = const_cast<char*>(tmpXml.getContent()); //we are modifying the buffer in-place, and Xml just poitns into the source buffer so it is safe to cast away const and modify.
	contentLen = tmpXml.getContentLen();
	char    *x       = content;
	char    *xend    = content + contentLen;
	int32_t     stackid = -1;
	int32_t     stackc  =  0;
	// . hack COL tag to NOT require a back tag
	// . do not leave it that way as it could mess up our parsing
	//g_nodes[25].m_hasBackTag = 0;
	for ( int32_t i = 0 ; i < n ; i++ ) {
		// get id of this node
		int32_t id = nodes[i].m_nodeId;
		
		// get it
		int32_t fk = g_nodes[id].m_filterKeep1;
		// if tag is <link ...> only keep it if it has
		// rel="stylesheet" or rel=stylesheet
		// just remove just the tag if this is 2
		if ( fk == 2 ) continue;
		// keep it if not in a stack
		if ( ! stackc && fk ) goto keepit;
		// if no front/back for tag, just skip it
		if ( ! nodes[i].m_hasBackTag ) continue;
		// start stack if none
		if ( stackc == 0 ) {
			// but not if this is a back tag
			if ( nodes[i].m_node[1] == '/' ) continue;
			// now start the stack
			stackid = id;
			stackc  =  1;
			continue;
		}
		// skip if this tag does not match what is on stack
		if ( id != stackid ) continue;
		// if ANOTHER front tag, inc stack
		if ( nodes[i].m_node[1] != '/' ) stackc++;
		// otherwise, dec the stack count
		else                             stackc--;
		// . ensure not negative from excess back tags
		// . reset stackid to -1 to indicate no stack
		if ( stackc <= 0 ) { stackid= -1; stackc = 0; }
		// skip it
		continue;
	keepit:
		// replace images with their alt text
		int32_t vlen;
		char *v;
		if ( id == TAG_IMG ) {
			v = nodes[i].getFieldValue("alt", &vlen );
			// try title if no alt text
			if ( ! v )
				v = nodes[i].getFieldValue("title", &vlen );
			if ( v ) { gbmemcpy ( x, v, vlen ); x += vlen; }
			continue;
		}
		// remove background image from body,table,td tags
		if ( id == TAG_BODY || id == TAG_TABLE || id == TAG_TD ) {
			v = nodes[i].getFieldValue("background", &vlen);
			// remove background, just sabotage it
			if ( v ) v[-4] = 'x';
		}
		// store it
		gbmemcpy ( x , nodes[i].m_node , nodes[i].m_nodeLen );
		x += nodes[i].m_nodeLen;
		// sanity check
		if ( x > xend ) { gbshutdownAbort(true);}
	}
	contentLen = x - content;
	content [ contentLen ] = '\0';
	// unhack COL tag
	//g_nodes[25].m_hasBackTag = 1;
	return contentLen;
}


// don't allow "> in our input boxes
int32_t cleanInput(char *outbuf, int32_t outbufSize, const char *inbuf, int32_t inbufLen){
	char *p = outbuf;
	int32_t numQuotes=0;
	int32_t lastQuote = 0;
	for (int32_t i=0;i<inbufLen;i++){
		if (p-outbuf >= outbufSize-1) break;
			
		if (inbuf[i] == '"'){
			numQuotes++;
			lastQuote = i;
		}
		// if we have an odd number of quotes and a close angle bracket
		// it could be an xss attempt
		if (inbuf[i] == '>' && (numQuotes & 1)) {
			p = outbuf+lastQuote;
			break;
		}
		*p = inbuf[i];
		p++;
	}
	*p = '\0';
	return p-outbuf;
}
