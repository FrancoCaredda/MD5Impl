#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <string>
#include <vector>
#include <array>
#include <sstream>
#include <iomanip>
#include <codecvt>
#include <locale>
#include <fstream>
#include <io.h>
#include <fcntl.h>
#include <unordered_map>

// 448 bits
#define PADDING_SIZE 56 
// 512 bits
#define CHUNK_SIZE 64
// 8 bytes for the length
#define LENGTH_SIZE 8

#define OPERATIONS 64

#define DEFAULT_MESSAGE_SIZE 255

// Round functions
#define F(B, C, D) (( (B) & (C) ) | ( (~(B)) & (D) ))
#define H(B, C, D) (( (D) & (B) ) | ( (~(D)) & (C) ))
#define J(B, C, D) ((B) ^ (C) ^ (D))
#define I(B, C, D) ((C) ^ ( (B) | (~(D)) ))

enum UserInput
{
	EXIT = 0,
	ENTER_STRING = 1,
	ENTER_FILE = 2,
	CHECK_VALIDITY = 3
};

uint32_t LeftRotate(uint32_t x, uint32_t shift)
{
	return (x << shift) | (x >> (32 - shift));
}

// Round shift values
static int s[CHUNK_SIZE] = {
	7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
	5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
	4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
	6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

// Constant K Values
static uint32_t K[CHUNK_SIZE] = {
	0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
	0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
	0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
	0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
	0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
	0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
	0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
	0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
	0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
	0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
	0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
	0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
	0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
	0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
	0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
	0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

std::vector<uint8_t> ConvertWideStringToBytes(const std::wstring& wideStr) 
{
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
	std::string utf8Str = converter.to_bytes(wideStr);

	std::vector<uint8_t> bytes(utf8Str.begin(), utf8Str.end());
	return bytes;
}

std::string BytesToHexString(const std::vector<uint8_t>& bytes) 
{
	std::stringstream stream;
	for (uint8_t byte : bytes) 
		stream << std::setfill('0') << std::setw(2) << std::hex << static_cast<int>(byte);
	
	return stream.str();
}

bool ReadFileUTF8(const std::wstring& filename, std::wstring& outMessage, size_t& outSize)
{
	FILE* file = _wfopen(filename.c_str(), L"r");

	if (!file)
		return false;

	_setmode(_fileno(file), _O_U8TEXT);

	fseek(file, 0, SEEK_END);
	outSize = ftell(file);

	fseek(file, 0, SEEK_SET);

	wchar_t line[255] = { 0 };
	while (fgetws(line, 255, file) != nullptr)
	{
		outMessage.append(line);
		memset(line, 0, 255 * sizeof(wchar_t));

		if (feof(file))
			break;
	}

	fclose(file);

	return true;
}

std::wstring RemoveInvisibleCharachter(const std::wstring& str)
{
	std::wstring result;
	result.resize(str.size() - 1);

	memcpy(const_cast<wchar_t*>(result.data()), str.data() + 1, (str.size() - 1) * sizeof(wchar_t));

	return result;
}
bool ReadCache(const std::wstring& filename, std::unordered_map<std::wstring, std::wstring>& outCache)
{
	std::wstring data;
	size_t size;
	if (ReadFileUTF8(filename, data, size))
	{
		std::wstringstream stream(data);
		std::wstring line;

		while (stream.good())
		{
			stream >> line;
			int delimIndex = line.find_first_of(':', 0);

			if (delimIndex == std::string::npos)
				break;

			outCache[line.substr(0, delimIndex)] = line.substr(delimIndex + 1, line.size() - delimIndex);
		}
	}
	else
	{
		return false;
	}

	return true;
}

std::string HashMD5(const std::vector<uint8_t>& bytes, size_t size)
{
	// Adding the padding
	// 1. Add additional byte to save 0x80 (10000000)
	int paddingLength = (PADDING_SIZE - ((bytes.size() + 1) % CHUNK_SIZE)) % CHUNK_SIZE;
	// 2. Allocate the padding array
	std::vector<uint8_t> paddedBytes(bytes.size() + 1 + paddingLength + LENGTH_SIZE);
	// 3. Copy the original data into paddedBytes array
	memcpy(paddedBytes.data(), bytes.data(), bytes.size());
	// 4. Insert 0x80
	paddedBytes[bytes.size()] = 0x80;
	// 5. Insert length in bits
	uint64_t length = size * 8;
	// Using pointer arethmerics to copy the length bytes into array fastly
	memcpy(paddedBytes.data() + paddedBytes.size() - LENGTH_SIZE, &length, sizeof(uint64_t));

	// MD5 Process
	// 1. Initialize buffer
	uint32_t A = 0x67452301;
	uint32_t B = 0xefcdab89;
	uint32_t C = 0x98badcfe;
	uint32_t D = 0x10325476;

	// 2. Start processing every 512 bit chunk
	for (int i = 0; i < paddedBytes.size() / CHUNK_SIZE; i++)
	{
		// Copy the chunk
		std::array<uint32_t, 16> M;

		memcpy(M.data(), paddedBytes.data() + (i * M.size() * sizeof(uint32_t)), (M.size() * sizeof(uint32_t)));

		// Initialize the chunk hash
		uint32_t A0 = A, B0 = B, C0 = C, D0 = D;

		// Executing 64 operations on chunks
		for (int j = 0; j < OPERATIONS; j++)
		{
			uint32_t function = 0, g = 0;
			if (j <= 15)
			{
				function = F(B0, C0, D0);
				g = j;
			}
			else if (j <= 31)
			{
				function = H(B0, C0, D0);
				g = (5 * j + 1) % 16;
			}
			else if (j <= 47)
			{
				function = J(B0, C0, D0);
				g = (3 * j + 5) % 16;
			}
			else if (j <= 63)
			{
				function = I(B0, C0, D0);
				g = (7 * j) % 16;
			}

			function += A0 + K[j] + M[g];
			A0 = D0;
			D0 = C0;
			C0 = B0;
			B0 += LeftRotate(function, s[j]);
		}

		A += A0;
		B += B0;
		C += C0;
		D += D0;
	}

	// 3. Split the buffer in individual byte
	std::vector<uint8_t> resultBytes = {
		static_cast<uint8_t>(A), static_cast<uint8_t>(A >> 8),
		static_cast<uint8_t>(A >> 16), static_cast<uint8_t>(A >> 24),

		static_cast<uint8_t>(B), static_cast<uint8_t>(B >> 8),
		static_cast<uint8_t>(B >> 16), static_cast<uint8_t>(B >> 24),

		static_cast<uint8_t>(C), static_cast<uint8_t>(C >> 8),
		static_cast<uint8_t>(C >> 16), static_cast<uint8_t>(C >> 24),

		static_cast<uint8_t>(D), static_cast<uint8_t>(D >> 8),
		static_cast<uint8_t>(D >> 16), static_cast<uint8_t>(D >> 24),
	};

	return BytesToHexString(resultBytes);
}

int main(int argc, char** argv)
{
	_setmode(_fileno(stdin), _O_U16TEXT);
	
	std::unordered_map<std::wstring, std::wstring> cache;
	ReadCache(L"cache.txt", cache);

	wchar_t filename[DEFAULT_MESSAGE_SIZE] = { 0 };
	wchar_t buffer[DEFAULT_MESSAGE_SIZE] = { 0 };

	std::string message;
	message.reserve(DEFAULT_MESSAGE_SIZE);
	
	FILE* outputFileStream = _wfopen(L"output.txt", L"a+");

	if (outputFileStream == nullptr)
	{
		std::cout << "Error: can't open output file" << std::endl;
		return -1;
	}

	_setmode(_fileno(outputFileStream), _O_U8TEXT);

	int choice = -1;

	do
	{
		if (choice == UserInput::ENTER_STRING)
		{
			std::wcin.ignore(1, '\n');
			std::wcin.getline(buffer, DEFAULT_MESSAGE_SIZE);

			auto bytes = ConvertWideStringToBytes(buffer);

			std::string result = HashMD5(bytes, bytes.size());
			std::wstring hash = std::wstring(result.begin(), result.end());

			std::wcout << L"Result: " << hash << std::endl;

			std::wstring response = std::wstring(L"H(") + buffer + L") = " + hash + L"\n";
			fputws(response.c_str(), outputFileStream);

			memset(buffer, 0, DEFAULT_MESSAGE_SIZE);

			system("pause");
		}
		else if (choice == UserInput::ENTER_FILE)
		{
			std::wcin >> filename;
			
			std::wstring message;
			size_t size;
			if (ReadFileUTF8(filename, message, size))
			{
				auto bytes = ConvertWideStringToBytes(message);

				std::string result = HashMD5(bytes, size);
				std::wstring hash = std::wstring(result.begin(), result.end());
				std::cout << "Result: " << result << std::endl;

				std::wstring response = std::wstring(L"H(") + filename + L") = " + hash + L"\n";
				fputws(response.c_str(), outputFileStream);

				cache[filename] = hash;
			}
			else
			{
				std::wcout << "The file " << filename << " can't be read" << std::endl;
			}

			memset(filename, 0, DEFAULT_MESSAGE_SIZE);
			system("pause");
		}
		else if (choice == UserInput::CHECK_VALIDITY)
		{
			std::wcin >> filename;
			std::wstring message;
			size_t size;
			if (ReadFileUTF8(filename, message, size))
			{
				auto bytes = ConvertWideStringToBytes(message);
				std::string result = HashMD5(bytes, size);
				std::wstring hash = std::wstring(result.begin(), result.end());

				if (cache.find(filename) != cache.end())
				{
					std::wstring savedMessage = cache[filename];
					std::cout << "Is file corrupted? " << std::boolalpha << (savedMessage != hash) << std::endl;

					std::wstring response = std::wstring(L"Is file corrupted? ") + ((savedMessage != hash) ? L"true" : L"false");

					fputws(response.c_str(), outputFileStream);
				}
				else
				{
					std::cout << "The file wasn't in the cache. Its hash: " << result << std::endl;
					std::wstring response = std::wstring(L"The file wasn't in the cache. Its hash: ") + hash;

					fputws(response.c_str(), outputFileStream);

					cache[filename] = hash;
				}
			}
			else
			{
				std::cout << "The file " << filename << " can't be read" << std::endl;
			}

			memset(filename, 0, DEFAULT_MESSAGE_SIZE);
			system("pause");
		}

		system("cls");
		std::wcout << "Select command: 0 (exit), 1 (enter a message), 2 (enter a file), 3 (check validity): ";
		std::wcin >> choice;
		//message.clear();
	} while (choice != UserInput::EXIT);

	fclose(outputFileStream);

	// Write cache
	FILE* outputCache = _wfopen(L"cache.txt", L"w+");
	_setmode(_fileno(outputCache), _O_U8TEXT);

	for (auto it = cache.cbegin(); it != cache.cend(); it++)
	{
		std::wstring outResult = it->first + L":" + it->second + L"\n";
		fputws(outResult.c_str(), outputCache);
	}

	fclose(outputCache);


	return 0;
}