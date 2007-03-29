//
// This file is part of the aMule Project.
//
// Copyright (c) 2006 Marcelo Roberto Jimenez ( phoenix@amule.org )
// Copyright (c) 2006 aMule Team ( admin@amule.org / http://www.amule.org )
//
// Any parts of this program derived from the xMule, lMule or eMule project,
// or contributed by third-party developers are copyrighted by their
// respective authors.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301, USA
//


// This define must not conflict with the one in the standard header
#ifndef AMULE_UPNP_H
#define AMULE_UPNP_H


#include <map>
#include <string>
#include <sstream>
#include <vector>

#include <upnp/upnp.h>
#include <upnp/upnptools.h>
#include <upnp/ixml.h>

#include <QMutex>


#ifdef UPNP_C
	std::string stdEmptyString;
#else // UPNP_C
	extern std::string stdEmptyString;
#endif // UPNP_C

#include <QWaitCondition>

/**
 * Case insensitive std::string comparison
 */
bool stdStringIsEqualCI(
	const std::string &s1,
	const std::string &s2);


class CUPnPPortMapping
{
private:
	std::string m_port;
	std::string m_protocol;
	std::string m_enabled;
	std::string m_description;
	std::string m_key;

public:
	CUPnPPortMapping(
		int port = 0,
		const std::string &protocol = stdEmptyString,
		bool enabled = false,
		const std::string &description = stdEmptyString);
	~CUPnPPortMapping() {}

	const std::string &getPort() const
		{ return m_port; }
	const std::string &getProtocol() const
		{ return m_protocol; }
	const std::string &getEnabled() const
		{ return m_enabled; }
	const std::string &getDescription() const
		{ return m_description; }
	const std::string &getKey() const
		{ return m_key; }
};

class CUPnPControlPoint;


class CUPnPLib
{
public:
	static const std::string &UPNP_ROOT_DEVICE;
	static const std::string &UPNP_DEVICE_IGW;
	static const std::string &UPNP_DEVICE_WAN;
	static const std::string &UPNP_DEVICE_WAN_CONNECTION;
	static const std::string &UPNP_DEVICE_LAN;
	static const std::string &UPNP_SERVICE_LAYER3_FORWARDING;
	static const std::string &UPNP_SERVICE_WAN_COMMON_INTERFACE_CONFIG;
	static const std::string &UPNP_SERVICE_WAN_IP_CONNECTION;
	static const std::string &UPNP_SERVICE_WAN_PPP_CONNECTION;
	CUPnPControlPoint &m_ctrlPoint;

public:
	CUPnPLib(CUPnPControlPoint &ctrlPoint);
	~CUPnPLib() {}

	// Convenience function so we don't have to write explicit calls
	// to char2unicode every time
	std::string GetUPnPErrorMessage(int code) const;

	// Convenience function to avoid repetitive processing of error
	// messages
	std::string processUPnPErrorMessage(
		const std::string &messsage,
		int code,
		const DOMString errorString,
		IXML_Document *doc) const;

	// Processing response to actions
	void ProcessActionResponse(
		IXML_Document *RespDoc,
		const std::string &actionName) const;

	// IXML_Element
	IXML_Element *Element_GetRootElement(
		IXML_Document *doc) const;
	IXML_Element *Element_GetFirstChild(
		IXML_Element *parent) const;
	IXML_Element *Element_GetNextSibling(
		IXML_Element *child) const;
	const DOMString Element_GetTag(
		IXML_Element *element) const;
	const std::string Element_GetTextValue(
		IXML_Element *element) const;
	const std::string Element_GetChildValueByTag(
		IXML_Element *element,
		const DOMString tag) const;
	IXML_Element *Element_GetFirstChildByTag(
		IXML_Element *element,
		const DOMString tag) const;
	IXML_Element *Element_GetNextSiblingByTag(
		IXML_Element *element,
		const DOMString tag) const;
	const std::string Element_GetAttributeByTag(
		IXML_Element *element,
		const DOMString tag) const;

	// ixml api
	IXML_Node *(*m_ixmlNode_getFirstChild)(IXML_Node *nodeptr);
	IXML_Node *(*m_ixmlNode_getNextSibling)(IXML_Node *nodeptr);
	const DOMString (*m_ixmlNode_getNodeName)(IXML_Node *nodeptr);
	const DOMString (*m_ixmlNode_getNodeValue)(IXML_Node *nodeptr);
	IXML_NamedNodeMap *(*m_ixmlNode_getAttributes)(IXML_Node *nodeptr);
	void (*m_ixmlDocument_free)(IXML_Document *doc);
	IXML_Node *(*m_ixmlNamedNodeMap_getNamedItem)(
		IXML_NamedNodeMap *nnMap, const DOMString name);
	void (*m_ixmlNamedNodeMap_free)(IXML_NamedNodeMap *nnMap);
};


