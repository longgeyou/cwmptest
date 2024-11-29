


/*============================================================
soapmsg

1、该数据模型有 多个版本
    cwmp-1-0.xsd
    cwmp-1-1.xsd
    cwmp-1-2.xsd
    cwmp-1-3.xsd
    cwmp-1-4.xsd
2、本程序参考《tr-069-1-6-1》并支持cwmp-1-4.xsd，有些数据模型在
    cwmp-1-3.xsd、cwmp-1-4.xsd 中没有描述，所以参考cwmp-1-2.xsd
    及以前的版本
============================================================*/






#include <stdio.h>
#include <string.h>


#include "soapmsg.h"
#include "pool2.h"





/*-------------------------------------------------------------
https://cwmp-data-models.broadband-forum.org/cwmp-1-0.xsd
https://schemas.xmlsoap.org/soap/envelope/
https://schemas.xmlsoap.org/soap/encoding/


参考示例：
<soap:Envelope
       xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
       xmlns:cwmp="urn:dslforum-org:cwmp-1-0">
    <soap:Header>
        <cwmp:ID soap:mustUnderstand="1">1234</cwmp:ID>
    </soap:Header>
    <soap:Body>
        <soap:Fault>
            <faultcode>Client</faultcode>
            <faultstring>CWMP fault</faultstring>
            <detail>
                <cwmp:Fault>
                    <FaultCode>9000</FaultCode>
                    <FaultString>Upload method not supported</FaultString>
                </cwmp:Fault>
            </detail>
        </soap:Fault>
    </soap:Body>
</soap:Envelope>

方案：
树形结构，soap_obj_t      对象到文本
-------------------------------------------------------------*/

//返回值
#define RET_OK 0
#define RET_FAILD -1
#define RET_EXIST 1


/*==============================================================
                        基础
==============================================================*/
//#define SOAP_STRING_SOAP_ENVELOPE "soap:Envelope"
//#define SOAP_STRING_XMLNS_SOAP "xmlns:soap"
//#define SOAP_STRING_XMLNS_SOAP_VALUE "\"http://schemas.xmlsoap.org/soap/envelope/\""
//#define SOAP_STRING_XMLNS_CWMP "xmlns:cwmp"
//#define SOAP_STRING_XMLNS_CWMP_VALUE "\"urn:dslforum-org:cwmp-1-0\""
//#define SOAP_STRING_SOAP_HEADER "soap:Header"
//#define SOAP_STRING_SOAP_BODY "soap:Body"


//设置 soap 对象的 soap:Envelope 、soap:Header 信息
//注意参数 soap ，子成员 root 节点，一般是还没有设置过数值？？或者调用者可以先清空
int soapmsg_set_base(soap_obj_t *soap, soap_header_t *header)
{
    if(soap == NULL || soap->root == NULL)return RET_FAILD;

    //清空
    //soap_node_clear(soap->root);
    
    //添加信封 soap:Envelope
    if(soap->root->name != NULL)strcpy(soap->root->name, "soap:Envelope");
    if(soap->root->attr != NULL)
    {
        keyvalue_append_set_str(soap->root->attr, "xmlns:soap", 
            "\"http://schemas.xmlsoap.org/soap/envelope/\"");
            
        keyvalue_append_set_str(soap->root->attr, "xmlns:cwmp", 
            "\"urn:dslforum-org:cwmp-1-4\"");
    }
        
    //添加头部 ，如果 header 为 NULL则跳过
    //char buf[16];
    if(header != NULL)
    {
        soap_node_t *nodeHeader = soap_create_node_set_value("soap:Header", NULL);
        if(nodeHeader != NULL)
        {
            if(header->idEn == 1)
            {
                soap_node_t *nodeID = soap_create_node_set_value("cwmp:ID", header->ID);
                if(nodeID != NULL)
                    keyvalue_append_set_str(nodeID->attr, "soapenv:mustUnderstand", "1");
                soap_node_append_son(nodeHeader, nodeID);                
            }

            if(header->holdRequestsEn == 1)
            {
                //sprintf(buf, "%d", header->HoldRequests);
                soap_node_t *nodeHoldRequests = soap_create_node_set_value("cwmp:HoldRequests", header->HoldRequests);
                if(nodeHoldRequests != NULL)
                    keyvalue_append_set_str(nodeHoldRequests->attr, "soapenv:mustUnderstand", "1");
                soap_node_append_son(nodeHeader, nodeHoldRequests);
            }

            soap_node_append_son(soap->root, nodeHeader);
        }
        
    }

    //添加身体节点 soap:Body
    soap_node_t *nodeBody = soap_create_node_set_value("soap:Body", NULL);
    if(nodeBody != NULL)
    {
        soap_node_append_son(soap->root, nodeBody);
    }

    return RET_OK;
}


