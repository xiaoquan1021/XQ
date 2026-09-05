#ifndef XQ_XMLIO_UTIL_H
#define XQ_XMLIO_UTIL_H

#include <xqModuleCommonExports.h>

#include <mitkPoint.h>
#include <mitkVector.h>
#include <tinyxml2.h>

#include <string>

// Utility for reading/writing common types with tinyxml2
class XQMODULECOMMON_EXPORT xq_XmlIOUtil
{
public:
    explicit xq_XmlIOUtil(tinyxml2::XMLDocument& doc);
    xq_XmlIOUtil() = delete;

    tinyxml2::XMLElement* BuildCoordinateElement(const char* name, double v[3]);
    tinyxml2::XMLElement* BuildVertexElement(
        const char* name, int id, const mitk::Point3D& point);

    void ParseCoordinates(tinyxml2::XMLElement* element, double xyz[3]);
    mitk::Point3D ParsePoint3D(tinyxml2::XMLElement* element);
    mitk::Vector3D ParseVector3D(tinyxml2::XMLElement* element);

    static void WriteStringAttribute(tinyxml2::XMLElement* elem,
                             const char* name,
                             const std::string& value);
    static std::string ReadStringAttribute(tinyxml2::XMLElement* elem,
                                    const char* name,
                                    const std::string& defaultVal = "");

    tinyxml2::XMLDocument& document;
};

#endif // XQ_XMLIO_UTIL_H
