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


#define UPNP_C


#include "UPnP.h"

#include <QMutexLocker>
#include <QString>


#ifdef __GNUC__
	#if __GNUC__ >= 4
		#define REINTERPRET_CAST(x) reinterpret_cast<x>
	#endif
#endif
#ifndef REINTERPRET_CAST
	// Let's hope that function pointers are equal in size to data pointers
	#define REINTERPRET_CAST(x) (x)
#endif

CUPnPPortMapping::CUPnPPortMapping(
	int port,
	const std::string &protocol,
	bool enabled,
	const std::string &description)
:
m_port(),
m_protocol(protocol),
m_enabled(enabled ? "1" : "0"),
m_description(description),
m_key()
{
	std::ostringstream oss;
	oss << port;
	m_port = oss.str();
	m_key = m_protocol + m_port;
}


const std::string &CUPnPLib::UPNP_ROOT_DEVICE =
	"upnp:rootdevice";

const std::string &CUPnPLib::UPNP_DEVICE_IGW =
	"urn:schemas-upnp-org:device:InternetGatewayDevice:1";
const std::string &CUPnPLib::UPNP_DEVICE_WAN =
	"urn:schemas-upnp-org:device:WANDevice:1";
const std::string &CUPnPLib::UPNP_DEVICE_WAN_CONNECTION =
	"urn:schemas-upnp-org:device:WANConnectionDevice:1";
const std::string &CUPnPLib::UPNP_DEVICE_LAN =
	"urn:schemas-upnp-org:device:LANDevice:1";

const std::string &CUPnPLib::UPNP_SERVICE_LAYER3_FORWARDING =
	"urn:schemas-upnp-org:service:Layer3Forwarding:1";
const std::string &CUPnPLib::UPNP_SERVICE_WAN_COMMON_INTERFACE_CONFIG =
	"urn:schemas-upnp-org:service:WANCommonInterfaceConfig:1";
const std::string &CUPnPLib::UPNP_SERVICE_WAN_IP_CONNECTION =
	"urn:schemas-upnp-org:service:WANIPConnection:1";
const std::string &CUPnPLib::UPNP_SERVICE_WAN_PPP_CONNECTION =
	"urn:schemas-upnp-org:service:WANPPPConnection:1";

CUPnPLib::CUPnPLib(CUPnPControlPoint &ctrlPoint) : m_ctrlPoint(ctrlPoint) {}


std::string CUPnPLib::GetUPnPErrorMessage(int code) const
{
	return UpnpGetErrorMessage(code);
}


std::string CUPnPLib::processUPnPErrorMessage(
	const std::string &messsage,
	int errorCode,
	const DOMString errorString,
	IXML_Document *doc) const
{
	std::ostringstream msg;
	if (errorString == NULL || *errorString == 0) {
		errorString = "Not available";
	}
	if (errorCode > 0) {
		msg << "Error: " <<
			messsage <<
			": Error code :'";
		if (doc) {
			CUPnPError e(*this, doc);
			msg << e.getErrorCode() <<
				"', Error description :'" <<
				e.getErrorDescription() <<
				"'.";
		} else {
			msg << errorCode <<
				"', Error description :'" <<
				errorString <<
				"'.";
		}
                qDebug("UPnP: %s", msg.str().c_str());
	} else {
		msg << "Error: " <<
			messsage <<
			": UPnP SDK error: " <<
			GetUPnPErrorMessage(errorCode) <<
			" (" << errorCode << ").";
                qDebug("UPnP: %s", msg.str().c_str());
	}

	return msg.str();
}


void CUPnPLib::ProcessActionResponse(
	IXML_Document *RespDoc,
	const std::string &actionName) const
{
	std::ostringstream msg;
	msg << "Response: ";
	IXML_Element *root = Element_GetRootElement(RespDoc);
	IXML_Element *child = Element_GetFirstChild(root);
	if (child) {
		while (child) {
			const DOMString childTag = Element_GetTag(child);
			std::string childValue = Element_GetTextValue(child);
			msg << "\n    " <<
				childTag << "='" <<
				childValue << "'";
			child = Element_GetNextSibling(child);
		}
	} else {
		msg << "\n    Empty response for action '" <<
			actionName << "'.";
	}
        qDebug("UPnP: %s", msg.str().c_str());
}


/**
 *  This function returns the root node of a given document.
 */
IXML_Element *CUPnPLib::Element_GetRootElement(
	IXML_Document *doc) const
{
	IXML_Element *root = REINTERPRET_CAST(IXML_Element *)(
		ixmlNode_getFirstChild(
			REINTERPRET_CAST(IXML_Node *)(doc)));

	return root;
}


/**
 *  This function returns the first child of a given element.
 */
IXML_Element *CUPnPLib::Element_GetFirstChild(
	IXML_Element *parent) const
{
	IXML_Node *node = REINTERPRET_CAST(IXML_Node *)(parent);
	IXML_Node *child = ixmlNode_getFirstChild(node);

	return REINTERPRET_CAST(IXML_Element *)(child);
}


/**
 *  This function returns the next sibling of a given child.
 */
IXML_Element *CUPnPLib::Element_GetNextSibling(
	IXML_Element *child) const
{
	IXML_Node *node = REINTERPRET_CAST(IXML_Node *)(child);
	IXML_Node *sibling = ixmlNode_getNextSibling(node);

	return REINTERPRET_CAST(IXML_Element *)(sibling);
}


/**
 *  This function returns the element tag (name)
 */
const DOMString CUPnPLib::Element_GetTag(
	IXML_Element *element) const
{
	IXML_Node *node = REINTERPRET_CAST(IXML_Node *)(element);
	const DOMString tag = ixmlNode_getNodeName(node);

	return tag;
}


/**
 *  This function returns the TEXT node value of the current node.
 */
const std::string CUPnPLib::Element_GetTextValue(
	IXML_Element *element) const
{
	if (!element) {
		return stdEmptyString;
	}
	IXML_Node *text = ixmlNode_getFirstChild(
		REINTERPRET_CAST(IXML_Node *)(element));
	const DOMString s = ixmlNode_getNodeValue(text);
	std::string ret;
	if (s) {
		ret = s;
	}

	return ret;
}


/**
 *  This function returns the TEXT node value of the first child matching tag.
 */
const std::string CUPnPLib::Element_GetChildValueByTag(
	IXML_Element *element,
	const DOMString tag) const
{
	IXML_Element *child =
		Element_GetFirstChildByTag(element, tag);

	return Element_GetTextValue(child);
}


/**
 * Returns the first child element that matches the requested tag or
 * NULL if not found.
 */
IXML_Element *CUPnPLib::Element_GetFirstChildByTag(
	IXML_Element *element,
	const DOMString tag) const
{
	if (!element || !tag) {
		return NULL;
	}

	IXML_Node *node = REINTERPRET_CAST(IXML_Node *)(element);
	IXML_Node *child = ixmlNode_getFirstChild(node);
	const DOMString childTag = ixmlNode_getNodeName(child);
	while(child && childTag && strcmp(tag, childTag)) {
		child = ixmlNode_getNextSibling(child);
		childTag = ixmlNode_getNodeName(child);
	}

	return REINTERPRET_CAST(IXML_Element *)(child);
}


