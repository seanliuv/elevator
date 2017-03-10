#ifndef XML_PARSER_H
#define XML_PARSER_H

#include <libxml/parser.h>
#include <libxml/tree.h>

#define CHECK_X_CHAR(x)	\
	do\
	{\
		if((x) == NULL)\
		{\
			printf("configure file error!\n");\
		}\
	}while(0)

inline void GET_UINT_FROM_XML(const char * label, unsigned int * x, xmlNodePtr xmlNode)
{
	if(!xmlStrcmp(xmlNode->name, BAD_CAST label))
	{
		xmlChar * x_char = xmlNodeGetContent(xmlNode);
		CHECK_X_CHAR(x_char);
		*x = atoi((const char *)x_char);
		xmlFree(x_char);
	}
}

inline void GET_INT_FROM_XML(const char * label, int * x, xmlNodePtr xmlNode)
{
	if(!xmlStrcmp(xmlNode->name, BAD_CAST label))
	{
		xmlChar * x_char = xmlNodeGetContent(xmlNode);
		CHECK_X_CHAR(x_char);
		*x = atoi((const char *)x_char);
		xmlFree(x_char);
	}
}

#endif //XML_PARSER_H