class CUPnPControlPoint;

/*
 * Even though we can retrieve the upnpLib handler from the upnpControlPoint,
 * we must pass it separetly at this point, because the class CUPnPControlPoint
 * must be declared after.
 *
 * CUPnPLib can only be removed from the constructor once we agree to link to
 * UPnPLib explicitly, making this dlopen() stuff unnecessary.
 */
template <typename T, char const *XML_ELEMENT_NAME, char const *XML_LIST_NAME>
class CXML_List : public std::map<const std::string, T *>
{
public:
	CXML_List(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *parent,
		const std::string &url);
	~CXML_List();
};


template <typename T, char const *XML_ELEMENT_NAME, char const *XML_LIST_NAME>
CXML_List<T, XML_ELEMENT_NAME, XML_LIST_NAME>::CXML_List(
	const CUPnPControlPoint &upnpControlPoint,
	CUPnPLib &upnpLib,
	IXML_Element *parent,
	const std::string &url)
{
	IXML_Element *elementList =
		upnpLib.Element_GetFirstChildByTag(parent, XML_LIST_NAME);
	unsigned int i = 0;
	for (	IXML_Element *element = upnpLib.Element_GetFirstChildByTag(elementList, XML_ELEMENT_NAME);
		element;
		element = upnpLib.Element_GetNextSiblingByTag(element, XML_ELEMENT_NAME)) {
		// Add a new element to the element list
		T *upnpElement = new T(upnpControlPoint, upnpLib, element, url);
		(*this)[upnpElement->GetKey()] = upnpElement;
		++i;
	}
	std::ostringstream msg;
	msg << "\n    " << XML_LIST_NAME << ": " <<
		i << " " << XML_ELEMENT_NAME << "s.";
	//AddDebugLogLineM(false, logUPnP, msg);
}


template <typename T, char const *XML_ELEMENT_NAME, char const *XML_LIST_NAME>
CXML_List<T, XML_ELEMENT_NAME, XML_LIST_NAME>::~CXML_List()
{
	typename CXML_List<T, XML_ELEMENT_NAME, XML_LIST_NAME>::iterator it;
	for(it = this->begin(); it != this->end(); ++it) {
		delete (*it).second;
	}
}

#ifdef UPNP_C
	char s_argument[] = "argument";
	char s_argumentList[] = "argumentList";
	char s_action[] = "action";
	char s_actionList[] = "actionList";
	char s_allowedValue[] = "allowedValue";
	char s_allowedValueList[] = "allowedValueList";
	char s_stateVariable[] = "stateVariable";
	char s_serviceStateTable[] = "serviceStateTable";
	char s_service[] = "service";
	char s_serviceList[] = "serviceList";
	char s_device[] = "device";
	char s_deviceList[] = "deviceList";
#else // UPNP_C
	extern char s_argument[];
	extern char s_argumentList[];
	extern char s_action[];
	extern char s_actionList[];
	extern char s_allowedValue[];
	extern char s_allowedValueList[];
	extern char s_stateVariable[];
	extern char s_serviceStateTable[];
	extern char s_service[];
	extern char s_serviceList[];
	extern char s_device[];
	extern char s_deviceList[];
#endif // UPNP_C


class CUPnPArgument;
typedef CXML_List<CUPnPArgument, s_argument, s_argumentList> ArgumentList;
class CUPnPAction;
typedef CXML_List<CUPnPAction, s_action, s_actionList> ActionList;
class CUPnPStateVariable;
typedef CXML_List<CUPnPStateVariable, s_stateVariable, s_serviceStateTable> ServiceStateTable;
class CUPnPAllowedValue;
typedef CXML_List<CUPnPAllowedValue, s_allowedValue, s_allowedValueList> AllowedValueList;
class CUPnPService;
typedef CXML_List<CUPnPService, s_service, s_serviceList> ServiceList;
class CUPnPDevice;
typedef CXML_List<CUPnPDevice, s_device, s_deviceList> DeviceList;


class CUPnPError
{
private:
	IXML_Element *m_root;
	const std::string m_ErrorCode;
	const std::string m_ErrorDescription;
public:
	CUPnPError(
		const CUPnPLib &upnpLib,
		IXML_Document *errorDoc);
	~CUPnPError() {}
	const std::string &getErrorCode() const
		{ return m_ErrorCode; }
	const std::string &getErrorDescription() const
		{ return m_ErrorDescription; }
};


class CUPnPArgument
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	const std::string m_name;
	const std::string m_direction;
	bool m_retval;
	const std::string m_relatedStateVariable;

