/*
 * Windows 98 msvcrt.dll does not export _strtoi64 / _strtoui64.
 * Modern MinGW scanf/printf pull those imports in; provide local copies so
 * the PE does not depend on missing CRT exports.
 */
#ifdef METRO_WIN32

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>

static unsigned long long metro_parse_u64(const char *nptr, char **endptr,
                                          int base, int *neg_out,
                                          int *overflow_out) {
  const char *s = nptr;
  unsigned long long acc = 0;
  int neg = 0;
  int overflow = 0;
  int any = 0;

  while (isspace((unsigned char)*s)) {
    s++;
  }

  if (*s == '+') {
    s++;
  } else if (*s == '-') {
    neg = 1;
    s++;
  }

  if (base == 0) {
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
      base = 16;
    } else if (s[0] == '0') {
      base = 8;
    } else {
      base = 10;
    }
  }

  if (base == 16 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
    s += 2;
  }

  while (*s) {
    int digit;
    unsigned char c = (unsigned char)*s;
    if (c >= '0' && c <= '9') {
      digit = c - '0';
    } else if (c >= 'a' && c <= 'z') {
      digit = c - 'a' + 10;
    } else if (c >= 'A' && c <= 'Z') {
      digit = c - 'A' + 10;
    } else {
      break;
    }
    if (digit >= base) {
      break;
    }
    if (acc > (ULLONG_MAX - (unsigned long long)digit) / (unsigned long long)base) {
      overflow = 1;
      acc = ULLONG_MAX;
    } else {
      acc = acc * (unsigned long long)base + (unsigned long long)digit;
    }
    any = 1;
    s++;
  }

  if (endptr != NULL) {
    *endptr = (char *)(any ? s : nptr);
  }
  if (neg_out != NULL) {
    *neg_out = neg;
  }
  if (overflow_out != NULL) {
    *overflow_out = overflow;
  }
  return acc;
}

long long __cdecl _strtoi64(const char *nptr, char **endptr, int base) {
  int neg = 0;
  int overflow = 0;
  unsigned long long acc;

  if (base != 0 && (base < 2 || base > 36)) {
    if (endptr != NULL) {
      *endptr = (char *)nptr;
    }
    errno = EINVAL;
    return 0;
  }

  acc = metro_parse_u64(nptr, endptr, base, &neg, &overflow);
  if (overflow) {
    errno = ERANGE;
    return neg ? LLONG_MIN : LLONG_MAX;
  }
  if (neg) {
    if (acc > (unsigned long long)LLONG_MAX + 1ULL) {
      errno = ERANGE;
      return LLONG_MIN;
    }
    return -(long long)acc;
  }
  if (acc > (unsigned long long)LLONG_MAX) {
    errno = ERANGE;
    return LLONG_MAX;
  }
  return (long long)acc;
}

unsigned long long __cdecl _strtoui64(const char *nptr, char **endptr,
                                      int base) {
  int neg = 0;
  int overflow = 0;
  unsigned long long acc;

  if (base != 0 && (base < 2 || base > 36)) {
    if (endptr != NULL) {
      *endptr = (char *)nptr;
    }
    errno = EINVAL;
    return 0;
  }

  acc = metro_parse_u64(nptr, endptr, base, &neg, &overflow);
  if (overflow) {
    errno = ERANGE;
    return ULLONG_MAX;
  }
  if (neg) {
    /* Match MSVCRT: unary minus on unsigned wrap. */
    return (unsigned long long)(-(long long)acc);
  }
  return acc;
}

/* MinGW scanf may reference these names as well. */
long long __cdecl strtoll(const char *nptr, char **endptr, int base) {
  return _strtoi64(nptr, endptr, base);
}

unsigned long long __cdecl strtoull(const char *nptr, char **endptr, int base) {
  return _strtoui64(nptr, endptr, base);
}

#endif /* METRO_WIN32 */