/**
 * Returns the next sibling element that matches the requested tag. Should be
 * used with the return value of Element_GetFirstChildByTag().
 */
IXML_Element *CUPnPLib::Element_GetNextSiblingByTag(
	IXML_Element *element, const DOMString tag) const
{
	if (!element || !tag) {
		return NULL;
	}

	IXML_Node *child = REINTERPRET_CAST(IXML_Node *)(element);
	const DOMString childTag = NULL;
	do {
		child = ixmlNode_getNextSibling(child);
		childTag = ixmlNode_getNodeName(child);
	} while(child && childTag && strcmp(tag, childTag));

	return REINTERPRET_CAST(IXML_Element *)(child);
}


const std::string CUPnPLib::Element_GetAttributeByTag(
	IXML_Element *element, const DOMString tag) const
{
	IXML_NamedNodeMap *NamedNodeMap = ixmlNode_getAttributes(
		REINTERPRET_CAST(IXML_Node *)(element));
	IXML_Node *attribute = ixmlNamedNodeMap_getNamedItem(NamedNodeMap, (char*)tag);
	const DOMString s = ixmlNode_getNodeValue(attribute);
	std::string ret;
	if (s) {
		ret = s;
	}
	ixmlNamedNodeMap_free(NamedNodeMap);

	return ret;
}


CUPnPError::CUPnPError(
	const CUPnPLib &upnpLib,
	IXML_Document *errorDoc)
:
m_root            (upnpLib.Element_GetRootElement(errorDoc)),
m_ErrorCode       (upnpLib.Element_GetChildValueByTag(m_root, "errorCode")),
m_ErrorDescription(upnpLib.Element_GetChildValueByTag(m_root, "errorDescription"))
{
}


CUPnPArgument::CUPnPArgument(
	const CUPnPControlPoint &upnpControlPoint,
	CUPnPLib &upnpLib,
	IXML_Element *argument,
	const std::string&)
:
m_UPnPControlPoint(upnpControlPoint),
m_name                (upnpLib.Element_GetChildValueByTag(argument, "name")),
m_direction           (upnpLib.Element_GetChildValueByTag(argument, "direction")),
m_retval              (upnpLib.Element_GetFirstChildByTag(argument, "retval")),
m_relatedStateVariable(upnpLib.Element_GetChildValueByTag(argument, "relatedStateVariable"))
{
	std::ostringstream msg;
	msg <<	"\n    Argument:"                  <<
		"\n        name: "                 << m_name <<
		"\n        direction: "            << m_direction <<
		"\n        retval: "               << m_retval <<
		"\n        relatedStateVariable: " << m_relatedStateVariable;
        qDebug("UPnP: %s", msg.str().c_str());
}


CUPnPAction::CUPnPAction(
	const CUPnPControlPoint &upnpControlPoint,
	CUPnPLib &upnpLib,
	IXML_Element *action,
	const std::string &SCPDURL)
:
m_UPnPControlPoint(upnpControlPoint),
m_ArgumentList(upnpControlPoint, upnpLib, action, SCPDURL),
m_name(upnpLib.Element_GetChildValueByTag(action, "name"))
{
	std::ostringstream msg;
	msg <<	"\n    Action:"    <<
		"\n        name: " << m_name;
        qDebug("UPnP: %s", msg.str().c_str());
}


CUPnPAllowedValue::CUPnPAllowedValue(
	const CUPnPControlPoint &upnpControlPoint,
	CUPnPLib &upnpLib,
	IXML_Element *allowedValue,
	const std::string &)
:
m_UPnPControlPoint(upnpControlPoint),
m_allowedValue(upnpLib.Element_GetTextValue(allowedValue))
{
	std::ostringstream msg;
	msg <<	"\n    AllowedValue:"      <<
		"\n        allowedValue: " << m_allowedValue;
        qDebug("UPnP: %s", msg.str().c_str());
}


CUPnPStateVariable::CUPnPStateVariable(
	const CUPnPControlPoint &upnpControlPoint,
	CUPnPLib &upnpLib,
	IXML_Element *stateVariable,
	const std::string &SCPDURL)
:
m_UPnPControlPoint(upnpControlPoint),
m_AllowedValueList(upnpControlPoint, upnpLib, stateVariable, SCPDURL),
m_name        (upnpLib.Element_GetChildValueByTag(stateVariable, "name")),
m_dataType    (upnpLib.Element_GetChildValueByTag(stateVariable, "dataType")),
m_defaultValue(upnpLib.Element_GetChildValueByTag(stateVariable, "defaultValue")),
m_sendEvents  (upnpLib.Element_GetAttributeByTag (stateVariable, "sendEvents"))
{
	std::ostringstream msg;
	msg <<	"\n    StateVariable:"     <<
		"\n        name: "         << m_name <<
		"\n        dataType: "     << m_dataType <<
		"\n        defaultValue: " << m_defaultValue <<
		"\n        sendEvents: "   << m_sendEvents;
        qDebug("UPnP: %s", msg.str().c_str());
}


CUPnPSCPD::CUPnPSCPD(
	const CUPnPControlPoint &upnpControlPoint,
	CUPnPLib &upnpLib,
	IXML_Element *scpd,
	const std::string &SCPDURL)
:
m_UPnPControlPoint(upnpControlPoint),
m_ActionList(upnpControlPoint, upnpLib, scpd, SCPDURL),
m_ServiceStateTable(upnpControlPoint, upnpLib, scpd, SCPDURL),
m_SCPDURL(SCPDURL)
{
}


CUPnPArgumentValue::CUPnPArgumentValue()
:
m_argument(),
m_value()
{
}


CUPnPArgumentValue::CUPnPArgumentValue(
	const std::string &argument, const std::string &value)
:
m_argument(argument),
m_value(value)
{
}


CUPnPService::CUPnPService(
	const CUPnPControlPoint &upnpControlPoint,
	CUPnPLib &upnpLib,
	IXML_Element *service,
	const std::string &URLBase)