public:
	CUPnPArgument(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *argument,
		const std::string &SCPDURL);
	~CUPnPArgument() {}
	const std::string &GetName() const
		{ return m_name; }
	const std::string &GetDirection() const
		{ return m_direction; }
	bool GetRetVal() const
		{ return m_retval; }
	const std::string &GetRelatedStateVariable() const
		{ return m_relatedStateVariable; }
	const std::string &GetKey() const
		{ return m_name; }
};



class CUPnPAction
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	ArgumentList m_ArgumentList;
	const std::string m_name;

public:
	CUPnPAction(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *action,
		const std::string &SCPDURL);
	~CUPnPAction() {}
	const std::string &GetName() const
		{ return m_name; }
	const std::string &GetKey() const
		{ return m_name; }
	const ArgumentList &GetArgumentList() const
		{ return m_ArgumentList; }
};


class CUPnPAllowedValue
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	const std::string m_allowedValue;

public:
	CUPnPAllowedValue(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *allowedValue,
		const std::string &SCPDURL);
	~CUPnPAllowedValue() {}
	const std::string &GetAllowedValue() const
		{ return m_allowedValue; }
	const std::string &GetKey() const
		{ return m_allowedValue; }
};


class CUPnPStateVariable
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	AllowedValueList m_AllowedValueList;
	const std::string m_name;
	const std::string m_dataType;
	const std::string m_defaultValue;
	const std::string m_sendEvents;

public:
	CUPnPStateVariable(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *stateVariable,
		const std::string &URLBase);
	~CUPnPStateVariable() {}
	const std::string &GetNname() const
		{ return m_name; }
	const std::string &GetDataType() const
		{ return m_dataType; }
	const std::string &GetDefaultValue() const
		{ return m_defaultValue; }
	const std::string &GetKey() const
		{ return m_name; }
	const AllowedValueList &GetAllowedValueList() const
		{ return m_AllowedValueList; }
};


class CUPnPSCPD
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	ActionList m_ActionList;
	ServiceStateTable m_ServiceStateTable;
	const std::string m_SCPDURL;

public:
	CUPnPSCPD(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *scpd,
		const std::string &SCPDURL);
	~CUPnPSCPD() {}
	const ActionList &GetActionList() const
		{ return m_ActionList; }
	const ServiceStateTable &GetServiceStateTable() const
		{ return m_ServiceStateTable; }
};


class CUPnPArgumentValue
{
private:
	std::string m_argument;
	std::string m_value;

public:
	CUPnPArgumentValue();
	CUPnPArgumentValue(const std::string &argument, const std::string &value);
	~CUPnPArgumentValue() {}

	const std::string &GetArgument() const	{ return m_argument; }
	const std::string &GetValue() const	{ return m_value; }
	const std::string &SetArgument(const std::string& argument)	{ return m_argument = argument; }
	const std::string &SetValue(const std::string &value)		{ return m_value = value; }
};


class CUPnPService
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	CUPnPLib &m_upnpLib;
	const std::string m_serviceType;
	const std::string m_serviceId;
	const std::string m_SCPDURL;
	const std::string m_controlURL;
	const std::string m_eventSubURL;
	std::string m_absSCPDURL;
	std::string m_absControlURL;
	std::string m_absEventSubURL;
	int m_timeout;
	Upnp_SID m_SID;
	std::auto_ptr<CUPnPSCPD> m_SCPD;

public:
	CUPnPService(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *service,
		const std::string &URLBase);
	~CUPnPService();

	const std::string &GetServiceType() const
		{ return m_serviceType; }
	const std::string &GetServiceId() const
		{ return m_serviceId; }
	const std::string &GetSCPDURL() const
		{ return m_SCPDURL; }
	const std::string &GetAbsSCPDURL() const
		{ return m_absSCPDURL; }
	const std::string &GetControlURL() const
		{ return m_controlURL; }
	const std::string &GetEventSubURL() const
		{ return m_eventSubURL; }
	const std::string &GetAbsControlURL() const
		{ return m_absControlURL; }
	const std::string &GetAbsEventSubURL() const
		{ return m_absEventSubURL; }
	int GetTimeout() const
		{ return m_timeout; }
	void SetTimeout(int t)
		{ m_timeout = t; }
	int *GetTimeoutAddr()
		{ return &m_timeout; }
	char *GetSID()
		{ return m_SID; }
	void SetSID(const char *s)
		{ memcpy(m_SID, s, sizeof(Upnp_SID)); }
	const std::string &GetKey() const
		{ return m_serviceId; }
	bool IsSubscribed() const
		{ return m_SCPD.get() != NULL; }
	void SetSCPD(CUPnPSCPD *SCPD)
		{ m_SCPD.reset(SCPD); }

	bool Execute(
		const std::string &ActionName,
		const std::vector<CUPnPArgumentValue> &ArgValue) const;
	const std::string GetStateVariable(
		const std::string &stateVariableName) const;
};


