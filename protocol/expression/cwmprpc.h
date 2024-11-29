


#ifndef _CWMPRPC_H_
#define _CWMPRPC_H_




#include "link.h"
/*==============================================================
                        其他数据类型
==============================================================*/
typedef unsigned int unsignedInt;
typedef unsigned char boolean;


/*--------------------------------------------------------------
dateTime 数据类型
    由 SOAP dateTime 类型定义的 ISO 8601 日期时间格式的子集。
    
    除非在该类型变量的定义中明确指出，否则所有时间都必须使用 UTC（协调世界时）表示。
    如果 CPE 无法获取绝对时间，应改为表示自启动以来的相对时间，其中启动时间假定为公
    元1年的1月1日，即 0001-01-01T00:00:00。例如，自启动以来2天3小时4分钟5秒将表示为
    0001-01-03T03:04:05。自启动以来的相对时间必须使用无时区表示法。任何年份小于1000
    的无时区值必须解释为自启动以来的相对时间。
    
    如果时间未知或不适用，必须使用以下表示“未知时间”的值：0001-01-01T00:00:00Z。
    除非在定义中明确指出，否则任何表示自启动以来相对时间之外的 dateTime 值（如上所述）
    必须使用带有时区的 UTC 表示法（即，必须包含“Z”、“-00:00”或“+00:00”作为时区后缀）。
--------------------------------------------------------------*/
typedef struct rpc_dateTime_t{
    unsigned int year;
    unsigned char mouth;
    unsigned char day;
    unsigned char hour;
    unsigned char minute;
    unsigned char second;
}dateTime;


/*==============================================================
                        A.3.1 Generic Methods
==============================================================*/

/*--------------------------------------------------------------
A.3.1.1 GetRPCMethods
获取方法列表；厂商可以自定义方法， Examples: X_012345_MyMethod, X_ACME_COM_MyMethod.
acs和cpe一般都支持 GetRPCMethods

1、 GetRPCMethods arguments
    空
2、 GetRPCMethodsResponse arguments
    2.1 MethodList
        类型：string(64)[]
        描述：字符串数组，如果是acs方法，那么一般为：
                "GetRPCMethods"
                "SetParameterValues"
                "GetParameterValues"
                "GetParameterNames"
                "SetParameterAttributes"
                "GetParameterAttributes"
                "AddObject"
                "DeleteObject"
                "Reboot"
                "Download"
                
               如果是cpe方法：
                "Inform"
                "GetRPCMethods"
                "TransferComplete"
3、模型
    在网页 https://cwmp-data-models.broadband-forum.org/cwmp-1-0.xsd
    搜索关键词 GetRPCMethods 、GetRPCMethodsResponse
    以后不再赘述
--------------------------------------------------------------*/
// GetRPCMethods
typedef struct rpc_GetRPCMethods_t{
    void *null;     //代表无参数
}rpc_GetRPCMethods_t;

// GetRPCMethodsResponse
typedef struct rpc_GetRPCMethodsResponse_t{
    link_obj_t *MethodList; //用链表存储， 数据类型为            char [64]
}rpc_GetRPCMethodsResponse_t;



/*==============================================================
                        A.3.2 CPE Methods
==============================================================*/

/*--------------------------------------------------------------
A.3.2.1 SetParameterValues
设置参数值

1、 SetParameterValues arguments
    1.1 ParameterList
        类型：ParameterValueStruct[]
        解释：设置的参数值；有一些注意事项，请参考文档
    1.2 ParameterKey
        类型：string(32)
        解释：要设置 ParameterKey 参数的值。只有当 SetParameterValues 
                成功完成时，CPE 必须将 ParameterKey 设置为该参数指定的
                值。如果 SetParameterValues 没有成功完成（意味着请求的
                参数值更改未生效），则 ParameterKey 的值不得修改。
                ParameterKey 为 ACS 提供了一种可靠且可扩展的手段来跟踪
                由 ACS 所做的更改。此参数的值由 ACS 自行决定，也可以为
                空。
2、关于类型： ParameterValueStruct
    2.1 Name
        类型：string(256)
        解释：参数名
    2.1 Value
        类型：anySimpleType
        解释：参数值；这个类型参考 “A.2.1 Data Types”，例如：
                <Value xsi:type="xsd:string">code12345</Value>

        
3、 SetParameterValuesResponse arguments
    3.1 Status
        类型：int[0:1]
        解释：0 = All Parameter changes have been validated and applied.
                1 = All Parameter changes have been validated and committed,
                but some or all are not yetapplied (for example, if a reboot 
                is required before the new values are applied).
--------------------------------------------------------------*/
// ParameterValueStruct
typedef struct rpc_ParameterValueStruct_t{
    char Name[256];
    void *Value;
    int valueLen;
}ParameterValueStruct;