/*==============================================================
                        A.3.1 Generic Methods
==============================================================*/
/*--------------------------------------------------------------
A.3.1.1 GetRPCMethods
--------------------------------------------------------------*/
// GetRPCMethods
soap_node_t *soapmsg_to_node_GetRPCMethods(rpc_GetRPCMethods_t data)
{
    soap_node_t *node = soap_create_node_set_value("cwmp:GetRPCMethods", NULL);
    return node;
}

// GetRPCMethodsResponse
soap_node_t *soapmsg_to_node_GetRPCMethodsResponse(rpc_GetRPCMethodsResponse_t data)
{
    char buf64[64];
    soap_node_t *node = soap_create_node_set_value("cwmp:GetRPCMethodsResponse", NULL);
  
    if(node == NULL)return NULL;

    // 添加 MethodList （方法列表） 
    snprintf(buf64, 64, "xs:string[%d]", data.MethodList->nodeNum);    
    soap_node_t *nodeMethodList = soap_create_node_set_value("MethodList", NULL);
    if(nodeMethodList != NULL)  
        keyvalue_append_set_str(nodeMethodList->attr, "soapenc:arrayType", buf64);
        
    LINK_FOREACH(data.MethodList, probe)
    {
        if(probe->data != NULL && probe->en == 1)
        {
            soap_node_append_son(nodeMethodList,  soap_create_node_set_value("string", (char *)(probe->data)));
        }       
    }        
    soap_node_append_son(node, nodeMethodList);
    
    return node;
}

/*==============================================================
                        A.3.2 CPE Methods
==============================================================*/

/*--------------------------------------------------------------
A.3.2.1 SetParameterValues
--------------------------------------------------------------*/
// SetParameterValues
soap_node_t *soapmsg_to_node_SetParameterValues(rpc_SetParameterValues_t data)
{
    char buf512[512] = {0};
    char buf64[64] = {0};
    int ret;

    soap_node_t *node = soap_create_node_set_value("cwmp:SetParameterValues", NULL);

    //添加 ParameterList 成员，该成员是一个链表，链表节点类型 ParameterValueStruct
    soap_node_t *nodeParameterList = soap_create_node_set_value("ParameterList", NULL);
    snprintf(buf64, 64, "tns:ParameterValueStruct[%d]", data.ParameterList->nodeNum); 
    if(nodeParameterList != NULL)  
        keyvalue_append_set_str(nodeParameterList->attr, "soapenc:arrayType", buf64);
    
    LINK_FOREACH(data.ParameterList, probe)
    {
        if(probe->data != NULL && probe->en == 1)
        {
            ParameterValueStruct *member = (ParameterValueStruct *)probe->data;
            soap_node_t *nodeParameterValueStruct = soap_create_node_set_value("ParameterValueStruct", NULL);

            //添加 Name
            soap_node_append_son(nodeParameterValueStruct, soap_create_node_set_value("Name", member->Name));

            //添加 Value（注意：这里先把 void * 类型的 Value ，转化为16进制显示的字符串）
            ret = strpro_hex2str(member->Value, member->valueLen, buf512, 512);            
            if(ret == 0)
                soap_node_append_son(nodeParameterValueStruct, soap_create_node_set_value("Value", buf512));
          
            soap_node_append_son(nodeParameterList, nodeParameterValueStruct);
 
        }       
    }   
    soap_node_append_son(node, nodeParameterList);

    //添加 ParameterKey 成员
    soap_node_append_son(node, soap_create_node_set_value("ParameterKey", data.ParameterKey));
    
    return node;
}

