#include <iostream>
#include <string>
#include <vector>

// 448 bits
#define PADDING_SIZE 56 
// 512 bits
#define CHUNK_SIZE 64
// 8 bytes for the length
#define LENGTH_SIZE 8

// Round functions
#define F(B, C, D) (( (B) & (C) ) | ( (~(B)) & (D) ))
#define H(B, C, D) (( (D) & (B) ) | ( (~(D)) & (C) ))
#define J(B, C, D) ((B) ^ (C) ^ (D))
#define I(B, C, D) ((C) ^ ( (B) | (~(D)) ))

std::vector<uint8_t> ReadBytes(const std::wstring& str)
{
	std::vector<uint8_t> bytes;
	bytes.resize(str.length() * sizeof(wchar_t));

	memcpy(bytes.data(), str.data(), str.length() * sizeof(wchar_t));
	return bytes;
}

int main(int argc, char** argv)
{
#ifdef _DEBUG
	std::wcout << "sizeof(wchar_t) = " << sizeof(wchar_t) << std::endl;
#endif // _DEBUG

	std::wstring message = L"abc";

#ifndef _DEBUG
	std::wcout << "Enter a message: " << std::endl;
	std::wcin >> message;
	std::wcout << message << std::endl;
#endif // !_DEBUG
	// Convert the input into bytes
	std::vector<uint8_t> bytes = ReadBytes(message);
#ifdef _DEBUG
	std::wcout << "Bytes: ";
	for (int i = 0; i < bytes.size(); i++)
		std::wcout << std::hex << bytes[i] << " ";

	std::wcout << std::endl;
#endif // _DEBUG

	// Adding the padding
	// 1. Add additional byte to save 0x80 (10000000)
	int paddingLength = (PADDING_SIZE - ((bytes.size() + 1) % CHUNK_SIZE)) % CHUNK_SIZE;
	// 2. Allocate the padding array
	std::vector<uint8_t> paddedBytes(bytes.size() + 1 + paddingLength + LENGTH_SIZE);
	// 3. Copy the original data into paddedBytes array
	memcpy(paddedBytes.data(), bytes.data(), bytes.size());
	// 4. Insert 0x80
	paddedBytes[bytes.size()] = 0x80;
	// 5. Insert length
	uint64_t length = bytes.size();
	// Using pointer arethmerics to copy the length bytes into array fastly
	memcpy(paddedBytes.data() + bytes.size() + 1 + paddingLength, &length, sizeof(uint64_t)); 

#ifdef _DEBUG
	std::wcout << "Padded Bytes: ";
	for (int i = 0; i < paddedBytes.size(); i++)
		std::wcout << std::hex << paddedBytes[i] << " ";

	std::wcout << std::endl;
	std::wcout << "sizeof(paddedBytes) = " << std::dec << paddedBytes.size() << std::endl;
#endif // _DEBUG

	// MD5 Process

	return 0;
}