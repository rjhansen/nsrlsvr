/* Copyright (c) 2011-2014, Robert J. Hansen <rjh@secret-alchemy.com>
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.*/

#include "handler.hpp"
#include <algorithm>
#include <ctype.h>
#include <iostream>
#include <stdexcept>

using std::pair;
using std::string;
using std::transform;
using std::make_pair;

typedef unsigned long long ULONG64;
typedef pair<ULONG64, ULONG64> pair64;

pair64 to_pair64(string input)
{
    ULONG64 left(0);
    ULONG64 right(0);
    int index(0);
    char ch(0);
    int val(0);

    if (input.size() != 32)
    {
        throw std::invalid_argument("invalid data passed to to_pair64");
    }

    transform(input.begin(), input.end(), input.begin(), ::tolower);

    for (index = 0 ; index < 32 ; index += 1)
    {
        ch = input[index];
        val = (ch >= '0' && ch <= '9') ? ch - '0' : ch - 'a' + 10;
        if (index < 16)
            left = (left << 4) + val;
        else
            right = (right << 4) + val;
    }
    return make_pair(left, right);
}

bool operator<(const pair64& lhs, const pair64& rhs)
{
    return (lhs.first < rhs.first) ? true : (lhs.first == rhs.first && lhs.second < rhs.second) ? true : false;
}

bool operator==(const pair64& lhs, const pair64& rhs)
{
    return (lhs.first == rhs.first) && (lhs.second == rhs.second);
}

bool operator>(const pair64& lhs, const pair64& rhs)
{
    return ((not (lhs < rhs)) && (not (lhs == rhs)));
}