// SetParameterValuesResponse
soap_node_t *soapmsg_to_node_SetParameterValuesResponse(rpc_SetParameterValuesResponse_t data)
{
    char buf8[8] = {0};
    soap_node_t *node = soap_create_node_set_value("cwmp:SetParameterValuesResponse", NULL);
    
    //添加 Status 成员
    sprintf(buf8, "%d", data.Status);
    soap_node_append_son(node, soap_create_node_set_value("Status", buf8));
    
    return node;
}

/*--------------------------------------------------------------
A.3.2.2 GetParameterValues
--------------------------------------------------------------*/
// GetParameterValues
soap_node_t *soapmsg_to_node_GetParameterValues(rpc_GetParameterValues_t data)
{
    char buf64[64] = {0};
    soap_node_t *node = soap_create_node_set_value("cwmp:GetParameterValues", NULL);
    
    //添加 ParameterNames 成员
    soap_node_t *nodeParameterNames = soap_create_node_set_value("ParameterNames", NULL);
    snprintf(buf64, 64, "xs:string[%d]", data.ParameterNames->nodeNum);
    if(nodeParameterNames != NULL)
        keyvalue_append_set_str(nodeParameterNames->attr, "soapenc:arrayType", buf64);
    
    LINK_FOREACH(data.ParameterNames, probe)
    {
        if(probe->data != NULL && probe->en == 1)
        {
            soap_node_append_son(nodeParameterNames, soap_create_node_set_value("string", probe->data));
        }   
    }

    soap_node_append_son(node, nodeParameterNames);
    
    return node;
}

// GetParameterValuesResponse
soap_node_t *soapmsg_to_node_GetParameterValuesResponse(rpc_GetParameterValuesResponse_t data)
{
    char buf512[512] = {0};
    char buf64[64] = {0};
    int ret;
    soap_node_t *node = soap_create_node_set_value("cwmp:GetParameterValuesResponse", NULL);
    
    //1、添加 ParameterList 链表成员，成员类型 ParameterValueStruct
    soap_node_t *nodeParameterList = soap_create_node_set_value("ParameterList", NULL);
    snprintf(buf64, 64, "tns:ParameterValueStruct[%d]", data.ParameterList->nodeNum);
    if(nodeParameterList != NULL)
        keyvalue_append_set_str(nodeParameterList->attr, "soapenc:arrayType", buf64);

    LINK_FOREACH(data.ParameterList, probe)
    {
        if(probe->data != NULL && probe->en == 1)
        {
            ParameterValueStruct *member = (ParameterValueStruct *)(probe->data);
            soap_node_t *nodeParameterValueStruct = soap_create_node_set_value("ParameterValueStruct", NULL);

            //1.1 添加 Name
            soap_node_append_son(nodeParameterValueStruct, soap_create_node_set_value("Name", member->Name));

            //1.2 添加 Value（注意：这里先把 void * 类型的 Value ，转化为16进制显示的字符串）
            ret = strpro_hex2str(member->Value, member->valueLen, buf512, 512);            
            if(ret == 0)
                soap_node_append_son(nodeParameterValueStruct, soap_create_node_set_value("Value", buf512));            
            else
                LOG_ALARM("strpro_hex2str 失败\n");
                      
            soap_node_append_son(nodeParameterList, nodeParameterValueStruct); 
        }  
    }

    soap_node_append_son(node, nodeParameterList);
    
    return node;
}

/*--------------------------------------------------------------
待定
A.3.2.3 GetParameterNames
A.3.2.4 SetParameterAttributes
A.3.2.5 GetParameterAttributes
A.3.2.6 AddObject
A.3.2.7 DeleteObject
A.3.2.8 Download
A.3.2.9 Reboot 
--------------------------------------------------------------*/


