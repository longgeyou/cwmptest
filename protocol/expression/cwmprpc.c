


/*============================================================
cwmp 远程过程调用 数据模型

1、参考《tr-069-1-6-1》 A.6 RPC Method XML Schema
    https://cwmp-data-models.broadband-forum.org/cwmp-1-0.xsd
2、该数据模型有 多个版本
    cwmp-1-0.xsd
    cwmp-1-1.xsd
    cwmp-1-2.xsd
    cwmp-1-3.xsd
    cwmp-1-4.xsd
3、本程序参考《tr-069-1-6-1》并支持cwmp-1-4.xsd，有些数据模型在
    cwmp-1-3.xsd、cwmp-1-4.xsd 中没有描述，所以参考cwmp-1-2.xsd
    及以前的版本
============================================================*/




#include <stdio.h>
#include <string.h>

#include "cwmprpc.h"

/*==============================================================
                        cpe支持的rpc方法
                            【默认支持，但对端可能部分支持，需要注意】
==============================================================*/
const char *cwmprpc_cpe_method_g[] = {
    "GetRPCMethods",
    "SetParameterValues",
    "GetParameterValues",
    "GetParameterNames",
    "SetParameterAttributes",
    "GetParameterAttributes",
    "AddObject",
    "DeleteObject",
    "Reboot",
    "Download"
};


const char *cwmprpc_cpe_method_soap_name_g[] = {
    "cwmp:GetRPCMethods",
    "cwmp:SetParameterValues",
    "cwmp:GetParameterValues",
    "cwmp:GetParameterNames",
    "cwmp:SetParameterAttributes",
    "cwmp:GetParameterAttributes",
    "cwmp:AddObject",
    "cwmp:DeleteObject",
    "cwmp:Reboot",
    "cwmp:Download"
};

//对应的响应
const char *cwmprpc_cpe_methodResponse_g[] = {
    "GetRPCMethodsResponse",
    "SetParameterValuesResponse",
    "GetParameterValuesResponse",
    "GetParameterNamesResponse",
    "SetParameterAttributesResponse",
    "GetParameterAttributesResponse",
    "AddObjectResponse",
    "DeleteObjectResponse",
    "RebootResponse",
    "DownloadResponse"
};


const char *cwmprpc_cpe_methodResponse_soap_name_g[] = {
    "cwmp:GetRPCMethodsResponse",
    "cwmp:SetParameterValuesResponse",
    "cwmp:GetParameterValuesResponse",
    "cwmp:GetParameterNamesResponse",
    "cwmp:SetParameterAttributesResponse",
    "cwmp:GetParameterAttributesResponse",
    "cwmp:AddObjectResponse",
    "cwmp:DeleteObjectResponse",
    "cwmp:RebootResponse",
    "cwmp:DownloadResponse"
};


/*==============================================================
                        acs支持的rpc方法
==============================================================*/

const char *cwmprpc_acs_method_g[] = {
    "Inform",
    "GetRPCMethods",
    "TransferComplete"
};

const char *cwmprpc_acs_method_soap_name_g[] = {
    "cwmp:Inform",
    "cwmp:GetRPCMethods",
    "cwmp:TransferComplete"
};

//对应的响应
const char *cwmprpc_acs_methodResponse_g[] = {
    "InformResponse",
    "GetRPCMethodsResponse",
    "TransferCompleteResponse"
};

const char *cwmprpc_acs_methodResponse_soap_name_g[] = {
    "cwmp:InformResponse",
    "cwmp:GetRPCMethodsResponse",
    "cwmp:TransferCompleteResponse"
};