:
m_UPnPControlPoint(upnpControlPoint),
m_upnpLib(upnpLib),
m_serviceType(upnpLib.Element_GetChildValueByTag(service, "serviceType")),
m_serviceId  (upnpLib.Element_GetChildValueByTag(service, "serviceId")),
m_SCPDURL    (upnpLib.Element_GetChildValueByTag(service, "SCPDURL")),
m_controlURL (upnpLib.Element_GetChildValueByTag(service, "controlURL")),
m_eventSubURL(upnpLib.Element_GetChildValueByTag(service, "eventSubURL")),
m_timeout(1801),
m_SCPD(NULL)
{
	std::ostringstream msg;
	int errcode;

	char *scpdURL = new char[URLBase.length() + m_SCPDURL.length() + 1];
	errcode = UpnpResolveURL(
		(char*)URLBase.c_str(),
		(char*)m_SCPDURL.c_str(),
		scpdURL);
	if( errcode != UPNP_E_SUCCESS ) {
		msg << "Error generating scpdURL from " <<
			"|" << URLBase << "|" <<
			m_SCPDURL << "|.";
                qDebug("UPnP: %s", msg.str().c_str());
	} else {
		m_absSCPDURL = scpdURL;
	}

	char *controlURL = new char[
		URLBase.length() + m_controlURL.length() + 1];
	errcode = UpnpResolveURL(
		(char*)URLBase.c_str(),
		(char*)m_controlURL.c_str(),
		controlURL);
	if( errcode != UPNP_E_SUCCESS ) {
		msg << "Error generating controlURL from " <<
			"|" << URLBase << "|" <<
			m_controlURL << "|.";
                qDebug("UPnP: %s", msg.str().c_str());
	} else {
		m_absControlURL = controlURL;
	}

	char *eventURL = new char[
		URLBase.length() + m_eventSubURL.length() + 1];
	errcode = UpnpResolveURL(
		(char*)URLBase.c_str(),
		(char*)m_eventSubURL.c_str(),
		eventURL);
	if( errcode != UPNP_E_SUCCESS ) {
		msg << "Error generating eventURL from " <<
			"|" << URLBase << "|" <<
			m_eventSubURL << "|.";
                qDebug("UPnP: %s", msg.str().c_str());
	} else {
		m_absEventSubURL = eventURL;
	}

	msg <<	"\n    Service:"             <<
		"\n        serviceType: "    << m_serviceType <<
		"\n        serviceId: "      << m_serviceId <<
		"\n        SCPDURL: "        << m_SCPDURL <<
		"\n        absSCPDURL: "     << m_absSCPDURL <<
		"\n        controlURL: "     << m_controlURL <<
		"\n        absControlURL: "  << m_absControlURL <<
		"\n        eventSubURL: "    << m_eventSubURL <<
		"\n        absEventSubURL: " << m_absEventSubURL;
        qDebug("UPnP: %s", msg.str().c_str());

	if (	m_serviceType == upnpLib.UPNP_SERVICE_WAN_IP_CONNECTION ||
		m_serviceType == upnpLib.UPNP_SERVICE_WAN_PPP_CONNECTION) {
		//if (!upnpLib.m_ctrlPoint.WanServiceDetected()) {
			// This condition can be used to suspend the parse
			// of the XML tree.
			upnpLib.m_ctrlPoint.SetWanService(this);
			// Log it
			msg.str("");
			msg << "WAN Service Detected: '" <<
				m_serviceType << "'.";
                        qDebug("UPnP: %s", msg.str().c_str());
			// Subscribe
			upnpLib.m_ctrlPoint.Subscribe(*this);
#if 0
		} else {
			msg.str("");
			msg << "WAN service detected again: '" <<
				m_serviceType <<
				"'. Will only use the first instance.";
                        qDebug("UPnP: %s", msg.str().c_str());
		}
#endif
	} else {
		msg.str("");
		msg << "Uninteresting service detected: '" <<
			m_serviceType << "'. Ignoring.";
                qDebug("UPnP: %s", msg.str().c_str());
	}
}


CUPnPService::~CUPnPService()
{
}


bool CUPnPService::Execute(
	const std::string &ActionName,
	const std::vector<CUPnPArgumentValue> &ArgValue) const
{
	std::ostringstream msg;
	std::ostringstream msgAction("Sending action ");
	// Check for correct action name
	ActionList::const_iterator itAction =
		m_SCPD->GetActionList().find(ActionName);
	if (itAction == m_SCPD->GetActionList().end()) {
		msg << "Invalid action name '" << ActionName <<
			"' for service '" << GetServiceType() << "'.";
                qDebug("UPnP: %s", msg.str().c_str());
		return false;
	}
	msgAction << ActionName << "(";
	bool firstTime = true;
	// Check for correct Argument/Value pairs
	const CUPnPAction &action = *(itAction->second);
	for (unsigned int i = 0; i < ArgValue.size(); ++i) {
		ArgumentList::const_iterator itArg =
			action.GetArgumentList().find(ArgValue[i].GetArgument());
		if (itArg == action.GetArgumentList().end()) {
			msg << "Invalid argument name '" << ArgValue[i].GetArgument() <<
				"' for action '" << action.GetName() <<
				"' for service '" << GetServiceType() << "'.";
                        qDebug("UPnP: %s", msg.str().c_str());
			return false;
		}
		const CUPnPArgument &argument = *(itArg->second);
		if (tolower(argument.GetDirection()[0]) != 'i' ||
		    tolower(argument.GetDirection()[1]) != 'n') {
			msg << "Invalid direction for argument '" <<
				ArgValue[i].GetArgument() <<
				"' for action '" << action.GetName() <<
				"' for service '" << GetServiceType() << "'.";
                        qDebug("UPnP: %s", msg.str().c_str());
			return false;
		}
		const std::string relatedStateVariableName =
			argument.GetRelatedStateVariable();
		if (!relatedStateVariableName.empty()) {
			ServiceStateTable::const_iterator itSVT =
				m_SCPD->GetServiceStateTable().
				find(relatedStateVariableName);
			if (itSVT == m_SCPD->GetServiceStateTable().end()) {
				msg << "Inconsistent Service State Table, did not find '" <<
					relatedStateVariableName <<
					"' for argument '" << argument.GetName() <<
					"' for action '" << action.GetName() <<
					"' for service '" << GetServiceType() << "'.";
                                qDebug("UPnP: %s", msg.str().c_str());
				return false;
			}
			const CUPnPStateVariable &stateVariable = *(itSVT->second);
			if (	!stateVariable.GetAllowedValueList().empty() &&
				stateVariable.GetAllowedValueList().find(ArgValue[i].GetValue()) ==
					stateVariable.GetAllowedValueList().end()) {
				msg << "Value not allowed '" << ArgValue[i].GetValue() <<
					"' for state variable '" << relatedStateVariableName <<
					"' for argument '" << argument.GetName() <<
					"' for action '" << action.GetName() <<
					"' for service '" << GetServiceType() << "'.";
                                qDebug("UPnP: %s", msg.str().c_str());
				return false;
			}
		}
		if (firstTime) {
			firstTime = false;
		} else {
			msgAction << ", ";
		}
		msgAction <<
			ArgValue[i].GetArgument() <<
			"='" <<
			ArgValue[i].GetValue() <<
			"'";
	}
	msgAction << ")";
        qDebug("UPnP: %s", msgAction.str().c_str());
	// Everything is ok, make the action
	IXML_Document *ActionDoc = NULL;
	if (ArgValue.size()) {
		for (unsigned int i = 0; i < ArgValue.size(); ++i) {
			int ret = UpnpAddToAction(
				&ActionDoc,
				(char*)action.GetName().c_str(),
				(char*)GetServiceType().c_str(),
				(char*)ArgValue[i].GetArgument().c_str(),
				(char*)ArgValue[i].GetValue().c_str());
			if (ret != UPNP_E_SUCCESS) {
				m_upnpLib.processUPnPErrorMessage(
					"m_UpnpAddToAction", ret, NULL, NULL);
				return false;
			}
		}
	} else {
		ActionDoc = UpnpMakeAction(
			(char*)action.GetName().c_str(),
			(char*)GetServiceType().c_str(),
			0, NULL);
		if (!ActionDoc) {
			msg << "Error: m_UpnpMakeAction returned NULL.";
                        qDebug("UPnP: %s", msg.str().c_str());
			return false;
		}
	}

	// Send the action synchronously
	IXML_Document *RespDoc = NULL;
	int ret = UpnpSendAction(
		m_UPnPControlPoint.GetUPnPClientHandle(),
		(char*)GetAbsControlURL().c_str(),
		(char*)GetServiceType().c_str(),
		NULL, ActionDoc, &RespDoc);
	if (ret != UPNP_E_SUCCESS) {
		m_upnpLib.processUPnPErrorMessage(
			"m_UpnpSendAction", ret, NULL, RespDoc);
		ixmlDocument_free(ActionDoc);
		ixmlDocument_free(RespDoc);
		return false;
	}
	ixmlDocument_free(ActionDoc);

	// Check the response document
	m_upnpLib.ProcessActionResponse(
		RespDoc, action.GetName());

	// Free the response document
	ixmlDocument_free(RespDoc);

	return true;
}