/*==============================================================
                        A.3.3 ACS Methods
==============================================================*/
/*--------------------------------------------------------------
A.3.3.1 Inform
--------------------------------------------------------------*/
// Inform
soap_node_t *soapmsg_to_node_Inform(rpc_Inform_t data)
{
    int i;
    char buf8[8] = {0};
    char buf64[64] = {0};
    char buf512[512] = {0};
    int ret;
    
    soap_node_t *node = soap_create_node_set_value("cwmp:Inform", NULL);
    
    //1、添加 DeviceId 成员
    /*
        例子：
            <DeviceId xmlns="http://example.com/myschema">
                <Manufacturer>ExampleCorp</Manufacturer>
                <OUI>123456</OUI>
                <ProductClass>ModelX</ProductClass>
                <SerialNumber>SN1234567890</SerialNumber>
            </DeviceId>
    */
    soap_node_t *nodeDeviceId = soap_create_node_set_value("DeviceId", NULL);    
    soap_node_append_son(nodeDeviceId, soap_create_node_set_value("Manufacturer", data.DeviceId.Manufacturer));
    soap_node_append_son(nodeDeviceId, soap_create_node_set_value("OUI", data.DeviceId.OUI));
    soap_node_append_son(nodeDeviceId, soap_create_node_set_value("ProductClass", data.DeviceId.ProductClass));
    soap_node_append_son(nodeDeviceId, soap_create_node_set_value("SerialNumber", data.DeviceId.SerialNumber));
    soap_node_append_son(node, nodeDeviceId);

    //2、添加 Event 成员，例子
    /*<EventList soapenc:arrayType="tns:EventStruct[3]">
        <EventStruct>
            <!-- EventStruct的内容 -->
        </EventStruct>
        <EventStruct>
            <!-- EventStruct的内容 -->
        </EventStruct>
        <EventStruct>
            <!-- EventStruct的内容 -->
        </EventStruct>
    </EventList>*/
    //关于 soapenc:arrayType="tns:EventStruct[3] 有待考虑
    
    soap_node_t *nodeEventList = soap_create_node_set_value("EventList", NULL);
    snprintf(buf64, 64, "tns:EventStruct[%d]", data.eventNum);
    if(nodeEventList != NULL)
        keyvalue_append_set_str(nodeEventList->attr, "soapenc:arrayType", buf64);
        
    for(i = 0; i < data.eventNum && i < 64; i++)
    {
        soap_node_t *nodeEventStruct = soap_create_node_set_value("EventStruct", NULL);
        soap_node_append_son(nodeEventStruct, soap_create_node_set_value("EventCode", (data.Event)[i].EventCode));
        soap_node_append_son(nodeEventStruct, soap_create_node_set_value("CommandKey", (data.Event)[i].CommandKey));
        soap_node_append_son(nodeEventList, nodeEventStruct);
    }
    soap_node_append_son(node, nodeEventList);
    
    //3、添加 MaxEnvelopes 成员
    snprintf(buf8, 8, "%d", data.MaxEnvelopes);
    soap_node_append_son(node, soap_create_node_set_value("MaxEnvelopes", buf8));

    //4、添加 CurrentTime 成员
    //dateTime类型例子 <CurrentTime>2024-11-19T17:51:00</CurrentTime>
    snprintf(buf64, 64, "%04d-%02d-%02dT%02d:%02d:%02d", 
                data.CurrentTime.year + 1, data.CurrentTime.mouth + 1, data.CurrentTime.day + 1,
                data.CurrentTime.hour, data.CurrentTime.minute, data.CurrentTime.second);
    soap_node_append_son(node, soap_create_node_set_value("CurrentTime", buf64));
    
    //5、添加 RetryCount
    snprintf(buf8, 8, "%d", data.RetryCount);
    soap_node_append_son(node, soap_create_node_set_value("RetryCount", buf8));

    //6、添加 ParameterList 链表的所有成员， 成员类型 ParameterValueStruct
    soap_node_t *nodeParameterList = soap_create_node_set_value("ParameterList", NULL);
    snprintf(buf64, 64, "tns:ParameterValueStruct[%d]", 
                            data.ParameterList->nodeNum);  //注意链表的nodeNum成员，我没有做很严格测试 
                                                                                        
    if(nodeEventList != NULL)
        keyvalue_append_set_str(nodeEventList->attr, "soapenc:arrayType", buf64);
    LINK_FOREACH(data.ParameterList, probe){
        if(probe->data != NULL && probe->en == 1)
        {
            ParameterValueStruct *member = (ParameterValueStruct *)probe->data;

            soap_node_t *nodeParameterValueStruct = soap_create_node_set_value("ParameterValueStruct", NULL);
            //6.1 添加 Name
            soap_node_append_son(nodeParameterValueStruct, soap_create_node_set_value("Name", member->Name));

            //6.2 添加 Value
            ret = strpro_hex2str(member->Value, member->valueLen, buf512, 512);            
            if(ret == 0)
                soap_node_append_son(nodeParameterValueStruct, soap_create_node_set_value("Value", buf512));            
            else
                LOG_ALARM("strpro_hex2str 失败\n");

            soap_node_append_son(nodeParameterList, nodeParameterValueStruct);
        }      
    }
    soap_node_append_son(node, nodeParameterList);
    
    
    return node;
}

