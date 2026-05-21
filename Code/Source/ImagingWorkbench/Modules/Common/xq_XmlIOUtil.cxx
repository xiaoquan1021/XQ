#include "xq_XmlIOUtil.h"

xq_XmlIOUtil::xq_XmlIOUtil(tinyxml2::XMLDocument& doc)
    : document(doc)
{
}

tinyxml2::XMLElement* xq_XmlIOUtil::BuildCoordinateElement(
    const char* name, double v[3])
{
    tinyxml2::XMLElement* elem = document.NewElement(name);
    elem->SetAttribute("x", v[0]);
    elem->SetAttribute("y", v[1]);
    elem->SetAttribute("z", v[2]);
    return elem;
}

tinyxml2::XMLElement* xq_XmlIOUtil::BuildVertexElement(
    const char* name, int id, const mitk::Point3D& point)
{
    tinyxml2::XMLElement* elem = document.NewElement(name);
    elem->SetAttribute("id", id);
    elem->SetAttribute("x", point[0]);
    elem->SetAttribute("y", point[1]);
    elem->SetAttribute("z", point[2]);
    return elem;
}

void xq_XmlIOUtil::ParseCoordinates(tinyxml2::XMLElement* element, double xyz[3])
{
    if (!element)
    {
        xyz[0] = xyz[1] = xyz[2] = 0.0;
        return;
    }
    xyz[0] = element->DoubleAttribute("x", 0.0);
    xyz[1] = element->DoubleAttribute("y", 0.0);
    xyz[2] = element->DoubleAttribute("z", 0.0);
}

mitk::Point3D xq_XmlIOUtil::ParsePoint3D(tinyxml2::XMLElement* element)
{
    mitk::Point3D point;
    if (!element)
    {
        point[0] = point[1] = point[2] = 0.0;
        return point;
    }
    point[0] = element->DoubleAttribute("x", 0.0);
    point[1] = element->DoubleAttribute("y", 0.0);
    point[2] = element->DoubleAttribute("z", 0.0);
    return point;
}

mitk::Vector3D xq_XmlIOUtil::ParseVector3D(tinyxml2::XMLElement* element)
{
    mitk::Vector3D vec;
    if (!element)
    {
        vec[0] = vec[1] = vec[2] = 0.0;
        return vec;
    }
    vec[0] = element->DoubleAttribute("x", 0.0);
    vec[1] = element->DoubleAttribute("y", 0.0);
    vec[2] = element->DoubleAttribute("z", 0.0);
    return vec;
}

void xq_XmlIOUtil::WriteStringAttribute(tinyxml2::XMLElement* elem,
                                 const char* name,
                                 const std::string& value)
{
    if (elem)
    {
        elem->SetAttribute(name, value.c_str());
    }
}

std::string xq_XmlIOUtil::ReadStringAttribute(tinyxml2::XMLElement* elem,
                                         const char* name,
                                         const std::string& defaultVal)
{
    if (!elem)
    {
        return defaultVal;
    }
    const char* val = elem->Attribute(name);
    if (val)
    {
        return std::string(val);
    }
    return defaultVal;
}