const std::string CUPnPService::GetStateVariable(
	const std::string &stateVariableName) const
{
	std::ostringstream msg;
	DOMString StVarVal;
	int ret = UpnpGetServiceVarStatus(
		m_UPnPControlPoint.GetUPnPClientHandle(),
		(char*)GetAbsControlURL().c_str(),
		(char*)stateVariableName.c_str(),
		&StVarVal);
	if (ret != UPNP_E_SUCCESS) {
		msg << "GetStateVariable(\"" <<
			stateVariableName <<
			"\"): in a call to m_UpnpGetServiceVarStatus";
		m_upnpLib.processUPnPErrorMessage(
			msg.str(), ret, StVarVal, NULL);
		return stdEmptyString;
	}
	msg << "GetStateVariable: " <<
		stateVariableName <<
		"='" <<
		StVarVal <<
		"'.";
        qDebug("UPnP: %s", msg.str().c_str());
	return stdEmptyString;
}


CUPnPDevice::CUPnPDevice(
	const CUPnPControlPoint &upnpControlPoint,
	CUPnPLib &upnpLib,
	IXML_Element *device,
	const std::string &URLBase)
:
m_UPnPControlPoint(upnpControlPoint),
m_DeviceList(upnpControlPoint, upnpLib, device, URLBase),
m_ServiceList(upnpControlPoint, upnpLib, device, URLBase),
m_deviceType       (upnpLib.Element_GetChildValueByTag(device, "deviceType")),
m_friendlyName     (upnpLib.Element_GetChildValueByTag(device, "friendlyName")),
m_manufacturer     (upnpLib.Element_GetChildValueByTag(device, "manufacturer")),
m_manufacturerURL  (upnpLib.Element_GetChildValueByTag(device, "manufacturerURL")),
m_modelDescription (upnpLib.Element_GetChildValueByTag(device, "modelDescription")),
m_modelName        (upnpLib.Element_GetChildValueByTag(device, "modelName")),
m_modelNumber      (upnpLib.Element_GetChildValueByTag(device, "modelNumber")),
m_modelURL         (upnpLib.Element_GetChildValueByTag(device, "modelURL")),
m_serialNumber     (upnpLib.Element_GetChildValueByTag(device, "serialNumber")),
m_UDN              (upnpLib.Element_GetChildValueByTag(device, "UDN")),
m_UPC              (upnpLib.Element_GetChildValueByTag(device, "UPC")),
m_presentationURL  (upnpLib.Element_GetChildValueByTag(device, "presentationURL"))
{
	std::ostringstream msg;
	int presURLlen = strlen(URLBase.c_str()) +
		strlen(m_presentationURL.c_str()) + 2;
	char presURL[presURLlen];
	int errcode = UpnpResolveURL(
		(char*)URLBase.c_str(),
		(char*)m_presentationURL.c_str(),
		presURL);
	if (errcode != UPNP_E_SUCCESS) {
		msg << "Error generating presentationURL from " <<
			"|" << URLBase << "|" <<
			m_presentationURL << "|.";
                qDebug("UPnP: %s", msg.str().c_str());
	} else {
		m_presentationURL = presURL;
	}

	msg.str("");
	msg <<	"\n    Device: "                <<
		"\n        friendlyName: "      << m_friendlyName <<
		"\n        deviceType: "        << m_deviceType <<
		"\n        manufacturer: "      << m_manufacturer <<
		"\n        manufacturerURL: "   << m_manufacturerURL <<
		"\n        modelDescription: "  << m_modelDescription <<
		"\n        modelName: "         << m_modelName <<
		"\n        modelNumber: "       << m_modelNumber <<
		"\n        modelURL: "          << m_modelURL <<
		"\n        serialNumber: "      << m_serialNumber <<
		"\n        UDN: "               << m_UDN <<
		"\n        UPC: "               << m_UPC <<
		"\n        presentationURL: "   << m_presentationURL;
        qDebug("UPnP: %s", msg.str().c_str());
}


CUPnPRootDevice::CUPnPRootDevice(
	const CUPnPControlPoint &upnpControlPoint,
	CUPnPLib &upnpLib,
	IXML_Element *rootDevice,
	const std::string &OriginalURLBase,
	const std::string &FixedURLBase,
	const char *location,
	int expires)
:
CUPnPDevice(upnpControlPoint, upnpLib, rootDevice, FixedURLBase),
m_UPnPControlPoint(upnpControlPoint),
m_URLBase(OriginalURLBase),
m_location(location),
m_expires(expires)
{
	std::ostringstream msg;
	msg <<
		"\n    Root Device: "       <<
		"\n        URLBase: "       << m_URLBase <<
		"\n        Fixed URLBase: " << FixedURLBase <<
		"\n        location: "      << m_location <<
		"\n        expires: "       << m_expires;
        qDebug("UPnP: %s", msg.str().c_str());
}


CUPnPControlPoint *CUPnPControlPoint::s_CtrlPoint = NULL;