// SetParameterValues
typedef struct rpc_SetParameterValues_t{
    //因为是未定长度数组，所以用链表表示；数组成员类型 ParameterValueStruct
    link_obj_t *ParameterList;  
    char ParameterKey[32];
}rpc_SetParameterValues_t;

// SetParameterValuesResponse
typedef struct rpc_SetParameterValuesResponse_t{
    int Status;     //注意数值范围 0-1
}rpc_SetParameterValuesResponse_t;




/*--------------------------------------------------------------
A.3.2.2 GetParameterValues

得到参数值

1、 GetParameterValues arguments
    1.1 ParameterNames
        类型：string(256)[]
        解释：字符串数组，每一个字符串代表方法里的一个请求的参数    
                Below is an example of a full Parameter name:
                    Device.DeviceInfo.SerialNumber
                Below is an example of a Partial Path Name:
                    Device.DeviceInfo.       
2、 GetParameterValuesResponse arguments
    3.1 ParameterList
        类型：ParameterValueStruct[]；该类型在 A.3.2.1 SetParameterValues 中有所描述
        解释：存储键值对数组，代表了回应的参数名和参数值
--------------------------------------------------------------*/
// GetParameterValues
typedef struct rpc_GetParameterValues_t{
    link_obj_t *ParameterNames;     //链表成员数据的类型 ： char [256]
}rpc_GetParameterValues_t;

// GetParameterValuesResponse
typedef struct rpc_GetParameterValuesResponse_t{
    link_obj_t *ParameterList;  //链表成员数据类型 ： ParameterValueStruct_t
}rpc_GetParameterValuesResponse_t;




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
信息通告

1、 Inform arguments
    1.1 DeviceId
        类型： DeviceIdStruct
        描述：描述设备的结构体（该结构体接下来会有解释）。
    1.2 Event
        类型：EventStruct[64]
        描述：通告里面的“事件”描述（详细内容在 3.7.1.5 章节），用于指导cpe请求建立与
              acs的会话（该结构体接下来会有解释）。
    1.3 MaxEnvelopes
        类型：unsignedInt
        描述：在 《tr-069-1-6-1》指定的 cwmp-1-4.xsd 版本下，该参数必须忽略，并且设置
                为1（之前的版本支持多个信封）。
    1.4 CurrentTime
        类型：dateTime
        描述：必须是 local time-zone offset from UTC（详细见 A.2.1 Data Types），起始
                0001-01-01T00:00:00，例子2 days, 3 hours, 4 minutes and 5 seconds since
                boot ： 0001-01-03T03:04:05；未知时间则是 0001-01-01T00:00:00Z。
    1.5 RetryCount
        类型：unsignedInt
        描述：试图重建会话的次数
    1.6 ParameterList
        类型：ParameterValueStruct[]
        描述：（原文翻译如下）        
                如表17中规定的那样，这是一个名称-值对数组。此参数必须包含以下参数的名称-值对：
                - 所有由 ACS 将 Notification 属性设置为“主动通知”或“被动通知”的参数，这些参数的值自上
                    次成功的非心跳 Inform 通知以来已被除 ACS 之外的实体修改过（包括 CPE 自身所做的修改）。
                - 所有在相应数据模型中定义为需要强制主动通知的参数（无论 Notification 属性的值如何），
                    这些参数的值自上次成功的非心跳 Inform 通知以来已被除 ACS 之外的实体修改过（包括 CPE 自
                    身所做的修改）。
                - 所有在相应数据模型中定义为每次 Inform 都必需的参数。
                    如果某个参数自上次成功的 Inform 通知以来多次发生变化，则该参数只需列出一次，并且只提供
                    最近的值。在这种情况下，即使参数的值已恢复到上次成功的 Inform 通知时的值，也必须将其包
                    含在 ParameterList 中。
                每当 CPE 重启，或者 ACS URL 被修改时，CPE 可以在此时清除因值变化而待通知的参数记录（尽
                管，CPE 必须保留所有参数的 Notification 属性值）。如果 CPE 清除了因值变化而待通知的参数
                记录，它必须同时丢弃对应的“4 值变化”事件。
                如果 ParameterList 中列出的至少一个参数的值自上次成功的非心跳 Inform 通知以来已被除 ACS
                之外的实体修改过，Inform 消息必须包含 EventCode “4 值变化”。这包括因每次 Inform 都必需而
                列出的任何参数的值变化。否则，Inform 消息不得包含 EventCode “4 值变化”。
                如果 Inform 消息确实包含“4 值变化”EventCode，则 ParameterList 必须仅包含满足上述三个标准
                之一的参数。如果 Inform 消息不包含“4 值变化”EventCode，则 ParameterList 可以根据 CPE 的
                判断包含额外的参数。
                需要注意的是，如果 Inform 消息包含“8 诊断完成”EventCode，则 CPE 不需要在 ParameterList 
                中包含与相应诊断结果相关的任何参数。如上所述，如果 Inform 消息中同时存在“4 值变化”EventCode，
                则 ParameterList 必须仅包含满足上述三个标准之一的参数。
    