class CUPnPDevice
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;

	// Please, lock these lists before use
	DeviceList m_DeviceList;
	ServiceList m_ServiceList;

	const std::string m_deviceType;
	const std::string m_friendlyName;
	const std::string m_manufacturer;
	const std::string m_manufacturerURL;
	const std::string m_modelDescription;
	const std::string m_modelName;
	const std::string m_modelNumber;
	const std::string m_modelURL;
	const std::string m_serialNumber;
	const std::string m_UDN;
	const std::string m_UPC;
	std::string m_presentationURL;

public:
	CUPnPDevice(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *device,
		const std::string &URLBase);
	~CUPnPDevice() {}

	const std::string &GetUDN() const
		{ return m_UDN; }
	const std::string &GetDeviceType() const
		{ return m_deviceType; }
	const std::string &GetFriendlyName() const
		{ return m_friendlyName; }
	const std::string &GetPresentationURL() const
		{ return m_presentationURL; }
	const std::string &GetKey() const
		{ return m_UDN; }
};


class CUPnPRootDevice : public CUPnPDevice
{
private:
	const CUPnPControlPoint &m_UPnPControlPoint;
	const std::string m_URLBase;
	const std::string m_location;
	int m_expires;

public:
	CUPnPRootDevice(
		const CUPnPControlPoint &upnpControlPoint,
		CUPnPLib &upnpLib,
		IXML_Element *rootDevice,
		const std::string &OriginalURLBase,
		const std::string &FixedURLBase,
		const char *location,
		int expires);
	~CUPnPRootDevice() {}

	const std::string &GetURLBase() const
		{ return m_URLBase; }
	const std::string &GetLocation() const
		{ return m_location; }
	int GetExpires() const
		{ return m_expires; }
	void SetExpires(int expires)
		{ m_expires = expires; }
};


typedef std::map<const std::string, CUPnPRootDevice *> RootDeviceMap;
typedef std::map<const std::string, CUPnPService *> ServiceMap;
typedef std::map<const std::string, CUPnPPortMapping> PortMappingMap;


class CUPnPControlPoint
{
private:
	static CUPnPControlPoint *s_CtrlPoint;
	// upnp stuff
	CUPnPLib m_upnpLib;
	UpnpClient_Handle m_UPnPClientHandle;
	RootDeviceMap m_RootDeviceMap;
	ServiceMap m_ServiceMap;
	PortMappingMap m_ActivePortMappingsMap;
	QMutex m_RootDeviceListMutex;
	bool m_IGWDeviceDetected;
	CUPnPService *m_WanService;
	QMutex m_WaitForSearchTimeout;
        QWaitCondition searchTimeoutCondition;
        unsigned short m_UPnPPort;

public:
	CUPnPControlPoint(unsigned short udpPort);
	~CUPnPControlPoint();
	void Subscribe(CUPnPService &service);
	void Unsubscribe(CUPnPService &service);
	bool AddPortMappings(
		std::vector<CUPnPPortMapping> &upnpPortMapping);
	bool DeletePortMappings(
		std::vector<CUPnPPortMapping> &upnpPortMapping);
        unsigned short getUPnPPort() const{
          return m_UPnPPort;
        }

	UpnpClient_Handle GetUPnPClientHandle()	const
		{ return m_UPnPClientHandle; }

	bool GetIGWDeviceDetected() const
		{ return m_IGWDeviceDetected; }
	void SetIGWDeviceDetected(bool b)
		{ m_IGWDeviceDetected = b; }
	bool WanServiceDetected() const
		{ return !m_ServiceMap.empty(); }
	void SetWanService(CUPnPService *service)
		{ m_WanService = service; }

	// Callback function
	static int Callback(
		Upnp_EventType EventType,
		void* Event,
		void* Cookie);

private:
	void OnEventReceived(
		const std::string &Sid,
		int EventKey,
		IXML_Document *ChangedVariables);
	void AddRootDevice(
		IXML_Element *rootDevice,
		const std::string &urlBase,
		const char *location,
		int expires);
	void RemoveRootDevice(
		const char *udn);
	void RefreshPortMappings();
	bool PrivateAddPortMapping(
		CUPnPPortMapping &upnpPortMapping);
	bool PrivateDeletePortMapping(
		CUPnPPortMapping &upnpPortMapping);
};


#endif /* AMULE_UPNP_H */

// File_checked_for_headers