CUPnPControlPoint::CUPnPControlPoint(unsigned short udpPort)
:
m_upnpLib(*this),
m_UPnPClientHandle(),
m_RootDeviceMap(),
m_ServiceMap(),
m_ActivePortMappingsMap(),
m_IGWDeviceDetected(false),
m_WanService(NULL),
m_UPnPPort(udpPort)
{
	// Pointer to self
	s_CtrlPoint = this;
	// Null string at first
	std::ostringstream msg;

	// Start UPnP
	int ret;
	char *ipAddress = NULL;
	unsigned short port = 0;
	ret = UpnpInit(ipAddress, udpPort);
	if (ret != UPNP_E_SUCCESS) {
		msg << "error(UpnpInit): Error code ";
		goto error;
	}
	port = UpnpGetServerPort();
	ipAddress = UpnpGetServerIpAddress();
	msg << "bound to " << ipAddress << ":" <<
		port << ".";
        qDebug("UPnP: %s", msg.str().c_str());
	msg.str("");
	ret = UpnpRegisterClient(
		static_cast<Upnp_FunPtr>(&CUPnPControlPoint::Callback),
		&m_UPnPClientHandle,
		&m_UPnPClientHandle);
	if (ret != UPNP_E_SUCCESS) {
		msg << "error(UpnpRegisterClient): Error registering callback: ";
		goto error;
	}

	// We could ask for just the right device here. If the root device
	// contains the device we want, it will respond with the full XML doc,
	// including the root device and every sub-device it has.
	//
	// But lets find out what we have in our network by calling UPNP_ROOT_DEVICE.
	//
	// We should not search twice, because this will produce two
	// UPNP_DISCOVERY_SEARCH_TIMEOUT events, and we might end with problems
	// on the mutex.
	ret = UpnpSearchAsync(m_UPnPClientHandle, 3, m_upnpLib.UPNP_ROOT_DEVICE.c_str(), NULL);
	//ret = UpnpSearchAsync(m_UPnPClientHandle, 3, m_upnpLib.UPNP_DEVICE_IGW.c_str(), this);
	//ret = UpnpSearchAsync(m_UPnPClientHandle, 3, m_upnpLib.UPNP_DEVICE_LAN.c_str(), this);
	//ret = UpnpSearchAsync(m_UPnPClientHandle, 3, m_upnpLib.UPNP_DEVICE_WAN_CONNECTION.c_str(), this);
	if (ret != UPNP_E_SUCCESS) {
		msg << "error(UpnpSearchAsync): Error sending search request: ";
		goto error;
	}

	// Wait for the UPnP initialization to complete.
	{
		// Lock the search timeout mutex
		m_WaitForSearchTimeout.lock();

		// Lock it again, so that we block. Unlocking will only happen
		// when the UPNP_DISCOVERY_SEARCH_TIMEOUT event occurs at the
		// callback.
                searchTimeoutCondition.wait(&m_WaitForSearchTimeout);
// 		QMutexLocker lock(&m_WaitForSearchTimeout);s
                m_WaitForSearchTimeout.unlock();
	}
	return;

	// Error processing
error:
	UpnpFinish();
	msg << ret << ": " << m_upnpLib.GetUPnPErrorMessage(ret) << ".";
        qDebug("UPnP: %s", msg.str().c_str());
}


CUPnPControlPoint::~CUPnPControlPoint()
{
	for(	RootDeviceMap::iterator it = m_RootDeviceMap.begin();
		it != m_RootDeviceMap.end();
		++it) {
		delete it->second;
	}
	// Remove all first
	// RemoveAll();
	UpnpUnRegisterClient(m_UPnPClientHandle);
	UpnpFinish();
}


bool CUPnPControlPoint::AddPortMappings(
	std::vector<CUPnPPortMapping> &upnpPortMapping)
{
	std::ostringstream msg;
	if (!WanServiceDetected()) {
                emit noWanServiceDetected();
		msg <<  "UPnP Error: "
			"CUPnPControlPoint::AddPortMapping: "
			"Wan Service not detected.";
                qDebug("UPnP: %s", msg.str().c_str());
		return false;
	}

	int n = upnpPortMapping.size();
	bool ok = false;

	// Check the number of port mappings before
	std::istringstream PortMappingNumberOfEntries(
		m_WanService->GetStateVariable(
			"PortMappingNumberOfEntries"));
	unsigned long oldNumberOfEntries;
	PortMappingNumberOfEntries >> oldNumberOfEntries;

	// Add the enabled port mappings
	for (int i = 0; i < n; ++i) {
		if (upnpPortMapping[i].getEnabled() == "1") {
			// Add the mapping to the control point
			// active mappings list
			m_ActivePortMappingsMap[upnpPortMapping[i].getKey()] =
				upnpPortMapping[i];

			// Add the port mapping
			PrivateAddPortMapping(upnpPortMapping[i]);
		}
	}

	// Test some variables, this is deprecated, might not work
	// with some routers
	m_WanService->GetStateVariable("ConnectionType");
	m_WanService->GetStateVariable("PossibleConnectionTypes");
	m_WanService->GetStateVariable("ConnectionStatus");
	m_WanService->GetStateVariable("Uptime");
	m_WanService->GetStateVariable("LastConnectionError");
	m_WanService->GetStateVariable("RSIPAvailable");
	m_WanService->GetStateVariable("NATEnabled");
	m_WanService->GetStateVariable("ExternalIPAddress");
	m_WanService->GetStateVariable("PortMappingNumberOfEntries");
	m_WanService->GetStateVariable("PortMappingLeaseDuration");

	// Just for testing
	std::vector<CUPnPArgumentValue> argval;
	argval.resize(0);
	m_WanService->Execute("GetStatusInfo", argval);

#if 0
	// These do not work. Their value must be requested for a
	// specific port mapping.
	m_WanService->GetStateVariable("PortMappingEnabled");
	m_WanService->GetStateVariable("RemoteHost");
	m_WanService->GetStateVariable("ExternalPort");
	m_WanService->GetStateVariable("InternalPort");
	m_WanService->GetStateVariable("PortMappingProtocol");
	m_WanService->GetStateVariable("InternalClient");
	m_WanService->GetStateVariable("PortMappingDescription");
#endif

	// Debug only
	msg.str("");
	msg << "CUPnPControlPoint::DeletePortMappings: "
		"m_ActivePortMappingsMap.size() == " <<
		m_ActivePortMappingsMap.size();
        qDebug("UPnP: %s", msg.str().c_str());

	// Not very good, must find a better test
	PortMappingNumberOfEntries.str(
		m_WanService->GetStateVariable(
			"PortMappingNumberOfEntries"));
	unsigned long newNumberOfEntries;
	PortMappingNumberOfEntries >> newNumberOfEntries;
	ok = newNumberOfEntries - oldNumberOfEntries == 4;

	return ok;
}


void CUPnPControlPoint::RefreshPortMappings()
{
	for (	PortMappingMap::iterator it = m_ActivePortMappingsMap.begin();
		it != m_ActivePortMappingsMap.end();
		++it) {
		PrivateAddPortMapping(it->second);
	}

	// For testing
	m_WanService->GetStateVariable("PortMappingNumberOfEntries");
}


