
#ifndef UTILS_GROME_INCLUDED_
#define UTILS_GROME_INCLUDED_

#include <sstream>
#include <vector>

#include <tchar.h>

#include <Engine/sdk.h>

#include "toolbox/Utils.h"

using namespace csdk;

extern iRootInterface *g_sdk_root;


std::ostream & operator<<(std::ostream & out, t_float3 f);
std::string fromUnicode(const std::wstring & str);
std::wstring toUnicode(const std::string & str);


#define AUTO_CLOSE_ARRAY(arr) \
	std::vector<AutoCloseInterface> vl_v##arr(arr->No()); \
	for (unsigned i=0; i<arr->No(); ++i) vl_v##arr[i].setInterface(arr->Elem(i));

#define AUTO_CLOSE(i) assert(i);AutoCloseInterface cl##i(i);
class AutoCloseInterface
{
public:
	AutoCloseInterface() : interface_(NULL) {}
	AutoCloseInterface(iSdkInterface * i_face) : interface_(i_face) {}
	~AutoCloseInterface()
	{
		if (interface_) interface_->CloseInterface();
	}

	void setInterface(iSdkInterface * i_face) { interface_ = i_face; }

protected:
	iSdkInterface * interface_;
};


#define AUTO_DEL_NODE(i) assert(i);AutoDelNode ad##i(i);
class AutoDelNode
{
public:
	AutoDelNode(iSdkInterface * node) : node_(node) {}
	~AutoDelNode() { g_sdk_root->DelNode(node_); }

protected:
	iSdkInterface * node_;
};

class AssertException : public Exception
{
};

#undef assert
#define assert(exp) assertFunGrome(exp, #exp, __LINE__, __FILE__)

void assertFunGrome(bool exp, const char * exp_str, unsigned line, const char * file);


void msgBox(const std::string & msg, const std::string & caption = "Info");
void msgBox(const std::wstring & msg, const std::wstring & caption = M_SZ("Info"));



#endif
