/* $OpenBSD: a_int.c,v 1.39 2022/04/27 17:42:08 jsing Exp $ */
/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 *
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 *
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 *
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <openssl/asn1.h>
#include <openssl/asn1t.h>
#include <openssl/bn.h>
#include <openssl/buffer.h>
#include <openssl/err.h>

#include "bytestring.h"

const ASN1_ITEM ASN1_INTEGER_it = {
	.itype = ASN1_ITYPE_PRIMITIVE,
	.utype = V_ASN1_INTEGER,
	.sname = "ASN1_INTEGER",
};

ASN1_INTEGER *
ASN1_INTEGER_new(void)
{
	return (ASN1_INTEGER *)ASN1_item_new(&ASN1_INTEGER_it);
}

void
ASN1_INTEGER_free(ASN1_INTEGER *a)
{
	ASN1_item_free((ASN1_VALUE *)a, &ASN1_INTEGER_it);
}

static int
ASN1_INTEGER_valid(const ASN1_INTEGER *a)
{
	return (a != NULL && a->length >= 0);
}

ASN1_INTEGER *
ASN1_INTEGER_dup(const ASN1_INTEGER *x)
{
	if (!ASN1_INTEGER_valid(x))
		return NULL;

	return ASN1_STRING_dup(x);
}

int
ASN1_INTEGER_cmp(const ASN1_INTEGER *x, const ASN1_INTEGER *y)
{
	int neg, ret;

	/* Compare signs */
	neg = x->type & V_ASN1_NEG;
	if (neg != (y->type & V_ASN1_NEG)) {
		if (neg)
			return -1;
		else
			return 1;
	}

	ret = ASN1_STRING_cmp(x, y);

	if (neg)
		return -ret;
	else
		return ret;
}

int
ASN1_INTEGER_set(ASN1_INTEGER *a, long v)
{
	int j, k;
	unsigned int i;
	unsigned char buf[sizeof(long) + 1];
	long d;

	a->type = V_ASN1_INTEGER;
	/* XXX ssl/ssl_asn1.c:i2d_SSL_SESSION() depends upon this bound vae */
	if (a->length < (int)(sizeof(long) + 1)) {
		free(a->data);
		a->data = calloc(1, sizeof(long) + 1);
	}
	if (a->data == NULL) {
		ASN1error(ERR_R_MALLOC_FAILURE);
		return (0);
	}
	d = v;
	if (d < 0) {
		d = -d;
		a->type = V_ASN1_NEG_INTEGER;
	}

	for (i = 0; i < sizeof(long); i++) {
		if (d == 0)
			break;
		buf[i] = (int)d & 0xff;
		d >>= 8;
	}
	j = 0;
	for (k = i - 1; k >= 0; k--)
		a->data[j++] = buf[k];
	a->length = j;
	return (1);
}

/*
 * XXX this particular API is a gibbering eidrich horror that makes it
 * impossible to determine valid return cases from errors.. "a bit
 * ugly" is preserved for posterity, unfortunately this is probably
 * unfixable without changing public API
 */
long
ASN1_INTEGER_get(const ASN1_INTEGER *a)
{
	int neg = 0, i;
	unsigned long r = 0;

	if (a == NULL)
		return (0L);
	i = a->type;
	if (i == V_ASN1_NEG_INTEGER)
		neg = 1;
	else if (i != V_ASN1_INTEGER)
		return -1;

	if (!ASN1_INTEGER_valid(a))
		return -1; /* XXX best effort */

	if (a->length > (int)sizeof(long)) {
		/* hmm... a bit ugly, return all ones */
		return -1;
	}
	if (a->data == NULL)
		return 0;

	for (i = 0; i < a->length; i++) {
		r <<= 8;
		r |= (unsigned char)a->data[i];
	}

	if (r > LONG_MAX)
		return -1;

	if (neg)
		return -(long)r;
	return (long)r;
}

ASN1_INTEGER *
BN_to_ASN1_INTEGER(const BIGNUM *bn, ASN1_INTEGER *ai)
{
	ASN1_INTEGER *ret;
	int len, j;

	if (ai == NULL)
		ret = ASN1_INTEGER_new();
	else
		ret = ai;
	if (ret == NULL) {
		ASN1error(ERR_R_NESTED_ASN1_ERROR);
		goto err;
	}

	if (!ASN1_INTEGER_valid(ret))
		goto err;

	if (BN_is_negative(bn))
		ret->type = V_ASN1_NEG_INTEGER;
	else
		ret->type = V_ASN1_INTEGER;
	j = BN_num_bits(bn);
	len = ((j == 0) ? 0 : ((j / 8) + 1));
	if (ret->length < len + 4) {
		unsigned char *new_data = realloc(ret->data, len + 4);
		if (!new_data) {
			ASN1error(ERR_R_MALLOC_FAILURE);
			goto err;
		}
		ret->data = new_data;
	}
	ret->length = BN_bn2bin(bn, ret->data);

	/* Correct zero case */
	if (!ret->length) {
		ret->data[0] = 0;
		ret->length = 1;
	}
	return (ret);

 err:
	if (ret != ai)
		ASN1_INTEGER_free(ret);
	return (NULL);
}