bool CUPnPControlPoint::PrivateAddPortMapping(
	CUPnPPortMapping &upnpPortMapping)
{
	// Get an IP address. The UPnP server one must do.
	std::string ipAddress(UpnpGetServerIpAddress());

	// Start building the action
	std::string actionName("AddPortMapping");
	std::vector<CUPnPArgumentValue> argval(8);

	// Action parameters
	argval[0].SetArgument("NewRemoteHost");
	argval[0].SetValue("");
	argval[1].SetArgument("NewExternalPort");
	argval[1].SetValue(upnpPortMapping.getPort());
	argval[2].SetArgument("NewProtocol");
	argval[2].SetValue(upnpPortMapping.getProtocol());
	argval[3].SetArgument("NewInternalPort");
	argval[3].SetValue(upnpPortMapping.getPort());
	argval[4].SetArgument("NewInternalClient");
	argval[4].SetValue(ipAddress);
	argval[5].SetArgument("NewEnabled");
	argval[5].SetValue("1");
	argval[6].SetArgument("NewPortMappingDescription");
	argval[6].SetValue(upnpPortMapping.getDescription());
	argval[7].SetArgument("NewLeaseDuration");
	argval[7].SetValue("0");

	// Execute
	bool ret = true;
	for (ServiceMap::iterator it = m_ServiceMap.begin();
	     it != m_ServiceMap.end(); ++it) {
		ret &= it->second->Execute(actionName, argval);
	}

	return ret;
}


bool CUPnPControlPoint::DeletePortMappings(
	std::vector<CUPnPPortMapping> &upnpPortMapping)
{
	std::ostringstream msg;
	if (!WanServiceDetected()) {
		msg <<  "UPnP Error: "
			"CUPnPControlPoint::DeletePortMapping: "
			"Wan Service not detected.";
                qDebug("UPnP: %s", msg.str().c_str());
		return false;
	}

	int n = upnpPortMapping.size();
	bool ok = false;

	// Check the number of port mappings before
	std::istringstream PortMappingNumberOfEntries(
		m_WanService->GetStateVariable(
			"PortMappingNumberOfEntries"));
	unsigned long oldNumberOfEntries;
	PortMappingNumberOfEntries >> oldNumberOfEntries;

	// Delete the enabled port mappings
	for (int i = 0; i < n; ++i) {
		if (upnpPortMapping[i].getEnabled() == "1") {
			// Delete the mapping from the control point
			// active mappings list
			PortMappingMap::iterator it =
				m_ActivePortMappingsMap.find(
					upnpPortMapping[i].getKey());
			if (it != m_ActivePortMappingsMap.end()) {
				m_ActivePortMappingsMap.erase(it);
			} else {
				msg <<  "UPnP Error: "
					"CUPnPControlPoint::DeletePortMapping: "
					"Mapping was not found in the active "
					"mapping map.";
                                qDebug("UPnP: %s", msg.str().c_str());
			}

			// Delete the port mapping
			PrivateDeletePortMapping(upnpPortMapping[i]);
		}
	}

	// Debug only
	msg.str("");
	msg << "CUPnPControlPoint::DeletePortMappings: "
		"m_ActivePortMappingsMap.size() == " <<
		m_ActivePortMappingsMap.size();
        qDebug("UPnP: %s", msg.str().c_str());

	// Not very good, must find a better test
	PortMappingNumberOfEntries.str(
		m_WanService->GetStateVariable(
			"PortMappingNumberOfEntries"));
	unsigned long newNumberOfEntries;
	PortMappingNumberOfEntries >> newNumberOfEntries;
	ok = oldNumberOfEntries - newNumberOfEntries == 4;

	return ok;
}


bool CUPnPControlPoint::PrivateDeletePortMapping(
	CUPnPPortMapping &upnpPortMapping)
{
	// Start building the action
	std::string actionName("DeletePortMapping");
	std::vector<CUPnPArgumentValue> argval(3);

	// Action parameters
	argval[0].SetArgument("NewRemoteHost");
	argval[0].SetValue("");
	argval[1].SetArgument("NewExternalPort");
	argval[1].SetValue(upnpPortMapping.getPort());
	argval[2].SetArgument("NewProtocol");
	argval[2].SetValue(upnpPortMapping.getProtocol());

	// Execute
	bool ret = true;
	for (ServiceMap::iterator it = m_ServiceMap.begin();
	     it != m_ServiceMap.end(); ++it) {
		ret &= it->second->Execute(actionName, argval);
	}

	return ret;
}


