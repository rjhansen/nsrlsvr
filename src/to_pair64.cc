/*
Copyright (c) 2015-2016, Robert J. Hansen <rjh@sixdemonbag.org>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "main.h"
#include <algorithm>
#include <ctype.h>
#include <iostream>
#include <stdexcept>

using std::pair;
using std::string;
using std::transform;
using std::make_pair;

string
from_pair64(pair64 input)
{
  static string hexadecimal{ "0123456789ABCDEF" };

  string left = "", right = "";
  uint64_t first = input.first;
  uint64_t second = input.second;

  for (int i = 0; i < 16; ++i) {
    left = hexadecimal[first & 0x0F] + left;
    right = hexadecimal[second & 0x0F] + right;
    first >>= 4;
    second >>= 4;
  }

  return left + right;
}

pair64
to_pair64(string input)
{
  ulong64 left{ 0 };
  ulong64 right{ 0 };
  uint8_t val1{ 0 };
  uint8_t val2{ 0 };
  size_t index{ 0 };
  char ch1{ 0 };
  char ch2{ 0 };

  transform(input.begin(), input.end(), input.begin(), ::tolower);

  for (index = 0; index < 16; index += 1) {
    ch1 = input[index];
    ch2 = input[index + 16];

    val1 = (ch1 >= '0' and ch1 <= '9') ? static_cast<uint8_t>(ch1 - '0')
                                       : static_cast<uint8_t>(ch1 - 'a') + 10;
    val2 = (ch2 >= '0' and ch2 <= '9') ? static_cast<uint8_t>(ch2 - '0')
                                       : static_cast<uint8_t>(ch2 - 'a') + 10;

    left = (left << 4) + val1;
    right = (right << 4) + val2;
  }
  return make_pair(left, right);
}

bool
operator<(const pair64& lhs, const pair64& rhs)
{
  return (lhs.first < rhs.first)
           ? true
           : (lhs.first == rhs.first and lhs.second < rhs.second) ? true
                                                                  : false;
}

bool
operator==(const pair64& lhs, const pair64& rhs)
{
  return (lhs.first == rhs.first) and (lhs.second == rhs.second);
}

bool
operator>(const pair64& lhs, const pair64& rhs)
{
  return ((not(lhs < rhs)) and (not(lhs == rhs)));
}
