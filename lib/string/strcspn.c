#include <string.h>

/**
 * @brief Get the length of a complementary substring.
 * 
 * Computes the length of the maximum initial segment of the C string s1 which
 * consists entirely of characters that are not part of the C string s2.
 * 
 * @param s1 The C string to be scanned.
 * @param s2 The C string containing the characters to match.
 * 
 * @return The length of the computed segment of the C string s1.
 * 
 * @sa memchr, strchr, strpbrk, strrchr, strspn, strstr, strtok
 */
size_t
strcspn(const char *s1, const char *s2)
{
  const char *p;

  for (p = s1; *p != '\0'; p++)
    // Check whether the current character is part of s2.
    if (strchr(s2, *p) != NULL)
      break;

  return (size_t) (p - s1);
}
