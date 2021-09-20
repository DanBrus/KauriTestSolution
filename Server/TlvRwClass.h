#pragma once
#include <string>
#include <cmath>

class TlvRwClass
{
public:
	//Теги взяты с сайта https://pro-ldap.ru/tr/zytrax/tech/asn1.html#der-overview
	enum tag : uint8_t {
		BOOLEAN = 1,
		INTEGER = 2,
		BITSTRING = 3,
		NULLTAG = 5
	};

private:
	std::basic_string<uint8_t> bufer;

public:

	void writeToBuffer(const uint8_t* data, size_t length);

	std::string readMsg();

	std::basic_string<uint8_t> getTlv(const std::string data);
};