// InformResponse
soap_node_t *soapmsg_to_node_InformResponse(rpc_InformResponse_t data)
{
    char buf8[8] = {0};
    soap_node_t *node = soap_create_node_set_value("cwmp:InformResponse", NULL);

    //添加 MaxEnvelopes 成员
    snprintf(buf8, 8, "%d", data.MaxEnvelopes);
    soap_node_append_son(node, soap_create_node_set_value("MaxEnvelopes", buf8));

    return node;
}





/*--------------------------------------------------------------
待定
A.3.3.2 TransferComplete
A.3.3.3 AutonomousTransferComplete
--------------------------------------------------------------*/


/*==============================================================
                        A.4 Optional RPC Messages
==============================================================*/
/*--------------------------------------------------------------
待定
CPE Methods
A.4.1.1 GetQueuedTransfers
A.4.1.2 ScheduleInform
A.4.1.3 SetVouchers
A.4.1.4 GetOptions
A.4.1.5 Upload
A.4.1.6 FactoryReset
A.4.1.7 GetAllQueuedTransfers
A.4.1.8 ScheduleDownload
A.4.1.9 CancelTransfer
A.4.1.10 ChangeDUState
ACS Methods
A.4.2.1 Kicked
A.4.2.2 RequestDownload 
A.4.2.3 DUStateChangeComplete
A.4.2.4 AutonomousDUStateChangeComplete
--------------------------------------------------------------*/




/*==============================================================
                        A.5 Fault Handling
                            A.5.1 CPE Fault Codes
                            A.5.2 ACS Fault Codes
==============================================================*/

