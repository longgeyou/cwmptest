


#ifndef _SOAPMSG_H_
#define _SOAPMSG_H_



#include "soap.h"
#include "cwmprpc.h"
#include "keyvalue.h"
#include "log.h"
#include "strpro.h"




/*==============================================================
                        接口
==============================================================*/

void soapmsg_test();
void soapmsg_test2();
void soapmsg_test3();

//-----------------------------------------------------------基础
int soapmsg_set_base(soap_obj_t *soap, soap_header_t *header);


//-----------------------------------------------------------A.3.1 Generic Methods
// GetRPCMethods
soap_node_t *soapmsg_to_node_GetRPCMethods(rpc_GetRPCMethods_t data);
// GetRPCMethodsResponse
soap_node_t *soapmsg_to_node_GetRPCMethodsResponse(rpc_GetRPCMethodsResponse_t data);


//-----------------------------------------------------------A.3.2 CPE Methods
// SetParameterValues
soap_node_t *soapmsg_to_node_SetParameterValues(rpc_SetParameterValues_t data);
// SetParameterValuesResponse
soap_node_t *soapmsg_to_node_SetParameterValuesResponse(rpc_SetParameterValuesResponse_t data);
// GetParameterValues
soap_node_t *soapmsg_to_node_GetParameterValues(rpc_GetParameterValues_t data);
// GetParameterValuesResponse
soap_node_t *soapmsg_to_node_GetParameterValuesResponse(rpc_GetParameterValuesResponse_t data);


//----------------------------------------------------------- A.3.3 ACS Methods
// Inform
soap_node_t *soapmsg_to_node_Inform(rpc_Inform_t data);
// InformResponse
soap_node_t *soapmsg_to_node_InformResponse(rpc_InformResponse_t data);

//----------------------------------------------------------- A.5 Fault Handling
soap_node_t *soapmsg_to_node_Fault(rpc_fault_t data, rpc_soap_fault_t soapData);







#endif

