/*
MIT License
Copyright (c) 2020 James Edward Anhalt III - https://github.com/jeaiii
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

namespace fnv1a
{
    using size_t = decltype(sizeof(0));
    using u32 = decltype(0xffffffffu);

    template<class T, T N> struct constant { enum : T { value = N }; };

    // log based recursion for constexpr strings > 510 chars
    constexpr u32 hash32_c(const char* s, size_t n, u32 h = 2166136261u)
    {
        return n < 1 ? h : hash32_c(s + n / 2 + 1, n - n / 2 - 1, hash32_c(s + 1, n / 2, (s[0] & 0xffu ^ h) * 16777619u & 0xffffffffu));
    }

    // tail recursion that optimizaes well for use at runtime
    constexpr u32 hash32_n(const char* s, size_t n, u32 h = 2166136261u)
    {
        return n < 1 ? h : hash32_n(s + 1, n - 1, (s[0] & 0xffu ^ h) * 16777619u & 0xffffffffu);
    }

    // null terminated strings
    constexpr u32 hash32_s(const char* s, u32 h = 2166136261u)
    {
        return s[0] == '\0' ? h : hash32_s(s + 1, (s[0] & 0xffu ^ h) * 16777619u & 0xffffffffu);
    }

}

#define FNV1A_HASH32(STR) fnv1a::constant<fnv1a::u32, fnv1a::hash32_c("" STR, sizeof(STR) - 1)>::value

static_assert(FNV1A_HASH32("") == 0x811c9dc5u, "");
static_assert(FNV1A_HASH32("a") == 0xe40c292cu, "");
static_assert(FNV1A_HASH32("foobar") == 0xbf9cf968u, "");
static_assert(FNV1A_HASH32("HVGZq9") == 0u, "");