BIGNUM *
ASN1_INTEGER_to_BN(const ASN1_INTEGER *ai, BIGNUM *bn)
{
	BIGNUM *ret;

	if (!ASN1_INTEGER_valid(ai))
		return (NULL);

	if ((ret = BN_bin2bn(ai->data, ai->length, bn)) == NULL)
		ASN1error(ASN1_R_BN_LIB);
	else if (ai->type == V_ASN1_NEG_INTEGER)
		BN_set_negative(ret, 1);
	return (ret);
}

int
i2a_ASN1_INTEGER(BIO *bp, const ASN1_INTEGER *a)
{
	int i, n = 0;
	static const char h[] = "0123456789ABCDEF";
	char buf[2];

	if (a == NULL)
		return (0);

	if (a->type & V_ASN1_NEG) {
		if (BIO_write(bp, "-", 1) != 1)
			goto err;
		n = 1;
	}

	if (a->length == 0) {
		if (BIO_write(bp, "00", 2) != 2)
			goto err;
		n += 2;
	} else {
		for (i = 0; i < a->length; i++) {
			if ((i != 0) && (i % 35 == 0)) {
				if (BIO_write(bp, "\\\n", 2) != 2)
					goto err;
				n += 2;
			}
			buf[0] = h[((unsigned char)a->data[i] >> 4) & 0x0f];
			buf[1] = h[((unsigned char)a->data[i]) & 0x0f];
			if (BIO_write(bp, buf, 2) != 2)
				goto err;
			n += 2;
		}
	}
	return (n);

 err:
	return (-1);
}

int
a2i_ASN1_INTEGER(BIO *bp, ASN1_INTEGER *bs, char *buf, int size)
{
	int ret = 0;
	int i, j,k, m,n, again, bufsize;
	unsigned char *s = NULL, *sp;
	unsigned char *bufp;
	int num = 0, slen = 0, first = 1;

	bs->type = V_ASN1_INTEGER;

	bufsize = BIO_gets(bp, buf, size);
	for (;;) {
		if (bufsize < 1)
			goto err_sl;
		i = bufsize;
		if (buf[i - 1] == '\n')
			buf[--i] = '\0';
		if (i == 0)
			goto err_sl;
		if (buf[i - 1] == '\r')
			buf[--i] = '\0';
		if (i == 0)
			goto err_sl;
		if (buf[i - 1] == '\\') {
			i--;
			again = 1;
		} else
			again = 0;
		buf[i] = '\0';
		if (i < 2)
			goto err_sl;

		bufp = (unsigned char *)buf;
		if (first) {
			first = 0;
			if ((bufp[0] == '0') && (buf[1] == '0')) {
				bufp += 2;
				i -= 2;
			}
		}
		k = 0;
		if (i % 2 != 0) {
			ASN1error(ASN1_R_ODD_NUMBER_OF_CHARS);
			goto err;
		}
		i /= 2;
		if (num + i > slen) {
			if ((sp = recallocarray(s, slen, num + i, 1)) == NULL) {
				ASN1error(ERR_R_MALLOC_FAILURE);
				goto err;
			}
			s = sp;
			slen = num + i;
		}
		for (j = 0; j < i; j++, k += 2) {
			for (n = 0; n < 2; n++) {
				m = bufp[k + n];
				if ((m >= '0') && (m <= '9'))
					m -= '0';
				else if ((m >= 'a') && (m <= 'f'))
					m = m - 'a' + 10;
				else if ((m >= 'A') && (m <= 'F'))
					m = m - 'A' + 10;
				else {
					ASN1error(ASN1_R_NON_HEX_CHARACTERS);
					goto err;
				}
				s[num + j] <<= 4;
				s[num + j] |= m;
			}
		}
		num += i;
		if (again)
			bufsize = BIO_gets(bp, buf, size);
		else
			break;
	}
	bs->length = num;
	bs->data = s;
	return (1);

 err_sl:
	ASN1error(ASN1_R_SHORT_LINE);
 err:
	free(s);
	return (ret);
}