// This function is static
int CUPnPControlPoint::Callback(Upnp_EventType EventType, void *Event, void * /*Cookie*/)
{
	std::ostringstream msg;
	// Somehow, this is unreliable. UPNP_DISCOVERY_ADVERTISEMENT_ALIVE events
	// happen with a wrong cookie and... boom!
	// CUPnPControlPoint *upnpCP = static_cast<CUPnPControlPoint *>(Cookie);
	CUPnPControlPoint *upnpCP = CUPnPControlPoint::s_CtrlPoint;

	//fprintf(stderr, "Callback: %d, Cookie: %p\n", EventType, Cookie);
	switch (EventType) {
	case UPNP_DISCOVERY_ADVERTISEMENT_ALIVE:
		//fprintf(stderr, "Callback: UPNP_DISCOVERY_ADVERTISEMENT_ALIVE\n");
		msg << "error(UPNP_DISCOVERY_ADVERTISEMENT_ALIVE): ";
		goto upnpDiscovery;
	case UPNP_DISCOVERY_SEARCH_RESULT: {
		//fprintf(stderr, "Callback: UPNP_DISCOVERY_SEARCH_RESULT\n");
		msg << "error(UPNP_DISCOVERY_SEARCH_RESULT): ";
		// UPnP Discovery
upnpDiscovery:
		struct Upnp_Discovery *d_event = (struct Upnp_Discovery *)Event;
		IXML_Document *doc = NULL;
		int ret;
		if (d_event->ErrCode != UPNP_E_SUCCESS) {
			msg << upnpCP->m_upnpLib.GetUPnPErrorMessage(d_event->ErrCode) << ".";
                        qDebug("UPnP: %s", msg.str().c_str());
		}
		// Get the XML tree device description in doc
		ret = UpnpDownloadXmlDoc(d_event->Location, &doc);
		if (ret != UPNP_E_SUCCESS) {
			msg << "Error retrieving device description from " <<
				d_event->Location << ": " <<
				upnpCP->m_upnpLib.GetUPnPErrorMessage(ret) << ".";
                                qDebug("UPnP: %s", msg.str().c_str());
		}
		if (doc) {
			// Get the root node
			IXML_Element *root =
				upnpCP->m_upnpLib.Element_GetRootElement(doc);
			// Extract the URLBase
			const std::string urlBase = upnpCP->m_upnpLib.
				Element_GetChildValueByTag(root, "URLBase");
			// Get the root device
			IXML_Element *rootDevice = upnpCP->m_upnpLib.
				Element_GetFirstChildByTag(root, "device");
			// Extract the deviceType
			std::string devType(upnpCP->m_upnpLib.
				Element_GetChildValueByTag(rootDevice, "deviceType"));
			// Only add device if it is an InternetGatewayDevice
                        if(QString(devType.c_str()).toUpper() == QString(upnpCP->m_upnpLib.UPNP_DEVICE_IGW.c_str()).toUpper()){
				// This condition can be used to auto-detect
				// the UPnP device we are interested in.
				// Obs.: Don't block the entry here on this
				// condition! There may be more than one device,
				// and the first that enters may not be the one
				// we are interested in!
				upnpCP->SetIGWDeviceDetected(true);
				// Log it if not UPNP_DISCOVERY_ADVERTISEMENT_ALIVE,
				// we don't want to spam our logs.
				if (EventType != UPNP_DISCOVERY_ADVERTISEMENT_ALIVE) {
					msg.str("Internet Gateway Device Detected.");
					qDebug("UPnP: %s", msg.str().c_str());
				}
				// Add the root device to our list
				upnpCP->AddRootDevice(rootDevice, urlBase,
					d_event->Location, d_event->Expires);
			}
			// Free the XML doc tree
			ixmlDocument_free(doc);
		}
		break;
	}
	case UPNP_DISCOVERY_SEARCH_TIMEOUT: {
		//fprintf(stderr, "Callback: UPNP_DISCOVERY_SEARCH_TIMEOUT\n");
		// Search timeout
		msg << "UPNP_DISCOVERY_SEARCH_TIMEOUT.";
                qDebug("UPnP: %s", msg.str().c_str());

		// Unlock the search timeout mutex
// 		upnpCP->m_WaitForSearchTimeout.unlock();
                upnpCP->searchTimeoutCondition.wakeOne();

		break;
	}
	case UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE: {
		//fprintf(stderr, "Callback: UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE\n");
		// UPnP Device Removed
		struct Upnp_Discovery *dab_event = (struct Upnp_Discovery *)Event;
		if (dab_event->ErrCode != UPNP_E_SUCCESS) {
			msg << "error(UPNP_DISCOVERY_ADVERTISEMENT_BYEBYE): " <<
				upnpCP->m_upnpLib.GetUPnPErrorMessage(dab_event->ErrCode) <<
				".";
                        qDebug("UPnP: %s", msg.str().c_str());
		}
		std::string devType = dab_event->DeviceType;
		// Check for an InternetGatewayDevice and removes it from the list
		if (QString(devType.c_str()).toUpper() == QString(upnpCP->m_upnpLib.UPNP_DEVICE_IGW.c_str()).toUpper()) {
			upnpCP->RemoveRootDevice(dab_event->DeviceId);
		}
		break;
	}
	case UPNP_EVENT_RECEIVED: {
		//fprintf(stderr, "Callback: UPNP_EVENT_RECEIVED\n");
		// Event reveived
		struct Upnp_Event *e_event = (struct Upnp_Event *)Event;
		const std::string Sid = e_event->Sid;
		// Parses the event
		upnpCP->OnEventReceived(Sid, e_event->EventKey, e_event->ChangedVariables);
		break;
	}
	case UPNP_EVENT_SUBSCRIBE_COMPLETE:
		//fprintf(stderr, "Callback: UPNP_EVENT_SUBSCRIBE_COMPLETE\n");
		msg << "error(UPNP_EVENT_SUBSCRIBE_COMPLETE): ";
		goto upnpEventRenewalComplete;
	case UPNP_EVENT_UNSUBSCRIBE_COMPLETE:
		//fprintf(stderr, "Callback: UPNP_EVENT_UNSUBSCRIBE_COMPLETE\n");
		msg << "error(UPNP_EVENT_UNSUBSCRIBE_COMPLETE): ";
		goto upnpEventRenewalComplete;
	case UPNP_EVENT_RENEWAL_COMPLETE: {
		//fprintf(stderr, "Callback: UPNP_EVENT_RENEWAL_COMPLETE\n");
		msg << "error(UPNP_EVENT_RENEWAL_COMPLETE): ";
upnpEventRenewalComplete:
		struct Upnp_Event_Subscribe *es_event =
			(struct Upnp_Event_Subscribe *)Event;
		if (es_event->ErrCode != UPNP_E_SUCCESS) {
			msg << "Error in Event Subscribe Callback";
			upnpCP->m_upnpLib.processUPnPErrorMessage(
				msg.str(), es_event->ErrCode, NULL, NULL);
		}

		break;
	}

	case UPNP_EVENT_AUTORENEWAL_FAILED:
		//fprintf(stderr, "Callback: UPNP_EVENT_AUTORENEWAL_FAILED\n");
		msg << "error(UPNP_EVENT_AUTORENEWAL_FAILED): ";
		goto upnpEventSubscriptionExpired;
	case UPNP_EVENT_SUBSCRIPTION_EXPIRED: {
		//fprintf(stderr, "Callback: UPNP_EVENT_SUBSCRIPTION_EXPIRED\n");
		msg << "error(UPNP_EVENT_SUBSCRIPTION_EXPIRED): ";
upnpEventSubscriptionExpired:
		struct Upnp_Event_Subscribe *es_event =
			(struct Upnp_Event_Subscribe *)Event;
		Upnp_SID newSID;
		int TimeOut = 1801;
		int ret = UpnpSubscribe(
			upnpCP->m_UPnPClientHandle,
			es_event->PublisherUrl,
			&TimeOut,
			newSID);
		if (ret != UPNP_E_SUCCESS) {
			msg << "Error Subscribing to EventURL";
			upnpCP->m_upnpLib.processUPnPErrorMessage(
				msg.str(), es_event->ErrCode, NULL, NULL);
		} else {
			ServiceMap::iterator it =
				upnpCP->m_ServiceMap.find(es_event->PublisherUrl);
			if (it != upnpCP->m_ServiceMap.end()) {
				CUPnPService &service = *(it->second);
				service.SetTimeout(TimeOut);
				service.SetSID(newSID);
				msg << "Re-subscribed to EventURL '" <<
					es_event->PublisherUrl <<
					"' with SID == '" <<
					newSID << "'.";
                                qDebug("UPnP: %s", msg.str().c_str());
				// In principle, we should test to see if the
				// service is the same. But here we only have one
				// service, so...
				upnpCP->RefreshPortMappings();
			} else {
				msg << "Error: did not find service " <<
					newSID << " in the service map.";
                                qDebug("UPnP: %s", msg.str().c_str());
			}
		}
		break;
	}
	case UPNP_CONTROL_ACTION_COMPLETE: {
		//fprintf(stderr, "Callback: UPNP_CONTROL_ACTION_COMPLETE\n");
		// This is here if we choose to do this asynchronously
		struct Upnp_Action_Complete *a_event =
			(struct Upnp_Action_Complete *)Event;
		if (a_event->ErrCode != UPNP_E_SUCCESS) {
			upnpCP->m_upnpLib.processUPnPErrorMessage(
				"m_UpnpSendActionAsync",
				a_event->ErrCode, NULL,
				a_event->ActionResult);
		} else {
			// Check the response document
			upnpCP->m_upnpLib.ProcessActionResponse(
				a_event->ActionResult,
				"<m_UpnpSendActionAsync>");
		}
		/* No need for any processing here, just print out results.
		 * Service state table updates are handled by events.
		 */
		break;
	}
	case UPNP_CONTROL_GET_VAR_COMPLETE: {
		//fprintf(stderr, "Callback: UPNP_CONTROL_GET_VAR_COMPLETE\n");
		msg << "error(UPNP_CONTROL_GET_VAR_COMPLETE): ";
		struct Upnp_State_Var_Complete *sv_event =
			(struct Upnp_State_Var_Complete *)Event;
		if (sv_event->ErrCode != UPNP_E_SUCCESS) {
			msg << "m_UpnpGetServiceVarStatusAsync";
			upnpCP->m_upnpLib.processUPnPErrorMessage(
				msg.str(), sv_event->ErrCode, NULL, NULL);
		}
		break;
	}
	// ignore these cases, since this is not a device
	case UPNP_CONTROL_GET_VAR_REQUEST:
		//fprintf(stderr, "Callback: UPNP_CONTROL_GET_VAR_REQUEST\n");
		msg << "error(UPNP_CONTROL_GET_VAR_REQUEST): ";
		goto eventSubscriptionRequest;
	case UPNP_CONTROL_ACTION_REQUEST:
		//fprintf(stderr, "Callback: UPNP_CONTROL_ACTION_REQUEST\n");
		msg << "error(UPNP_CONTROL_ACTION_REQUEST): ";
		goto eventSubscriptionRequest;
	case UPNP_EVENT_SUBSCRIPTION_REQUEST:
		//fprintf(stderr, "Callback: UPNP_EVENT_SUBSCRIPTION_REQUEST\n");
		msg << "error(UPNP_EVENT_SUBSCRIPTION_REQUEST): ";
eventSubscriptionRequest:
		msg << "This is not a device, event ignored.";
                qDebug("UPnP: %s", msg.str().c_str());
		break;
	default:
		// Humm, this is not good, we forgot to handle something...
		fprintf(stderr,
			"Callback: default... Unknown event:'%d', not good.\n",
			EventType);
		msg << "error(UPnP::Callback): Event not handled:'" <<
			EventType << "'.";
                qDebug("UPnP: %s", msg.str().c_str());
		fprintf(stderr, "%s\n", msg.str().c_str());
		// Better not throw in the callback. Who would catch it?
		//throw CUPnPException(msg);
		break;
	}

	return 0;
}