2、 InformResponse arguments
    2.1 MaxEnvelopes
        类型：unsignedInt
        描述：每条 http 消息报文负荷可包含的 soap信封个数，在 《tr-069-1-6-1》指定的 
                cwmp-1-4.xsd 版本下要求为1，并且忽略
3、 DeviceIdStruct definition
    3.1 Manufacturer
        类型： string(64)
        描述：设备制造商（仅用于显示），该值必须与 DeviceInfo,Manufacturer 参数的值相同。
    3.2 OUI
        类型：string(6)
        描述：来自于设备厂商的组织唯一标识符，参见 http://standards.ieee.org/faqs/OUI.html
    3.3 ProductClass
        类型：string(64)
        描述：用于辨识产品的类别
    3.4 SerialNumber
        类型：string(64)
        描述：独一无二的序列号，用于区别设备
4、 EventStruct definition
    4.1 EventCode
        类型：string(64)
        描述：事件码，例如 "0 BOOTSTRAP"，参见 Section 3.7.1.5,
    4.1 CommandKey
        类型：string(64)
        描述：指定的命令关键字，例如 ScheduleInform method (EventCode = “M ScheduleInform”)        
--------------------------------------------------------------*/
// DeviceIdStruct
typedef struct rpc_DeviceIdStruct_t{
    char Manufacturer[64];
    char OUI[6];
    char ProductClass[64];
    char SerialNumber[64];
}DeviceIdStruct;

// EventStruct 
typedef struct rpc_EventStruct_t{
    char EventCode[64];
    char CommandKey[64];
}EventStruct;


// Inform
typedef struct rpc_Inform_t{
    DeviceIdStruct DeviceId;        
    EventStruct Event[64];
    int eventNum;   //指定用到的 Event 的个数
    unsignedInt MaxEnvelopes;         
    dateTime CurrentTime;
    unsignedInt RetryCount;
    link_obj_t * ParameterList;     //数据类型为 ParameterValueStruct
}rpc_Inform_t;

// InformResponse
typedef struct rpc_InformResponse_t{
    unsignedInt MaxEnvelopes;
}rpc_InformResponse_t;



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

//错误码 静态数据
#define RPC_FAULT_DESCRIPTION_STRING_LENGTH 256
typedef enum rpc_fault_code_type_e{
    rpc_faultCodeType_Server,
    rpc_faultCodeType_Client
}rpc_fault_code_type_e;