/*
 * This converts an ASN1 INTEGER into its content encoding.
 * The internal representation is an ASN1_STRING whose data is a big endian
 * representation of the value, ignoring the sign. The sign is determined by
 * the type: V_ASN1_INTEGER for positive and V_ASN1_NEG_INTEGER for negative.
 *
 * Positive integers are no problem: they are almost the same as the DER
 * encoding, except if the first byte is >= 0x80 we need to add a zero pad.
 *
 * Negative integers are a bit trickier...
 * The DER representation of negative integers is in 2s complement form.
 * The internal form is converted by complementing each octet and finally
 * adding one to the result. This can be done less messily with a little trick.
 * If the internal form has trailing zeroes then they will become FF by the
 * complement and 0 by the add one (due to carry) so just copy as many trailing
 * zeros to the destination as there are in the source. The carry will add one
 * to the last none zero octet: so complement this octet and add one and finally
 * complement any left over until you get to the start of the string.
 *
 * Padding is a little trickier too. If the first bytes is > 0x80 then we pad
 * with 0xff. However if the first byte is 0x80 and one of the following bytes
 * is non-zero we pad with 0xff. The reason for this distinction is that 0x80
 * followed by optional zeros isn't padded.
 */

int
i2c_ASN1_INTEGER(ASN1_INTEGER *a, unsigned char **pp)
{
	int pad = 0, ret, i, neg;
	unsigned char *p, *n, pb = 0;

	if (!ASN1_INTEGER_valid(a))
		return 0;

	neg = a->type & V_ASN1_NEG;
	if (a->length == 0)
		ret = 1;
	else {
		ret = a->length;
		i = a->data[0];
		if (!neg && (i > 127)) {
			pad = 1;
			pb = 0;
		} else if (neg) {
			if (i > 128) {
				pad = 1;
				pb = 0xFF;
			} else if (i == 128) {
				/*
				 * Special case: if any other bytes non zero we pad:
				 * otherwise we don't.
				 */
				for (i = 1; i < a->length; i++) if (a->data[i]) {
					pad = 1;
					pb = 0xFF;
					break;
				}
			}
		}
		ret += pad;
	}
	if (pp == NULL)
		return (ret);
	p= *pp;

	if (pad)
		*(p++) = pb;
	if (a->length == 0)
		*(p++) = 0;
	else if (!neg)
		memcpy(p, a->data, a->length);
	else {
		/* Begin at the end of the encoding */
		n = a->data + a->length - 1;
		p += a->length - 1;
		i = a->length;
		/* Copy zeros to destination as long as source is zero */
		while (!*n) {
			*(p--) = 0;
			n--;
			i--;
		}
		/* Complement and increment next octet */
		*(p--) = ((*(n--)) ^ 0xff) + 1;
		i--;
		/* Complement any octets left */
		for (; i > 0; i--)
			*(p--) = *(n--) ^ 0xff;
	}

	*pp += ret;
	return (ret);
}

static void
asn1_aint_twos_complement(uint8_t *data, size_t data_len)
{
	uint8_t carry = 1;
	ssize_t i;

	for (i = data_len - 1; i >= 0; i--) {
		data[i] = (data[i] ^ 0xff) + carry;
		if (data[i] != 0)
			carry = 0;
	}
}

static int
asn1_aint_keep_twos_padding(const uint8_t *data, size_t data_len)
{
	size_t i;

	/*
	 * If a two's complement value has a padding byte (0xff) and the rest
	 * of the value is all zeros, the padding byte cannot be removed as when
	 * converted from two's complement this becomes 0x01 (in the place of
	 * the padding byte) followed by the same number of zero bytes.
	 */
	if (data_len <= 1 || data[0] != 0xff)
		return 0;
	for (i = 1; i < data_len; i++) {
		if (data[i] != 0)
			return 0;
	}
	return 1;
}