/*==============================================================
                        A.5 Fault Handling
                            A.5.1 CPE Fault Codes
                            A.5.2 ACS Fault Codes
==============================================================*/
//9000 - 9032 的cpe错误码；厂商自定义错误码范围 9800 – 9899 
const rpc_fault_code_t cpe_fault_code[]={    
{9000, rpc_faultCodeType_Server, 
      "Method not supported"},
{9001, rpc_faultCodeType_Server, 
      "Request denied (no reason specified)"},
{9002, rpc_faultCodeType_Server, 
      "Internal error"},
{9003, rpc_faultCodeType_Client, 
      "Invalid arguments"},
{9004, rpc_faultCodeType_Server, 
      "Resources exceeded (when used in association with SetParameterValues, this MUST NOT be" 
      "used to indicate Parameters in error)"},    
{9005, rpc_faultCodeType_Client, 
      "Invalid Parameter name (associated with Set/GetParameterValues, GetParameterNames,"
      "Set/GetParameterAttributes, AddObject, and DeleteObject)"},
{9006, rpc_faultCodeType_Client, 
      "Invalid Parameter type (associated with SetParameterValues)"},
{9007, rpc_faultCodeType_Client, 
      "Invalid Parameter value (associated with SetParameterValues)"},
{9008, rpc_faultCodeType_Client, 
      "Attempt to set a non-writable Parameter (associated with SetParameterValues)"},
{9009, rpc_faultCodeType_Server, 
      "Notification request rejected (associated with SetParameterAttributes method)."},
{9010, rpc_faultCodeType_Server, 
      "File transfer failure (associated with Download, ScheduleDownload,"
      "TransferComplete or AutonomousTransferComplete methods)."},
{9011, rpc_faultCodeType_Server, 
      "Upload failure (associated with Upload, TransferComplete or AutonomousTransferComplete methods)."},
{9012, rpc_faultCodeType_Server, 
      "File transfer server authentication failure (associated with Upload, Download,"
      "TransferComplete, AutonomousTransferComplete, DUStateChangeComplete, or"
      "AutonomousDUStateChangeComplete methods)."},
{9013, rpc_faultCodeType_Server, 
      "Unsupported protocol for file transfer (associated with Upload, Download,"
      "ScheduleDownload, DUStateChangeComplete, or AutonomousDUStateChangeComplete methods)."},
{9014, rpc_faultCodeType_Server, 
      "File transfer failure: unable to join multicast group (associated with Download,"
      "TransferComplete or AutonomousTransferComplete methods)."},
{9015, rpc_faultCodeType_Server, 
      "File transfer failure: unable to contact file server (associated with Download,"
      "TransferComplete, AutonomousTransferComplete, DUStateChangeComplete, or"
      "AutonomousDUStateChangeComplete methods)."},
{9016, rpc_faultCodeType_Server, 
        "File transfer failure: unable to access file (associated with Download,"
        "TransferComplete, AutonomousTransferComplete, DUStateChangeComplete, or"
        "AutonomousDUStateChangeComplete methods)."},
{9017, rpc_faultCodeType_Server, 
        "File transfer failure: unable to complete download (associated with Download,"
        "TransferComplete, AutonomousTransferComplete, DUStateChangeComplete, or"
        "AutonomousDUStateChangeComplete methods)."},
{9018, rpc_faultCodeType_Server, 
        "File transfer failure: file corrupted or otherwise unusable (associated with"
        "Download, TransferComplete, AutonomousTransferComplete,"
        "DUStateChangeComplete, or AutonomousDUStateChangeComplete methods)."},
{9019, rpc_faultCodeType_Server, 
        "File transfer failure: file authentication failure (associated with Download,"
        "TransferComplete or AutonomousTransferComplete methods)."},
{9020, rpc_faultCodeType_Client, 
        "File transfer failure: unable to complete download within specified time windows"
        "(associated with TransferComplete method)."},
{9021, rpc_faultCodeType_Client, 
        "Cancelation of file transfer not permitted in current transfer state (associated with"
        "CancelTransfer method)."},
{9022, rpc_faultCodeType_Server, 
        "Invalid UUID Format (associated with DUStateChangeComplete or"
        "AutonomousDUStateChangeComplete methods: Install, Update, and Uninstall)"},
{9023, rpc_faultCodeType_Server, 
        "Unknown Execution Environment (associated with DUStateChangeComplete or"
        "AutonomousDUStateChangeComplete methods: Install only)"},
{9024, rpc_faultCodeType_Server, 
    "Disabled Execution Environment (associated with DUStateChangeComplete or"
    "AutonomousDUStateChangeComplete methods: Install, Update, and Uninstall)"},
{9025, rpc_faultCodeType_Server, 
    "Deployment Unit to Execution Environment Mismatch (associated with"
    "DUStateChangeComplete or AutonomousDUStateChangeComplete methods:"
    "Install and Update)"},
{9026, rpc_faultCodeType_Server, 
    "Duplicate Deployment Unit (associated with DUStateChangeComplete or"
    "AutonomousDUStateChangeComplete methods: Install only)"},
{9027, rpc_faultCodeType_Server, 
    "System Resources Exceeded (associated with DUStateChangeComplete or"
    "AutonomousDUStateChangeComplete methods: Install and Update)"},
{9028, rpc_faultCodeType_Server, 
    "Unknown Deployment Unit (associated with DUStateChangeComplete or"
    "AutonomousDUStateChangeComplete methods: Update and Uninstall)"},
{9029, rpc_faultCodeType_Server, 
    "Invalid Deployment Unit State (associated with DUStateChangeComplete or"
    "AutonomousDUStateChangeComplete methods: Install, Update and Uninstall)"},
{9030, rpc_faultCodeType_Server, 
        "Invalid Deployment Unit Update – Downgrade not permitted (associated with"
        "DUStateChangeComplete or AutonomousDUStateChangeComplete methods: Update only)"},
{9031, rpc_faultCodeType_Server, 
        "Invalid Deployment Unit Update – Version not specified (associated with"
        "DUStateChangeComplete or AutonomousDUStateChangeComplete methods: Update only)"},
{9032, rpc_faultCodeType_Server, 
        "Invalid Deployment Unit Update – Version already exists (associated with"
        "DUStateChangeComplete or AutonomousDUStateChangeComplete methods: Update only)"}
};