/*--------------------------------------------------------------
例子：
<soap:Envelope
               xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/"
               xmlns:cwmp="urn:dslforum-org:cwmp-1-0">
    <soap:Header>
        <cwmp:ID soap:mustUnderstand="1">1234</cwmp:ID>
    </soap:Header>
    <soap:Body>
        <soap:Fault>
            <faultcode>Client</faultcode>
            <faultstring>CWMP fault</faultstring>
            <detail>
                <cwmp:Fault>
                    <FaultCode>9000</FaultCode>
                    <FaultString>Upload method not supported</FaultString>
                </cwmp:Fault>
            </detail>
        </soap:Fault>
    </soap:Body>
</soap:Envelope>
--------------------------------------------------------------*/
soap_node_t *soapmsg_to_node_Fault(rpc_fault_t data, rpc_soap_fault_t soapData)
{
    char buf8[8] = {0};

    //1、设置 soap:Fault
    soap_node_t *nodeSoapFault = soap_create_node_set_value("soap:Fault", NULL);
    soap_node_append_son(nodeSoapFault, soap_create_node_set_value("faultCode", soapData.FaultCode));
    soap_node_append_son(nodeSoapFault, soap_create_node_set_value("faultString", soapData.FaultString));
    soap_node_t *nodeDetail = soap_create_node_set_value("detail", NULL);
    soap_node_append_son(nodeSoapFault, nodeDetail);

    //2、设置 cwmp:Fault
    soap_node_t *node = soap_create_node_set_value("cwmp:Fault", NULL);

    //2.1 添加 FaultCode 、FaultString
    snprintf(buf8, 8, "%d", data.FaultCode);
    soap_node_append_son(node, soap_create_node_set_value("FaultCode", buf8));
    soap_node_append_son(node, soap_create_node_set_value("FaultString", data.FaultString));
    
    //2.2 添加 SetParameterValuesFault
    soap_node_t *nodeSpvf = soap_create_node_set_value("SetParameterValuesFault", NULL);
    soap_node_append_son(nodeSpvf, soap_create_node_set_value("ParameterName", data.SetParameterValuesFault.ParameterName));
    snprintf(buf8, 8, "%d", data.SetParameterValuesFault.FaultCode);
    soap_node_append_son(nodeSpvf, soap_create_node_set_value("FaultCode", buf8));
    soap_node_append_son(nodeSpvf, soap_create_node_set_value("FaultString", data.SetParameterValuesFault.FaultString));
        
    soap_node_append_son(node, nodeSpvf);
    soap_node_append_son(nodeDetail, node);

   
    return nodeSoapFault;
}





/*==============================================================
                        测试
==============================================================*/

//基础测试
void soapmsg_test()
{
    soap_obj_t *soap = soap_create();
    soap_header_t header = {
        .ID = "long123",
        .idEn = 1,
        .HoldRequests = "false",
        .holdRequestsEn = 1
    };
    soapmsg_set_base(soap, &header);

    //节点显示测试
    printf("\n显示节点soap->root\n");
    soap_node_show(soap->root);

    //节点到文本测试
    char buf1024[1024 + 8] = {0};
    int pos = 0;
    soap_node_to_str(soap->root, buf1024, &pos, 1024);

    printf("\n节点到文本:\n%s\n", buf1024);

    //文本到节点测试
    soap_node_t *root = soap_str2node(buf1024);
    printf("\n显示节点root\n");
    soap_node_show(root);
    
}


