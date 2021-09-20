#include "TlvRwClass.h"

void TlvRwClass::writeToBuffer(const uint8_t* data, size_t length)
{
	bufer.append(data, length);
}

std::string TlvRwClass::readMsg()
{
	if(bufer.length() < 2)
		return std::string();

	int dataLen = 0;
	int lenDataLen = 1;
	if (bufer[1] <= 127)
		dataLen = bufer[1];
	else {
		lenDataLen = bufer[1] & 0x7F;
		for (int i = 0; i < lenDataLen; i++) {
			dataLen += static_cast<int>(std::pow(2, (lenDataLen - i - 1) * 8)) * bufer[2 + i];
		}
		lenDataLen++;
	}

	if(bufer.length() < (lenDataLen + dataLen + 1))
		return std::string();

	std::string result;
	result.append((char*)bufer.c_str() + 1 + lenDataLen, dataLen);

	bufer = bufer.substr(dataLen + 1 + lenDataLen, bufer.size() - dataLen + 1 + lenDataLen);
	return result;
}

std::basic_string<uint8_t> TlvRwClass::getTlv(const std::string data)
{
	std::basic_string<uint8_t> result;

	result += (data.length() == 0) ? tag::NULLTAG : tag::BITSTRING;
	
	//int lenPartLength = 1;
	if (data.length() <= 127) {
		result += data.length();
	}
	else {
		int len = std::ceil(log2(data.length()) / 8);
		result += len + 0x80;
		for (int i = 0; i < len; i++) {
			//result += ((0xff << (len - 1)) & (data.length() << i);
			result += static_cast<uint8_t>(data.length() >> (8 * (len - i - 1)));
		}
	}

	result.append((uint8_t*)data.c_str(), data.length());

	return result;
}
