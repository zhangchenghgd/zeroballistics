
#include "UtilsGrome.h"


std::ostream & operator<<(std::ostream & out, t_float3 f)
{
	out << f.r 
		<< ", "
		<< f.g 
		<< ", "
		<< f.b;
	return out;
}


std::string fromUnicode(const std::wstring & str)
{
	int buf_size = WideCharToMultiByte(CP_ACP, 0, str.c_str(), -1, NULL, 0, NULL, NULL);
	std::vector<char> buf(buf_size);
	WideCharToMultiByte(CP_ACP, 0, str.c_str(), -1, &buf[0], buf.size(), NULL, NULL);
	return std::string(&buf[0]);
}

std::wstring toUnicode(const std::string & str)
{
	int size = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, NULL, 0);
	std::vector<WCHAR> buf(size);
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &buf[0], buf.size());
	return std::wstring(&buf[0]);
}

void assertFunGrome(bool exp, const char * exp_str, unsigned line, const char * file)
{
	if (exp) return;

	std::string msg = exp_str;
	msg += "\nLine " + toString(line) + "\nFile " + file;

	MessageBox(NULL, toUnicode(msg).c_str(), M_SZ("Assertion Failure"), MB_OK);

	throw AssertException();
}


void msgBox(const std::string & msg, const std::string & caption)
{
	MessageBox(NULL, toUnicode(msg).c_str(), toUnicode(caption).c_str(), MB_OK);
}


void msgBox(const std::wstring & msg, const std::wstring & caption)
{
	MessageBox(NULL, msg.c_str(), caption.c_str(), MB_OK);
}