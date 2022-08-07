#include <bit>
#include <ctime>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdio>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <functional>
#include <algorithm>
#include <array>
#include <chrono>
#include <intrin.h>
#include <emmintrin.h>

// uncomment this line to write info to stdout, which takes away precious CPU time
#define NO_OUTPUT


#ifdef NO_OUTPUT
#define OUTPUT(x) do;while(false)
#else
#define OUTPUT(x) do{x;}while(false)
#endif


using uint = unsigned int;

std::vector<uint> wordbits;
std::vector<std::string> words;
std::unordered_map<uint, size_t> bitstoindex;
std::vector<uint> letterindex[26];
uint letterorder[26];

std::string_view getword(const char*& _str, const char* end)
{
    const char* str = _str;
    while(*str == '\n' || *str == '\r')
	{
		if (++str == end)
            return (_str = str), std::string_view{};
	}

    const char* start = str;
    while(str != end && *str != '\n' && *str != '\r')
        ++str;

    _str = str;
    return std::string_view{ start, str };
}

uint getbits(std::string_view word)
{
    uint r = 0;
    for (char c : word)
        r |= 1 << (c - 'a');
    return r;
}

void readwords(const char* file)
{
	struct { int f, l; } freq[26] = { };
	for (int i = 0; i < 26; i++)
		freq[i].l = i;

    // open file
    std::vector<char> buf;
    std::ifstream in(file);
    in.seekg(0, std::ios::end);
    buf.resize(in.tellg());
    in.seekg(0, std::ios::beg);
    in.read(&buf[0], buf.size());

    const char* str = &buf[0];
	const char* strEnd = str + buf.size();

    // read words
    std::string_view word;
    while(!(word = getword(str, strEnd)).empty())
    {
        if (word.size() != 5)
            continue;
        uint bits = getbits(word);
        if (std::popcount(bits) != 5)
            continue;

        if (!bitstoindex.contains(bits))
        {
            bitstoindex[bits] = wordbits.size();
            wordbits.push_back(bits);
            words.emplace_back(word);

            // count letter frequency
            for(char c: word)
                freq[c - 'a'].f++;
        }
    }

    // rearrange letter order based on lettter frequency (least used letter gets lowest index)
    std::sort(std::begin(freq), std::end(freq), [](auto a, auto b) { return a.f < b.f; });
	uint reverseletterorder[26];
    for (int i = 0; i < 26; i++)
	{
		letterorder[i] = freq[i].l;
        reverseletterorder[freq[i].l] = i;
    }

    // build index based on least used letter
    for (uint w : wordbits)
    {
        uint m = w;
		uint letter = std::countr_zero(m);
        uint min = reverseletterorder[letter];
		m &= m - 1; // drop lowest set bit
        while(m)
        {
            letter = std::countr_zero(m);
            min = std::min(min, reverseletterorder[letter]);
			m &= m - 1;
		}

        letterindex[min].push_back(w);
    }

	for (int i = 0; i < 26; i++)
	{
		letterindex[i].push_back(~0);
		letterindex[i].push_back(~0);
		letterindex[i].push_back(~0);
	}
}

using WordArray = std::array<uint, 5>;
using OutputFn = std::function<void(const WordArray&)>;

long long start;
long long timeUS() { return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count(); }

int findwords(std::vector<WordArray>& solutions, uint totalbits, int numwords, WordArray& words, uint maxLetter, bool skipped)
{
	if (numwords == 5)
	{
		solutions.push_back(words);
		return 1;
	}

	int numsolutions = 0;
	size_t max = wordbits.size();
    WordArray newwords = words;

    // walk over all letters in a certain order until we find an unused one
	for (uint i = maxLetter; i < 26; i++)
	{
        uint letter = letterorder[i];
        uint m = 1 << letter;
        if (totalbits & m)
            continue;

        // take all words from the index of this letter and add each word to the solution if all letters of the word aren't used before.
        auto& index = letterindex[i];
        auto pWords = &index[0];
        auto pEnd = pWords + index.size();
        __m128i current = _mm_set1_epi32(totalbits);
        for (; pWords < pEnd; pWords += 4)
		{
            __m128i wordsbits = _mm_loadu_epi32(pWords);
            __m128i mask = _mm_cmpeq_epi32(_mm_and_si128(wordsbits, current), _mm_setzero_si128());
            int mvmask = _mm_movemask_epi8(mask);
            if (!mvmask)
                continue;

            for (int m = 1, j = 0; m < 0x10000; m <<= 4, j++)
			{
				if (!(mvmask & m))
					continue;

                uint w = pWords[j];
                if (w & 0x8000'000)
                    continue;

                newwords[numwords] = w;
                if (!bitstoindex.contains(w))
                    __debugbreak();
				numsolutions += findwords(solutions, totalbits | w, numwords + 1, newwords, i + 1, skipped);
			}

			OUTPUT(if (numwords == 0) std::cout << "\33[2K\rFound " << numsolutions << " solutions. Running time: " << (timeUS() - start) << "us");
		}

        if (skipped)
            break;
        skipped = true;
	}

	return numsolutions;
}

int findwords(std::vector<WordArray>& solutions)
{
    WordArray words = { };
    return findwords(solutions, 0, 0, words, 0, false);
}

int main()
{
    start = timeUS();
    readwords("words_alpha.txt");
    std::vector<WordArray> solutions;
    solutions.reserve(10000);

    OUTPUT(
        std::cout << wordbits.size() << " unique words\n";
	    std::cout << "letter order: ";
	    for (int i = 0; i < 26; i++)
		    std::cout << char('a' + letterorder[i]);
	    std::cout << "\n";
    );

    auto startAlgo = timeUS();
    int num = findwords(solutions);

    auto startOutput = timeUS();
	std::ofstream out("solutions.txt");
    for (auto& words : solutions)
    {
        for (auto w : words)
            out << words[bitstoindex[w]] << "\t";
        out << "\n";
    };

	OUTPUT(std::cout << "\n");

	long long end = timeUS();
	std::cout << num << " solutions written to solutions.txt.\n";
    std::cout << "Total time: " << end - start << "us (" << (end - start) / 1.e6f << "s)\n";
    std::cout << "Read:    " << std::setw(8) << startAlgo - start << "us\n";
	std::cout << "Process: " << std::setw(8) << startOutput - startAlgo << "us\n";
	std::cout << "Write:   " << std::setw(8) << end - startOutput << "us\n";
}