//测试部分 rpc方法的 soap 封装 ；GetRPCMethods 等等
void soapmsg_test2()
{
    soap_obj_t *soap = soap_create();
    soap_header_t header = {
        .ID = "long123",
        .idEn = 1,
        .HoldRequests = "false",
        .holdRequestsEn = 1
    };

    char buf64[64] = {0};
    //char buf256[256] = {0};
    char *strTmp;
    
    //1、添加基础内容
    soapmsg_set_base(soap, &header);
    soap_node_t *root = soap->root;

    soap_node_t *body = soap_node_get_son(root, "soap:Body");
    

    //2、添加rpc方法 GetRPCMethods
    rpc_GetRPCMethods_t dataGetRPCMethods ={
        .null = NULL      
    };
    //soap_node_t *nodeGetRPCMethods = soapmsg_to_node_GetRPCMethods(dataGetRPCMethods);
    soap_node_append_son(body, soapmsg_to_node_GetRPCMethods(dataGetRPCMethods));

    //2-1、添加rpc方法 GetRPCMethodsResponse
    rpc_GetRPCMethodsResponse_t dataGetRPCMethodsResponse;
    dataGetRPCMethodsResponse.MethodList = link_create();
    char *methodListTmp[] = {
        "method_1",
        "method_2"
    }; 
    link_append_by_set_value(dataGetRPCMethodsResponse.MethodList, (void *)(methodListTmp[0]), strlen(methodListTmp[0]));
    link_append_by_set_value(dataGetRPCMethodsResponse.MethodList, (void *)(methodListTmp[1]), strlen(methodListTmp[1]));
    soap_node_append_son(body, soapmsg_to_node_GetRPCMethodsResponse(dataGetRPCMethodsResponse));

    link_destroy_and_data(dataGetRPCMethodsResponse.MethodList);

    //3、添加rpc方法 SetParameterValues
    link_obj_t *linkParameterList = link_create();
    ParameterValueStruct *pvs1 = (ParameterValueStruct *)link_append_by_malloc
                                    (linkParameterList, sizeof(ParameterValueStruct));    
    strcpy(pvs1->Name, "parameter_1");
    strcpy(buf64, "value");
    pvs1->Value = buf64;
    pvs1->valueLen = strlen(pvs1->Value);
    

    rpc_SetParameterValues_t dataSetParameterValues = {
        .ParameterList = linkParameterList,
        .ParameterKey = "paramter key"
    };

    soap_node_append_son(body, soapmsg_to_node_SetParameterValues(dataSetParameterValues));
    //pool_show();
    link_destroy_and_data(linkParameterList);   //释放动态内存
    

    
    //4、添加rpc方法 SetParameterValuesResponse    
    rpc_SetParameterValuesResponse_t dataSetParameterValuesResponse = {
        .Status = 1
    };
    soap_node_append_son(body, soapmsg_to_node_SetParameterValuesResponse(dataSetParameterValuesResponse));
    
    //5、添加rpc方法 GetParameterValues
    link_obj_t *linkParameterNames = link_create();     //链表成员数据的类型 ： char [256]
    
    strTmp = (char *)link_append_by_malloc(linkParameterNames, sizeof(256)); 
    strcpy(strTmp, "parameter_name_1"); 
    
    strTmp = (char *)link_append_by_malloc(linkParameterNames, sizeof(256)); 
    strcpy(strTmp, "parameter_name_2"); 
    
    rpc_GetParameterValues_t dataGetParameterValues = {
        .ParameterNames = linkParameterNames     //链表成员数据的类型 ： char [256]
    };

    soap_node_append_son(body, soapmsg_to_node_GetParameterValues(dataGetParameterValues));
    link_destroy_and_data(linkParameterNames);


    //6、添加rpc方法 GetParameterValuesResponse

    link_obj_t *gpvr_linkParameterList = link_create();  //链表成员类型 ParameterValueStruct
    
    ParameterValueStruct *gpvr_pvs1 = (ParameterValueStruct *)link_append_by_malloc
                                    (gpvr_linkParameterList, sizeof(ParameterValueStruct));
    strcpy(gpvr_pvs1->Name, "parameter_1");
    strcpy(buf64, "value");
    gpvr_pvs1->Value = buf64;
    gpvr_pvs1->valueLen = strlen(gpvr_pvs1->Value); 

    rpc_GetParameterValuesResponse_t dataGetParameterValuesResponse = {
        .ParameterList = gpvr_linkParameterList
    };
    
    soap_node_append_son(body, soapmsg_to_node_GetParameterValuesResponse(dataGetParameterValuesResponse));
    link_destroy_and_data(gpvr_linkParameterList);


    //7、添加rpc方法 Inform
    rpc_Inform_t dataInform;
    
    //7.1 Inform 的 DeviceId
    /*
        <DeviceIdStruct>
          <Manufacturer>ExampleManufacturer</Manufacturer>
          <OUI>001122</OUI>
          <ProductClass>ModelX</ProductClass>
          <SerialNumber>1234567890ABCDEF</SerialNumber>
        </DeviceIdStruct>
    */
    strcpy(dataInform.DeviceId.Manufacturer, "设备制造商的名称");
    strcpy(dataInform.DeviceId.OUI, "设备制造商的组织唯一标识符"); 
    //strncpy(dataInform.DeviceId.OUI, "设备制造商的组织唯一标识符", 6); //OUI只有6个字节
    strcpy(dataInform.DeviceId.ProductClass, "特定序列号的产品类别的标识符");
    strcpy(dataInform.DeviceId.SerialNumber, "特定设备的标识符");
    
    //7.2 Inform 的 Event、eventNum、MaxEnvelopes、 CurrentTime、 RetryCount
    
    dataInform.eventNum = 2;
    
    strcpy(dataInform.Event[0].EventCode, "0 BOOTSTRAP");
    strcpy(dataInform.Event[0].CommandKey, "CommandKey_0");
    strcpy(dataInform.Event[1].EventCode, "1 BOOT");
    strcpy(dataInform.Event[1].CommandKey, "CommandKey_1");
    
    dataInform.MaxEnvelopes = 1;
    cwmprpc_str2dateTime("2024-11-19T17:51:00", &(dataInform.CurrentTime));
    dataInform.RetryCount = 0;

    //7.3 Inform 的 ParameterList，链表成员类型为 ParameterValueStruct
    dataInform.ParameterList = link_create();
    ParameterValueStruct *informPvs1 = (ParameterValueStruct *)link_append_by_malloc
                                        (dataInform.ParameterList, sizeof(ParameterValueStruct));
    strcpy(informPvs1->Name, "parameter_1");
    strcpy(buf64, "value_1");
    informPvs1->Value = buf64;
    informPvs1->valueLen = strlen(informPvs1->Value);

    ParameterValueStruct *informPvs2 = (ParameterValueStruct *)link_append_by_malloc
                                        (dataInform.ParameterList, sizeof(ParameterValueStruct));
    strcpy(informPvs2->Name, "parameter_2");
    strcpy(buf64, "value_2");
    informPvs2->Value = buf64;
    informPvs2->valueLen = strlen(informPvs2->Value);
    
    soap_node_append_son(body, soapmsg_to_node_Inform(dataInform));

    link_destroy_and_data(dataInform.ParameterList);

    //8、添加rpc方法 InformResponse
    rpc_InformResponse_t dataInformResponse = {.MaxEnvelopes = 1};
    soap_node_append_son(body, soapmsg_to_node_InformResponse(dataInformResponse));
        
    
//    //节点显示测试
//    printf("\n显示节点soap->root\n");
//    soap_node_show(soap->root);

    //节点到文本测试
    char buf2048[2048 + 8] = {0};
    int pos = 0;
    soap_node_to_str(soap->root, buf2048, &pos, 2048);

    printf("\n节点到文本:\n%s\n", buf2048);

    pool_show();
    soap_destroy(soap); //释放内存
    pool_show();
}