typedef struct rpc_fault_code_t{
    int codeNum;
    rpc_fault_code_type_e type;
    char describ[RPC_FAULT_DESCRIPTION_STRING_LENGTH + 8];   // 解释 description    
}rpc_fault_code_t;






/*--------------------------------------------------------------
错误码的数据结构
<xs:element name="Fault">
    <xs:complexType>
        <xs:sequence>
            <xs:element name="FaultCode">
                <xs:simpleType>
                    <xs:union>
                        <xs:simpleType>
                            <xs:restriction base="tns:CPEFaultCodeType"/>
                        </xs:simpleType>
                        <xs:simpleType>
                            <xs:restriction base="tns:CPEVendorFaultCodeType"/>
                        </xs:simpleType>
                        <xs:simpleType>
                            <xs:restriction base="tns:ACSFaultCodeType"/>
                        </xs:simpleType>
                        <xs:simpleType>
                            <xs:restriction base="tns:ACSVendorFaultCodeType"/>
                        </xs:simpleType>
                    </xs:union>
                </xs:simpleType>
            </xs:element>
            <xs:element name="FaultString" type="xs:string" minOccurs="0"/>
            <xs:element name="SetParameterValuesFault" minOccurs="0" maxOccurs="unbounded">
                <xs:complexType>
                    <xs:sequence>
                        <xs:element name="ParameterName" type="xs:string"/>
                        <xs:element name="FaultCode">
                            <xs:simpleType>
                                <xs:union>
                                    <xs:simpleType>
                                        <xs:restriction base="tns:CPEFaultCodeType"/>
                                    </xs:simpleType>
                                    <xs:simpleType>
                                        <xs:restriction base="tns:CPEVendorFaultCodeType"/>
                                    </xs:simpleType>
                                </xs:union>
                            </xs:simpleType>
                        </xs:element>
                        <xs:element name="FaultString" type="xs:string" minOccurs="0"/>
                    </xs:sequence>
                </xs:complexType>
            </xs:element>
        </xs:sequence>
    </xs:complexType>
</xs:element>
--------------------------------------------------------------*/
#define RPC_FAULT_STRING_LENGTH 512
#define RPC_FAULT_PARAMETER_NAME_STRING_LENGTH 128
typedef struct rpc_SetParameterValuesFault_t{
    char ParameterName[RPC_FAULT_PARAMETER_NAME_STRING_LENGTH];
    int FaultCode;  
    char FaultString[RPC_FAULT_STRING_LENGTH + 8]; 
}rpc_SetParameterValuesFault_t;
    

typedef struct rpc_fault_t{
    int FaultCode;  //注意这里有简化, 错误码有取值范围CPEFaultCodeType、
                    //CPEVendorFaultCodeType、ACSFaultCodeType
                    //ACSVendorFaultCodeType，可能需要枚举
    char FaultString[RPC_FAULT_STRING_LENGTH + 8]; 
    rpc_SetParameterValuesFault_t SetParameterValuesFault;   
}rpc_fault_t;

typedef struct rpc_soap_fault_t{
    char FaultCode[32];  // soap:Fault 值为 Client 或者 Server
    char FaultString[RPC_FAULT_STRING_LENGTH + 8];  
}rpc_soap_fault_t;


/*==============================================================
                        全局变量
==============================================================*/
extern const rpc_fault_code_t cpe_fault_code[];
extern const rpc_fault_code_t acs_fault_code[];

extern const char *cwmprpc_cpe_method_g[];
extern const char *cwmprpc_cpe_method_soap_name_g[];

/*==============================================================
                        接口
==============================================================*/
void cwmprpc_test();

int cwmprpc_dateTime2str(dateTime date, char *out, int outLen);
int cwmprpc_str2dateTime(char *in, dateTime *date);

int cwmprpc_cpe_method_soap_name_match(char *in);
int cwmprpc_acs_method_soap_name_match(char *in);
int cwmprpc_cpe_methodResponse_soap_name_match(char *in);
int cwmprpc_acs_methodResponse_soap_name_match(char *in);



#endif