int
c2i_ASN1_INTEGER_cbs(ASN1_INTEGER **out_aint, CBS *cbs)
{
	ASN1_INTEGER *aint = NULL;
	uint8_t *data = NULL;
	size_t data_len = 0;
	uint8_t padding, val;
	uint8_t negative = 0;
	int ret = 0;

	if (out_aint == NULL)
		goto err;

	if (*out_aint != NULL) {
		ASN1_INTEGER_free(*out_aint);
		*out_aint = NULL;
	}

	if (CBS_len(cbs) == 0) {
		/* XXX INVALID ENCODING? */
		ASN1error(ERR_R_ASN1_LENGTH_MISMATCH);
		goto err;
	}
	if (!CBS_peek_u8(cbs, &val))
		goto err;

	/* Top most bit indicates sign, padding is all zeros or all ones. */
	negative = (val >> 7);
	padding = ~(negative - 1) & 0xff;

	/*
	 * Ensure that the first 9 bits are not all zero or all one, as per
	 * X.690 section 8.3.2. Remove the padding octet if possible.
	 */
	if (CBS_len(cbs) > 1 && val == padding) {
		if (!asn1_aint_keep_twos_padding(CBS_data(cbs), CBS_len(cbs))) {
			if (!CBS_get_u8(cbs, &padding))
				goto err;
			if (!CBS_peek_u8(cbs, &val))
				goto err;
			if ((val >> 7) == (padding >> 7)) {
				/* XXX INVALID ENCODING? */
				ASN1error(ERR_R_ASN1_LENGTH_MISMATCH);
				goto err;
			}
		}
	}

	if (!CBS_stow(cbs, &data, &data_len))
		goto err;
	if (data_len > INT_MAX)
		goto err;

	if ((aint = ASN1_INTEGER_new()) == NULL)
		goto err;

	/*
	 * Negative integers are handled as a separate type - convert from
	 * two's complement for internal representation.
	 */
	if (negative) {
		aint->type = V_ASN1_NEG_INTEGER;
		asn1_aint_twos_complement(data, data_len);
	}

	aint->data = data;
	aint->length = (int)data_len;
	data = NULL;

	*out_aint = aint;
	aint = NULL;

	ret = 1;

 err:
	ASN1_INTEGER_free(aint);
	freezero(data, data_len);

	return ret;
}

ASN1_INTEGER *
c2i_ASN1_INTEGER(ASN1_INTEGER **out_aint, const unsigned char **pp, long len)
{
	ASN1_INTEGER *aint = NULL;
	CBS content;

	if (out_aint != NULL) {
		ASN1_INTEGER_free(*out_aint);
		*out_aint = NULL;
	}

	if (len < 0) {
		ASN1error(ASN1_R_LENGTH_ERROR);
		return NULL;
	}

	CBS_init(&content, *pp, len);

	if (!c2i_ASN1_INTEGER_cbs(&aint, &content))
		return NULL;

	*pp = CBS_data(&content);

	if (out_aint != NULL)
		*out_aint = aint;

	return aint;
}

int
i2d_ASN1_INTEGER(ASN1_INTEGER *a, unsigned char **out)
{
	return ASN1_item_i2d((ASN1_VALUE *)a, out, &ASN1_INTEGER_it);
}

ASN1_INTEGER *
d2i_ASN1_INTEGER(ASN1_INTEGER **a, const unsigned char **in, long len)
{
	return (ASN1_INTEGER *)ASN1_item_d2i((ASN1_VALUE **)a, in, len,
	    &ASN1_INTEGER_it);
}

/* This is a version of d2i_ASN1_INTEGER that ignores the sign bit of
 * ASN1 integers: some broken software can encode a positive INTEGER
 * with its MSB set as negative (it doesn't add a padding zero).
 */

ASN1_INTEGER *
d2i_ASN1_UINTEGER(ASN1_INTEGER **a, const unsigned char **pp, long length)
{
	ASN1_INTEGER *ret = NULL;
	const unsigned char *p;
	unsigned char *s;
	long len;
	int inf, tag, xclass;
	int i;

	if ((a == NULL) || ((*a) == NULL)) {
		if ((ret = ASN1_INTEGER_new()) == NULL)
			return (NULL);
	} else
		ret = (*a);

	if (!ASN1_INTEGER_valid(ret)) {
		i = ERR_R_ASN1_LENGTH_MISMATCH;
		goto err;
	}

	p = *pp;
	inf = ASN1_get_object(&p, &len, &tag, &xclass, length);
	if (inf & 0x80) {
		i = ASN1_R_BAD_OBJECT_HEADER;
		goto err;
	}

	if (tag != V_ASN1_INTEGER) {
		i = ASN1_R_EXPECTING_AN_INTEGER;
		goto err;
	}

	/* We must malloc stuff, even for 0 bytes otherwise it
	 * signifies a missing NULL parameter. */
	if (len < 0 || len > INT_MAX) {
		i = ERR_R_ASN1_LENGTH_MISMATCH;
		goto err;
	}
	s = malloc(len + 1);
	if (s == NULL) {
		i = ERR_R_MALLOC_FAILURE;
		goto err;
	}
	ret->type = V_ASN1_INTEGER;
	if (len) {
		if ((*p == 0) && (len != 1)) {
			p++;
			len--;
		}
		memcpy(s, p, len);
		p += len;
	}

	free(ret->data);
	ret->data = s;
	ret->length = (int)len;
	if (a != NULL)
		(*a) = ret;
	*pp = p;
	return (ret);

 err:
	ASN1error(i);
	if (a == NULL || *a != ret)
		ASN1_INTEGER_free(ret);
	return (NULL);
}