//8000 - 8006 的acs错误码；厂商自定义错误码范围 8800 – 8899
const rpc_fault_code_t acs_fault_code[]={
{8000, rpc_faultCodeType_Server, 
        "Method not supported"},
{8001, rpc_faultCodeType_Server, 
        "Request denied (no reason specified)"},
{8002, rpc_faultCodeType_Server, 
        "Internal error"},
{8003, rpc_faultCodeType_Client, 
        "Invalid arguments"},
{8004, rpc_faultCodeType_Server, 
        "Resources exceeded"},
{8005, rpc_faultCodeType_Server, 
        "Retry request"},
{8006, rpc_faultCodeType_Server, 
        "ACS version incompatible with CPE"}
    
};








/*==============================================================
                        应用
==============================================================*/

//typedef struct rpc_dateTime_t{
//    unsigned int year;
//    unsigned char mouth;
//    unsigned char day;
//    unsigned char hour;
//    unsigned char minute;
//    unsigned char second;
//}dateTime;
//dateTime 到字符串
//例子 2024-11-19T17:51:00
int cwmprpc_dateTime2str(dateTime date, char *out, int outLen)
{
    if(out == NULL)return -1;
    snprintf(out, outLen,"%04d-%02d-%02dT%02d:%02d:%02d",
                date.year + 1, date.mouth + 1, date.day + 1,    //因为从 1年1月1日开始，所以要加1
                date.hour, date.minute, date.second);
    return 0;
}
//字符串到 dateTime
int cwmprpc_str2dateTime(char *in, dateTime *date)
{
    if(in == NULL || date == NULL)return -1;

    sscanf(in, "%4d-%2d-%2dT%2d:%2d:%2d", 
                (int *)(&(date->year)), (int *)(&(date->mouth)), (int *)(&(date->day)),
                (int *)(&(date->hour)), (int *)(&(date->minute)), (int *)(&(date->second)));
    date->year--;
    date->mouth--;
    date->day--;
    return 0;
}


//匹配 cpe   方法，例如 "cwmp:GetParameterValues",
int cwmprpc_cpe_method_soap_name_match(char *in)
{
    if(in == NULL)return -1;

    int i;
    int num;
    num = sizeof(cwmprpc_cpe_method_soap_name_g) / sizeof(char *);
    for(i = 0; i < num; i++)
    {
        if(strcmp(cwmprpc_cpe_method_soap_name_g[i], in) == 0)
            return 0;   //说明匹配到了
    }
    
    return -2;
}

//匹配 acs 方法，例如 "Inform"
int cwmprpc_acs_method_soap_name_match(char *in)
{
    if(in == NULL)return -1;

    int i;
    int num;
    num = sizeof(cwmprpc_acs_method_soap_name_g) / sizeof(char *);
    for(i = 0; i < num; i++)
    {
        //printf("i:%d in:【%s】name:【%s】\n", i, in, cwmprpc_acs_method_soap_name_g[i]);
        if(strcmp(cwmprpc_acs_method_soap_name_g[i], in) == 0)
            return 0;   //说明匹配到了
    }
    
    return -2;
}

//匹配 cpe   方法响应，例如 "cwmp:GetParameterValuesResponse",
int cwmprpc_cpe_methodResponse_soap_name_match(char *in)
{
    if(in == NULL)return -1;

    int i;
    int num;
    num = sizeof(cwmprpc_cpe_methodResponse_soap_name_g) / sizeof(char *);
    for(i = 0; i < num; i++)
    {
        if(strcmp(cwmprpc_cpe_methodResponse_soap_name_g[i], in) == 0)
            return 0;   //说明匹配到了
    }
    
    return -2;
}

//匹配 acs 方法，例如 "InformResponse"
int cwmprpc_acs_methodResponse_soap_name_match(char *in)
{
    if(in == NULL)return -1;

    int i;
    int num;
    num = sizeof(cwmprpc_acs_methodResponse_soap_name_g) / sizeof(char *);
    for(i = 0; i < num; i++)
    {
        if(strcmp(cwmprpc_acs_methodResponse_soap_name_g[i], in) == 0)
            return 0;   //说明匹配到了
    }
    
    return -2;
}




/*==============================================================
                        测试
==============================================================*/
void cwmprpc_test()
{
//    dateTime date;
//    date.year = 1;
//    date.mouth = 1;
//    date.day = 1;
//    date.hour = 1;
//    date.minute = 1;
//    date.second = 1;
//
//       char buf64[64] = {0};
//    cwmprpc_dateTime2str(date, buf64, 64);
//
//    printf("--->%s\n", buf64);
//
//    dateTime date2;
//    cwmprpc_str2dateTime(buf64, &date2);
//    
//    printf("%04d-%02d-%02dT%02d:%02d:%02d\n",
//                date.year + 1, date.mouth + 1, date.day + 1,
//                date.hour, date.minute, date.second);

    printf("--->%d\n", cwmprpc_acs_method_soap_name_match("cwmp:GetRPCMethods"));        
}



