void CUPnPControlPoint::OnEventReceived(
		const std::string &Sid,
		int EventKey,
		IXML_Document *ChangedVariablesDoc)
{
	std::ostringstream msg;
	msg << "UPNP_EVENT_RECEIVED:" <<
		"\n    SID: " << Sid <<
		"\n    Key: " << EventKey <<
		"\n    Property list:";
	IXML_Element *root =
		m_upnpLib.Element_GetRootElement(ChangedVariablesDoc);
	IXML_Element *child =
		m_upnpLib.Element_GetFirstChild(root);
	if (child) {
		while (child) {
			IXML_Element *child2 =
				m_upnpLib.Element_GetFirstChild(child);
			const DOMString childTag =
				m_upnpLib.Element_GetTag(child2);
			std::string childValue =
				m_upnpLib.Element_GetTextValue(child2);
			msg << "\n        " <<
				childTag << "='" <<
				childValue << "'";
			child = m_upnpLib.Element_GetNextSibling(child);
		}
	} else {
		msg << "\n    Empty property list.";
	}
        qDebug("UPnP: %s", msg.str().c_str());
	// Freeing that doc segfaults. Probably should not be freed.
	//m_upnpLib.m_ixmlDocument_free(ChangedVariablesDoc);
}


void CUPnPControlPoint::AddRootDevice(
	IXML_Element *rootDevice, const std::string &urlBase,
	const char *location, int expires)
{
	// Lock the Root Device List
	QMutexLocker lock(&m_RootDeviceListMutex);

	// Root node's URLBase
	std::string OriginalURLBase(urlBase);
	std::string FixedURLBase(OriginalURLBase.empty() ?
		location :
		OriginalURLBase);

	// Get the UDN (Unique Device Name)
	std::string UDN(
		m_upnpLib.Element_GetChildValueByTag(rootDevice, "UDN"));
	RootDeviceMap::iterator it = m_RootDeviceMap.find(UDN);
	bool alreadyAdded = it != m_RootDeviceMap.end();
	if (alreadyAdded) {
		// Just set the expires field
		it->second->SetExpires(expires);
	} else {
		// Add a new root device to the root device list
		CUPnPRootDevice *upnpRootDevice = new CUPnPRootDevice(
			*this, m_upnpLib, rootDevice,
			OriginalURLBase, FixedURLBase,
			location, expires);
		m_RootDeviceMap[upnpRootDevice->GetUDN()] = upnpRootDevice;
	}
}


void CUPnPControlPoint::RemoveRootDevice(const char *udn)
{
	// Lock the Root Device List
	QMutexLocker lock(&m_RootDeviceListMutex);

	// Remove
	std::string UDN(udn);
	RootDeviceMap::iterator it = m_RootDeviceMap.find(UDN);
	if (it != m_RootDeviceMap.end()) {
		delete it->second;
		m_RootDeviceMap.erase(UDN);
	}
}


void CUPnPControlPoint::Subscribe(CUPnPService &service)
{
	std::ostringstream msg;
	int errcode = UpnpSubscribe(m_UPnPClientHandle,
		service.GetAbsEventSubURL().c_str(),
		service.GetTimeoutAddr(),
		service.GetSID());
	if (errcode == UPNP_E_SUCCESS) {
		m_ServiceMap[service.GetAbsEventSubURL()] = &service;
		msg << "Successfully subscribed to service " <<
			service.GetServiceType() << ", absEventSubURL: " <<
			service.GetAbsEventSubURL() << ".";
                qDebug("UPnP: %s", msg.str().c_str());

		IXML_Document *scpdDoc = NULL;
		errcode = UpnpDownloadXmlDoc(
			service.GetAbsSCPDURL().c_str(), &scpdDoc);
		if (errcode == UPNP_E_SUCCESS) {
			// Get the root node
			IXML_Element *scpdRoot =
				m_upnpLib.Element_GetRootElement(scpdDoc);
			CUPnPSCPD *scpd = new CUPnPSCPD(*this, m_upnpLib,
				scpdRoot, service.GetAbsSCPDURL());
			service.SetSCPD(scpd);
		} else {
			msg.str("");
			msg << "Error getting SCPD Document from " <<
				service.GetAbsSCPDURL() << ".";
                        qDebug("UPnP: %s", msg.str().c_str());
		}
	} else {
		msg << "Error subscribing to service " <<
			service.GetServiceType() << ", absEventSubURL: " <<
			service.GetAbsEventSubURL() << ", error: " <<
			m_upnpLib.GetUPnPErrorMessage(errcode) << ".";
		goto error;
	}

	return;

	// Error processing
error:
    qDebug("UPnP: %s", msg.str().c_str());
}


void CUPnPControlPoint::Unsubscribe(CUPnPService &service)
{
	ServiceMap::iterator it = m_ServiceMap.find(service.GetAbsEventSubURL());
	m_ServiceMap.erase(it);
	UpnpUnSubscribe(m_UPnPClientHandle, service.GetSID());
}


// File_checked_for_headers
