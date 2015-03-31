#include "main.h"
#include <algorithm>
#include <ctype.h>
#include <iostream>
#include <stdexcept>

using std::pair;
using std::string;
using std::transform;
using std::make_pair;

pair64 to_pair64(string input)
{
    ulong64 left { 0ULL };
    ulong64 right { 0ULL };
    int index {0};
    char ch1 { 0 };
    char ch2 { 0 };
    int val1 { 0 };
    int val2 { 0 };

    transform(input.begin(),
              input.end(),
              input.begin(),
              ::tolower);

    for (index = 0 ; index < 16 ; index += 1)
    {
        ch1 = input[index];
        ch2 = input[index + 16];

        val1 = (ch1 >= '0' and ch1 <= '9') ?
               ch1 - '0' :
               ch1 - 'a' + 10;
        val2 = (ch2 >= '0' and ch2 <= '9') ?
               ch2 - '0' :
               ch2 - 'a' + 10;

        left = (left << 4) + val1;
        right = (right << 4) + val2;
    }
    return make_pair(left, right);
}

bool operator<(const pair64& lhs, const pair64& rhs)
{
    return (lhs.first < rhs.first) ?
           true :
           (lhs.first == rhs.first and lhs.second < rhs.second) ?
           true :
           false;
}

bool operator==(const pair64& lhs, const pair64& rhs)
{
    return (lhs.first == rhs.first) and
           (lhs.second == rhs.second);
}

bool operator>(const pair64& lhs, const pair64& rhs)
{
    return ((not (lhs < rhs)) and
            (not (lhs == rhs)));
}