//测试rpc 中 Fault 的soap封装
void soapmsg_test3()
{
    soap_obj_t *soap = soap_create();
    soap_header_t header = {
        .ID = "long123",
        .idEn = 1,
        .HoldRequests = "false",    //注意这个值只能是 false 或者 ture
        .holdRequestsEn = 1
    };

    //char buf64[64] = {0};
    //char buf256[256] = {0};
    //char *strTmp;
    //int index;
    
    //1、添加基础内容
    soapmsg_set_base(soap, &header);
    soap_node_t *root = soap->root;

    soap_node_t *body = soap_node_get_son(root, "soap:Body");

    //2、添加 rpc 的 Fault
    //index = 0;
    rpc_fault_t dataFault;
    dataFault.FaultCode = cpe_fault_code[0].codeNum;
    strcpy(dataFault.FaultString, cpe_fault_code[0].describ); 

    dataFault.SetParameterValuesFault.FaultCode = cpe_fault_code[1].codeNum;
    strcpy(dataFault.SetParameterValuesFault.FaultString, cpe_fault_code[1].describ);
    strcpy(dataFault.SetParameterValuesFault.ParameterName, "parameter_1");   

    rpc_soap_fault_t soapFault = {
        .FaultCode = "Client",
        .FaultString = "CPE Fault detail"
    };
    
    soap_node_append_son(body, soapmsg_to_node_Fault(dataFault, soapFault));

    //节点到文本测试
    char buf2048[2048 + 8] = {0};
    int pos = 0;
    soap_node_to_str(soap->root, buf2048, &pos, 2048);

    printf("\n节点到文本:\n%s\n", buf2048);

    pool_show();
    soap_destroy(soap); //释放内存
    pool_show();

}












