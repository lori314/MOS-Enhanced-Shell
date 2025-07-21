#include <types.h>

void *memcpy(void *dst, const void *src, size_t n) {
	void *dstaddr = dst;
	void *max = dst + n;

	if (((u_long)src & 3) != ((u_long)dst & 3)) {
		while (dst < max) {
			*(char *)dst++ = *(char *)src++;
		}
		return dstaddr;
	}

	while (((u_long)dst & 3) && dst < max) {
		*(char *)dst++ = *(char *)src++;
	}

	// copy machine words while possible
	while (dst + 4 <= max) {
		*(uint32_t *)dst = *(uint32_t *)src;
		dst += 4;
		src += 4;
	}

	// finish the remaining 0-3 bytes
	while (dst < max) {
		*(char *)dst++ = *(char *)src++;
	}
	return dstaddr;
}

void *memset(void *dst, int c, size_t n) {
	void *dstaddr = dst;
	void *max = dst + n;
	u_char byte = c & 0xff;
	uint32_t word = byte | byte << 8 | byte << 16 | byte << 24;

	while (((u_long)dst & 3) && dst < max) {
		*(u_char *)dst++ = byte;
	}

	// fill machine words while possible
	while (dst + 4 <= max) {
		*(uint32_t *)dst = word;
		dst += 4;
	}

	// finish the remaining 0-3 bytes
	while (dst < max) {
		*(u_char *)dst++ = byte;
	}
	return dstaddr;
}

size_t strlen(const char *s) {
	int n;

	for (n = 0; *s; s++) {
		n++;
	}

	return n;
}

char *strcpy(char *dst, const char *src) {
	char *ret = dst;

	while ((*dst++ = *src++) != 0) {
	}

	return ret;
}

const char *strchr(const char *s, int c) {
	for (; *s; s++) {
		if (*s == c) {
			return s;
		}
	}
	return 0;
}

int strcmp(const char *p, const char *q) {
	while (*p && *p == *q) {
		p++, q++;
	}

	if ((u_int)*p < (u_int)*q) {
		return -1;
	}

	if ((u_int)*p > (u_int)*q) {
		return 1;
	}

	return 0;
}

// Add implementations for the missing functions

void *memmove(void *dst, const void *src, size_t n) {
	const char *s = src;
	char *d = dst;

	// Check for overlap where copying forwards would corrupt the source.
	if (s < d && s + n > d) {
		// Destination is ahead of the source in memory, so copy backwards.
		s += n;
		d += n;
		while (n-- > 0) {
			*--d = *--s;
		}
	} else {
		// No overlap, or destination is behind the source, so copy forwards.
		while (n-- > 0) {
			*d++ = *s++;
		}
	}

	return dst;
}

char *strncpy(char *dst, const char *src, size_t n) {
	char *ret = dst;
	size_t i;

	// Copy at most n characters from src to dst.
	for (i = 0; i < n && src[i] != '\0'; i++) {
		dst[i] = src[i];
	}

	// If src was shorter than n, pad the rest of dst with null bytes.
	for (; i < n; i++) {
		dst[i] = '\0';
	}

	return ret;
}